// Actual production Web SerialClient over a .NET CDC port adapter. The adapter
// changes host transport only; all response data is from the connected board.
// This does NOT test Chrome Web Serial permissions, DOM/UI, or unplug behavior.
import {SerialClient} from '../../web/src/connection/client.js';
import fs from 'node:fs/promises';
import {spawn} from 'node:child_process';
import assert from 'node:assert/strict';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const here=path.dirname(fileURLToPath(import.meta.url)),out=path.join(here,'hardware_2026-10-04');
const portName=process.argv[2];if(!/^COM\d+$/.test(portName||''))throw Error('Pass confirmed port');
const baseline=JSON.parse(await fs.readFile(path.join(out,'baseline.json'),'utf8'));
const evidence=[],faults=[],queue=[];let waiter=null,proc,closed=false,buf='',tagWaiter=null;
const encoder=new TextEncoder();
function feedRaw(line){for(let offset=0;offset<line.length;offset+=7){const bytes=encoder.encode(line.slice(offset,offset+7));if(waiter){const w=waiter;waiter=null;w({value:bytes,done:false});}else queue.push(bytes);}}
function done(){closed=true;if(waiter){waiter({done:true});waiter=null;}}
const transport={
 async open(){
  const ready=new Promise((resolve,reject)=>{tagWaiter={resolve,reject};});
  proc=spawn('C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe',['-NoProfile','-ExecutionPolicy','Bypass','-File',path.join(here,'serial_bridge.ps1'),'-Port',portName],{windowsHide:true,stdio:['pipe','pipe','pipe']});
  proc.stdin.on('error',e=>{tagWaiter?.reject(e);done();});proc.on('error',e=>{tagWaiter?.reject(e);done();});proc.on('close',done);
  proc.stdout.on('data',b=>{buf+=b.toString();while(buf.includes('\n')){const i=buf.indexOf('\n'),line=buf.slice(0,i).trim();buf=buf.slice(i+1);const r=JSON.parse(line);if(r.bridge_error){tagWaiter?.reject(Error(r.bridge_error));done();}else if(r.bridge){tagWaiter?.resolve(r);tagWaiter=null;}else{evidence.push({response:r,time:new Date().toISOString()});feedRaw(line+'\n');}}});
  proc.stderr.on('data',b=>evidence.push({stderr:b.toString()}));await ready;
 },
 async setSignals({dataTerminalReady}){if(closed)return;const reply=new Promise((resolve,reject)=>tagWaiter={resolve,reject});proc.stdin.write('__ctl__'+JSON.stringify({action:'signals',dtr:dataTerminalReady})+'\n');await reply;},
 readable:{getReader(){return {async read(){if(queue.length)return {value:queue.shift(),done:false};if(closed)return {done:true};return new Promise(resolve=>waiter=resolve);},async cancel(){if(waiter){waiter({done:true});waiter=null;}},releaseLock(){}};}},
 writable:{getWriter(){return {async write(bytes){evidence.push({request:new TextDecoder().decode(bytes),time:new Date().toISOString()});await new Promise((resolve,reject)=>proc.stdin.write(bytes,e=>e?reject(e):resolve()));},async abort(){},releaseLock(){}};}},
 async close(){if(!closed){proc.stdin.end('__exit__\n');await Promise.race([new Promise(r=>proc.once('close',r)),new Promise(r=>setTimeout(r,3000))]);if(!closed)proc.kill();}done();}
};
const client=new SerialClient(transport,e=>faults.push({code:e.code,message:e.message}));
const report={kind:'real_board_with_host_adapter',browser_ui_tested:false,flash_written:false,started:new Date().toISOString()};
try{
 await client.open();const h=await client.request('hello');assert.equal(h.serial,baseline.hello.result.serial);assert.equal(h.firmware_version,'0.10.0');
 const initialRevision=client.revision,expected=JSON.parse(await fs.readFile(path.join(out,'original-profiles.json'),'utf8'));
 for(let n=0;n<10;n++){
  const responses=await Promise.all(['hello','get_status','get_device','get_motion','get_config'].map(op=>client.request(op)));
  assert.equal(responses[0].serial,h.serial);assert.equal(responses[1].config_ready,true);assert.equal(responses[1].apply_failed,false);assert.equal(responses[2].active_profile,baseline.get_device.result.active_profile);assert.deepEqual(responses[4].config,expected[responses[2].active_profile].config);assert.equal(client.revision,initialRevision);
 }
 await assert.rejects(client.request('get_profile',{profile_id:999999}),e=>e.code==='INVALID_PROFILE');
 assert.equal((await client.request('hello')).serial,h.serial);assert.equal(client.alive,true);assert.deepEqual(faults,[]);
 report.result={passed:true,version:h.firmware_version,serial:h.serial,revision:client.revision,queued_read_requests:50,expected_device_error_recovered:true,receive_fragment_bytes:7,faults};
}catch(e){report.failure={message:e.message,stack:e.stack};process.exitCode=1;}
finally{await client.close();report.finished=new Date().toISOString();await fs.writeFile(path.join(out,'web-client-report.json'),JSON.stringify(report,null,2));await fs.writeFile(path.join(out,'web-client-cdc.jsonl'),evidence.map(x=>JSON.stringify(x)).join('\n')+'\n');console.log(JSON.stringify(report));}
