import { assert, validName } from './config.js';
/** 新建模板的默认名称：英文+数字，旧固件（只收ASCII）也能接受；用户可以改成中文。 */
export function suggestName(device){
  const used=new Set((device.profiles??[]).map(p=>p.name));
  for(let i=1;i<100;i++){const name='Custom '+i;if(!used.has(name))return name;}
  return 'Custom';
}
const nonAscii=/[^\x00-\x7f]/;
function nameHint(error,name){
  if(error?.code==='INVALID_PROFILE'&&nonAscii.test(name))return Object.assign(new Error('设备拒绝了这个名称：较旧的固件只支持英文、数字和符号，请更新固件后再使用中文名，或改用英文名称',{cause:error}),{code:error.code});
  return error;
}
/** 确认前不发送写命令；弹窗期间会话、选择和revision仍须保持一致。 */
export async function templateAction(device,kind,confirm,{id=device.selected,name='',validateDraft=()=>{},isCurrent=()=>true}={}) {
  device.checkWritable();assert(device.hello.capabilities.profiles,'此固件不支持模板');
  const selected=device.selected,revision=device.revision,currentName=device.profile.name,preset=selected<=4;
  let prompt;
  const persistence='保存会写入设备的完整配置，包括其他已应用的设置。';
  switch(kind) {
    case 'select':
      if(!device.localDirty){await device.selectTemplate(id);return true;}
      prompt={title:'切换模板',description:'当前网页草稿尚未应用。确认后将舍弃草稿并读取所选模板；预设模板会恢复默认设置。',confirmLabel:'舍弃草稿并切换'};break;
    case 'save':
      validateDraft();
      prompt=preset?{title:'新建模板后保存',description:'预设模板不能覆盖。请输入自定义模板名称，将当前设置保存到新模板中。'+persistence,confirmLabel:'新建并保存',name:suggestName(device)}:{title:'保存到当前模板',description:'确认用当前设置更新模板「'+currentName+'」？'+persistence,confirmLabel:'确认保存'};break;
    case 'create':
      validateDraft();prompt={title:'新建模板',description:'复制当前设置（包括网页草稿）并保存到新模板。活动模板保持不变。'+persistence,confirmLabel:'新建并保存',name:suggestName(device)};break;
    case 'rename':
      assert(!preset,'预设模板名称不能修改');validName(name);
      prompt={title:'修改模板名称',description:'确认将「'+currentName+'」重命名为「'+name+'」？'+persistence,confirmLabel:'确认修改'};break;
    case 'delete':
      assert(!preset,'预设模板不能删除');prompt={title:'删除自定义模板',description:'确认删除模板「'+currentName+'」？删除后无法通过页面撤销。'+persistence,confirmLabel:'确认删除',danger:true};break;
    case 'restore':
      assert(preset,'只能恢复预设模板');prompt={title:'恢复预设模板',description:'确认恢复「'+currentName+'」的默认设置？当前模板的网页草稿和设备中的修改将被替换；其他模板保持不变。',confirmLabel:'恢复默认设置'};break;
    case 'activate':
      prompt={title:'切换活动模板',description:'确认让设备使用模板「'+currentName+'」？若需要掉电后继续使用，请再保存到设备。',confirmLabel:'确认切换'};break;
    default:throw new Error('未知模板操作');
  }
  const decision=await confirm(prompt);if(!decision)return false;
  assert(isCurrent()&&device.selected===selected&&device.revision===revision,'确认期间设备或模板已变化，请重新读回后操作');
  device.checkWritable();
  if(kind==='create'||kind==='save'&&preset) {
    validName(decision.name);validateDraft();
    try{await device.createFromDraft(decision.name);}catch(error){throw nameHint(error,decision.name);}
    await device.saveTemplate();
  } else if(kind==='save') {validateDraft();await device.saveTemplate();}
  else if(kind==='select')await device.selectTemplate(id,{discardDraft:true});
  else if(kind==='restore')await device.selectTemplate(selected,{discardDraft:true});
  else {
    const op={rename:'rename_profile',delete:'delete_profile',activate:'set_active_profile'}[kind];
    try{await device.mutate(op,{profile_id:selected,...(kind==='rename'?{name}: {})});}catch(error){throw nameHint(error,name);}
    if(kind==='rename'||kind==='delete')await device.mutate('save_config');
  }
  return true;
}
