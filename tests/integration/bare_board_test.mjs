// Unattended real-board CDC tests. Requires an explicitly confirmed port and
// existing baseline backups. Never saves Flash, changes identity, or reboots.
import fs from 'node:fs/promises';
import {spawn} from 'node:child_process';
import path from 'node:path';
import assert from 'node:assert/strict';
import {fileURLToPath} from 'node:url';
const here=path.dirname(fileURLToPath(import.meta.url)),root=path.resolve(here,'../..');
const out=process.env.FLUXRAIL_TEST_OUT?path.resolve(process.env.FLUXRAIL_TEST_OUT):path.join(here,'hardware_2026-10-04'),port=process.argv[2];
const expectedVersion=process.env.FLUXRAIL_TEST_VERSION;
const reviewGate=process.env.FLUXRAIL_REVIEW_GATE==='1';
if(!expectedVersion)throw Error('FLUXRAIL_TEST_VERSION must be explicit');
if(!process.env.FLUXRAIL_TEST_OUT)throw Error('FLUXRAIL_TEST_OUT must be explicit');
if(!reviewGate)throw Error('FLUXRAIL_REVIEW_GATE=1 is required for real-board stages');
try{await fs.access(path.join(out,'report.json'));throw Error('Run report exists; prepare a new evidence directory');}catch(e){if(e.code!=='ENOENT')throw e;}
if(!/^COM\d+$/.test(port||''))throw Error('Pass confirmed FluxRail COM port');
const originals=JSON.parse(await fs.readFile(path.join(out,'original-profiles.json'),'utf8'));
const baseline=JSON.parse(await fs.readFile(path.join(out,'baseline.json'),'utf8'));
const serial=baseline.hello.result.serial,active=baseline.get_device.result.active_profile;
const report={started:new Date().toISOString(),port,serial,kind:'real_board',stages:[],restored:false,flash_written:false,game_tested:false};
const sleep=ms=>new Promise(r=>setTimeout(r,ms)),clone=x=>structuredClone(x),changed=new Set();
let proc,lines=[],buffer='',errors='',closed=false,id=10000,revision=null,baselineConfirmed=false;
const latency=[];
async function save(){await fs.writeFile(path.join(out,'report.json'),JSON.stringify(report,null,2));}
async function stage(name,summary,result,next,interfaces,limits='外设缺失，不能证明物理键、传感器、灯色或游戏功能。'){
 const entry={name,time:new Date().toISOString(),summary,result,next,interfaces,limits,review_status:'pending'};report.stages.push(entry);await save();
 const pendingName=name.replaceAll('通过','完成，待审核');
 await fs.appendFile(path.join(root,'docs/progress/测试.md'),`\n## ${expectedVersion} ${pendingName}\n\n1. 当前完成/未完成：${summary} 本阶段尚待Luna审核，未作最终通过判定。\n2. 代码与接口：tests/integration/bare_board_test.mjs、serial_bridge.ps1；${interfaces}\n3. 决定及理由：只使用确认的${port}/CDC v1；RAM修改后读回与恢复，不写Flash、不换身份、不重启。\n4. 实际结果：实板 ${JSON.stringify(result)}。本脚本语法检查另记；编译/模拟未新增，游戏未执行。原始证据：${path.relative(root,out).replaceAll('\\','/')}/automated-cdc.jsonl、report.json。\n5. 未解决/依赖：${limits}\n6. 下一步：${next}；先等待Luna审核本阶段。\n`,'utf8');console.log(JSON.stringify(entry));
 if(reviewGate){
  const reviewPath=path.join(out,`${report.stages.length}.review.json`);
  console.log(JSON.stringify({waiting_for_luna_review:report.stages.length,reviewPath}));
  let review;
  while(!review){try{review=JSON.parse(await fs.readFile(reviewPath,'utf8'));}catch(e){if(e.code!=='ENOENT')throw e;await sleep(250);}}
  assert.equal(review.stage,report.stages.length);assert.equal(review.reviewer,'gpt-6-luna');
  assert.equal(review.run_started,report.started);assert.equal(review.stage_name,name);
  entry.review=review;entry.review_status=review.continue?'reviewed':'requires_stop';await save();
  await fs.appendFile(path.join(root,'docs/progress/测试.md'),`\nLuna审核第${review.stage}阶段：${review.evaluation}；继续决定：${review.continue?'继续':'停止'}。审核原文与范围见 ${path.relative(root,reviewPath).replaceAll('\\','/')}。\n`,'utf8');
  assert.equal(review.continue,true,'Luna review requires stopping');
 }
}
async function read(){const t=Date.now();while(!lines.length){if(closed||Date.now()-t>8000)throw Error('Bridge timeout/exit: '+errors);await sleep(2);}const r=JSON.parse(lines.shift());if(r.bridge_error)throw Error(r.bridge_error);return r;}
async function log(req,resp,ms){await fs.appendFile(path.join(out,'automated-cdc.jsonl'),JSON.stringify({time:new Date().toISOString(),request:req,response:resp,ms})+'\n');}
async function start(){
 proc=spawn('C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe',['-NoProfile','-ExecutionPolicy','Bypass','-File',path.join(here,'serial_bridge.ps1'),'-Port',port],{cwd:root,windowsHide:true,stdio:['pipe','pipe','pipe']});
 proc.stdout.on('data',b=>{buffer+=b.toString();while(buffer.includes('\n')){const n=buffer.indexOf('\n');lines.push(buffer.slice(0,n).trim());buffer=buffer.slice(n+1);}});
 proc.stderr.on('data',b=>errors+=b.toString());proc.on('error',e=>{errors+=e.message;closed=true;});proc.on('close',()=>closed=true);proc.stdin.on('error',e=>errors+=e.message);assert.equal((await read()).bridge,'open');
}
async function request(op,payload={}){
 if(['save_config','enter_bootloader','reboot_to_saved_mode','set_startup_usb_mode'].includes(op))throw Error('Forbidden in this test');
 const m={protocol_version:1,id:++id,op,payload};assert.ok(Buffer.byteLength(JSON.stringify(m))<=4096);
 const t=performance.now();proc.stdin.write(JSON.stringify(m)+'\n');const r=await read(),ms=performance.now()-t;await log(m,r,ms);latency.push(ms);assert.equal(r.protocol_version,1);assert.equal(r.id,id);revision=r.revision;return r;
}
async function ok(op,p={}){const r=await request(op,p);assert.equal(r.ok,true,op+': '+JSON.stringify(r.error));return r.result;}
async function control(c){const t=performance.now();proc.stdin.write('__ctl__'+JSON.stringify(c)+'\n');const r=await read();await log({host_control:c},r,performance.now()-t);return r;}
async function exchange(text,{replies=1,chunk_size=0,delay_ms=0}={}){const r=await control({action:'exchange',data:Buffer.from(text,'ascii').toString('base64'),replies,chunk_size,delay_ms});return r.lines.map(x=>JSON.parse(x));}
const message=(op='hello',payload={})=>({protocol_version:1,id:++id,op,payload});
const crc32=b=>{let c=0xffffffff;for(const x of b){c^=x;for(let i=0;i<8;i++)c=(c>>>1)^((c&1)?0xedb88320:0);}return (c^0xffffffff)>>>0;};
async function snapshot(){let offset=0,meta;const blocks=[];while(true){const r=await request('export_config',{offset});assert.ok(r.ok);const v=r.result;meta||={revision:r.revision,crc:v.crc32,length:v.length};assert.deepEqual([r.revision,v.crc32,v.length,v.offset],[meta.revision,meta.crc,meta.length,offset]);assert.equal(v.schema,2);const b=Buffer.from(v.data,'hex');assert.ok(b.length>0&&b.length<=512);blocks.push(b);offset+=b.length;assert.equal(offset,v.next_offset);assert.ok(offset<=meta.length);if(offset===meta.length)break;}const data=Buffer.concat(blocks);assert.equal(crc32(data),meta.crc);return {data,...meta};}
async function healthy(){const s=await ok('get_status');assert.ok(s.config_ready&&!s.applying&&!s.apply_failed);assert.equal(s.maintenance,'idle');assert.equal(s.physical_mask,0);assert.equal(s.valid_slots,baseline.get_status.result.valid_slots);return s;}
async function setProfile(p,c=p.config,m=p.motion){report.restored=false;changed.add(p.profile_id);const rev=revision;await ok('set_profile',{expected_revision:rev,profile_id:p.profile_id,config:c,motion:m});assert.equal(revision,rev+1);const r=await ok('get_profile',{profile_id:p.profile_id});assert.deepEqual(r.config,c);assert.deepEqual(r.motion,m);await healthy();}
async function restore(){
 assert.equal((await ok('hello')).serial,serial);for(const pid of changed){const p=originals[pid],r=await ok('get_profile',{profile_id:p.profile_id});if(JSON.stringify(r.config)!==JSON.stringify(p.config)||JSON.stringify(r.motion)!==JSON.stringify(p.motion))await setProfile(p);if(JSON.stringify(r.io4_bindings)!==JSON.stringify(p.io4_bindings))await ok('set_io4_bindings',{expected_revision:revision,profile_id:p.profile_id,bindings:p.io4_bindings});}
 if((await ok('get_device')).active_profile!==active)await ok('set_active_profile',{expected_revision:revision,profile_id:active});
 for(const p of Object.values(originals))assert.deepEqual(await ok('get_profile',{profile_id:p.profile_id}),p);
 assert.deepEqual(await ok('get_device'),baseline.get_device.result);
 const final=await snapshot(),original=await fs.readFile(path.join(out,'original-schema2.bin'));await fs.writeFile(path.join(out,'final-schema2.bin'),final.data);
 // DeviceCodec::snapshot starts with schema u32 then revision u32.
 const normalized=Buffer.from(final.data);original.copy(normalized,4,4,8);assert.deepEqual(normalized,original);
 report.restored=true;report.final_revision=revision;report.final_snapshot_crc=final.crc;await healthy();await save();
}
try {
 await start();const h=await ok('hello');assert.equal(h.product,'FluxRail');assert.equal(h.serial,serial);assert.equal(h.firmware_version,expectedVersion);assert.equal(h.usb_mode,'NORMAL_NKRO');await healthy();
 for(const p of Object.values(originals))assert.deepEqual(await ok('get_profile',{profile_id:p.profile_id}),p);
 const snap=await snapshot();assert.deepEqual(snap.data,await fs.readFile(path.join(out,'original-schema2.bin')));
 baselineConfirmed=true;
 await stage('阶段1复核通过：身份与备份一致','身份、四模板及完整schema2备份核对通过；RAM/协议/稳定性待测。',{version:h.firmware_version,serial,revision,length:snap.length,crc:snap.crc},'安全RAM参数与模板切换','hello/get_status/get_profile/export_config');
 const p=originals[active],times=[];
 for(let i=0;i<30;i++){
  const c=clone(p.config),m=clone(p.motion);c.scan.press_debounce_us=500+i%5*500;c.scan.release_debounce_us=750+i%4*250;c.keyboard.retrigger_gap_us=1000+i%3*500;
  c.keyboard.bindings[0].action=i%3===0?{type:'disabled'}:i%3===1?{type:'mouse_button',button:1}:{type:'keyboard',usage:4,modifiers:2};
  c.lighting.mode=['static','breathing','color_cycle','rainbow_wave','press'][i%5];c.lighting.brightness_level=[0,25,50,75,100][i%5];c.lighting.global_color=[i,20,40];c.lighting.reverse=!!(i%2);c.indicator.enabled=!!(i%2);
  m.hall.enabled=true;m.hall.invert=!!(i%2);m.hall.center_gain=[0.5,1,2][i%3];m.routing.idle_us=5000+i%4*5000;
  for(let k=0;k<2;k++){m.knobs[k].enabled=false;m.knobs[k].gain=[0.5,1,2][(i+k)%3];m.knobs[k].axis=(i+k)%2;m.knobs[k].invert=!!(i%2);}
  const t=performance.now();await setProfile(p,c,m);times.push(performance.now()-t);await sleep(55);
  const motion=await ok('get_motion');assert.equal(motion.sample_revision,revision);assert.equal(motion.hall.valid,false);assert.equal(motion.hall.active,false);assert.equal(motion.hall.output,0);assert.ok(motion.knobs.every(k=>!k.enabled&&!k.active));
 }
 await setProfile(p);
 for(let n=0;n<5;n++)for(const pid of [1,2,3,4]){await ok('set_active_profile',{expected_revision:revision,profile_id:pid});assert.equal((await ok('get_device')).active_profile,pid);assert.deepEqual((await ok('get_config')).config,originals[pid].config);await healthy();}
 const bindings=[...p.io4_bindings];bindings[0]=0;await ok('set_io4_bindings',{expected_revision:revision,profile_id:p.profile_id,bindings});assert.deepEqual((await ok('get_profile',{profile_id:p.profile_id})).io4_bindings,bindings);await ok('set_io4_bindings',{expected_revision:revision,profile_id:p.profile_id,bindings:p.io4_bindings});await restore();
 await stage('阶段2通过：RAM应用与恢复','30组参数、20次模板切换、IO4映射RAM读回完成并还原；协议与稳定性待测。',{groups:30,switches:20,restored:report.restored,revision,apply_roundtrip_ms:{min:Math.min(...times),max:Math.max(...times)}},'错误请求与CRC损坏导入','set_profile/set_active_profile/get_config/get_motion/set_io4_bindings','无校准Hall持续valid=false/active=false/output0；无外设无法验证实际映射与颜色或运行时Y保持。');
 const before=await snapshot();let negative=0;
 async function expect(op,payload,code){const rev=revision,r=await request(op,payload);assert.equal(r.ok,false);assert.equal(r.error.code,code);assert.equal(r.revision,rev);negative++;await healthy();}
 await expect('no_such_operation',{},'UNSUPPORTED_OPERATION');await expect('get_profile',{profile_id:999999},'INVALID_PROFILE');await expect('set_active_profile',{expected_revision:revision-1,profile_id:1},'REVISION_CONFLICT');
 const bad=clone(p.config);bad.lighting.brightness_level=101;await expect('set_profile',{expected_revision:revision,profile_id:p.profile_id,config:bad,motion:p.motion},'INVALID_PROFILE');
 const badMotion=clone(p.motion);badMotion.knobs[0].axis=2;await expect('set_profile',{expected_revision:revision,profile_id:p.profile_id,config:p.config,motion:badMotion},'INVALID_PROFILE');
 await expect('get_motion',{unexpected:true},'INVALID_PAYLOAD');await expect('set_io4_bindings',{expected_revision:revision,profile_id:p.profile_id,bindings:[99,...p.io4_bindings.slice(1)]},'INVALID_PAYLOAD');
 for(const [raw,code] of [['{bad json}\n','INVALID_JSON'],[JSON.stringify({...message(),protocol_version:2})+'\n','PROTOCOL_MISMATCH'],['{"protocol_version":1,"id":'+(++id)+',"op":"hello","op":"get_device","payload":{}}\n','INVALID_REQUEST'],['x'.repeat(4200)+'\n','LINE_TOO_LONG']]){const [r]=await exchange(raw);assert.equal(r.ok,false);assert.equal(r.error.code,code);negative++;await healthy();}
 const split=message('get_device'),[sr]=await exchange(JSON.stringify(split)+'\r\n',{chunk_size:3,delay_ms:2});assert.equal(sr.id,split.id);assert.ok(sr.ok);
 const burst=[message('hello'),message('get_status'),message('get_device')],br=await exchange('\n'+burst.map(x=>JSON.stringify(x)+'\n').join(''),{replies:3});assert.deepEqual(br.map(x=>x.id),burst.map(x=>x.id));assert.ok(br.every(x=>x.ok));
 const corrupt=Buffer.from(before.data);corrupt[corrupt.length-1]^=1;const importRev=revision;
 await ok('begin_import',{expected_revision:revision,length:corrupt.length,crc32:before.crc});for(let offset=0;offset<corrupt.length;offset+=512)await ok('import_chunk',{expected_revision:revision,offset,data:corrupt.subarray(offset,offset+512).toString('hex')});
 const r=await request('commit_import',{expected_revision:revision});assert.equal(r.ok,false);assert.equal(r.error.code,'INVALID_TRANSFER');assert.equal(revision,importRev);negative++;
 assert.deepEqual((await snapshot()).data,before.data);
 await stage('阶段3通过：协议边界与错误恢复','非法参数/revision/JSON/超长行/损坏导入均拒绝；分片/连续行正常，RAM字节未改变。',{negative_cases:negative,fragment_bytes:3,burst_replies:3,revision},'20次端口重开和20次DTR切换','CDC JSON边界、import/export');
 let recovery=await snapshot();
 for(let i=0;i<20;i++){
  await exchange('{"protocol_version":1,"id":123',{replies:0});assert.equal((await control({action:'reopen'})).bridge,'reopened');assert.equal((await ok('hello')).serial,serial);await healthy();
  await exchange('{"protocol_version":1,"id":456',{replies:0});assert.equal((await control({action:'dtr'})).bridge,'dtr');assert.equal((await ok('hello')).serial,serial);await healthy();
 }
 assert.deepEqual((await snapshot()).data,recovery.data);
 await stage('阶段4通过：CDC会话恢复','20次端口重开与20次DTR切换含残留半行，握手恢复且配置不变。',{reopen:20,dtr:20,partial_line_discard:true,revision},'5分钟10Hz轮询和1分钟无间隔通信压力','SerialPort.Close/Open、DtrEnable、resetSession','CDC会话重连不等于物理USB拔插/重新枚举或断电恢复。');
 // Read-only idle window before enabling software load, no external sensors assumed.
 const idleStart=await ok('get_motion');await sleep(10000);let idleEnd=await ok('get_motion');
 const idleDeadline=Date.now()+5000;
 while(idleEnd.timestamp_us===idleStart.timestamp_us){assert.ok(Date.now()<idleDeadline,'Motion snapshot stopped advancing');await sleep(55);idleEnd=await ok('get_motion');}
 function rates(a,b){
  assert.equal(a.sample_revision,b.sample_revision);assert.equal(a.epoch,b.epoch);
  const us=(b.timestamp_us-a.timestamp_us)>>>0;assert.ok(us>0&&us<1800000000);
  return {start:a,end:b,device_elapsed_us:us,attempts_added:(b.hall.adc_attempts-a.hall.adc_attempts)>>>0,hall_hz:((b.hall.adc_attempts-a.hall.adc_attempts)>>>0)*1e6/us,skipped_periods_added:(b.hall.skipped_periods-a.hall.skipped_periods)>>>0,samples_added:(b.hall.samples-a.hall.samples)>>>0};
 }
 const idleRate=expectedVersion==='0.10.1'?rates(idleStart,idleEnd):null;
 if(expectedVersion==='0.10.1'){
  const loadConfig=clone(p.config),loadMotion=clone(p.motion);
  loadConfig.lighting.enabled=true;loadConfig.lighting.mode='rainbow_wave';loadConfig.lighting.brightness_level=25;
  loadMotion.hall.enabled=true;for(const k of loadMotion.knobs)k.enabled=true;
  await setProfile(p,loadConfig,loadMotion);await sleep(100);recovery=await snapshot();
 }
 const stable=await ok('get_motion'),stableRev=revision,epoch=stable.epoch;let prev=stable,cycles=0,duplicates=0,maxCore=stable.core1_max_service_us;
 const t0=performance.now();let next=t0,lastPrint=t0;report.stability_running=true;await save();
 function checkMotion(m){assert.equal(m.sample_revision,stableRev);assert.equal(m.epoch,epoch);assert.ok(m.sample_ready&&m.adc_started&&m.ssi_started);assert.equal(m.calibration_valid,false);assert.equal(m.hall.valid,false);assert.equal(m.hall.output,0);for(const key of ['adc_errors','queue_full'])assert.equal(m.hall[key],stable.hall[key]);assert.equal(m.telemetry_dropped,stable.telemetry_dropped);assert.ok(m.hall.samples>=prev.hall.samples);assert.ok(((m.timestamp_us-prev.timestamp_us)>>>0)<10000000);if(m.timestamp_us===prev.timestamp_us)duplicates++;maxCore=Math.max(maxCore,m.core1_max_service_us);prev=m;}
 while(performance.now()-t0<300000){
  checkMotion(await ok('get_motion'));await healthy();assert.equal(revision,stableRev);cycles++;
  if(cycles%100===0)assert.equal((await ok('hello')).serial,serial);if(cycles%300===0)assert.deepEqual((await snapshot()).data,recovery.data);
  if(performance.now()-lastPrint>=30000){lastPrint=performance.now();report.stability_progress={seconds:(lastPrint-t0)/1000,cycles,last_sample:prev};await save();console.log(JSON.stringify({progress:'stability',seconds:Math.round((lastPrint-t0)/1000),cycles}));}
  next+=100;await sleep(Math.max(0,next-performance.now()));
 }
 const pacedSeconds=(performance.now()-t0)/1000,pacedPollHz=cycles/pacedSeconds;
 assert.ok(pacedPollHz>=9.5&&pacedPollHz<=10.5,`Actual paced poll rate ${pacedPollHz} Hz outside 10Hz +/-5% test tolerance`);
 const pacedEnd=await ok('get_motion'),ts=performance.now();let stress=0;
 while(performance.now()-ts<60000){await healthy();if(stress%10===0)checkMotion(await ok('get_motion'));stress++;}
 assert.deepEqual((await snapshot()).data,recovery.data);report.stability_running=false;
 const stressEnd=await ok('get_motion');
 if(expectedVersion==='0.10.1'){
  report.hall_rates={idle:idleRate,paced:rates(stable,pacedEnd),stress:rates(pacedEnd,stressEnd)};
  report.load={hall_enabled:true,both_knobs_enabled:true,lighting_mode:'rainbow_wave',brightness_level:25,physical_inputs_present:false};
  await fs.writeFile(path.join(out,'hall-rate-report.json'),JSON.stringify({rates:report.hall_rates,load:report.load},null,2));
  await save();
 }
 const sorted=[...latency].sort((a,b)=>a-b),hostLatency={count:sorted.length,p50:sorted[Math.floor(sorted.length*.5)],p95:sorted[Math.floor(sorted.length*.95)],p99:sorted[Math.floor(sorted.length*.99)],max:sorted.at(-1)};
 await stage('阶段5通过：持续通信稳定性','5分钟目标10Hz轮询及1分钟无间隔通信完成，最终恢复待复核。',{paced_seconds:pacedSeconds,poll_cycles:cycles,paced_poll_hz:pacedPollHz,paced_poll_test_tolerance_hz:[9.5,10.5],duplicate_snapshots:duplicates,stress_seconds:(performance.now()-ts)/1000,stress_status_requests:stress,host_latency_ms:hostLatency,core1_max_service_us:maxCore,hall_samples_added:prev.hall.samples-stable.hall.samples,queue_full_added:prev.hall.queue_full-stable.hall.queue_full,telemetry_dropped_added:prev.telemetry_dropped-stable.telemetry_dropped,adc_errors_added:prev.hall.adc_errors-stable.hall.adc_errors,revision:stableRev,epoch,hall_hz:report.hall_rates?Object.fromEntries(Object.entries(report.hall_rates).map(([k,v])=>[k,v.hall_hz])):null},'最终配置恢复核对并关闭串口','get_motion/get_status/hello/export_config','主机请求耗时含Windows桥接，不是按键/USB延迟。20Hz遥测不能证明500us扫描无丢采；最大诊断含历史/配置维护停顿，外设驱动负载不等于真实运动/键输入。Hall频率计算见hall-rate-report.json，不以通信通过代替1kHz阈值判定。');
}catch(e){report.failure={message:e.message,stack:e.stack,time:new Date().toISOString()};console.error(e.stack);}
finally {
 if(proc&&!closed&&baselineConfirmed)try{await restore();}catch(e){report.restore_error=e.stack;console.error('Restore failed: '+e.stack);}
 report.finished=new Date().toISOString();await save();
 try {await stage('本轮CDC测试结束与恢复核对',report.failure?'有失败项，完成范围见前述阶段。':'授权CDC/RAM/协议/会话恢复/稳定性完成。',{restored:report.restored,revision,flash_written:false,failure:report.failure?.message,restore_error:report.restore_error},report.failure?'从report.json失败项定位继续，先核对RAM恢复。':'网页协议客户端实板适配复验；外设到货后测真实输入。','get_profile/get_device/export_config；report.json、original/final-schema2.bin','逐模板/meta及二进制校验，只有revision字段允许增加；不含真实Web Serial端到端、HID物理输入、Flash/断电/IO4/游戏。缺MT6701时全零SSI帧valid=true待澄清。');}
 finally {
 if(proc&&!closed){proc.stdin.end('__exit__\n');await Promise.race([new Promise(r=>proc.once('close',r)),sleep(3000)]);if(!closed)proc.kill();}
 report.port_close_verified=closed;report.finished=new Date().toISOString();await save();
 }
}
process.exitCode=report.failure||report.restore_error?1:0;
