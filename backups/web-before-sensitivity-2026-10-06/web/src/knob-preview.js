/** 旋钮本地交互示意，不读取传感器、不生成HID、不发送CDC，不修改设备草稿。
 * 拖动产生的累计圈数仅用于解释倍率/反向；真实设备仍须使用公共累计圈数遥测。
 * 顺时针为本示意的正方向，不能据此推断实板SSI安装方向或主机像素位移。
 */
export function angleInTurn(turns){return ((turns%1+1)%1)*360;}
export function angleDelta(previous,current){return ((current-previous+540)%360)-180;}
export function boundedTurns(turns){return Math.max(-1000,Math.min(1000,turns));}
export function projectPreview(turns,config){
  const scaled=turns*config.gain*(config.invert?-1:1);
  return {rawAngle:angleInTurn(turns),scaledTurns:scaled,scaledAngle:angleInTurn(scaled),axis:config.axis===1?'Y':'X'};
}
const node=(tag,text='',className='')=>{const e=document.createElement(tag);e.textContent=text;e.className=className;return e;};
function svgNode(tag,attributes){const e=document.createElementNS('http://www.w3.org/2000/svg',tag);for(const[k,v]of Object.entries(attributes))e.setAttribute(k,String(v));return e;}
const fixed=value=>(Math.abs(value)<0.00005?0:value).toFixed(3);
export function mountKnobPreview(container,{id,state,getConfig}){
  // getConfig每次取当前完整draft，应用/读回重建后不引用过期对象；未连接的默认值仅属示意。
  const panel=node('div','','knob-preview'),badge=node('span','本地模拟 · 不发送设备输入','preview-badge');
  const dial=node('div','','interactive-dial');dial.tabIndex=0;dial.setAttribute('role','slider');
  dial.setAttribute('aria-label',`旋钮${id+1}本地旋转预览`);dial.setAttribute('aria-valuemin','-1000');dial.setAttribute('aria-valuemax','1000');
  const svg=svgNode('svg',{viewBox:'0 0 200 200','aria-hidden':'true'});
  svg.append(svgNode('circle',{cx:100,cy:100,r:79,class:'preview-ring'}),svgNode('circle',{cx:100,cy:100,r:63,class:'preview-face'}));
  for(let i=0;i<12;i++)svg.append(svgNode('line',{x1:100,y1:20,x2:100,y2:i%3===0?29:25,transform:`rotate(${i*30} 100 100)`,class:'preview-tick'}));
  const start=svgNode('text',{x:100,y:11,'text-anchor':'middle',class:'preview-zero'});start.textContent='0°';svg.append(start);
  const raw=svgNode('g',{class:'preview-raw'});raw.append(svgNode('circle',{cx:100,cy:28,r:6}));
  const scaled=svgNode('g',{class:'preview-scaled'});scaled.append(svgNode('path',{d:'M100 57 91 73h6v40h6V73h6z'}));
  svg.append(raw,scaled,svgNode('circle',{cx:100,cy:100,r:5,class:'preview-hub'}));dial.append(svg);
  const direction=node('p','','preview-direction'),metrics=node('div','','preview-metrics');
  const values={};for(const[key,label]of [['raw','模拟输入圈数'],['rawAngle','旋钮当前位置'],['scaled','倍率后圈数'],['scaledAngle','箭头当前位置']]){
    const cell=node('div');cell.append(node('span',label));values[key]=node('strong');cell.append(values[key]);metrics.append(cell);
  }
  const buttons=node('div','','preview-buttons');
  for(const[label,delta]of [['逆时针 ¼圈',-.25],['顺时针 ¼圈',.25],['顺时针 1圈',1],['复位',null]]){
    const button=node('button',label);button.type='button';button.onclick=()=>{if(delta===null){state.turns=0;state.delta=0;update();}else advance(delta);};buttons.append(button);
  }
  const settings=node('p','','hint preview-settings');
  panel.append(badge,dial,node('p','外圈圆点：模拟旋钮位置；内箭头：倍率后的旋转。','hint preview-legend'),direction,metrics,buttons,settings);
  container.append(panel);
  function update(){
    const current=getConfig(),config=current??{gain:1,invert:false,axis:id,enabled:true},p=projectPreview(state.turns,config);
    // 使用未回绕累计角驱动图形，+1圈仍能完整转一圈；读数单独显示0～360°圈内位置。
    raw.style.transform=`rotate(${state.turns*360}deg)`;scaled.style.transform=`rotate(${p.scaledTurns*360}deg)`;
    values.raw.textContent=fixed(state.turns)+' 圈';values.rawAngle.textContent=p.rawAngle.toFixed(1)+'°';values.scaled.textContent=fixed(p.scaledTurns)+' 圈';values.scaledAngle.textContent=p.scaledAngle.toFixed(1)+'°';
    const delta=state.delta*config.gain*(config.invert?-1:1),arrow=!delta?'—':p.axis==='X'?(delta>0?'→':'←'):(delta>0?'↓':'↑');
    direction.textContent=`${arrow} 模拟输出轴 ${p.axis} · ${!state.delta?'未旋转':state.delta>0?'顺时针输入':'逆时针输入'}${current&&!config.enabled?' · 来源关闭，仅显示倍率效果':''}`;
    settings.textContent=current?`使用网页设置：倍率 ${config.gain}，${config.invert?'反向':'正向'}。真实设备圈数另见遥测圆盘。`:'未读取设备配置：示意使用倍率 1、正向；默认轴仅用于本地预览。';
    dial.setAttribute('aria-valuenow',String(state.turns));dial.setAttribute('aria-valuetext',`本地输入 ${fixed(state.turns)} 圈，倍率后 ${fixed(p.scaledTurns)} 圈，输出轴 ${p.axis}`);
  }
  function advance(delta){const next=boundedTurns(state.turns+delta);state.delta=next-state.turns;state.turns=next;update();}
  const pointerAngle=event=>{const rect=dial.getBoundingClientRect();return Math.atan2(event.clientX-rect.left-rect.width/2,rect.top+rect.height/2-event.clientY)*180/Math.PI;};
  let pointer=null,last=0;
  dial.addEventListener('pointerdown',event=>{if(event.button!==0)return;event.preventDefault();dial.focus();pointer=event.pointerId;last=pointerAngle(event);dial.setPointerCapture(pointer);dial.classList.add('dragging');});
  dial.addEventListener('pointermove',event=>{if(event.pointerId!==pointer)return;const angle=pointerAngle(event);advance(angleDelta(last,angle)/360);last=angle;});
  const release=()=>{pointer=null;dial.classList.remove('dragging');};
  dial.addEventListener('pointerup',release);dial.addEventListener('pointercancel',release);dial.addEventListener('lostpointercapture',release);
  // 普通滚轮驱动示意；Ctrl/Meta滚轮保留浏览器缩放快捷键。
  dial.addEventListener('wheel',event=>{if(!event.deltaY||event.ctrlKey||event.metaKey)return;event.preventDefault();advance(event.deltaY>0?1/24:-1/24);},{passive:false});
  dial.addEventListener('keydown',event=>{
    if(['ArrowRight','ArrowUp','ArrowLeft','ArrowDown'].includes(event.key)){event.preventDefault();advance(['ArrowRight','ArrowUp'].includes(event.key)?1/24:-1/24);}
    else if(event.key==='Home'){event.preventDefault();state.turns=0;state.delta=0;update();}
  });update();return update;
}
