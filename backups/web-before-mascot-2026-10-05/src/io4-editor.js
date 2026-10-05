/** IO4映射只编辑既有Profile.io4_bindings[12]，不翻译键盘usage、不产生HID。
 * 独立网页草稿必须先通过set_io4_bindings实际ACK和完整读回，才能save/冷切换。
 * Device负责revision/维护/错误后只读锁；所有持久化仍走唯一save_config。
 */
export const io4Functions=['禁用','左 A','左 B','左 C','右 A','右 B','右 C','左菜单','右菜单','左侧键','右侧键','Test','Service'];
const order=[7,6,5,4,8,3,2,1,0,9,10,11];
const make=(tag,text='')=>{const e=document.createElement(tag);e.textContent=text;return e;};
export function mountIo4Editor(context){
  const panel=make('details');panel.id='io4-mapping-editor';panel.className='experiment-group';
  panel.append(make('summary','IO4 · 按键功能映射'));
  const body=make('div');body.className='io4-editor-body';panel.append(body);
  const hint=text=>{const p=make('p',text);p.className='hint';body.append(p);};
  const identity=make('p');identity.id='io4-mapping-profile';body.append(identity);
  hint('在普通模式下连接设备，为当前模板设置十二路 IO4 按键功能。修改后应用到 RAM，再设为活动模板并保存。切换到 IO4 后使用这组映射；普通键位单独保留。');
  const grid=make('div');grid.className='io4-key-grid';body.append(grid);
  const selectors=Array(12),readback=Array(12);
  for(const id of order){
    const card=make('label');card.className='io4-key-card';
    card.append(make('strong',id<10?'BT'+(id+1):'P'+(id-9)+' · 旋钮按压'));
    const select=make('select');select.id='io4-binding-'+id;select.setAttribute('aria-label','IO4 '+(id<10?'BT'+(id+1):'P'+(id-9))+'功能');
    io4Functions.forEach((name,value)=>{const option=make('option',name);option.value=value;select.append(option);});
    selectors[id]=select;select.onchange=()=>{status();context.onControlsChange?.();};
    const confirmed=make('small');readback[id]=confirmed;card.append(select,confirmed);grid.append(card);
  }
  const message=make('p');message.id='io4-mapping-status';message.setAttribute('role','status');body.append(message);
  const actions=make('div');actions.className='toolbar';body.append(actions);
  const button=(id,text,task,success)=>{const b=make('button',text);b.id=id;b.type='button';b.onclick=()=>context.runAction(task,success);actions.append(b);return b;};
  // 每个按钮对应一个已存在公共操作；不把网页选择或FIFO入队当成已采用/已保存。
  const apply=button('io4-mapping-apply','应用 IO4 映射到 RAM',async()=>{
    const d=context.getDevice();d.checkWritable();
    if(!matches(d))throw Error('设备/模板已变化，请读回IO4映射后重新编辑');
    const values=selectors.map(s=>Number(s.value));
    if(!values.every(v=>Number.isInteger(v)&&v>=0&&v<=12))throw Error('IO4功能无效');
    await d.mutate('set_io4_bindings',{profile_id:d.selected,bindings:values});await context.refresh();
  },'IO4映射已由设备确认并读回。请启用需要的模板并保存，再切换IO4。');
  const activate=button('io4-mapping-activate','将本模板设为活动',async()=>{
    const d=context.getDevice();if(hasDraft())throw Error('先应用IO4映射草稿');
    await d.mutate('set_active_profile',{profile_id:d.selected});await context.refresh();
  },'该模板已成为活动模板，IO4会使用它的功能表。请保存到设备。');
  const save=button('io4-mapping-save','保存到设备',async()=>{
    if(hasDraft())throw Error('先应用IO4映射草稿');
    await context.getDevice().mutate('save_config');await context.refresh();
  },'设备已确认完整配置保存。现在可切换到IO4身份。');
  const refresh=button('io4-mapping-refresh','读回映射（舍弃草稿）',async()=>{await context.refresh();},'IO4映射已从设备读回，未应用选择已舍弃。');
  const link=make('a','下一步：USB 身份切换 →');link.href='#update';actions.append(link);
  hint('多个按键可绑定同一功能，任一按住即保持该功能。P1 / P2 为旋钮按压；按住 BT10 复位或上电可返回普通模式。');
  document.getElementById('experimental-content').insertBefore(panel,document.getElementById('experiment-calibration'));
  let loadedDevice=null,loadedProfile=null,revision=null,selected=null,base=null;
  const hasDraft=()=>!!base&&selectors.some((s,i)=>Number(s.value)!==base[i]);
  const matches=d=>loadedDevice===d&&loadedProfile===d?.profile&&revision===d?.revision&&selected===d?.selected&&d?.revision===d?.client?.revision;
  function status(){
    if(!base){message.textContent='请连接整合固件并读回模板；IO4映射功能在普通USB模式下编辑。';return;}
    const changes=selectors.reduce((n,s,i)=>n+(Number(s.value)!==base[i]?1:0),0);
    message.textContent=changes?'IO4 网页草稿：'+changes+' 路修改尚未应用，切换模式前先应用。':'IO4映射与设备RAM读回一致；掉电保留需要保存。';
  }
  function sync(d,busy=false){
    // 真实Device.refresh会生成新profile对象，明确读回才能舍弃旧草稿；
    // 仅轮询status不重置选择。不同设备即使revision/profile相同也重新加载。
    if(loadedDevice!==d||loadedProfile!==d?.profile){
      loadedDevice=d;loadedProfile=d?.profile;revision=d?.revision;selected=d?.selected;
      base=Array.isArray(d?.profile?.io4_bindings)&&d.profile.io4_bindings.length===12?[...d.profile.io4_bindings]:null;
      selectors.forEach((s,i)=>{s.value=base?.[i]??0;readback[i].textContent=base?'设备已读回：'+io4Functions[base[i]]:'设备尚未读回';});
    }
    const writable=!!(base&&d?.client?.alive&&d.ready&&matches(d)&&d.status.config_ready&&!d.status.applying&&d.status.maintenance==='idle'&&!d.localDirty&&!busy&&d.hello.capabilities.io4);
    selectors.forEach(s=>s.disabled=!writable);apply.disabled=!writable||!hasDraft();
    activate.disabled=!writable||hasDraft()||d?.selected===d?.meta?.active_profile;
    save.disabled=!writable||hasDraft()||!d?.hello?.capabilities.flash_save;
    refresh.disabled=!d?.client?.alive||busy;
    identity.textContent=base?'编辑模板：'+(d.profile.name||'ID '+d.selected)+'（ID '+d.selected+'） · '+(d.selected===d.meta?.active_profile?'当前活动模板':'尚未激活，IO4仍使用活动模板 ID '+d.meta?.active_profile):'等待设备模板';
    status();
    if(base&&d?.revision!==d?.client?.revision)message.textContent='设备revision已变化，旧草稿禁止写入；请读回映射后重新编辑。';
    else if(d?.localDirty)message.textContent='普通设置还有网页草稿，请先应用或读回，再编辑IO4映射。';
    else if(base&&!d?.client?.alive)message.textContent='设备已断开，映射仅供查看；重连读回后才能应用。';
  }
  return {sync,hasDraft};
}
