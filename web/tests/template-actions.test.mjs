import test from 'node:test';
import assert from 'node:assert/strict';
import { SerialClient } from '../src/connection/client.js';
import { Device } from '../src/device.js';
import { DemoHost, demoPort } from '../src/demo.js';
import { templateAction } from '../src/template-actions.js';
const writes=port=>port.host.requests.filter(r=>!['hello','get_status','get_device','get_profile','list_profiles'].includes(r.op));
async function connected(host=new DemoHost()) {const port=demoPort(host),client=new SerialClient(port);await client.open();const device=new Device(client);await device.handshake();return {port,client,device};}
const accept=async()=>({name:'My SDVX'});
test('预设草稿另存新模板，原槽与活动槽保持不变，保存等待最终读回',async()=>{
  const {port,client,device}=await connected();try {
    await device.selectTemplate(1);const original=structuredClone(device.profile),active=device.meta.active_profile;
    device.draft.config.lighting.brightness_level=83;device.draft.motion.knobs[0].gain=2;device.localDirty=true;
    const before=writes(port).length;let prompt;
    await templateAction(device,'save',async p=>{prompt=p;assert.equal(writes(port).length,before);return accept();});
    assert.equal(prompt.title,'新建模板后保存');assert.equal(device.selected,5);assert.equal(device.profile.name,'My SDVX');
    assert.equal(device.profile.config.lighting.brightness_level,83);assert.equal(device.profile.motion.knobs[0].gain,2);
    assert.deepEqual(port.host.device.profiles.find(p=>p.profile_id===1),original);assert.equal(device.meta.active_profile,active);
    assert.equal(client.last.dirty,false);assert.equal(client.last.saved_revision,device.revision);
    assert.equal(writes(port).filter(r=>r.op==='set_profile').every(r=>r.payload.profile_id===5),true);
  }finally{await client.close();}
});
test('预设运行配置允许应用并保存设备；再次点击同一预设恢复内置默认值',async()=>{
  const {port,client,device}=await connected();try{
    await device.selectTemplate(1);const defaults=structuredClone(device.profile),cal=structuredClone(port.host.device.calibration);
    device.draft.config.lighting.brightness_level=19;device.localDirty=true;await device.apply();await device.mutate('save_config');
    assert.equal(device.profile.config.lighting.brightness_level,19);const saved=client.last.saved_revision;
    await templateAction(device,'select',()=>{throw new Error('无草稿不需要弹窗');},{id:1});
    assert.deepEqual(device.profile,defaults);assert.deepEqual(port.host.device.calibration,cal);
    assert.equal(client.last.saved_revision,saved);assert.equal(client.last.dirty,true);
    assert.equal(device.meta.active_profile,4);
  }finally{await client.close();}
});
test('取消模板操作不发送写命令、不舍弃草稿、不切换模板',async()=>{
  const {port,client,device}=await connected();try{
    device.draft.config.lighting.brightness_level=81;device.localDirty=true;
    const draft=structuredClone(device.draft),revision=device.revision,before=writes(port).length;
    for(const kind of ['save','create','select','restore'])assert.equal(await templateAction(device,kind,async()=>null,{id:1}),false);
    assert.deepEqual(device.draft,draft);assert.equal(device.selected,4);assert.equal(device.revision,revision);assert.equal(device.localDirty,true);assert.equal(writes(port).length,before);
    device.localDirty=false;await device.createFromDraft('Custom');const beforeCustom=writes(port).length;
    for(const kind of ['save','rename','delete','activate'])assert.equal(await templateAction(device,kind,async()=>null,{name:'Renamed'}),false);
    assert.equal(writes(port).length,beforeCustom);
  }finally{await client.close();}
});
test('保存当前自定义模板无需先应用；确认之后才发送set_profile与save',async()=>{
  const {port,client,device}=await connected();try{
    await templateAction(device,'create',accept);const selected=device.selected,originalPreset=structuredClone(port.host.device.profiles[3]);
    device.draft.config.lighting.brightness_level=24;device.localDirty=true;const before=writes(port).length;
    await templateAction(device,'save',async prompt=>{assert.equal(prompt.title,'保存到当前模板');assert.equal(prompt.name,undefined);assert.equal(writes(port).length,before);return {};});
    assert.equal(device.selected,selected);assert.equal(device.profile.config.lighting.brightness_level,24);assert.equal(client.last.dirty,false);
    assert.deepEqual(port.host.device.profiles[3],originalPreset);assert.deepEqual(writes(port).slice(before).map(r=>r.op),['set_profile','save_config']);
  }finally{await client.close();}
});
test('拒绝直接覆盖预设、预设重命名和删除；无设备写入',async()=>{
  const {port,client,device}=await connected();try{
    const before=writes(port).length;await assert.rejects(device.saveTemplate(),/预设模板不能覆盖/);
    for(const kind of ['rename','delete'])await assert.rejects(templateAction(device,kind,accept,{name:'Wrong'}),/预设/);
    assert.equal(writes(port).length,before);
  }finally{await client.close();}
});
test('无效名称、无效草稿与容量已满均在创建写入前拒绝',async()=>{
  const {port,client,device}=await connected();try{
    const before=writes(port).length;await assert.rejects(templateAction(device,'create',async()=>({name:'中文'})),/名称/);assert.equal(writes(port).length,before);
    device.draft.config.lighting.brightness_level=101;device.localDirty=true;await assert.rejects(device.createFromDraft('Good'));assert.equal(writes(port).length,before);await device.refresh();
    for(let i=0;i<8;i++)await device.createFromDraft('Custom '+i);
    const full=writes(port).length;await assert.rejects(device.createFromDraft('Overflow'),/已满/);assert.equal(writes(port).length,full);
  }finally{await client.close();}
});
test('确认期间revision或会话变化时拒绝写入，保留原草稿',async()=>{
  const {port,client,device}=await connected();try{
    device.draft.config.lighting.brightness_level=82;device.localDirty=true;const before=writes(port).length;
    await assert.rejects(templateAction(device,'save',async()=>{client.revision++;return accept();}),/版本已变化/);
    assert.equal(writes(port).length,before);assert.equal(device.localDirty,true);client.revision=device.revision;
    await assert.rejects(templateAction(device,'save',accept,{isCurrent:()=>false}),/确认期间/);assert.equal(writes(port).length,before);
  }finally{await client.close();}
});
test('确认切换舍弃网页草稿；恢复预设不自动激活或保存',async()=>{
  const {port,client,device}=await connected();try{
    device.draft.config.lighting.brightness_level=82;device.localDirty=true;const before=writes(port).length;
    await templateAction(device,'select',async p=>{assert.match(p.description,/舍弃/);return {};},{id:1});
    assert.equal(device.localDirty,false);assert.equal(device.selected,1);assert.equal(device.meta.active_profile,4);
    assert.deepEqual(writes(port).slice(before).map(r=>r.op),['restore_preset']);
  }finally{await client.close();}
});
test('创建后应用失败锁定设备，不盲重试或删除新模板',async()=>{
  const host=new DemoHost(),handle=host.handle.bind(host);host.handle=r=>{if(r.op!=='set_profile')return handle(r);const response=handle({...r,op:'unsupported'});host.requests[host.requests.length-1]=structuredClone(r);return {...response,ok:false,error:{code:'TEST_REJECT',message:'模拟应用失败'}};};
  const {port,client,device}=await connected(host);try{
    await assert.rejects(templateAction(device,'create',accept),/模拟应用失败/);assert.equal(device.ready,false);assert.equal(device.localDirty,true);assert.equal(device.selected,5);
    assert.equal(writes(port).filter(r=>r.op==='create_profile').length,1);assert.equal(writes(port).filter(r=>r.op==='set_profile').length,1);
    assert.equal(writes(port).some(r=>['save_config','delete_profile'].includes(r.op)),false);
  }finally{await client.close();}
});

