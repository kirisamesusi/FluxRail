from pathlib import Path
src = Path(__file__).with_name('test_webserial_offscreen.cjs')
dst = src.with_name('test_webserial_offscreen_v2.cjs')
assert not dst.exists()
s = src.read_text(encoding='utf-8')
s = s.replace("'webserial-offscreen'", "'webserial-offscreen-v2'")
s = s.replace(" ses.setPermissionCheckHandler((contents,permission,requestingOrigin)=>permission==='serial'&&requestingOrigin===origin);", """ const sameOrigin=value=>{try{return new URL(value).origin===origin;}catch{return false;}};
 const permLog=value=>{report.permissions??=[];report.permissions.push(value);put('summary.json',report);};
 ses.setPermissionCheckHandler((contents,permission,requestingOrigin,details)=>{
  const allowed=permission==='serial'&&sameOrigin(details.securityOrigin)&&sameOrigin(contents?.getURL());
  permLog({kind:'check',permission,requestingOrigin,details,allowed});return allowed;
 });""")
s = s.replace(" ses.setPermissionRequestHandler((contents,permission,callback,details)=>callback(permission==='serial'&&details.requestingUrl?.startsWith(origin+'/')));", """ ses.setPermissionRequestHandler((contents,permission,callback,details)=>{
  const allowed=permission==='serial'&&sameOrigin(details.requestingUrl)&&sameOrigin(contents?.getURL());
  permLog({kind:'request',permission,details,allowed});callback(allowed);
 });""")
s = s.replace(" ses.setDevicePermissionHandler(details=>details.deviceType==='serial'&&details.origin===origin&&details.device?.serialNumber==='5054316560DD729C');", """ ses.setDevicePermissionHandler(details=>{
  const allowed=details.deviceType==='serial'&&sameOrigin(details.origin)&&details.device?.serialNumber==='5054316560DD729C';
  permLog({kind:'device',details,allowed});return allowed;
 });""")
s = s.replace("globalThis.cdcEvidence=[];", """globalThis.cdcEvidence=[];globalThis.requestPortErrors=[];
   const realRequestPort=navigator.serial.requestPort.bind(navigator.serial);
   navigator.serial.requestPort=async function(...args){try{return await realRequestPort(...args);}catch(error){globalThis.requestPortErrors.push({name:error.name,message:error.message,stack:error.stack});throw error;}};""")
s = s.replace("visible:document.visibilityState,", "visible:document.visibilityState,requestPortErrors:globalThis.requestPortErrors||[],")
dst.write_text(s,encoding='utf-8')
