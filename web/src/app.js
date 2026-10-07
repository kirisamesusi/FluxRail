import { applyAndSave } from './workflows.js';
import { mountGainControl } from './gain-control.js';
import { mountConfirmation } from './confirmation.js';
import { templateAction } from './template-actions.js';
import { mountModuleNavigation } from './layout.js';
import { SerialClient } from './connection/client.js';
import { Device } from './device.js';
import { keys, fromEvent, fromCharacter, actionLabel } from './keyboard.js';
import { hallFields, knobFields, pushFields, assert } from './config.js';
import { archive, exportSnapshot, importSnapshot, parseArchive } from './snapshot.js';
import { FirmwareUpdate } from './update/controller.js';
import { mountKnobPreview } from './knob-preview.js';
import { mountIntegration, pollIntegration, lockIntegration, hasIo4Draft, hasModeDraft, getModeDraft, resetModeDraft, hasCalibrationJob } from './integration.js';
/** 单页DOM编排层。网页草稿、设备RAM和已保存Flash是三个不同状态：
 * change只改draft；apply等待公共两核ACK再读回；save独立维护并等待最终ACK。
 * 所有设备字段用textContent展示，不将设备名称/文件内容注入HTML。
 * 运动/校准/升级适配复用统一CDC v1，由integration.js按能力接线。
 */
