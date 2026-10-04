// Separate headless Edge profile; real navigator.serial, no desktop interaction.
const fs=require('node:fs');const path=require('node:path');
const {chromium}=require('C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const out=path.join(__dirname,'pc_board_2026-10-04','webserial-headless-preflight');
if(fs.existsSync(out))throw Error('Refusing to overwrite evidence');fs.mkdirSync(out);
const report={started:new Date().toISOString(),desktop_control:false,headless:true,hardware_writes:0,prompts:[],errors:[]};
const put=()=>fs.writeFileSync(path.join(out,'summary.json'),JSON.stringify(report,null,2));
(async()=>{
 let context;
 try {
  context=await chromium.launchPersistentContext(path.join(out,'isolated-profile'),{executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true,args:['--disable-gpu','--disable-background-networking'],timeout:20000});
  const page=await context.newPage();
  await context.route('**/*',route=>route.request().url().startsWith('http://localhost:4174/')?route.continue():route.abort());
  page.on('console',msg=>{if(msg.type()==='error')report.errors.push(msg.text());});
  const cdp=await context.newCDPSession(page);await cdp.send('DeviceAccess.enable');
  cdp.on('DeviceAccess.deviceRequestPrompted',async event=>{
   report.prompts.push(event);put();
   const target=event.devices.filter(d=>/\bCOM29\b/.test(d.name));
   try {
    if(target.length===1)await cdp.send('DeviceAccess.selectPrompt',{id:event.id,deviceId:target[0].id});
    else await cdp.send('DeviceAccess.cancelPrompt',{id:event.id});
   }catch(error){report.errors.push(String(error));put();}
  });
  await page.goto('http://localhost:4174/');
  report.capabilities=await page.evaluate(()=>({secure:window.isSecureContext,serial:!!navigator.serial}));
  await page.evaluate(async()=>{
   globalThis.cdcEvidence=[];
   const {SerialClient}=await import('/src/connection/client.js');
   const originalOpen=SerialClient.prototype.open,originalSend=SerialClient.prototype.send;
   SerialClient.prototype.open=async function(...args){globalThis.testClient=this;return originalOpen.apply(this,args);};
   SerialClient.prototype.send=async function(op,payload,timeout){if(!['hello','get_status','get_device','list_profiles','get_profile','get_motion','get_usb_mode','get_io4_bindings'].includes(op))throw Error('Read-only preflight blocked '+op);try{const reply=await originalSend.call(this,op,payload,timeout);cdcEvidence.push({op,payload,response:structuredClone(this.last)});return reply;}catch(error){cdcEvidence.push({op,payload,error:String(error)});throw error;}};
  });
  await cdp.send('Runtime.evaluate',{expression:"document.querySelector('#connect').click()",userGesture:true});
  try {await page.waitForFunction(()=>document.querySelector('#connection-state').textContent==='已连接并完成读回',null,{timeout:14000});report.connected=true;}catch(error){report.connected=false;report.error=String(error);}
  report.page=await page.evaluate(()=>({connection:document.querySelector('#connection-state').textContent,notice:document.querySelector('#notice').textContent,portType:globalThis.testClient?.port?.constructor?.name,simulated:!!globalThis.testClient?.port?.simulated}));
  if(report.connected){
   report.hello=await page.evaluate(()=>testClient.request('hello'));
   if(report.hello.serial!=='5054316560DD729C'||report.hello.firmware_version!=='0.10.1'||report.hello.usb_mode!=='NORMAL_NKRO')throw Error('Identity mismatch');
   report.health=await page.evaluate(()=>testClient.request('get_status'));
   await page.evaluate(()=>document.querySelector('#disconnect').click());
   await page.waitForFunction(()=>document.querySelector('#connection-state').textContent==='未连接');
  }
  fs.writeFileSync(path.join(out,'cdc.json'),JSON.stringify(await page.evaluate(()=>cdcEvidence),null,2));
 }catch(error){report.error=String(error);report.stack=error.stack;}
 finally {if(context)await context.close();report.finished=new Date().toISOString();put();process.exitCode=report.connected&&!report.error?0:1;}
})();
