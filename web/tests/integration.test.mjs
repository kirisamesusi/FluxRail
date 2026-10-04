import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { decodeSnapshot,archive,parseArchive,hex,crc32,exportSnapshot } from '../src/snapshot.js';
import { verifyHello } from '../src/config.js';
import { Device } from '../src/device.js';
import { DemoHost } from '../src/demo.js';
const bytes=new Uint8Array(await readFile(new URL('../../tests/integration/schema2_defaults.bin',import.meta.url)));
test('schema2正式金样和IO4功能表/默认色逐字段兼容，schema1继续兼容',()=>{
  const d=decodeSnapshot(bytes);assert.equal(d.schema,2);assert.equal(bytes.length,1387);assert.deepEqual(d.profiles[1].motion.knobs.map(k=>k.enabled),[false,false]);
  assert.equal(d.profiles[0].io4_bindings.length,12);assert.deepEqual(d.profiles[0].config.indicator.color,[0,255,255]);
  const a=archive(bytes,{product:'FluxRail',board:'FluxRail RP2040 4MiB',serial:'x'});assert.equal(a.schema,2);assert.deepEqual(parseArchive(JSON.stringify(a)).bytes,bytes);
  assert.throws(()=>parseArchive(JSON.stringify({...a,schema:1,encoding:'hex-le-schema1'})));
});
test('schema2 startup IO4、错误功能enum及header拒绝',()=>{
  const b=bytes.slice();b[17]=1;assert.equal(decodeSnapshot(b).startup_usb_mode,'IO4');b[17]=2;assert.throws(()=>decodeSnapshot(b));
  const bad=bytes.slice();bad[18+329]=13;assert.throws(()=>decodeSnapshot(bad));
});
test('schema2完整分页导出在每块校验revision/CRC',async()=>{
  const client={revision:1,request:async(op,{offset})=>({schema:2,encoding:'hex-le-schema2',length:bytes.length,crc32:crc32(bytes),offset,data:hex(bytes.slice(offset,offset+512)),next_offset:Math.min(offset+512,bytes.length)})};
  assert.deepEqual(await exportSnapshot(client),bytes);
});
test('整合固件握手/读回及set_profile保留独立IO4表',async()=>{
  const host=new DemoHost();const original=host.handle.bind(host);const decoded=decodeSnapshot(bytes);
  const client={alive:true,revision:1,last:null,request:async(op,p={})=>{
    const response=original({protocol_version:1,id:1,op,payload:p});assert.equal(response.ok,true);
    if(op==='hello'){response.result.scope='device_schema2';response.result.firmware_version='0.9.0-integrated-software';response.result.capabilities.motion_telemetry=true;}
    if(op==='get_device')response.result.schema=2;
    if(op==='get_profile')response.result.io4_bindings=decoded.profiles.find(pr=>pr.profile_id===p.profile_id).io4_bindings;
    client.revision=response.revision;client.last=response;return response.result;
  }};
  const device=new Device(client);await device.handshake();assert.equal(device.meta.schema,2);const table=structuredClone(device.profile.io4_bindings);
  device.draft.motion.knobs[0].gain=2;await device.apply();assert.deepEqual(device.profile.io4_bindings,table);
  verifyHello(device.hello);
});
