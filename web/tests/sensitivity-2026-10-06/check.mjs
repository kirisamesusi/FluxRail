import assert from 'node:assert/strict';
import {chromium} from 'file:///C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright/index.mjs';
import {writeFile,readFile,access} from 'node:fs/promises';
import {fileURLToPath,pathToFileURL} from 'node:url';
import {mkdir} from 'node:fs/promises';
const out=process.argv[2]?pathToFileURL(process.argv[2].replace(/[\/]?$/,'/')):new URL('./',import.meta.url),base='http://localhost:4174';
const report={date_local:'2026-10-06',scope:'Production Web with disconnected UI, shipped demo and isolated in-browser CDC fixture. No hardware.',hardware_tested:false,compiled:false,views:[],checks:[]};
try{await access(new URL('browser-check.json',out));throw Error('Evidence exists; use another directory.');}catch(e){if(e.code!=='ENOENT')throw e;}
await mkdir(out,{recursive:true});
const browser=await chromium.launch({channel:'msedge',headless:true});report.browser=await browser.version();
const errors=[];
const modules=['device-status','keyboard','lighting','hall','knobs','profiles','update','experimental'];
const order=['10','11','7','6','5','4','3','2','1','0','8','9'];
async function nav(page,id){await page.locator('[data-module="'+id+'"]').click();await page.waitForFunction(id=>!document.getElementById(id).hidden,id);}
async function settle(page){await page.waitForFunction(()=>!document.getElementById('refresh').disabled);}
try{
 const page=await browser.newPage();page.on('pageerror',e=>errors.push(String(e)));
 for(const width of [1274,390,320]){
  await page.setViewportSize({width,height:width===1274?1291:844});await page.goto(base,{waitUntil:'networkidle'});
  assert.deepEqual(await page.locator('.module-nav [data-module]').evaluateAll(es=>es.map(e=>e.dataset.module)),modules);
  assert.equal(await page.locator('.module-panel:visible').getAttribute('id'),'device-status');
  for(const icon of ['.module-nav [data-module=device-status] use','#device-status .module-art use'])assert(await page.locator(icon).evaluate(e=>e.getBBox().width>0),'status icon renders');
  for(const id of modules){await nav(page,id);assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,id+'/'+width);report.views.push({width,module:id,overflow:false});}
  assert.equal(await page.locator('#save').count(),0);assert.equal(await page.locator('#diagnostics').count(),0);
  await nav(page,'device-status');await page.screenshot({path:fileURLToPath(new URL('disconnected-'+width+'.png',out)),fullPage:true});
 }
 await page.goto(base+'/#diagnostics');assert.equal(await page.locator('.module-panel:visible').getAttribute('id'),'device-status');assert(await page.locator('#input-status').evaluate(e=>e.open));
 await nav(page,'keyboard');await nav(page,'hall');await page.goBack();assert.equal(await page.locator('.module-panel:visible').getAttribute('id'),'keyboard');assert.equal(await page.evaluate(()=>document.activeElement.id),'keyboard-title');await page.goForward();assert.equal(await page.locator('.module-panel:visible').getAttribute('id'),'hall');
 report.checks.push('navigation, old diagnostics deep link, back/forward and eight responsive modules');
 await nav(page,'experimental');await page.locator('#experiment-device > summary').click();await page.locator('#demo').click();await settle(page);
 assert.match(await page.locator('#connection-state').textContent(),/模拟/);assert(!(await page.locator('#integration-bootloader').isEnabled()));assert(!(await page.locator('#io4-mode-switch').isEnabled()));
 assert.deepEqual(await page.locator('#key-grid [data-source]').evaluateAll(es=>es.map(e=>e.dataset.source)),order);
 assert.deepEqual(await page.locator('#input-test [data-source]').evaluateAll(es=>es.map(e=>e.dataset.source)),order);
 assert.deepEqual(await page.locator('#io4-mapping-editor [data-source]').evaluateAll(es=>es.map(e=>e.dataset.source)),order);
 await nav(page,'device-status');await page.locator('#hall-overview input').check();await nav(page,'hall');assert(await page.locator('#hall-fields input[type=checkbox]').first().isChecked());
 await page.locator('#hall-fields input[type=checkbox]').first().uncheck();await nav(page,'device-status');assert(!(await page.locator('#hall-overview input').isChecked()));
 const unchanged=await page.locator('#active-profile-state').textContent();await page.locator('#refresh').click();await page.locator('[data-cancel]').click();assert.match(await page.locator('#draft-state').textContent(),/草稿/);
 await page.locator('#refresh').click();await page.keyboard.press('Escape');assert.match(await page.locator('#draft-state').textContent(),/草稿/);
 await page.locator('#refresh').click();await page.locator('[data-confirm]').click();await settle(page);assert.match(await page.locator('#draft-state').textContent(),/一致/);
 for(const width of [1274,390]){
  await page.setViewportSize({width,height:844});
  for(const [id,controls]of [['hall',['hall-gain']],['knobs',['knob-gain-1','knob-gain-2']]]){
   await nav(page,id);for(const control of controls)assert.equal(await page.locator('#'+control+'-number').inputValue(),'5.0');
   await page.screenshot({path:fileURLToPath(new URL('default-'+id+'-'+width+'.png',out)),fullPage:true});
  }
 }
 await nav(page,'hall');const number=page.locator('#hall-gain-number');await number.fill('');await number.pressSequentially('1.5');await number.press('Tab');assert.equal(await page.locator('#hall-gain-ruler').inputValue(),'1.5');
 await number.fill('10');await number.press('Tab');assert.equal(await number.evaluate(e=>e.checkValidity()),true);assert.equal(await number.inputValue(),'10.0');
 await number.fill('5.14');await number.press('Tab');assert.equal(await number.inputValue(),'5.1');assert.equal(await page.locator('#hall-gain-ruler').inputValue(),'5.1');
 await number.fill('10.1');await number.press('Tab');assert.equal(await number.evaluate(e=>e.checkValidity()),false);
 await number.fill('1.5');await number.press('Tab');await page.locator('#apply').click();await settle(page);assert.match(await page.locator('#notice').textContent(),/已应用并保存/);assert.equal(await page.locator('#active-profile-state').textContent(),unchanged);
 assert.match(await page.locator('#save-state').textContent(),/已保存/);await page.locator('#apply').click();await settle(page);assert.match(await page.locator('#notice').textContent(),/已应用并保存/);
 for(const width of [1274,390,320]){await page.setViewportSize({width,height:844});for(const id of modules){await nav(page,id);assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,'connected '+id+'/'+width);report.views.push({width,module:id,connected:'shipped demo',overflow:false});}}
 report.checks.push('shipped demo capability limits, source order, overview/editor shared draft, refresh cancel/Escape/confirm, decimal 1.5, Hall10 acceptance and 5.14 normalized to5.1; out-of-range rejection, combined apply/save and RAM-only save');
 await page.locator('#disconnect').click();assert.match(await page.locator('#identity-state').textContent(),/上次读回/);assert.match(await page.locator('#active-profile-state').textContent(),/上次读回/);assert(!(await page.locator('#apply').isEnabled()));
 await page.close();
 // Fixture binds navigator.serial.requestPort to the existing DemoHost transport only.
 const fixture=await browser.newPage();fixture.on('pageerror',e=>errors.push(String(e)));
 await fixture.addInitScript(()=>{
  Object.defineProperty(navigator,'serial',{configurable:true,value:{requestPort:async()=>{
   const {DemoHost,demoPort}=await import('/src/demo.js');const host=new DemoHost(),original=host.handle.bind(host);
   host.device.profiles.forEach(p=>p.io4_bindings=Array(12).fill(0));host.device.profiles[3].motion.hall.center_gain=.5;host.device.profiles[3].motion.knobs[0].gain=12;host.device.profiles[3].motion.knobs[1].gain=1.003;host.mode='NORMAL_NKRO';
   host.testState={running:false,maintenance:'idle',failApply:false,failSave:false,serial:'TEST-FIXTURE-ONLY'};
   host.handle=r=>{
    const envelope=result=>({protocol_version:1,id:r.id,ok:true,revision:host.device.revision,applied_revision:host.device.revision,saved_revision:host.saved,dirty:host.device.revision!==host.saved,result});
    const special=['set_io4_bindings','set_startup_usb_mode','enter_bootloader','reboot_to_saved_mode','get_motion'];
    if(special.includes(r.op)){
     host.requests.push(structuredClone(r));
     if(['enter_bootloader','reboot_to_saved_mode'].includes(r.op)&&host.testState.failCommand){const response=envelope({});delete response.result;response.ok=false;response.error={code:'MAINTENANCE_REJECTED',message:'injected command rejection'};return response;}
     if(r.op==='set_io4_bindings'){host.device.profiles.find(p=>p.profile_id===r.payload.profile_id).io4_bindings=[...r.payload.bindings];host.device.revision++;}
     if(r.op==='set_startup_usb_mode'){host.mode=r.payload.mode;host.device.revision++;}
     if(r.op==='get_motion')return envelope({sample_ready:true,adc_started:true,ssi_started:true,raw_adc:[1,2,3],hall:{valid:true,mm:2,output:.5},calibration:{stage:0,job_id:0,collected:0,required:0},knobs:[{raw_turns:1.25,scaled_turns:2.5,valid:true,axis:0,crc_errors:0},{raw_turns:-1,scaled_turns:-2,valid:true,axis:1,crc_errors:0}],core1_max_service_us:20,calibration_generation:0,running:host.testState.running,adopting:false});
     return envelope({});
    }
    if((r.op==='set_profile'&&host.testState.failApply)||(r.op==='save_config'&&host.testState.failSave)){host.requests.push(structuredClone(r));const response=envelope({});delete response.result;response.ok=false;response.error={code:'FIXTURE_FAILURE',message:'injected failure'};return response;}
    const response=original(r);
    if(r.op==='hello'){response.result.firmware_version='0.10.2';response.result.serial=host.testState.serial;response.result.scope='device_schema2';Object.assign(response.result.capabilities,{io4:true,usb_cold_switch:true,bootloader_command:true,hall_calibration:true,motion_telemetry:true,hall:true,knobs:true,push_hardware:true});}
    if(r.op==='hello'&&window.testRuntime){response.result.scope='runtime_module1';Object.assign(response.result.capabilities,{profiles:false,flash_save:false,io4:false,usb_cold_switch:false,bootloader_command:false,hall_calibration:false,motion_telemetry:false});}
    if(r.op==='hello'&&window.testBadHello)response.result.product='OTHER-PRODUCT';
    if(r.op==='get_device'){response.result.schema=2;response.result.startup_usb_mode=host.mode;}
    if(r.op==='get_status')response.result.maintenance=host.testState.maintenance;
    return response;
   };
   const port=demoPort(host);port.simulated=false;window.testHost=host;return port;
  }}});
 });
 await fixture.goto(base);await fixture.locator('#connect').click();await settle(fixture);
 assert.equal(await fixture.locator('#hall-gain-number').inputValue(),'2.5');assert.equal(await fixture.locator('#knob-gain-1-number').inputValue(),'');assert.match(await fixture.locator('#hall-gain-help').textContent(),/5.0/);assert.match(await fixture.locator('#knob-gain-1-help').textContent(),/原值保留/);assert.match(await fixture.locator('#draft-state').textContent(),/一致/);
 await nav(fixture,'lighting');await fixture.locator('#light-fields input[type=color]').fill('#aabbcc');await fixture.locator('#light-fields input[type=color]').dispatchEvent('change');await fixture.locator('#apply').click();await settle(fixture);
 assert.equal(await fixture.locator('#hall-gain-number').inputValue(),'2.5');assert.equal(await fixture.locator('#knob-gain-1-number').inputValue(),'');
 assert.equal(await fixture.evaluate(()=>testHost.device.profiles[3].motion.hall.center_gain),.5);
 assert.equal(await fixture.evaluate(()=>testHost.device.profiles[3].motion.knobs[0].gain),12);
 assert.equal(await fixture.evaluate(()=>testHost.device.profiles[3].motion.knobs[1].gain),1.003);
 assert.equal(await fixture.locator('#knob-gain-2-number').inputValue(),'');assert.match(await fixture.locator('#knob-gain-2-help').textContent(),/原值保留/);
 let writes=()=>fixture.evaluate(()=>testHost.requests.filter(r=>!r.op.startsWith('get_')&&!['hello','list_profiles'].includes(r.op)).map(r=>r.op));
 let before=(await writes()).length;
 for(const cancel of ['cancel','Escape']){await fixture.locator('#io4-mode-switch').click();if(cancel==='cancel')await fixture.locator('[data-cancel]').click();else await fixture.keyboard.press('Escape');await settle(fixture);assert.equal((await writes()).length,before);assert.equal(await fixture.locator('#io4-mode-switch').getAttribute('aria-checked'),'false');}
 await fixture.locator('#io4-mode-switch').click();await fixture.locator('[data-confirm]').click();await settle(fixture);assert.equal((await writes()).length,before);assert.equal(await fixture.locator('#io4-mode-switch').getAttribute('aria-checked'),'true');await fixture.locator('#apply').click();await settle(fixture);assert.deepEqual((await writes()).slice(before),['set_startup_usb_mode','save_config']);
 for(const id of ['restart-device','integration-bootloader']){
  if(id==='integration-bootloader')await nav(fixture,'update');
  before=(await writes()).length;
  await fixture.locator('#'+id).click();if(id==='integration-bootloader'){assert.equal(await fixture.locator('.uf2-dialog-steps li').count(),3);await fixture.locator('.uf2-dialog-fallback > summary').click();assert.match(await fixture.locator('.uf2-dialog-fallback').textContent(),/BOOT.*RUN/);}await fixture.screenshot({path:fileURLToPath(new URL(id+'-confirmation.png',out))});await fixture.locator('[data-cancel]').click();await settle(fixture);assert.equal((await writes()).length,before);
  await fixture.locator('#'+id).click();await fixture.keyboard.press('Escape');await settle(fixture);assert.equal((await writes()).length,before);
  await fixture.locator('#'+id).click();await fixture.locator('[data-confirm]').click();await settle(fixture);assert.equal((await writes()).length,before+1);
 }
 report.checks.push('protocol fixture only: old gain0.5/12 read unchanged and unrelated save, IO4 selection no commands, mode/save sequence, reboot/UF2 cancel/Escape/confirm');
 await nav(fixture,'experimental');await fixture.locator('#io4-mapping-editor > summary').click();await fixture.locator('#io4-binding-10').selectOption('11');assert(!(await fixture.locator('#apply').isEnabled()));assert(!(await fixture.locator('#io4-mode-switch').isEnabled()));
 before=(await writes()).length;await fixture.locator('#io4-mapping-apply').click();await settle(fixture);assert.deepEqual((await writes()).slice(before),['set_io4_bindings','save_config']);assert.equal(await fixture.evaluate(()=>testHost.device.active_profile),4);
 await fixture.locator('#experiment-hall > summary').click();assert.equal(await fixture.locator('#hall-advanced-fields .parameter-help').count(),11);assert(await fixture.locator('#integration-left').isVisible());
 await fixture.evaluate(()=>testHost.testState.running=true);await fixture.waitForFunction(()=>document.getElementById('apply').disabled);assert(!(await fixture.locator('#integration-bootloader').isEnabled()));await fixture.evaluate(()=>testHost.testState.running=false);await fixture.waitForFunction(()=>!document.getElementById('apply').disabled);
 await fixture.evaluate(()=>testHost.testState.maintenance='writing');await fixture.waitForFunction(()=>document.getElementById('apply').disabled);await fixture.evaluate(()=>testHost.testState.maintenance='idle');await fixture.waitForFunction(()=>!document.getElementById('apply').disabled);
 await fixture.evaluate(()=>testHost.device.revision++);await fixture.waitForFunction(()=>document.getElementById('apply').disabled);await fixture.locator('#refresh').click();await settle(fixture);assert(await fixture.locator('#apply').isEnabled());
 report.checks.push('IO4 dedicated apply/save and mutual locks, same physical grid, eleven Hall help fields, direct shared calibration, calibration/maintenance/revision locks');
 for(const width of [1274,390,320]){
  await fixture.setViewportSize({width,height:width===1274?1291:844});
  for(const id of modules){await nav(fixture,id);assert.equal(await fixture.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,'fixture '+id+'/'+width);report.views.push({width,module:id,connected:'isolated fixture',overflow:false});}
  await nav(fixture,'knobs');await fixture.screenshot({path:fileURLToPath(new URL('knobs-'+width+'.png',out)),fullPage:true});
  const range=fixture.locator('#knob-gain-2-ruler'),ticks=fixture.locator('#knob-gain-2-ruler + .gain-ticks span');await range.scrollIntoViewIfNeeded();const box=await range.boundingBox();
  for(const n of [1,5,10]){await range.scrollIntoViewIfNeeded();const box=await range.boundingBox();const tick=await ticks.nth(n-1).boundingBox();assert(Math.abs(tick.x-(box.x+8+(box.width-16)*(n-1)/9))<1.5);await fixture.mouse.click(tick.x,box.y+box.height/2);assert(Math.abs(Number(await fixture.locator('#knob-gain-2-number').inputValue())-n)<.15,'ruler mouse '+width+'/'+n+' value '+await fixture.locator('#knob-gain-2-number').inputValue());}
  await fixture.locator('#knob-gain-2-number').fill('1.5');await fixture.locator('#knob-gain-2-number').press('Tab');await fixture.locator('#refresh').click();await fixture.locator('[data-confirm]').click();await settle(fixture);
  await nav(fixture,'device-status');await fixture.screenshot({path:fileURLToPath(new URL('status-'+width+'.png',out)),fullPage:true});
  await nav(fixture,'update');await fixture.locator('#integration-bootloader').click();await fixture.locator('.uf2-dialog-fallback > summary').click();assert.equal(await fixture.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);await fixture.screenshot({path:fileURLToPath(new URL('uf2-dialog-'+width+'.png',out))});await fixture.keyboard.press('Escape');await settle(fixture);
 }
 report.checks.push('integer tick centres/native thumb click targets at 1274/390/320, decimal input, fixture responsive views');
 // All three controls use the same scale, protocol payload still contains gain.
 for(const [module,id,key,knob] of [['hall','hall-gain','center_gain',null],['knobs','knob-gain-1','gain',0],['knobs','knob-gain-2','gain',1]]){
  await nav(fixture,module);const input=fixture.locator('#'+id+'-number');
  assert.equal(await input.getAttribute('step'),'0.1');assert.equal(await input.getAttribute('min'),'1');assert.equal(await input.getAttribute('max'),'10');
  for(const [scale,gain] of [[1,.2],[4.9,.98],[5,1],[5.1,1.02],[10,2]]){
   await input.fill(String(scale));await input.press('Tab');assert.equal(await input.inputValue(),scale.toFixed(1));
   await fixture.locator('#apply').click();await settle(fixture);
   assert.equal(await input.inputValue(),scale.toFixed(1));
   const stored=await fixture.evaluate(({key,knob})=>{const m=testHost.device.profiles[3].motion;return knob===null?m.hall[key]:m.knobs[knob][key];},{key,knob});assert.equal(stored,gain);
   const sent=await fixture.evaluate(({key,knob})=>{const m=testHost.requests.filter(r=>r.op==='set_profile').at(-1).payload.motion;return knob===null?m.hall[key]:m.knobs[knob][key];},{key,knob});assert.equal(sent,gain);
  }
  await input.fill('4.94');await input.press('Tab');assert.equal(await input.inputValue(),'4.9');
  await input.press('ArrowUp');await input.press('Tab');assert.equal(await input.inputValue(),'5.0');
  await fixture.locator('#refresh').click();await fixture.locator('[data-confirm]').click();await settle(fixture);
 }
 report.checks.push('status icon renders; Hall and both knobs endpoints/4.9/5.1, one decimal rounding, arrow0.1, actual CDC gains and save/readback, old precision and old12 retained');
 await nav(fixture,'hall');await fixture.locator('#hall-gain-number').fill('1.5');await fixture.locator('#hall-gain-number').press('Tab');await fixture.evaluate(()=>testHost.testState.failApply=true);before=(await writes()).length;await fixture.locator('#apply').click();await settle(fixture);assert.match(await fixture.locator('#notice').textContent(),/未开始保存/);assert.deepEqual((await writes()).slice(before),['set_profile']);assert(!(await fixture.locator('#apply').isEnabled()));
 await fixture.evaluate(()=>testHost.testState.failApply=false);await fixture.locator('#refresh').click();await fixture.locator('[data-confirm]').click();await settle(fixture);
 await fixture.locator('#hall-gain-number').fill('1.5');await fixture.locator('#hall-gain-number').press('Tab');await fixture.evaluate(()=>testHost.testState.failSave=true);before=(await writes()).length;await fixture.locator('#apply').click();await settle(fixture);assert.match(await fixture.locator('#notice').textContent(),/保存未确认/);assert.deepEqual((await writes()).slice(before),['set_profile','save_config']);assert(!(await fixture.locator('#apply').isEnabled()));
 await fixture.evaluate(()=>testHost.testState.failSave=false);await fixture.locator('#refresh').click();await settle(fixture);assert(await fixture.locator('#apply').isEnabled());
 report.checks.push('browser injected apply failure stops save; save failure leaves applied RAM, disables retry and refresh recovers');
 await fixture.locator('#disconnect').click();assert.match(await fixture.locator('#active-profile-state').textContent(),/上次读回/);assert.match(await fixture.locator('#integration-knob-0').textContent(),/等待当前设备/);await fixture.locator('#connect').click();await settle(fixture);assert.match(await fixture.locator('#identity-state').textContent(),/当前会话/);assert.equal(await fixture.locator('#hall-gain-number').inputValue(),'2.5');
 await nav(fixture,'update');
 // Synthetic parser fixture is deliberately not a flashable firmware build.
 const synthetic=new Uint8Array(512),view=new DataView(synthetic.buffer),encode=new TextEncoder();
 for(const [offset,value] of [[0,0x0a324655],[4,0x9e5d5157],[8,0x2000],[12,0x10000000],[16,256],[20,0],[24,1],[28,0xe48bff56],[508,0x0ab16f30]])view.setUint32(offset,value,true);
 synthetic.set(encode.encode('FLUXRAIL-FW-V1'),32+48);view.setUint32(32+48+16,1,true);synthetic.set(encode.encode('fluxrail-rp2040-4mib'),32+48+20);synthetic.set(encode.encode('0.10.2'),32+48+52);view.setUint32(32+48+84,0x46525746,true);
 await fixture.locator('#uf2-file').setInputFiles({name:'TEST-ONLY-parser.uf2',mimeType:'application/octet-stream',buffer:Buffer.from(synthetic)});await settle(fixture);assert.match(await fixture.locator('#uf2-result').textContent(),/0.10.2/);assert.match(await fixture.locator('#version-result').textContent(),/相同/);
 await fixture.locator('#uf2-file').setInputFiles({name:'invalid.uf2',mimeType:'application/octet-stream',buffer:Buffer.from([0])});await settle(fixture);assert.match(await fixture.locator('#uf2-result').textContent(),/固件不对/);
 report.checks.push('browser UF2 parser fixture recognizes payload0.10.2 and clears invalid candidate; no firmware uploaded');
 await fixture.evaluate(()=>testHost.testState.failCommand=true);await fixture.locator('#integration-bootloader').click();await fixture.locator('[data-confirm]').click();await settle(fixture);assert(!(await fixture.locator('#apply').isEnabled()));assert(!(await fixture.locator('#integration-bootloader').isEnabled()));assert.match(await fixture.locator('#notice').textContent(),/MAINTENANCE_REJECTED/);
 await fixture.evaluate(()=>testHost.testState.failCommand=false);await fixture.locator('#refresh').click();await settle(fixture);assert(await fixture.locator('#integration-bootloader').isEnabled());report.checks.push('non-timeout command rejection invalidates writable state until explicit readback');
 await fixture.locator('#disconnect').click();await fixture.evaluate(()=>{window.testRuntime=true;});await fixture.locator('#connect').click();await settle(fixture);
 assert.match(await fixture.locator('#active-profile-state').textContent(),/仅提供运行配置/);assert.match(await fixture.locator('#apply').textContent(),/不支持保存/);assert(!(await fixture.locator('#profile-select').isEnabled()));assert(!(await fixture.locator('#io4-mode-switch').isEnabled()));
 await nav(fixture,'keyboard');await fixture.locator('#key-0 select').selectOption('key:5');before=(await writes()).length;await fixture.locator('#apply').click();await settle(fixture);assert.deepEqual((await writes()).slice(before),['set_config']);assert.match(await fixture.locator('#notice').textContent(),/不支持保存/);
 await fixture.locator('#disconnect').click();await fixture.evaluate(()=>{window.testBadHello=true;});await fixture.locator('#connect').click();await fixture.waitForFunction(()=>!document.getElementById('connect').disabled);
 assert.equal(await fixture.locator('#status-data').textContent(),'等待当前设备 get_status');assert(!(await fixture.locator('#apply').isEnabled()));assert.match(await fixture.locator('#active-profile-state').textContent(),/连接后查看/);
 report.checks.push('legacy runtime get_config/set_config only and no fictional active template; failed new handshake clears previous diagnosis/cache');
 await fixture.close();assert.deepEqual(errors,[]);report.page_errors=errors;
 await writeFile(new URL('browser-check.json',out),JSON.stringify(report,null,2));console.log(JSON.stringify({views:report.views.length,checks:report.checks,page_errors:errors}));
}finally{await browser.close();}
