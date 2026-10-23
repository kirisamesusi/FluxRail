/** IO4映射只编辑既有Profile.io4_bindings[12]，不翻译键盘usage、不产生HID。
 * 独立网页草稿必须先通过set_io4_bindings实际ACK和完整读回，才能save/冷切换。
 * Device负责revision/维护/错误后只读锁；所有持久化仍走唯一save_config。
 */
export const io4Functions=['禁用','左 A','左 B','左 C','右 A','右 B','右 C','左菜单','右菜单','左侧键','右侧键','Test','Service'];
// 网页只显示 BT1～BT10；P1 / P2（旋钮按压）的映射不在网页显示和编辑，但读回值原样保留并随保存带回。
const visible=[0,1,2,3,4,5,6,7,8,9];
const make=(tag,text='')=>{const e=document.createElement(tag);e.textContent=text;return e;};
export function mountIo4Editor(context){
  const panel=make('details');panel.id='io4-mapping-editor';panel.className='card labg';
  panel.append(make('summary','IO4 · 按键功能映射'));
  const body=make('div');body.className='io4-editor-body gb';panel.append(body);
  const hint=text=>{const p=make('p',text);p.className='hint';body.append(p);};
  const identity=make('p');identity.id='io4-mapping-profile';body.append(identity);
  hint('在普通模式下连接设备，为当前模板设置十路 IO4 按键功能。改好后应用并保存。活动模板由模板管理单独选择。切换到 IO4 后使用这组映射；普通键位单独保留。');
  const scroller=make('div');scroller.className='io4-key-scroller';body.append(scroller);const grid=make('div');grid.className='io4-key-grid';scroller.append(grid);
  const selectors=Array(12),readback=Array(12);
  for(let id=0;id<12;id++){
    const select=make('select');select.id='io4-binding-'+id;select.setAttribute('aria-label','IO4 '+(id<10?'BT'+(id+1):'P'+(id-9))+'功能');
    io4Functions.forEach((name,value)=>{const option=make('option',name);option.value=value;select.append(option);});
    selectors[id]=select;select.onchange=()=>{status();context.onControlsChange?.();};
    const confirmed=make('small');readback[id]=confirmed;
    if(!visible.includes(id))continue; // 不显示的映射仍参与读回、比较与保存，值不会被丢弃
    const card=make('label');card.className='io4-key-card';card.setAttribute('data-source',String(id));
    card.append(make('strong','BT'+(id+1)),select,confirmed);grid.append(card);
  }
  const message=make('p');message.id='io4-mapping-status';message.setAttribute('role','status');body.append(message);
  const actions=make('div');actions.className='toolbar';body.append(actions);
  const button=(id,text,task,success)=>{const b=make('button',text);b.id=id;b.type='button';b.onclick=()=>context.runAction(task,success);actions.append(b);return b;};
  // 每个按钮对应一个已存在公共操作；不把网页选择或FIFO入队当成已采用/已保存。
  const apply=button('io4-mapping-apply','应用并保存 IO4 设置',async()=>{
    const d=context.getDevice();d.checkWritable();
    if(context.hasModeDraft?.()||context.calibrationBusy?.())throw Error('先处理启动模式草稿或校准任务');
    if(!matches(d))throw Error('设备/模板已变化，请读回IO4映射后重新编辑');
    const values=selectors.map(s=>Number(s.value));
    if(!values.every(v=>Number.isInteger(v)&&v>=0&&v<=12))throw Error('IO4功能无效');
    if(hasDraft())await d.mutate('set_io4_bindings',{profile_id:d.selected,bindings:values});
    try{await d.mutate('save_config');}catch(error){throw Error('IO4 设置已应用，但保存未确认；请重新读取核对结果。'+error.message);}
    await context.refresh();
  },'IO4设置已应用并保存，当前模板和保存状态已读回；活动模板未自动切换。');
  const refresh=button('io4-mapping-refresh','重新读取',async()=>{await context.readSettings();},null);
  hint('多个按键可绑定同一功能，任一按住即保持该功能。按住 BT10 复位或上电可返回普通模式。');
  document.getElementById('io4-editor-mount').append(panel);
  let loadedDevice=null,loadedProfile=null,revision=null,selected=null,base=null;
  const hasDraft=()=>!!base&&selectors.some((s,i)=>Number(s.value)!==base[i]);
  const matches=d=>loadedDevice===d&&loadedProfile===d?.profile&&revision===d?.revision&&selected===d?.selected&&d?.revision===d?.client?.revision;
  function status(){
    if(!base){message.textContent='请连接整合固件并读回模板；IO4映射功能在普通USB模式下编辑。';return;}
    const changes=selectors.reduce((n,s,i)=>n+(Number(s.value)!==base[i]?1:0),0);
    message.textContent=changes?'IO4 网页草稿：'+changes+' 个修改尚未应用，请应用并保存 IO4 设置。':'IO4映射与设备RAM读回一致；掉电保留需要保存。';
  }
  function sync(d,busy=false){
    // 真实Device.refresh会生成新profile对象，明确读回才能舍弃旧草稿；
    // 仅轮询status不重置选择。不同设备即使revision/profile相同也重新加载。
    if(loadedDevice!==d||loadedProfile!==d?.profile){
      loadedDevice=d;loadedProfile=d?.profile;revision=d?.revision;selected=d?.selected;
      base=Array.isArray(d?.profile?.io4_bindings)&&d.profile.io4_bindings.length===12?[...d.profile.io4_bindings]:null;
      selectors.forEach((s,i)=>{s.value=base?.[i]??0;readback[i].textContent=base?'设备已读回：'+io4Functions[base[i]]:'设备尚未读回';});
    }
    const writable=!!(base&&d?.client?.alive&&d.ready&&matches(d)&&d.status.config_ready&&!d.status.applying&&d.status.maintenance==='idle'&&!d.localDirty&&!busy&&d.hello.capabilities.io4&&!context.hasModeDraft?.()&&!context.calibrationBusy?.());
    selectors.forEach(s=>s.disabled=!writable);apply.disabled=!writable||!d?.hello?.capabilities.flash_save;
    refresh.disabled=!d?.client?.alive||busy;
    identity.textContent=base?'编辑模板：'+(d.profile.name||'ID '+d.selected)+'（ID '+d.selected+'） · '+(d.selected===d.meta?.active_profile?'当前活动模板':'尚未激活，IO4仍使用活动模板 ID '+d.meta?.active_profile):'等待设备模板';
    status();
    if(base&&d?.revision!==d?.client?.revision)message.textContent='设备revision已变化，旧草稿禁止写入；请读回映射后重新编辑。';
    else if(d?.localDirty)message.textContent='普通设置还有网页草稿，请先应用或读回，再编辑IO4映射。';
    else if(base&&!d?.client?.alive)message.textContent='设备已断开，映射仅供查看；重连读回后才能应用。';
  }
  return {sync,hasDraft};
}