const $=id=>document.getElementById(id);
const confirmChange=mountConfirmation($('confirmation-dialog'));
let client=null,device=null,busy=false,listening=null,importCandidate=null,recording=false,records=[],dropped=0,pollTimer=null;
let advancedSource=7;
const firmwareUpdate=new FirmwareUpdate({result:$('uf2-result'),version:$('version-result'),current:$('current-firmware'),getConnection:()=>client?.alive&&device?.ready?{hello:device.hello,simulated:!!client.port.simulated}:null});
const knobPreviewState=[{turns:0,delta:0},{turns:0,delta:0}];
let knobPreviewUpdates=[];
// 最上方 P1 / P2 分居左右；中排左 BT8～BT5、右 BT4～BT1，下排 BT9 / BT10。
const order=[10,11,7,6,5,4,3,2,1,0,8,9];
function element(tag,text='',className=''){const e=document.createElement(tag);e.textContent=text;if(className)e.className=className;return e;}
function notice(text,error=false){$('notice').textContent=text;$('notice').classList.toggle('error',error);}
// 同一时刻只有一张卡监听；录入、Esc、失焦、重建或写锁恢复时统一熄灭按钮。
function cancelListening(showNotice=true){
  if(listening===null)return;
  listening.button.classList.remove('listening');listening.button.setAttribute('aria-pressed','false');
  listening=null;if(showNotice)notice('已退出键盘监听');
}
function mark(){device.localDirty=true;$('draft-state').textContent='网页草稿未应用';cancelListening();syncBindingLabels();syncOverview();for(const update of knobPreviewUpdates)update();controls();}
function writable(){return !!(device?.ready&&client?.alive&&!busy&&device.revision===client.revision&&device.status.config_ready&&!device.status.applying&&device.status.maintenance==='idle'&&!hasCalibrationJob());}
function controls(){
  lockIntegration(device,busy);
  firmwareUpdate.render();
  $('uf2-file').disabled=busy;
  // 维护/写请求期间禁用编辑；出错或冲突后仅允许读回，避免用户继续覆盖未知状态。
  const connected=!!client?.alive,edit=writable()&&!hasIo4Draft(),caps=device?.hello?.capabilities||{},profiles=edit&&caps.profiles,templates=profiles&&!hasModeDraft();
  if(!edit)cancelListening(false);
  $('connect').disabled=busy||connected;$('demo').disabled=busy||connected;$('disconnect').disabled=busy||!connected;
  $('refresh').disabled=busy||!connected;$('apply').disabled=!edit||(!caps.flash_save&&!device?.localDirty);$('apply').textContent=caps.flash_save||!device?.hello?'应用并保存':'应用设置（不支持保存）';
  // 折叠只改变展示；高级字段仍受同一会话、能力、revision与维护锁保护。
  for(const id of ['key-editor','key-advanced-editor','light-editor','light-advanced-editor'])$(id).disabled=!edit;
  for(const id of ['overview-editor','hall-editor','hall-advanced-editor','knob-advanced-editor','routing-editor'])$(id).disabled=!profiles;
  // 旋钮本地模拟位于设置fieldset外：未连接/维护时也可演示，不绕过设备设置写锁。
  for(const settings of document.querySelectorAll('.knob-settings'))settings.disabled=!profiles;
  $('profile-select').disabled=!templates;$('profile-name').disabled=!templates||device.selected<=4;
  $('edit-template').disabled=!templates||!caps.flash_save;
  for(const button of document.querySelectorAll('[data-profile]'))button.disabled=!templates;
  for(const id of ['activate','rename','delete'])$(id).disabled=!templates||!!device?.localDirty;
  $('create').disabled=!templates||!caps.flash_save||device.profiles.length>=caps.max_profiles;
  $('restore').disabled=!templates||device.selected>4;
  if(templates){$('rename').disabled=device.localDirty||device.selected<=4;$('delete').disabled=device.localDirty||device.selected<=4;$('activate').disabled=device.localDirty||device.selected===device.meta.active_profile;}
  $('export').disabled=!edit||!caps.snapshot_transfer||!!device?.localDirty||hasModeDraft();
  $('import-file').disabled=!edit||!caps.snapshot_transfer||!!device?.localDirty||hasModeDraft();
  $('import-commit').disabled=!edit||!importCandidate||!!device?.localDirty||hasModeDraft()||importCandidate.revision!==device?.revision;
  $('record').disabled=busy||!connected;$('csv').disabled=records.length===0;
}
function field(container,label,object,key,type='number',options={}){
  // 直接引用当前完整草稿中的字段，保留未编辑的高级参数。只在change记录最后值，
  // 不向设备发滑块input洪泛，也不通过localStorage伪造设备掉电保存。
  const row=element('label',label,type==='checkbox'?'check-field':'');let input;
  if(type==='select'){
    input=element('select');for(const[value,text]of options.values){const o=element('option',text);o.value=value;input.append(o);}input.value=object[key];
  }else{
    input=element('input');input.type=type;
    if(type==='checkbox')input.checked=object[key];
    else if(type==='color')input.value='#'+object[key].map(v=>v.toString(16).padStart(2,'0')).join('');
    else {input.value=typeof object[key]==='number'&&options.step<1?Number(object[key].toFixed(6)):object[key];if(options.min!==undefined)input.min=options.min;if(options.max!==undefined)input.max=options.max;input.step=options.step??1;}
  }
  input.addEventListener('change',()=>{
    if(!input.checkValidity()){input.reportValidity();return;}
    object[key]=type==='checkbox'?input.checked:type==='color'?[1,3,5].map(i=>parseInt(input.value.slice(i,i+2),16)):type==='select'&&options.numeric?Number(input.value):type==='select'?input.value:input.valueAsNumber;
    mark();if(key==='brightness_level')$('brightness-text').textContent=`当前 ${object[key]}`;
  });row.append(input);container.append(row);return input;
}
const hallHelp={range_min:'输出范围的最小值，取值 0～1，须小于上界。',range_max:'输出范围的最大值，取值 0～1，须大于下界。',quiet_tau_us:'静止或慢速移动时的平滑程度。值越大，抖动越少，跟随也越慢；设为 0 可关闭此滤波。',moving_tau_us:'快速移动时的平滑程度。值越小，跟随越快；不能大于静止滤波。',fast_speed_mm_s:'达到这个速度时，使用移动滤波。低于此速度时逐渐过渡，不限制滑块速度。',activity_mm:'移动多远才算正在操作滑轨，用于来源切换，不是位置死区。实际门槛还会参考校准噪声。',activity_hold_us:'确认滑轨正在移动后，保持活动状态的时间（微秒）。',enter_signal:'三个传感器经校准后的信号总和达到此值，才开始接受位置，单位 ADC count。',exit_signal:'三路校准信号总和低于此值便停止接受位置。不能大于进入信号；单位 ADC count。',curve_tolerance:'允许读数偏离校准曲线的程度（ADC count）。放宽可能接受更多异常读数。',max_gap_us:'有效采样间隔超过此值（微秒），或采样中断时，以新位置重新建立参考，不沿用旧滤波状态。'};
function numbers(container,object,descriptors){for(const[key,label,min,max,type]of descriptors){const input=field(container,label,object,key,'number',{min,max,step:type.startsWith('u')?1:'any'});if(container.id==='hall-advanced-fields'){const row=input.parentElement;row.classList.add('parameter-field');const help=element('small',hallHelp[key],'parameter-help');help.id='hall-help-'+key;input.setAttribute('aria-describedby',help.id);row.append(help);}}}
// 两个页面复用相同物理分组；source_id 和设备配置数组保持不变。
function physicalKeyContainers(container){
  container.replaceChildren();
  const keys=element('div','','physical-keys-main');
  container.append(keys);
  return ()=>keys;
}
function renderKeys(){
  cancelListening(false); // 重建卡片前结束旧按钮监听，避免残留焦点和亮起状态。
  // DOM显示按左右物理顺序排列，存储永远按source_id=BT1..10/P1/P2；逐键RGB另按BT保存。
  const grid=$('key-grid'),destination=physicalKeyContainers(grid);
  for(const id of order){
    const binding=device.draft.config.keyboard.bindings[id];const card=element('div','','key-card');card.id='key-'+id;card.setAttribute('data-source',String(id));
    card.append(element('strong',id<10?'BT'+(id+1):'P'+(id-9)),element('div',actionLabel(binding.action),'binding-label'));
    const physical=element('small',id>=10&&!device.hello.capabilities.push_hardware?'硬件未接入':'状态未读取');physical.className='physical-state';card.append(physical);
    const select=element('select');select.setAttribute('aria-label',(id<10?'BT'+(id+1):'P'+(id-9))+'动作');
    for(const[value,label]of [['disabled','禁用'],['mouse:1','鼠标左键'],['mouse:2','鼠标右键'],['mouse:4','鼠标中键'],...keys.map(k=>['key:'+k.usage,k.label])]){const o=element('option',label);o.value=value;select.append(o);}
    let a=binding.action;const val=a.type==='disabled'?'disabled':a.type==='mouse_button'?'mouse:'+a.button:'key:'+a.usage;
    if(a.type==='keyboard'&&!keys.some(k=>k.usage===a.usage)){const o=element('option','HID '+a.usage);o.value=val;select.append(o);}select.value=val;
    function setAction(action){setBinding(id,action);}
    select.addEventListener('change',()=>{const[type,v]=select.value.split(':');setAction(type==='disabled'?{type:'disabled'}:type==='mouse'?{type:'mouse_button',button:Number(v)}:{type:'keyboard',usage:Number(v),modifiers:0});});card.append(select);
    const listen=element('button','监听键盘');listen.type='button';listen.setAttribute('aria-pressed','false');
    listen.addEventListener('click',()=>{
      cancelListening(false);listening={id,setAction,button:listen};
      listen.classList.add('listening');listen.setAttribute('aria-pressed','true');
      notice('正在录入按键：按下目标键；Esc 取消，切换焦点退出。长按重复输入将被忽略。');
    });listen.addEventListener('blur',()=>cancelListening());card.append(listen);
    destination(id).append(card);
  }
  renderAdvancedBinding();
}
function setBinding(id,action){
  // 菜单、监听和实验室字符录入共享唯一binding；不创建第二套键位配置。
  device.draft.config.keyboard.bindings[id].action=action;mark();renderKeys();renderPhysical();
}
function syncBindingLabels(){
  // HID数值编辑不重建整个表单（保留焦点），但立即同步基础卡片的标签和菜单。
  if(!device?.draft)return;
  for(let id=0;id<12;id++){
    const card=$('key-'+id);if(!card)continue;const a=device.draft.config.keyboard.bindings[id].action;
    card.querySelector('.binding-label').textContent=actionLabel(a);
    const select=card.querySelector('select'),value=a.type==='disabled'?'disabled':a.type==='mouse_button'?'mouse:'+a.button:'key:'+a.usage;
    if(![...select.options].some(o=>o.value===value)){const option=element('option','HID '+a.usage);option.value=value;select.append(option);}select.value=value;
  }
}
function renderAdvancedBinding(){
  const select=$('advanced-binding-source');select.replaceChildren();
  for(const id of order){const option=element('option',id<10?'BT'+(id+1):'P'+(id-9));option.value=id;select.append(option);}select.value=advancedSource;
  const f=$('advanced-binding-fields');f.replaceChildren();const binding=device.draft.config.keyboard.bindings[advancedSource];
  const row=element('label','手填 US 单字符'),text=element('input');text.type='text';text.maxLength=2;text.setAttribute('aria-label','US单字符');row.append(text);f.append(row);
  const set=element('button','录入字符');set.type='button';set.onclick=()=>{try{setBinding(advancedSource,fromCharacter(text.value));notice('字符已录入网页草稿，应用后生效');}catch(error){notice(error.message,true);}};f.append(set);
  if(binding.action.type==='keyboard'){field(f,'HID usage',binding.action,'usage','number',{min:4,max:231});field(f,'修饰掩码 0～255',binding.action,'modifiers','number',{min:0,max:255});}
  else f.append(element('p','当前为鼠标动作或禁用；录入字符后可编辑 HID 字段。','hint'));
}
$('advanced-binding-source').onchange=()=>{cancelListening();advancedSource=Number($('advanced-binding-source').value);renderAdvancedBinding();};
document.addEventListener('keydown',event=>{
  if(!listening)return;event.preventDefault();event.stopPropagation();if(event.key==='Escape'){cancelListening();return;}
  try{const action=fromEvent(event);if(action){const target=listening;cancelListening(false);target.setAction(action);notice('键位已录入网页草稿，应用后生效');}}catch(e){notice(e.message,true);}
});window.addEventListener('blur',cancelListening);
function renderHall(hall=null){
  const container=$('hall-fields');container.replaceChildren();
  if(hall){
    field(container,'启用滑轨输入',hall,'enabled','checkbox');
    field(container,'反转输出方向',hall,'invert','checkbox');
    mountGainControl(container,{object:hall,key:'center_gain',id:'hall-gain',label:'中心灵敏度',onChange:mark});
    return;
  }
  // 未连接或旧固件无motion时保留设置位置，空值表示尚未读回设备参数。
  for(const [label,type]of [['启用滑轨输入','checkbox'],['反转输出方向','checkbox'],['中心灵敏度','number']]){
    const row=element('label',label,type==='checkbox'?'check-field':'');
    const input=element('input');input.type=type;input.disabled=true;
    if(type==='number')input.placeholder='待读取';
    row.append(input);container.append(row);
  }
}
function renderEditors(){
  renderKeys();const c=device.draft.config;
  knobPreviewUpdates=[];
  const scan=$('scan-editor');scan.replaceChildren();field(scan,'重触发间隔 µs',c.keyboard,'retrigger_gap_us','number',{min:0,max:100000});
  for(const key of ['press_debounce_us','release_debounce_us'])field(scan,key==='press_debounce_us'?'按下消抖 µs':'释放消抖 µs',c.scan,key,'number',{min:0,max:20000});
  const l=$('light-fields');l.replaceChildren();field(l,'键灯启用',c.lighting,'enabled','checkbox');
  field(l,'灯效',c.lighting,'mode','select',{values:[['static','常亮'],['breathing','呼吸'],['color_cycle','同步换色'],['rainbow_wave','彩虹流动'],['press','按下即亮']]});
  field(l,'亮度0～100',c.lighting,'brightness_level','range',{min:0,max:100});const bt=element('span',`当前 ${c.lighting.brightness_level}`);bt.id='brightness-text';l.append(bt);
  field(l,'全局颜色',c.lighting,'global_color','color');
  const la=$('light-advanced-fields');la.replaceChildren();field(la,'使用逐键色',c.lighting,'per_key','checkbox');field(la,'周期 ms',c.lighting,'period_ms','number',{min:100,max:60000});field(la,'动画反向',c.lighting,'reverse','checkbox');
  const colors=$('key-colors');colors.replaceChildren();for(let id=0;id<10;id++)field(colors,'BT'+(id+1),c.lighting.key_colors,id,'color');
  const h=$('hall-fields'),ha=$('hall-advanced-fields'),k=$('knob-fields'),ka=$('knob-advanced-fields'),r=$('routing-fields');
  for(const container of [h,ha,k,ka,r])container.replaceChildren();
  // 同一motion对象按基础/进阶分配展示，descriptor只分组一次，完整set_profile保持不变。
  if(device.draft.motion){
    const m=device.draft.motion;renderHall(m.hall);numbers(ha,m.hall,hallFields.filter(([key])=>key!=='center_gain'));
    for(let id=0;id<2;id++){
      const card=element('div','','knob-card');card.append(element('h3',id===0?'旋钮 1 · U4 / FPC1 / P1':'旋钮 2 · U5 / FPC2 / P2'));
      knobPreviewUpdates.push(mountKnobPreview(card,{id,state:knobPreviewState[id],getConfig:()=>device?.draft?.motion?.knobs[id]}));
      const f=element('fieldset','','form-grid knob-settings');field(f,'启用旋钮输入',m.knobs[id],'enabled','checkbox');field(f,'输出轴',m.knobs[id],'axis','select',{numeric:true,values:[[0,'X'],[1,'Y']]});field(f,'反转输出方向',m.knobs[id],'invert','checkbox');mountGainControl(f,{object:m.knobs[id],key:'gain',id:'knob-gain-'+(id+1),label:'灵敏度',onChange:mark});card.append(f);k.append(card);
      const advanced=element('div','','knob-card'),af=element('div','','form-grid');advanced.append(element('h3','旋钮 '+(id+1)+' · 进阶参数'));numbers(af,m.knobs[id],knobFields.filter(([key])=>key!=='gain'));
      field(af,'启用 PUSH 防误触',m.knobs[id].push,'enabled','checkbox');numbers(af,m.knobs[id].push,pushFields);advanced.append(af);ka.append(advanced);
    }
    field(r,'接管静止滞回 µs',m.routing,'idle_us','number',{min:0,max:100000});field(r,'同时起动滑轨优先（工程起点）',m.routing,'simultaneous_hall_first','checkbox');
  }
  if(!device.draft.motion){renderHall();renderStandaloneKnobs();}
  renderOverview();
}
function renderStandaloneKnobs(){
  // 初始/旧runtime无motion时仍提供清楚标记的本地演示，不伪造已读取的设备配置。
  const grid=$('knob-fields');grid.replaceChildren();knobPreviewUpdates=[];
  for(let id=0;id<2;id++){const card=element('div','','knob-card');card.append(element('h3',id===0?'旋钮 1 · U4 / FPC1 / P1':'旋钮 2 · U5 / FPC2 / P2'));knobPreviewUpdates.push(mountKnobPreview(card,{id,state:knobPreviewState[id],getConfig:()=>null}));grid.append(card);}
}
function renderOverview(){
  for(const id of ['hall-overview','knob-overview'])$(id).replaceChildren();
  const motion=device?.draft?.motion;
  if(!motion){for(const id of ['hall-overview','knob-overview'])$(id).append(element('small',client?.alive?'此固件未提供启用设置':'连接后读取启用状态','device-awaiting'));}
  else{
    field($('hall-overview'),'启用滑轨输入',motion.hall,'enabled','checkbox').dataset.overview='hall';
    motion.knobs.forEach((knob,id)=>field($('knob-overview'),'启用旋钮 '+(id+1),knob,'enabled','checkbox').dataset.overview='knob-'+id);
  }
  syncOverview();
}
function syncOverview(){
  if(!device?.draft)return;
  const online=!!client?.alive,prefix=online&&device.ready&&device.revision===client.revision?'':'上次读回 · ';
  const active=device.profiles.find(p=>p.profile_id===device.meta?.active_profile);
  $('active-profile-state').textContent=prefix+(active?active.name+'（ID '+active.profile_id+'）':'此固件仅提供运行配置');
  $('editing-profile-state').textContent=prefix+'页面编辑：'+device.draft.name+(device.localDirty?' · 网页草稿':' · 已读回');
  $('key-overview').textContent=prefix+device.draft.config.keyboard.bindings.filter(b=>b.action.type!=='disabled').length+' / 12 个已绑定动作';
  const m=device.draft.motion;if(!m)return;
  for(const input of document.querySelectorAll('[data-overview]'))input.checked=input.dataset.overview==='hall'?m.hall.enabled:m.knobs[Number(input.dataset.overview.slice(-1))].enabled;
  const h=$('hall-fields').querySelector('input[type=checkbox]');if(h)h.checked=m.hall.enabled;
  document.querySelectorAll('.knob-settings').forEach((f,id)=>{const input=f.querySelector('input[type=checkbox]');if(input)input.checked=m.knobs[id].enabled;});
}
function renderInputTest(){
  const destination=physicalKeyContainers($('input-test'));for(const id of order){const tile=element('div',id<10?'BT'+(id+1):'P'+(id-9),'test-key');tile.id='test-key-'+id;tile.setAttribute('data-source',String(id));tile.append(element('small','等待连接'));destination(id).append(tile);}
}
function renderPhysical(){
  // get_status只含状态位图，不能据2Hz快照推断全部down/up事件或实时吞键。
  if(!device?.status)return;const mask=device.status.physical_mask;
  for(let id=0;id<12;id++){
    const unavailable=id>=10&&!device.hello.capabilities.push_hardware,down=!unavailable&&!!(mask&(1<<id)),text=unavailable?'硬件未接入':down?'设备：按下':'设备：松开';
    const card=$('key-'+id);if(card){card.classList.toggle('down',down);card.querySelector('.physical-state').textContent=text;}
    const tile=$('test-key-'+id);tile.classList.toggle('down',down);tile.classList.toggle('unavailable',unavailable);tile.querySelector('small').textContent=text;
  }
}
function renderStatus(){
  // 断开后保留上次配置供辨认，但清除实时按下态，避免旧快照冒充当前设备状态。
  if(!client?.alive){
    $('diagnostic-summary').textContent='已断开，等待重新连接读回物理状态。';renderInputTest();
    for(const card of document.querySelectorAll('.key-card')){card.classList.remove('down');card.querySelector('.physical-state').textContent='已断开 · 上次配置';}
    $('save-state').textContent='已断开，保存结果以重连读回为准';$('identity-state').textContent=device?.hello?'已断开 · 上次读回':'连接后查看';syncOverview();controls();return;
  }
  if(!device?.ready){$('identity-state').textContent='读回未完成 · 上次读回';$('save-state').textContent='保存结果未确认，请重新读取设置';$('draft-state').textContent='当前结果待读回';syncOverview();controls();return;}
  const e=device.latestEnvelope||device.envelope,s=device.status;
  $('save-state').textContent=(e.dirty?'设备设置尚未保存':'设备快照已保存')+(s.maintenance==='idle'?'':' · 维护 '+s.maintenance);
  $('status-data').textContent=JSON.stringify({revision:client.revision,applied_revision:e.applied_revision,saved_revision:e.saved_revision,dirty:e.dirty,...s},null,2);
  $('identity-state').textContent=device.revision===client.revision?'当前会话读回':'修订已变化 · 上次读回';syncOverview();
  $('draft-state').textContent=device.revision!==client.revision?'设备版本已变化：必须读回':device.localDirty||hasModeDraft()?'网页草稿未应用':'网页与已读模板一致';renderPhysical();controls();
}
function render(){
  // 仅完整读回成功后重建编辑器。不会在周期轮询时覆盖用户草稿或抢走输入焦点。
  const hello=device.hello,dl=$('identity');dl.replaceChildren();
  for(const[label,value]of [['设备型号',hello.product+' / '+hello.board],['固件 / 协议',hello.firmware_version+' / CDC v1'],['唯一序号',hello.serial],['当前 USB 模式',hello.usb_mode],['启动模式（设备 RAM）',device.meta?.startup_usb_mode||'此固件未提供'],['活动模板',device.meta?.active_profile??'模块1运行配置'],['设置修订号',device.revision]])dl.append(element('dt',label),element('dd',String(value)));
  $('demo-label').textContent=client.port.simulated?'模拟演示 · 不连接硬件':'';
  $('connection-state').textContent=client.port.simulated?'模拟会话':'已连接并完成读回';
  $('diagnostic-summary').textContent='已连接 · 物理状态约 2Hz 读回，无法判断毫秒级漏采。'+(hello.capabilities.push_hardware?'PUSH以设备读回为准。':'P1 / P2硬件尚未接入。');
  const select=$('profile-select');select.replaceChildren();for(const p of device.profiles){const option=element('option',`${p.profile_id} · ${p.name} ${p.preset?'（预置）':''}`);option.value=p.profile_id;select.append(option);}select.value=device.selected;$('profile-name').value=device.profile.name;
  for(const button of document.querySelectorAll('[data-profile]'))button.setAttribute('aria-pressed',String(Number(button.dataset.profile)===device.selected));
  $('update-cap').textContent=hello.capabilities.bootloader_command?'设备支持进入 UF2 下载模式。请手动复制文件，完成后重新连接并核对版本。':'此固件只支持板上BOOT手动进入UF2。';
  $('calibration-state').textContent=device.meta?`全局校准：${device.meta.calibration_valid?'有效共享校准记录':'无有效校准'} · generation ${device.meta.calibration_generation}`:'全局校准存储未提供';
  importCandidate=null;$('import-file').value='';$('import-preview').textContent='导入schema1/2完整设备快照。全部模板、活动槽、启动身份和全局机械校准都会纳入候选。';
  device.latestEnvelope=null;renderEditors();renderStatus();
}
async function action(task,success,failureText=null,returnFocus=null){
  if(busy)return;const origin=returnFocus??document.activeElement;cancelListening();busy=true;controls();
  try{await task();if(success)notice(success);}
  catch(error){notice(failureText??`${error.code?error.code+'：':''}${error.message}。配置/保存出错需读回核对结果。`,true);}
  finally{busy=false;renderStatus();controls();if((returnFocus||document.activeElement===document.body)&&origin?.isConnected&&!origin.disabled)origin.focus({preventScroll:true});}
}
function schedulePoll(){
  // 自调度：前次请求完成后再等500ms，忙时跳过，不积累轮询队列。
  // 状态记录最多600条，先丢最旧，并单独记录丢弃数；不阻塞实时USB输入。
  clearTimeout(pollTimer);pollTimer=setTimeout(async()=>{
    if(!client?.alive)return;
    if(!busy){const current=device;try{await current.poll();await pollIntegration(current);if(current!==device)return;renderStatus();if(recording){if(records.length===600){records.shift();dropped++;}records.push({host_time:new Date().toISOString(),revision:client.revision,...device.status});$('record-state').textContent=`${records.length}/600条 · 已移除最早记录 ${dropped} 条；主机轮询快照`;controls();}}catch(error){notice('诊断读回失败：'+error.message,true);}}
    schedulePoll();
  },device?.hello?.capabilities.motion_telemetry?100:500);
}
async function connect(simulated=false){
  await action(async()=>{
    assert(!client?.alive,'已有连接');let port;
    if(simulated){const { createDemoPort }=await import('./demo.js');port=await createDemoPort();}
    else {assert(window.isSecureContext&&navigator.serial,'请在桌面Chrome/Edge的localhost或HTTPS打开；当前浏览器无Web Serial');port=await navigator.serial.requestPort();}
    client=new SerialClient(port,error=>{clearTimeout(pollTimer);cancelListening(false);$('connection-state').textContent='已断开 / 结果待读回';notice('连接失败',true);renderStatus();});
    device=new Device(client);resetModeDraft(device);
    $('identity').replaceChildren(element('dt','设备'),element('dd','正在握手'));$('identity-state').textContent='等待当前设备读回';$('active-profile-state').textContent='连接后查看';$('editing-profile-state').textContent='页面编辑：连接后查看';$('key-overview').textContent='启用情况：连接后查看';
    $('status-data').textContent='等待当前设备 get_status';records=[];recording=false;dropped=0;$('record').textContent='开始状态记录';$('record-state').textContent='最多保留 600 条当前会话记录；超出后移除最早记录。';
    importCandidate=null;$('import-file').value='';$('import-preview').textContent='等待当前设备配置读回';
    $('key-grid').replaceChildren(element('p','等待当前设备设置','hint'));$('profile-select').replaceChildren();$('profile-name').value='';renderHall();renderStandaloneKnobs();renderOverview();await client.open();
    try{await device.handshake();render();schedulePoll();}catch(error){await client.close();throw error;}
  },simulated?'模拟会话已连接。全部结果仅用于界面/协议软件演示。':'已完成能力握手和配置读回。编辑只形成网页草稿。','连接失败');
}
$('connect').onclick=()=>connect(false);$('demo').onclick=()=>connect(true);
$('disconnect').onclick=()=>action(async()=>{clearTimeout(pollTimer);if(device?.localDirty||hasIo4Draft()||hasModeDraft()||client?.last?.dirty)notice('存在未应用或未保存变化；断开不保存。');await client.close();$('connection-state').textContent='未连接';},'已断开；设备RAM变化未自动保存，重连会重新读回。');
async function readSettings(){
  const current=device,observedRevision=client.revision,selected=current.selected;
  if(current.localDirty||hasIo4Draft()||hasModeDraft()){
    const scopes=[current.localDirty?'普通设置':null,hasIo4Draft()?'IO4 映射':null,hasModeDraft()?'启动模式':null].filter(Boolean).join('、');
    if(!await confirmChange({title:'重新读取设置',description:'将舍弃未应用的网页修改：'+scopes+'。设备 RAM 中已应用但未保存的设置仍保留。',confirmLabel:'舍弃并重新读取'}))return false;
    // An uncertain prior operation may have invalidated ready; refresh is the recovery path.
    assert(device===current&&client===current.client&&client.alive&&current.selected===selected,'确认期间连接或模板已变化');
    assert(client.revision===observedRevision,'确认期间修订已变化，请再次读取');
  }
  await current.refresh();resetModeDraft(current);render();notice('设置已重新读取；设备 RAM 中未保存设置仍保留。');return true;
}
$('refresh').onclick=()=>action(readSettings,null);
$('apply').onclick=()=>action(async()=>{
  const current=device,mode=getModeDraft();notice('正在应用并保存，请等待设备确认。');
  if(!current.hello.capabilities.flash_save){validateTemplateDraft();await current.apply();render();notice('设置已应用并读回；此固件不支持保存。');return;}
  await applyAndSave(current,{startupMode:mode,validateDraft:validateTemplateDraft,isCurrent:()=>device===current&&!hasCalibrationJob(),hasIo4Draft});
  resetModeDraft(current);render();notice('设置已应用并保存，当前模板和设备保存状态已读回。');
},null);
function validateTemplateDraft(){assert([...document.querySelectorAll('fieldset input')].every(i=>i.checkValidity()),'存在未修正的无效输入');}
function runTemplateAction(kind,options={},success='') {
  return action(async()=>{
    const current=device;assert(!hasModeDraft()&&!hasIo4Draft()&&!hasCalibrationJob(),'请先处理启动模式、IO4 草稿或校准任务');
    const changed=await templateAction(current,kind,confirmChange,{...options,validateDraft:validateTemplateDraft,isCurrent:()=>current===device&&!hasIo4Draft()&&!hasModeDraft()&&!hasCalibrationJob()});
    // 取消切换时，也将下拉框恢复到真正选中的模板。
    if(changed){render();notice(success);}else $('profile-select').value=device.selected;
  },null,null,$({save:'edit-template',create:'create',rename:'rename',delete:'delete',restore:'restore',activate:'activate',select:'profile-select'}[kind]));
}
function selectProfile(id){return runTemplateAction('select',{id},id<=4?'已恢复并读取预设默认设置；活动模板未切换。':'已读取自定义模板；活动模板未切换。');}
$('profile-select').onchange=()=>selectProfile(Number($('profile-select').value));
for(const button of document.querySelectorAll('[data-profile]'))button.onclick=()=>selectProfile(Number(button.dataset.profile));
$('edit-template').onclick=()=>runTemplateAction('save',{},'当前设置已保存到自定义模板，设备保存状态已读回。');
$('create').onclick=()=>runTemplateAction('create',{},'已新建并保存当前设置；活动模板未切换。');
$('activate').onclick=()=>runTemplateAction('activate',{},'活动模板已切换；需要掉电保留时，请保存到设备。');
$('rename').onclick=()=>runTemplateAction('rename',{name:$('profile-name').value},'模板名称已修改并保存。');
$('delete').onclick=()=>runTemplateAction('delete',{},'自定义模板已删除，设备保存状态已读回。');
$('restore').onclick=()=>runTemplateAction('restore',{},'预设默认设置已恢复；需要掉电保留时，请保存到设备。');
function download(name,text,type){const url=URL.createObjectURL(new Blob([text],{type}));const a=element('a');a.href=url;a.download=name;a.hidden=true;document.body.append(a);a.click();a.remove();setTimeout(()=>URL.revokeObjectURL(url),30000);}
$('export').onclick=()=>action(async()=>{device.checkWritable();const bytes=await exportSnapshot(client);download('FluxRail-config.json',JSON.stringify(archive(bytes,device.hello),null,2),'application/json');},'已导出设备完整快照与身份；未执行Flash保存。');
$('import-file').onchange=()=>action(async()=>{
  // 只在本地检查并呈现拟变更，不会在选择文件时发送begin/commit或save。
  importCandidate=null;device.checkWritable();const file=$('import-file').files[0];if(!file)return;assert(file.size<=20000,'配置文件过大');
  const parsed=parseArchive(await file.text());importCandidate={...parsed,revision:device.revision};const d=parsed.device;
  $('import-preview').textContent=`拟导入 ${d.profiles.length} 个模板，活动ID ${d.active_profile}，启动身份 ${d.startup_usb_mode}\n`+d.profiles.map(p=>`${p.profile_id}: ${p.name}`).join('\n')+`\n全局校准 ${d.calibration.valid?'有效记录':'无效/空'}。文件设备 ${parsed.archive.serial||'未知'}；当前设备 ${device.hello.serial}。\n跨安装校准可能不适用。网页仅检查结构/字段/范围，固件commit执行完整曲线语义检查。将替换全部RAM配置，未自动保存；确认这些拟变更后点击导入。`;
},'配置文件检查完成，请查看导入预览。');
$('import-commit').onclick=()=>action(async()=>{device.checkWritable();assert(!device.localDirty&&importCandidate,'请重新选择文件并预览');try{await importSnapshot(client,importCandidate.bytes,importCandidate.revision);await device.refresh();render();}catch(error){device.ready=false;throw error;}},'固件校验并确认完整导入，RAM读回完成。需要掉电保留时再保存。');
$('record').onclick=()=>{recording=!recording;$('record').textContent=recording?'停止状态记录':'开始状态记录';};
$('csv').onclick=()=>{const columns=['host_time','revision','physical_mask','config_ready','applying','maintenance','save_result','valid_slots','protocol_requests','protocol_rejected','overlong_lines'];const csv=columns.join(',')+'\r\n'+records.map(r=>columns.map(k=>'"'+String(r[k]??'').replaceAll('"','""')+'"').join(',')).join('\r\n');download('FluxRail-status.csv',csv,'text/csv;charset=utf-8');};
$('uf2-file').onchange=()=>action(async()=>{const file=$('uf2-file').files[0];await firmwareUpdate.check(file);notice(file?'UF2检查通过，请查看固件更新区的识别与版本比较结果。':'已清空 UF2 文件选择。');},null,'UF2检查未通过，请查看固件更新区的提示。');
// 浏览器尽力提示未保存离开；不自动save，不假定beforeunload一定可阻止关闭。
window.addEventListener('beforeunload',event=>{if(device?.localDirty||hasIo4Draft()||hasModeDraft()||client?.last?.dirty){event.preventDefault();event.returnValue='';}});
mountIntegration({getDevice:()=>device,getClient:()=>client,runAction:action,onControlsChange:controls,refresh:async()=>{await device.refresh();resetModeDraft(device);render();},confirm:confirmChange,notice,readSettings,hasModeDraft,calibrationBusy:hasCalibrationJob});
renderHall();renderStandaloneKnobs();renderInputTest();renderOverview();controls();
mountModuleNavigation({onNavigate:()=>cancelListening(false)});
