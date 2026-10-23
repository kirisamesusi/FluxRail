import test from 'node:test';
import assert from 'node:assert/strict';
import { SerialClient, LineFramer } from '../src/connection/client.js';
import { Device } from '../src/device.js';
import { DemoHost, demoPort } from '../src/demo.js';
import { fromCharacter, fromEvent } from '../src/keyboard.js';
import { validateConfig, validateMotion, validName, verifyHello } from '../src/config.js';
import { crc32, unhex, hex, decodeSnapshot, exportSnapshot, importSnapshot, archive, parseArchive } from '../src/snapshot.js';
import { validateUf2 } from '../src/update/uf2.js';
import { goldenHex } from './golden.js';
import { readFile } from 'node:fs/promises';
const gold=unhex(goldenHex),clone=x=>structuredClone(x);
async function connected(){const port=demoPort();const client=new SerialClient(port);await client.open();const device=new Device(client);await device.handshake();return {port,client,device};}
test('金样与公共二进制逐字节一致；模拟编码也一致',async()=>{
  const authoritative=await readFile(new URL('../../tests/integration/schema1_defaults.bin',import.meta.url));
  assert.equal(hex(authoritative),goldenHex);assert.equal(gold.length,1339);assert.equal(crc32(gold),3954875198);
  assert.equal(hex(new DemoHost().snapshot()),goldenHex);
  const d=decodeSnapshot(gold);assert.equal(d.active_profile,4);assert.equal(d.profiles.length,4);
  assert.deepEqual(d.profiles[1].motion.knobs.map(k=>k.enabled),[false,false]);assert.equal(d.calibration.valid,false);
});
test('schema1长度/身份/CRC/字段/范围拒绝，保留浮点精度',()=>{
  const hello={product:'FluxRail',board:'FluxRail RP2040 4MiB',serial:'x',firmware_version:'x'};
  const a=archive(gold,hello);assert.equal(hex(parseArchive(JSON.stringify(a)).bytes),goldenHex);
  for(const patch of [{schema:2},{board:'RP2040-Zero'},{length:1340},{crc32:1},{data:'f'.repeat(gold.length*2)}])assert.throws(()=>parseArchive(JSON.stringify({...a,...patch})));
  assert.throws(()=>decodeSnapshot(gold.slice(0,-1)));assert.throws(()=>decodeSnapshot(new Uint8Array([...gold,0])));
  const b=gold.slice();b[18+36]=3;assert.throws(()=>decodeSnapshot(b));
  const m=clone(decodeSnapshot(gold).profiles[0].motion);m.hall.activity_mm=Math.fround(0.01);validateMotion(m);
  m.hall.range_max=m.hall.range_min;assert.throws(()=>validateMotion(m));
  const numeric=clone(decodeSnapshot(gold).profiles[0].motion);numeric.hall.center_gain='1';assert.throws(()=>validateMotion(numeric));
  const config=clone(decodeSnapshot(gold).profiles[0].config);config.lighting.brightness_level=101;assert.throws(()=>validateConfig(config));
  validName('中文模板');validName('Custom 1');validName('一二三四五六七八九十');assert.throws(()=>validName('一二三四五六七八九十一'));assert.throws(()=>validName('bad"name'));assert.throws(()=>validName('a\\b'));assert.throws(()=>validName(' x'));assert.throws(()=>validName('x\u200b'));
});
test('12路键盘录入/字符/修饰键，重复绑定允许，长按重复忽略',()=>{
  assert.deepEqual(fromCharacter('A'),{type:'keyboard',usage:4,modifiers:2});assert.equal(fromCharacter('a').modifiers,0);
  assert.equal(fromCharacter(';').usage,51);assert.equal(fromCharacter('?').usage,56);assert.equal(fromCharacter('?').modifiers,2);
  assert.equal(fromCharacter('~').usage,53);assert.equal(fromCharacter(' ').usage,44);assert.throws(()=>fromCharacter('中'));
  assert.equal(fromEvent({code:'ArrowLeft'}).usage,80);assert.equal(fromEvent({code:'ShiftRight',shiftKey:true}).usage,229);
  assert.equal(fromEvent({code:'KeyA',repeat:true}),null);assert.equal(fromEvent({code:'KeyA',ctrlKey:true}).modifiers,1);
  const c=clone(decodeSnapshot(gold).profiles[0].config);for(const b of c.keyboard.bindings)b.action=fromCharacter('a');validateConfig(c);
});
test('有界分行覆盖半行、多行、CRLF、超长/非ASCII丢到换行恢复',()=>{
  const lines=[],errors=[];const f=new LineFramer(x=>lines.push(x),e=>errors.push(e),8),e=new TextEncoder();
  f.feed(e.encode('abc'));f.feed(e.encode('d\r\n1\n\n'));assert.deepEqual(lines,['abcd\r','1']);
  f.feed(e.encode('123456789junk\nOK\n'));f.feed(new Uint8Array([255,10]));assert.equal(errors.length,2);assert.equal(lines.at(-1),'OK');assert.ok(f.line.length<=8);
});
test('握手/编辑/显式RAM应用/显式保存，网页不自动写Flash',async()=>{
  const {client,device,port}=await connected();
  assert.equal(device.revision,1);assert.equal(device.profile.profile_id,4);assert.equal(port.host.requests.some(r=>r.op==='save_config'),false);
  device.draft.config.lighting.brightness_level=100;device.localDirty=true;await device.apply();assert.equal(device.profile.config.lighting.brightness_level,100);assert.equal(device.revision,2);
  assert.equal(port.host.requests.filter(r=>r.op==='save_config').length,0);assert.equal(client.last.dirty,true);
  await device.mutate('save_config');assert.equal(client.last.saved_revision,2);assert.equal(client.last.dirty,false);
  const line=JSON.stringify(port.host.requests.find(r=>r.op==='set_profile'));assert.ok(line.length<4096);
  const countTokens=x=>Array.isArray(x)?1+x.reduce((n,v)=>n+countTokens(v),0):x&&typeof x==='object'?1+Object.values(x).reduce((n,v)=>n+1+countTokens(v),0):1;
  assert.ok(countTokens(JSON.parse(line))<=384,'完整set_profile不超过实际C++解析token预算');
  await client.close();
});
test('轮询发现revision冲突不得覆盖旧草稿；重连或读回恢复',async()=>{
  const {client,device,port}=await connected();device.draft.config.lighting.brightness_level=20;device.localDirty=true;
  port.host.device.revision=2;await device.poll();await assert.rejects(device.apply(),/版本已变化/);
  assert.equal(port.host.requests.some(r=>r.op==='set_profile'),false);await device.refresh();assert.equal(device.localDirty,false);assert.equal(device.revision,2);await client.close();
});
test('模板容量12、稳定ID、不删预置、活动自定义删除回4',async()=>{
  const {client,device,port}=await connected();
  for(let i=0;i<8;i++)await device.mutate('create_profile',{profile_id:4,name:'Custom '+i});assert.equal(device.profiles.length,12);
  await assert.rejects(device.mutate('create_profile',{profile_id:4,name:'Overflow'}));assert.equal(device.ready,false);await device.refresh();
  await assert.rejects(device.mutate('delete_profile',{profile_id:1}));await device.refresh();
  await device.mutate('set_active_profile',{profile_id:5});assert.equal(device.meta.active_profile,5);
  await device.mutate('delete_profile',{profile_id:5});assert.equal(device.meta.active_profile,4);assert.equal(port.host.device.next_profile_id,13);await client.close();
});
test('全快照分块导出/导入、CRC检查、同revision和不自动保存',async()=>{
  const {client,device,port}=await connected();const bytes=await exportSnapshot(client);assert.equal(hex(bytes),goldenHex);
  await importSnapshot(client,bytes,device.revision);await device.refresh();assert.equal(device.revision,2);assert.equal(device.meta.calibration_generation,1);
  assert.equal(port.host.requests.filter(r=>r.op==='import_chunk').length,3);assert.equal(port.host.requests.some(r=>r.op==='save_config'),false);
  await assert.rejects(importSnapshot(client,bytes,1),/设备配置改变/);await client.close();
});
test('串行队列/id回绕/设备错误不会误成功',async()=>{
  const {client}=await connected();client.id=0xffffffff;const [a,b]=await Promise.all([client.request('get_status'),client.request('get_device')]);assert.equal(a.config_ready,true);assert.equal(b.schema,1);assert.equal(client.id,2);
  await assert.rejects(client.request('unimplemented',{expected_revision:client.revision}),e=>e.code==='SIMULATION_REJECTED');await client.close();
});
function faultyPort(write){let ctrl;const port={open:async()=>{port.readable=new ReadableStream({start(c){ctrl=c;}});port.writable=new WritableStream({write:bytes=>write(bytes,b=>ctrl.enqueue(b))});},setSignals:async()=>{},close:async()=>{}};return port;}
test('错误id/协议/JSON关闭会话，取消待请求并禁止队列后续写入',async()=>{
  for(const line of ['{bad}\n','{"protocol_version":2,"id":1,"ok":true,"result":{}}\n','{"protocol_version":1,"id":99,"ok":true,"result":{}}\n']){
    let writes=0;const c=new SerialClient(faultyPort((_,send)=>{writes++;send(new TextEncoder().encode(line));}));await c.open();
    const tasks=[c.request('hello'),c.request('save_config',{expected_revision:1})];const result=await Promise.allSettled(tasks);assert.ok(result.every(r=>r.status==='rejected'));assert.equal(writes,1);assert.equal(c.alive,false);await c.close();
  }
});
test('ACK缺失与ACK后写背压超时均中止，无盲重试',async()=>{
  for(const ack of [false,true]){
    let writes=0;const c=new SerialClient(faultyPort((_,send)=>{writes++;if(ack)send(new TextEncoder().encode('{"protocol_version":1,"id":1,"ok":true,"revision":1,"applied_revision":1,"saved_revision":null,"dirty":true,"result":{}}\n'));return ack?new Promise(()=>{}):undefined;}));await c.open();
    await assert.rejects(c.request('save_config',{expected_revision:1},20),e=>e.code==='TIMEOUT');assert.equal(writes,1);assert.equal(c.alive,false);
  }
});
test('不兼容产品/板型/协议scope拒绝，兼容旧runtime子集',()=>{
  const hello=new DemoHost().handle({protocol_version:1,id:1,op:'hello',payload:{}}).result;verifyHello(hello);
  for(const change of [{product:'old'},{board:'RP2040-Zero'},{scope:'unknown'},{usb_mode:'IO4'}])assert.throws(()=>verifyHello({...hello,...change}));
  verifyHello({...hello,scope:'runtime_module1',capabilities:{...hello.capabilities,profiles:false,max_profiles:0}});
});
test('旧runtime固件只读/写活动模块1子集，不猜测模板和保存命令',async()=>{
  const host=new DemoHost(),handle=host.handle.bind(host);
  host.handle=request=>{const reply=handle(request);if(request.op==='hello'){reply.result.scope='runtime_module1';reply.result.capabilities.profiles=false;reply.result.capabilities.flash_save=false;reply.result.capabilities.max_profiles=0;}return reply;};
  const client=new SerialClient(demoPort(host));await client.open();const device=new Device(client);await device.handshake();assert.equal(device.profile.motion,null);
  device.draft.config.lighting.brightness_level=35;device.localDirty=true;await device.apply();assert.equal(device.profile.config.lighting.brightness_level,35);
  assert.ok(host.requests.some(r=>r.op==='set_config'));assert.equal(host.requests.some(r=>['get_device','list_profiles','set_profile','save_config'].includes(r.op)),false);await client.close();
});
test('导出跨块revision改变拒绝拼接混合快照',async()=>{
  const {client,port}=await connected();const handle=port.host.handle.bind(port.host);port.host.handle=r=>{if(r.op==='export_config'&&r.payload.offset>0)port.host.device.revision=2;return handle(r);};
  await assert.rejects(exportSnapshot(client),/导出期间配置改变/);await client.close();
});
test('请求队列上限8，拒绝洪泛且不丢已排队请求',async()=>{
  const {client}=await connected();const tasks=Array.from({length:8},()=>client.request('get_status'));await assert.rejects(client.request('get_status'),e=>e.code==='QUEUE_FULL');await Promise.all(tasks);assert.equal(client.queued,0);await client.close();
});
test('成功ACK缺少保存/应用元数据终止会话',async()=>{
  const client=new SerialClient(faultyPort((_,send)=>send(new TextEncoder().encode('{"protocol_version":1,"id":1,"ok":true,"revision":1,"result":{}}\n'))));await client.open();await assert.rejects(client.request('hello'),e=>e.code==='INVALID_RESPONSE');assert.equal(client.alive,false);await client.close();
});
function uf2(){const b=new Uint8Array(512),v=new DataView(b.buffer);b.set(new TextEncoder().encode('FLUXRAIL-FW-V1'),32+64);v.setUint32(32+64+16,1,true);b.set(new TextEncoder().encode('fluxrail-rp2040-4mib'),32+64+20);b.set(new TextEncoder().encode('0.10.0'),32+64+52);v.setUint32(32+64+84,0x46525746,true);for(const[at,n]of [[0,0x0a324655],[4,0x9e5d5157],[8,0x2000],[12,0x10000000],[16,256],[20,0],[24,1],[28,0xe48bff56],[508,0x0ab16f30]])v.setUint32(at,n,true);return b;}
test('UF2 family/范围/块号/保留区检查，不上传设备',()=>{
  assert.equal(validateUf2(uf2()).payload_bytes,256);
  for(const[at,n]of [[0,0],[28,0],[12,0x103fb000],[16,477],[24,2],[20,1],[8,1]]){const b=uf2();new DataView(b.buffer).setUint32(at,n,true);assert.throws(()=>validateUf2(b));}
});
