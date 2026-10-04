from pathlib import Path
p=Path(__file__).with_name('test_webserial_offscreen_v3.cjs');o=p.with_name('test_webserial_offscreen_v4.cjs')
assert not o.exists()
s=p.read_text(encoding='utf-8').replace("'webserial-offscreen-v3'", "'webserial-offscreen-v4'")
s=s.replace("details.device?.serialNumber==='5054316560DD729C'", "details.device?.device_instance_id===correlation.deviceInstanceId")
s=s.replace("globalThis.requestPortErrors=[];", "globalThis.requestPortErrors=[];globalThis.serialOpenErrors=[];")
s=s.replace("globalThis.testClient=this;return realOpen.apply(this,args);", "globalThis.testClient=this;try{return await realOpen.apply(this,args);}catch(error){globalThis.serialOpenErrors.push({name:error.name,message:error.message,stack:error.stack});throw error;}")
s=s.replace("requestPortErrors:globalThis.requestPortErrors||[],", "requestPortErrors:globalThis.requestPortErrors||[],serialOpenErrors:globalThis.serialOpenErrors||[],")
o.write_text(s,encoding='utf-8')
