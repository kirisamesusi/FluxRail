/** 旋钮的换算与“手动测试”输入。设备面板上的旋钮只在模拟演示的手动测试模式下
 * 可以用滚轮和拖动；连接真实设备后只跟随硬件遥测，网页不产生任何设备输入。
 * 这里不读取传感器、不生成HID、不发送CDC，也不修改设备草稿。
 * 顺时针为本示意的正方向，不能据此推断实板SSI安装方向或主机像素位移。
 */
export function angleInTurn(turns){return ((turns%1+1)%1)*360;}
export function angleDelta(previous,current){return ((current-previous+540)%360)-180;}
export function boundedTurns(turns){return Math.max(-1000,Math.min(1000,turns));}
export function projectPreview(turns,config){
  const scaled=turns*config.gain*(config.invert?-1:1);
  return {rawAngle:angleInTurn(turns),scaledTurns:scaled,scaledAngle:angleInTurn(scaled),axis:config.axis===1?'Y':'X'};
}
/** 给旋钮元素挂上滚轮 / 拖动输入。enabled() 为 false（真实设备、未连接、非手动模式）时完全不响应；
 * rotate(deltaTurns) 由调用者累加到模拟信号上，调用者负责平滑显示，转动不会一格一格地跳。 */
export function attachManualKnob(element,{enabled,rotate}){
  const pointerAngle=event=>{const rect=element.getBoundingClientRect();return Math.atan2(event.clientX-rect.left-rect.width/2,rect.top+rect.height/2-event.clientY)*180/Math.PI;};
  let pointer=null,last=0;
  element.addEventListener('pointerdown',event=>{if(!enabled()||event.button!==0)return;pointer=event.pointerId;last=pointerAngle(event);element.setPointerCapture?.(pointer);});
  element.addEventListener('pointermove',event=>{if(event.pointerId!==pointer||!enabled())return;const angle=pointerAngle(event);rotate(angleDelta(last,angle)/360);last=angle;});
  const release=()=>{pointer=null;};
  element.addEventListener('pointerup',release);element.addEventListener('pointercancel',release);element.addEventListener('lostpointercapture',release);
  // 普通滚轮转动；Ctrl/Meta滚轮保留浏览器缩放快捷键。
  element.addEventListener('wheel',event=>{if(!enabled()||!event.deltaY||event.ctrlKey||event.metaKey)return;event.preventDefault();rotate(event.deltaY>0?1/24:-1/24);},{passive:false});
}
