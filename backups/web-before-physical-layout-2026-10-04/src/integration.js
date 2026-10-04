import { assert } from './config.js';
import { mountIo4Editor } from './io4-editor.js';
/** 整合适配只调用已冻结CDC v1，不产生实时运动。圈数来自设备累计值，
 * 页面丢快照也不积分相邻角度；记录表示10Hz诊断而非毫秒级性能验收。 */
let view=null,last=null,lastDevice=null;
const realDials=[];
const phases=['未启动','空场采集','等待传感器参考','参考采集','等待毫米标签','标签采集','检查曲线','全轨扫描','完成','失败','取消'];
function make(tag,text=''){const e=document.createElement(tag);e.textContent=text;return e;}
function clearTelemetry(){
  last=null;document.getElementById('integration-motion-summary').textContent='等待当前设备的真实遥测';
  document.getElementById('hall-data').textContent='连接支持运动遥测的设备后，显示三路 ADC、滑轨位置与信号质量。';
  for(let i=0;i<realDials.length;++i){realDials[i].raw.style.transform='rotate(0deg)';realDials[i].scaled.style.transform='rotate(0deg)';realDials[i].dial.setAttribute('aria-label','旋钮 '+(i+1)+' 当前设备遥测未读取');document.getElementById('integration-knob-'+i).textContent='等待当前设备的真实圈数';}
}
export function mountIntegration(context){
  // 设备诊断和共享校准集中到实验室；仅改变DOM容器，命令/轮询/能力锁不变。
  const parent=document.getElementById('integration-calibration-content');
  view=make('div');view.className='calibration-content';parent.append(view);
  const intro=make('p','查看滑轨与旋钮的设备状态，建立所有模板共用的滑轨校准。填写实测端点、确认磁铁已移开后开始；采用结果后需保存到设备。');intro.className='hint';view.append(intro);
  const availability=make('p','连接支持共享校准的设备后可使用以下操作。');availability.id='calibration-availability';availability.className='hint';view.append(availability);
  const fields=make('div');fields.className='form-grid';view.append(fields);
  const actions=make('div');actions.className='toolbar calibration-actions';view.append(actions);
  const inputs={};
  for(const [id,label,type,value]of [['left','实测左端 mm','number',''],['right','实测右端 mm','number',''],['removed','磁铁已移开，可采集空场','checkbox',''],['node','当前实测标签 mm','number','']]){
    const field=make('label',label),input=make('input');input.type=type;input.id='integration-'+id;if(type==='number')input.step='any';input.value=value;if(type==='checkbox')field.className='check-field';field.append(input);fields.append(field);inputs[id]=input;
  }
  const buttons=[];let job=0;
  const opButton=(text,op,extra=()=>({}))=>{
    const button=make('button',text);button.type='button';button.disabled=true;actions.append(button);buttons.push(button);
    button.onclick=()=>context.runAction(async()=>{
      const device=context.getDevice(),client=context.getClient();assert(client?.alive&&device?.ready&&device.hello.capabilities.hall_calibration,'请连接支持校准的整合固件');
      assert(!device.localDirty,'先应用或舍弃网页草稿');
      if(op==='calibration_start'){
        device.checkWritable();assert(inputs.removed.checked,'请移开磁铁后勾选确认');
        assert(inputs.left.value.trim()&&inputs.right.value.trim(),'填写实测端点，不使用估计行程');job=Date.now()%0xffffffff||1;
      }else job=last?.calibration?.job_id||job;
      assert(job,'没有校准任务，请先开始或读回');
      await client.request(op,{expected_revision:client.revision,job_id:job,...extra()},4000);
      await pollIntegration(device);
      if(op==='calibration_finish'||op==='calibration_cancel')await context.refresh();
    },'设备已确认校准命令。等待实际采样阶段，合格结果需显式保存。');
  };
  opButton('开始空场校准','calibration_start',()=>({magnet_removed:inputs.removed.checked,left_mm:Number(inputs.left.value),right_mm:Number(inputs.right.value)}));
  for(let i=0;i<3;++i)opButton('采集 H'+(i+1)+' 参考','calibration_reference',()=>({channel:i}));
  opButton('采集当前毫米标签','calibration_node',()=>{assert(inputs.node.value.trim(),'填写当前实测毫米标签');return {mm:Number(inputs.node.value)};});
  opButton('检查曲线并开始全轨扫描','calibration_begin_sweep');opButton('结束扫描并采用候选','calibration_finish');opButton('取消校准，保留旧记录','calibration_cancel');
  const summary=make('p','等待支持真实遥测/校准的设备');summary.id='integration-motion-summary';summary.className='soft-note';view.append(summary);
  const telemetryPanel=make('div');telemetryPanel.className='knob-telemetry';telemetryPanel.id='knob-telemetry';
  telemetryPanel.append(make('h3','设备旋钮状态'));
  const telemetryHint=make('p','连接支持运动遥测的设备后显示实际圈数与方向；上方圆盘可随时进行本地模拟。');telemetryHint.className='hint';telemetryPanel.append(telemetryHint);
  const telemetry=make('div');telemetry.className='knob-grid';telemetryPanel.append(telemetry);document.querySelector('#knobs').append(telemetryPanel);
  for(let i=0;i<2;++i){
    const card=make('div');card.className='knob-card knob-preview';card.append(make('h3','旋钮 '+(i+1)+' · 设备遥测'));
    const dial=make('div');dial.className='interactive-dial real-dial';dial.setAttribute('role','img');dial.setAttribute('aria-label','旋钮 '+(i+1)+' 设备累计圈数投影');
    const svg=document.createElementNS('http://www.w3.org/2000/svg','svg');svg.setAttribute('viewBox','0 0 200 200');
    const shape=(tag,attrs)=>{const e=document.createElementNS(svg.namespaceURI,tag);for(const[k,v]of Object.entries(attrs))e.setAttribute(k,String(v));return e;};
    svg.append(shape('circle',{cx:100,cy:100,r:79,class:'preview-ring'}),shape('circle',{cx:100,cy:100,r:63,class:'preview-face'}));
    const raw=shape('g',{class:'preview-raw'}),scaled=shape('g',{class:'preview-scaled'});
    raw.append(shape('circle',{cx:100,cy:28,r:6}));scaled.append(shape('path',{d:'M100 44 90 62h7v52h6V62h7z'}));svg.append(raw,scaled);dial.append(svg);card.append(dial);
    const label=make('p','等待整合固件真实圈数');label.id='integration-knob-'+i;card.append(label,make('p','外圈点：物理累计圈数；内箭头：设备倍率后圈数。快照约10Hz，不向设备发送运动。'));
    telemetry.append(card);realDials.push({raw,scaled,dial});
  }
  const note=make('p','校准需空场、H1/H2/H3参考、7～17个含端点与交接的实测标签，然后全轨扫描。失败/取消保留旧记录。诊断轮询不证明采样无丢失。');note.className='hint';view.append(note);
  const boot=make('button','进入 UF2 下载模式');boot.type='button';boot.id='integration-bootloader';boot.disabled=true;document.querySelector('#update')?.append(boot);
  boot.onclick=()=>context.runAction(async()=>{const d=context.getDevice();d.checkWritable();assert(!d.localDirty,'先应用或舍弃网页草稿');await context.getClient().request('enter_bootloader',{expected_revision:context.getClient().revision},4000);},'设备准备进入ROM下载模式；需手动复制FluxRail UF2，并重新连接确认版本。');
  const modePanel=make('div');modePanel.className='soft-note integration-mode';document.querySelector('#update').append(modePanel);
  const modeLabel=make('label','下次启动 USB 身份'),mode=make('select');mode.id='integration-mode';
  for(const [value,text]of [['NORMAL_NKRO','普通 NKRO + CDC'],['IO4','IO4（游戏兼容待实测）']]){const o=make('option',text);o.value=value;mode.append(o);}modeLabel.append(mode);modePanel.append(modeLabel);
  const setMode=make('button','应用启动身份到 RAM'),reboot=make('button','重启到已保存身份');setMode.type=reboot.type='button';modePanel.append(setMode,reboot);
  setMode.onclick=()=>context.runAction(async()=>{await context.getDevice().mutate('set_startup_usb_mode',{mode:mode.value});await context.refresh();},'启动身份已应用到RAM。请显式保存，再重启到已保存身份。');
  reboot.onclick=()=>context.runAction(async()=>{const d=context.getDevice();d.checkWritable();assert(!d.localDirty,'先应用网页草稿');await d.client.request('reboot_to_saved_mode',{expected_revision:d.revision,target:d.meta.startup_usb_mode},4000);},'重启请求已确认。IO4默认没有CDC；返回普通时按住BT10复位，再重新连接。');
  modePanel.append(make('p','身份修改不立即切换USB；先应用、保存，再冷重启。IO4返回普通：按住BT10（GP9）复位/上电。BOOTSEL保留为ROM恢复入口。'));
  view.io4Editor=mountIo4Editor(context);
  view.buttons=buttons;view.boot=boot;view.modeControls={mode,setMode,reboot};
}
export function lockIntegration(device,busy=false){
  if(!view)return;
  view.busy=busy;
  if(lastDevice!==device){clearTelemetry();lastDevice=device;view.modeRevision=undefined;}
  if(!device?.client?.alive||!device?.hello?.capabilities.motion_telemetry)clearTelemetry();
  view.io4Editor.sync(device,busy);
  const {mode,setMode,reboot}=view.modeControls;
  const writable=device?.client?.alive&&device?.ready&&!busy&&!device.localDirty&&device.revision===device.client.revision&&device.status.config_ready&&!device.status.applying&&device.status.maintenance==='idle';
  // IO4草稿也属于未应用配置；校准/UF2会改变配置或断开设备，应先处理草稿。
  const io4Draft=view.io4Editor.hasDraft();
  const modes=writable&&!io4Draft&&device.hello.capabilities.usb_cold_switch;
  mode.disabled=setMode.disabled=reboot.disabled=!modes;
  if(device?.meta){if(view.modeRevision!==device.revision){mode.value=device.meta.startup_usb_mode;view.modeRevision=device.revision;}reboot.disabled=!modes||device.client.last?.dirty;}
  const capable=device?.client?.alive&&device?.ready&&device.revision===device.client.revision&&device.hello.capabilities.hall_calibration;
  document.getElementById('calibration-availability').textContent=!device?.client?.alive?'连接支持共享校准的设备后可使用以下操作。':device?.hello?.capabilities.hall_calibration?'设备支持共享校准；完成采集并采用后，点击保存到设备。':device?.client?.port?.simulated?'模拟演示支持参数编辑与本地旋转预览；共享校准需连接支持校准的设备。':'当前固件未提供共享校准功能，请连接支持校准的整合固件。';
  const calibratable=capable&&device.status.config_ready&&!device.status.applying&&device.status.maintenance==='idle';
  for(const b of view.buttons)b.disabled=!calibratable||busy||device.localDirty||io4Draft||last?.adopting;
  for(const input of view.querySelectorAll('input'))input.disabled=!calibratable||busy||device.localDirty||io4Draft;
  view.boot.disabled=!writable||io4Draft||!device?.hello?.capabilities.bootloader_command||last?.running||last?.adopting;
}
export async function pollIntegration(device){
  if(!view)return;
  if(!device?.hello?.capabilities.motion_telemetry){clearTelemetry();return;}
  const m=await device.client.request('get_motion');last=m;
  document.getElementById('hall-data').textContent=m.sample_ready?'三路 ADC：'+m.raw_adc.join(' / ')+'\n滑轨：'+(m.hall.valid?m.hall.mm.toFixed(2)+' mm · 归一化输出 '+m.hall.output.toFixed(4):'等待有效校准或信号')+'\n质量：'+m.hall.quality:'等待设备采用采集配置';
  const phase=m.calibration?.stage??0;
  document.getElementById('integration-motion-summary').textContent=m.sample_ready?
    '采集 ADC '+(m.adc_started?'已启动':'失败')+' / SSI '+(m.ssi_started?'已启动':'失败')+' · 三ADC '+m.raw_adc.join(' / ')+'；滑轨 '+(m.hall.valid?m.hall.mm.toFixed(2)+' mm · 输出 '+m.hall.output.toFixed(4):'无有效输出，质量 '+m.hall.quality)+'；'+phases[phase]+' '+m.calibration.collected+'/'+m.calibration.required+'；核心1最大服务 '+m.core1_max_service_us+' µs':'采集尚未采用配置';
  for(let i=0;i<2;++i){const k=m.knobs?.[i];if(k){realDials[i].raw.style.transform='rotate('+k.raw_turns*360+'deg)';realDials[i].scaled.style.transform='rotate('+k.scaled_turns*360+'deg)';realDials[i].dial.setAttribute('aria-label','旋钮 '+(i+1)+' 物理 '+k.raw_turns.toFixed(4)+'圈、倍率后 '+k.scaled_turns.toFixed(4)+'圈');document.getElementById('integration-knob-'+i).textContent='旋钮 '+(i+1)+'：物理 '+k.raw_turns.toFixed(4)+' 圈 · 倍率后 '+k.scaled_turns.toFixed(4)+' 圈 · 箭头 '+(((k.scaled_turns%1)+1)%1*360).toFixed(1)+'° · PUSH '+(k.filtered_push?'按下':'松开')+' · CRC错 '+k.crc_errors+' · '+(k.valid?'有效':'质量 '+k.quality)+' · '+(k.axis?'Y':'X');}}
  lockIntegration(device,view.busy);
  document.getElementById('calibration-state').textContent=phases[phase]+' · job '+m.calibration.job_id+' · generation '+m.calibration_generation+(m.running?' · 其他来源继续工作':'');
}

// 共享页面可据此阻止未应用IO4选择时保存/切换模板/冷切换，避免保存旧映射。
export function hasIo4Draft(){return !!view?.io4Editor?.hasDraft();}
