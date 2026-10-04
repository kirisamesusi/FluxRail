// Actual Chromium Web Serial and production page, in a separate invisible process.
// Only chooser/temporary permission decisions are adapted. No mock serial stream.
const {app,BrowserWindow,session}=require('electron');
const fs=require('node:fs');const path=require('node:path');const assert=require('node:assert/strict');
const root=path.resolve(__dirname,'pc_board_2026-10-04');
const out=path.join(root,'webserial-offscreen-v4');
assert(!fs.existsSync(out),'Refusing to overwrite evidence');fs.mkdirSync(out);
app.setPath('userData',path.join(out,'isolated-browser-data'));
app.disableHardwareAcceleration();
const origin='http://localhost:4174';
const correlation=JSON.parse(fs.readFileSync(path.join(root,'webserial-pnp-correlation.json'),'utf8').replace(/^\uFEFF/,''));
assert.equal(correlation.parent,'USB\\VID_239A&PID_CAFE\\5054316560DD729C');
assert.equal(correlation.port,'COM29');
assert(Date.now()-Date.parse(correlation.time)<300000,'Fresh PnP identity required');
const report={started:new Date().toISOString(),kind:'offscreen_chromium_real_webserial',desktop_control:false,versions:process.versions,steps:[],ports:[],pnp_correlation:correlation,error:null,restored:false};
let win;
const put=(name,value)=>fs.writeFileSync(path.join(out,name),JSON.stringify(value,null,2));
const deadline=setTimeout(()=>{report.error='Overall timeout; independent CDC restoration required if mutation_attempted';put('summary.json',report);if(win&&!win.isDestroyed())win.destroy();app.exit(1);},180000);
app.whenReady().then(async()=>{
 const ses=session.fromPartition('fluxrail-'+Date.now());
 ses.webRequest.onBeforeRequest((details,callback)=>callback({cancel:!details.url.startsWith(origin+'/')}));
 const sameOrigin=value=>{try{return new URL(value).origin===origin;}catch{return false;}};
 const permLog=value=>{report.permissions??=[];report.permissions.push(value);put('summary.json',report);};
 ses.setPermissionCheckHandler((contents,permission,requestingOrigin,details)=>{
  const allowed=permission==='serial'&&sameOrigin(details.securityOrigin)&&sameOrigin(contents?.getURL());
  permLog({kind:'check',permission,requestingOrigin,details,allowed});return allowed;
 });
 ses.setPermissionRequestHandler((contents,permission,callback,details)=>{
  const allowed=permission==='serial'&&sameOrigin(details.requestingUrl)&&sameOrigin(contents?.getURL());
  permLog({kind:'request',permission,details,allowed});callback(allowed);
 });
 ses.setDevicePermissionHandler(details=>{
  const allowed=details.deviceType==='serial'&&sameOrigin(details.origin)&&details.device?.device_instance_id===correlation.deviceInstanceId;
  permLog({kind:'device',details,allowed});return allowed;
 });
 ses.on('select-serial-port',(event,ports,contents,callback)=>{
  event.preventDefault();report.ports.push(...ports);
  const targets=ports.filter(p=>p.portName===correlation.port&&p.deviceInstanceId===correlation.deviceInstanceId&&Number(p.vendorId)===0x239a&&Number(p.productId)===0xcafe);
  callback(targets.length===1?targets[0].portId:'');
 });
 win=new BrowserWindow({show:false,focusable:false,skipTaskbar:true,width:1280,height:900,webPreferences:{session:ses,offscreen:true,nodeIntegration:false,contextIsolation:true,sandbox:true,backgroundThrottling:false}});
 win.webContents.setWindowOpenHandler(()=>({action:'deny'}));
 win.webContents.on('will-navigate',(event,url)=>{if(!url.startsWith(origin+'/'))event.preventDefault();});
 const js=(code,gesture=false)=>win.webContents.executeJavaScript(code,gesture);
 const wait=async(code)=>{const stop=Date.now()+18000;while(Date.now()<stop){if(await js(code))return;await new Promise(r=>setTimeout(r,100));}throw Error('Page condition timeout: '+code);};
 const snap=async(name)=>{
  const s=await js(`({connection:document.querySelector('#connection-state').textContent,identity:document.querySelector('#identity').textContent,notice:document.querySelector('#notice').textContent,draft:document.querySelector('#draft-state').textContent,saved:document.querySelector('#save-state').textContent,visible:document.visibilityState,requestPortErrors:globalThis.requestPortErrors||[],serialOpenErrors:globalThis.serialOpenErrors||[],portType:globalThis.testClient?.port?.constructor?.name,simulated:!!globalThis.testClient?.port?.simulated})`);
  report.steps.push({name,time:new Date().toISOString(),...s});put(name+'.json',s);
  put('cdc.json',await js(`globalThis.cdcEvidence || []`));put('summary.json',report);
 };
 let original;
 try {
  await win.loadURL(origin+'/');
  await js(`(async()=>{
   globalThis.cdcEvidence=[];globalThis.requestPortErrors=[];globalThis.serialOpenErrors=[];
   const realRequestPort=navigator.serial.requestPort.bind(navigator.serial);
   navigator.serial.requestPort=async function(...args){try{return await realRequestPort(...args);}catch(error){globalThis.requestPortErrors.push({name:error.name,message:error.message,stack:error.stack});throw error;}};
   const {SerialClient}=await import('/src/connection/client.js');
   const realOpen=SerialClient.prototype.open,realSend=SerialClient.prototype.send;
   SerialClient.prototype.open=async function(...args){globalThis.testClient=this;try{return await realOpen.apply(this,args);}catch(error){globalThis.serialOpenErrors.push({name:error.name,message:error.message,stack:error.stack});throw error;}};
   SerialClient.prototype.send=async function(op,payload,timeout){const start=new Date().toISOString();try{const result=await realSend.call(this,op,payload,timeout);globalThis.cdcEvidence.push({time:start,op,payload,response:structuredClone(this.last)});return result;}catch(error){globalThis.cdcEvidence.push({time:start,op,payload,error:String(error)});throw error;}};
  })()`);
  assert(await js(`window.isSecureContext && !!navigator.serial`));
  await js(`document.querySelector('#connect').click()`,true);
  await wait(`document.querySelector('#connection-state').textContent==='已连接并完成读回'`);
  await snap('connected');
  const h=await js(`testClient.request('hello')`);
  assert.equal(h.serial,'5054316560DD729C');assert.equal(h.firmware_version,'0.10.1');assert.equal(h.usb_mode,'NORMAL_NKRO');
  assert.equal(await js(`testClient.port.constructor.name`),'SerialPort');
  const health=await js(`testClient.request('get_status')`);assert.equal(health.physical_mask,0);assert.equal(health.config_ready,true);
  original=await js(`testClient.request('get_profile',{profile_id:4})`);put('original-profile.json',original);
  const originalValue=original.config.scan.press_debounce_us;
  const testValue=originalValue===20000?19999:originalValue+1;
  await js(`(()=>{const input=document.querySelectorAll('#scan-editor input')[1];input.value=${testValue};input.dispatchEvent(new Event('change',{bubbles:true}));})()`);
  await snap('draft-only');
  assert.equal(await js(`cdcEvidence.filter(r=>r.op==='set_profile').length`),0);
  report.mutation_attempted=true;put('summary.json',report);
  await js(`document.querySelector('#apply').click()`);
  await wait(`document.querySelector('#notice').textContent.startsWith('设置已应用并读回')`);
  const applied=await js(`testClient.request('get_profile',{profile_id:4})`);
  assert.equal(applied.config.scan.press_debounce_us,testValue);
  await snap('applied');
  await js(`document.querySelector('#save').click()`);
  await wait(`document.querySelector('#confirmation-dialog').open`);
  await js(`document.querySelector('#confirmation-dialog [data-confirm]').click()`);
  await wait(`document.querySelector('#notice').textContent==='设备已完成保存，保存状态已读回。'`);
  await snap('saved');
  await js(`document.querySelector('#disconnect').click()`);
  await wait(`document.querySelector('#connection-state').textContent==='未连接'`);
  await snap('disconnected');
  await js(`document.querySelector('#connect').click()`,true);
  await wait(`document.querySelector('#connection-state').textContent==='已连接并完成读回'`);
  const reconnect=await js(`testClient.request('get_profile',{profile_id:4})`);
  assert.equal(reconnect.config.scan.press_debounce_us,testValue);
  await snap('reconnected');report.reconnect_saved_readback=true;
 } catch(error){report.error=String(error);report.stack=error.stack;}
 finally {
  try {
   if(original&&report.mutation_attempted){
    if(!await js(`!!globalThis.testClient?.alive`)){
     await js(`document.querySelector('#connect').click()`,true);
     await wait(`document.querySelector('#connection-state').textContent==='已连接并完成读回'`);
    }
    const health=await js(`testClient.request('get_status')`);
    assert.equal(health.physical_mask,0);assert.equal(health.config_ready,true);assert.equal(health.maintenance,'idle');
    await js(`testClient.request('set_profile',{expected_revision:testClient.revision,profile_id:4,config:${JSON.stringify(original.config)},motion:${JSON.stringify(original.motion)}},4000)`);
    await js(`testClient.request('save_config',{expected_revision:testClient.revision},12000)`);
    const final=await js(`testClient.request('get_profile',{profile_id:4})`);
    assert.deepEqual(final,original);report.restored=true;
   } else {report.no_device_mutation=true;}
   if(await js(`!!globalThis.testClient?.alive`)){await js(`document.querySelector('#disconnect').click()`);await wait(`document.querySelector('#connection-state').textContent==='未连接'`);}
   put('cdc.json',await js(`globalThis.cdcEvidence || []`));await snap('final');
  }catch(error){report.restore_error=String(error);}
  report.native_window_visible=win.isVisible();assert.equal(win.isVisible(),false);
  report.finished=new Date().toISOString();put('summary.json',report);clearTimeout(deadline);win.destroy();app.exit(report.error||report.restore_error?1:0);
 }
}).catch(error=>{report.error=String(error);report.stack=error.stack;put('summary.json',report);clearTimeout(deadline);app.exit(1);});
