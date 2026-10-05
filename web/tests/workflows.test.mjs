import test from 'node:test';
import assert from 'node:assert/strict';
import { Device } from '../src/device.js';
import { SerialClient } from '../src/connection/client.js';
import { DemoHost, demoPort } from '../src/demo.js';
import { applyAndSave, confirmedCommand } from '../src/workflows.js';

async function fixture(){
  const host=new DemoHost(),port=demoPort(host),client=new SerialClient(port);await client.open();
  const device=new Device(client);await device.handshake();
  return {host,client,device,writes:()=>host.requests.filter(r=>!['hello','get_status','get_device','list_profiles','get_profile'].includes(r.op))};
}
test('组合动作实际ACK/读回后保存；无网页修改时仍能保存设备RAM',async()=>{
  const f=await fixture();try{
    f.device.draft.config.lighting.brightness_level=21;f.device.localDirty=true;
    await applyAndSave(f.device);assert.equal(f.device.profile.config.lighting.brightness_level,21);assert.equal(f.client.last.dirty,false);
    assert.deepEqual(f.writes().map(r=>r.op),['set_profile','save_config']);
    const ops=f.host.requests.map(r=>r.op),apply=ops.indexOf('set_profile'),save=ops.indexOf('save_config');assert(ops.slice(apply+1,save).includes('get_profile'));
    await applyAndSave(f.device);assert.equal(f.writes().at(-1).op,'save_config');assert.equal(f.device.meta.active_profile,4);
  }finally{await f.client.close();}
});
test('应用失败不写Flash；失败锁保持，不允许直接重试',async()=>{
  const f=await fixture();try{
    f.device.draft.config.lighting.brightness_level=101;f.device.localDirty=true;
    await assert.rejects(applyAndSave(f.device),/未开始保存/);assert.equal(f.writes().length,0);
    f.device.draft.config.lighting.brightness_level=21;
    const handle=f.host.handle.bind(f.host);f.host.handle=r=>{const response=handle(r);if(r.op==='set_profile'){response.ok=false;delete response.result;response.error={code:'APPLY_REJECTED',message:'injected'};}return response;};
    await assert.rejects(applyAndSave(f.device),/未开始保存/);assert(!f.device.ready);assert(!f.writes().some(r=>r.op==='save_config'));
    await assert.rejects(applyAndSave(f.device),/读回/);
  }finally{await f.client.close();}
});
test('保存失败或ACK元数据丢失不冒充成功，锁写并要求完整读回',async()=>{
  for(const missing of [false,true]){const f=await fixture();try{
    f.device.draft.config.lighting.brightness_level=22;f.device.localDirty=true;
    const handle=f.host.handle.bind(f.host);f.host.handle=r=>{const response=handle(r);if(r.op==='save_config'){if(missing)delete response.saved_revision;else{response.ok=false;delete response.result;response.error={code:'SAVE_FAILED',message:'injected'};}}return response;};
    await assert.rejects(applyAndSave(f.device),/保存未确认/);assert(!f.device.ready);assert.equal(f.writes().filter(r=>r.op==='save_config').length,1);
    await assert.rejects(applyAndSave(f.device));assert.equal(f.writes().filter(r=>r.op==='save_config').length,1);
  }finally{await f.client.close();}}
});
test('IO4草稿/revision冲突/无保存能力/无效输入阻止组合动作',async()=>{
  const f=await fixture();try{
    await assert.rejects(applyAndSave(f.device,{hasIo4Draft:()=>true}),/IO4/);
    await assert.rejects(applyAndSave(f.device,{validateDraft:()=>{throw Error('invalid form');}}),/invalid form/);
    f.client.revision++;await assert.rejects(applyAndSave(f.device),/版本/);assert.equal(f.writes().length,0);
  }finally{await f.client.close();}
});
test('普通草稿、全局启动模式、完整保存顺序独立，不自动激活编辑模板',async()=>{
  const calls=[],d={client:{},selected:7,localDirty:true,meta:{startup_usb_mode:'NORMAL_NKRO'},hello:{capabilities:{flash_save:true,usb_cold_switch:true}},checkWritable(){},async apply(){calls.push('apply+readback');this.localDirty=false;},async mutate(op,p){calls.push([op,p]);if(op==='set_startup_usb_mode')this.meta.startup_usb_mode=p.mode;}};
  await applyAndSave(d,{startupMode:'IO4'});assert.deepEqual(calls,['apply+readback',['set_startup_usb_mode',{mode:'IO4'}],['save_config',undefined]]);assert.equal(d.selected,7);
});
test('重启/UF2取消不发命令，确认后复核revision/会话/维护/草稿/校准',async()=>{
  const f=await fixture();try{
    f.device.hello.capabilities.bootloader_command=f.device.hello.capabilities.usb_cold_switch=true;
    for(const op of ['enter_bootloader','reboot_to_saved_mode']){
      f.client.last.dirty=false;assert.equal(await confirmedCommand(f.device,op,async()=>null),false);assert.equal(f.writes().length,0);
      await assert.rejects(confirmedCommand(f.device,op,async()=>{f.client.revision++;return {};}),/版本/);f.client.revision=f.device.revision;
      await assert.rejects(confirmedCommand(f.device,op,async()=>({}),{isCurrent:()=>false}),/设备/);
      await assert.rejects(confirmedCommand(f.device,op,async()=>({}),{hasDraft:()=>true}),/草稿/);
      await assert.rejects(confirmedCommand(f.device,op,async()=>({}),{calibrationBusy:()=>true}),/校准/);
      await assert.rejects(confirmedCommand(f.device,op,async()=>{f.device.status.maintenance='writing';return {};}),/正忙/);f.device.status.maintenance='idle';
    }
    assert.equal(f.writes().length,0);
  }finally{await f.client.close();}
});
test('刷新被设备拒绝或维护中不能保留旧可写状态；完整读回恢复',async()=>{
  const f=await fixture();try{
    const request=f.client.request.bind(f.client);
    for(const reason of ['rejected','maintenance']){
      f.client.request=async(op,p)=>{
        if(op==='get_status'){if(reason==='rejected')throw Error('read failed');return {config_ready:true,applying:false,maintenance:'writing'};}
        return request(op,p);
      };
      await assert.rejects(f.device.refresh());assert(!f.device.ready);
      await assert.rejects(applyAndSave(f.device),/读回/);assert.equal(f.writes().length,0);
      f.client.request=request;await f.device.refresh();assert(f.device.ready);
    }
  }finally{await f.client.close();}
});
test('重启/UF2收到设备拒绝也锁写，不按旧空闲状态重试',async()=>{
  const f=await fixture();try{
    f.device.hello.capabilities.bootloader_command=true;
    const request=f.client.request.bind(f.client);
    f.client.request=async(op,p)=>{if(op==='enter_bootloader')throw Error('MAINTENANCE_REJECTED');return request(op,p);};
    await assert.rejects(confirmedCommand(f.device,'enter_bootloader',async()=>({})),/MAINTENANCE/);assert(!f.device.ready);
    await assert.rejects(applyAndSave(f.device),/读回/);f.client.request=request;await f.device.refresh();assert(f.device.ready);
  }finally{await f.client.close();}
});