test('schema2另存与保存保留十二路IO4表和全局启动身份',async()=>{
  const { readFile }=await import('node:fs/promises'),{ decodeSnapshot }=await import('../src/snapshot.js');
  const decoded=decodeSnapshot(new Uint8Array(await readFile(new URL('../../tests/integration/schema2_defaults.bin',import.meta.url))));
  const host=new DemoHost();host.device=decoded;const handle=host.handle.bind(host);
  host.handle=r=>{const response=handle(r);if(r.op==='hello'){response.result.scope='device_schema2';response.result.firmware_version='TEST-schema2';}if(r.op==='get_device')response.result.schema=2;return response;};
  const {port,client,device}=await connected(host);try{
    const table=structuredClone(device.profile.io4_bindings),startup=host.device.startup_usb_mode;
    device.draft.config.lighting.brightness_level=31;device.localDirty=true;await templateAction(device,'create',accept);
    assert.deepEqual(device.profile.io4_bindings,table);assert.equal(device.profile.config.lighting.brightness_level,31);
    device.draft.motion.knobs[0].gain=2;device.localDirty=true;await templateAction(device,'save',async()=>({}));
    assert.deepEqual(device.profile.io4_bindings,table);assert.equal(host.device.startup_usb_mode,startup);
    assert.equal(writes(port).some(r=>r.op==='set_io4_bindings'),false);assert.equal(client.last.dirty,false);
  }finally{await client.close();}
});
test('自定义模板重命名和删除均经确认并保存，取消不修改设备',async()=>{
  const {port,client,device}=await connected();try{
    await templateAction(device,'create',accept);const id=device.selected;
    await templateAction(device,'rename',async p=>{assert.equal(p.title,'修改模板名称');return {};},{name:'Renamed'});
    assert.equal(device.profile.name,'Renamed');assert.equal(client.last.dirty,false);
    const before=writes(port).length;await templateAction(device,'delete',async()=>null);assert.equal(writes(port).length,before);
    await templateAction(device,'delete',async p=>{assert.equal(p.title,'删除自定义模板');return {};});
    assert.equal(device.profiles.some(p=>p.profile_id===id),false);assert.equal(client.last.dirty,false);
  }finally{await client.close();}
});
