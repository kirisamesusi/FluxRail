// Third-round changes are confined to the independent presentation shell.
export function applyRound3() {
  const q = selector => { const node=document.querySelector(selector); if(!node)throw Error('Missing round-3 target: '+selector);return node; };
  const make = (tag,text='',className='') => {const node=document.createElement(tag);node.textContent=text;if(className)node.className=className;return node;};
  // A single warning groups the connection consequence and the recovery method.
  const dialog=q('#io4-mode-confirmation');
  q('#io4-mode-title').textContent='启用 IO4 启动模式？';
  const warning=make('div','','io4-warning'); warning.id='io4-mode-description';
  warning.append(make('strong','启用后，配置网页将无法连接设备。'));
  warning.append(make('p','应用并保存后，下次上电进入 IO4 模式。需要返回普通模式时，按住 BT10，再复位设备或重新接通电源。'));
  q('#io4-mode-description').replaceWith(warning); q('#io4-mode-confirmation .io4-recovery').remove();
  q('#io4-mode-confirm').textContent='了解，选择 IO4';
  // Keep the existing Profiles panel, but put its navigation after Knobs.
  const nav=q('.module-nav'),profileLink=q('.module-nav [data-module="profiles"]'),oldDiagnosticLink=q('.module-nav [data-module="diagnostics"]');
  const statusLink=oldDiagnosticLink.cloneNode(true); statusLink.href='#device-status';statusLink.dataset.module='device-status';
  statusLink.querySelector('span').textContent='设备状态'; statusLink.removeAttribute('aria-current');
  nav.prepend(statusLink);q('.module-nav [data-module="knobs"]').after(profileLink);oldDiagnosticLink.remove();
  // Device information, current-template overview and live input view share one homepage.
  const status=make('section','','module-panel');status.id='device-status';status.setAttribute('aria-labelledby','device-status-title');
  const heading=q('#profiles .section-heading').cloneNode(true);heading.querySelector('use').setAttribute('href','#art-diagnostics');
  heading.querySelector('.eyebrow').textContent='DEVICE STATUS';heading.querySelector('h2').id='device-status-title';heading.querySelector('h2').textContent='设备状态';
  heading.querySelector('p').textContent='查看设备信息、当前模板和输入状态。点击对应项目进入设置。';heading.querySelector('.badge').remove();status.append(heading);
  const info=make('div','','device-info');info.append(make('h3','设备信息'),q('#identity'));status.append(info);
  const template=make('div','','current-template-overview');
  const templateInfo=make('div');templateInfo.append(make('span','设备当前模板','overview-label'),make('strong','连接后查看'));
  const templateEdit=make('p','页面编辑：MULTI K（预设示意）','hint');templateInfo.append(templateEdit);
  const templateLink=make('a','管理模板 →','overview-link');templateLink.href='#profiles';template.append(templateInfo,templateLink);status.append(template);
  const grid=make('div','','device-module-grid');grid.setAttribute('aria-label','输入启用情况与设置入口');
  for(const [id,title,description,enableLabels] of [
    ['keyboard','按键','12 个按键，分别设置动作。',[]],
    ['hall','滑轨','位置、方向和中心倍率。',['启用滑轨输入']],
    ['knobs','旋钮','两个旋钮的方向、倍率和输出轴。',['启用旋钮 1','启用旋钮 2']]
  ]) {
    const card=make('div','','device-module-card');card.append(make('h3',title),make('p',description,'hint'));
    if(id==='keyboard')card.append(make('p','启用情况：连接后查看','device-awaiting'));
    else for(const text of enableLabels){const label=make('label',text,'check-field');const input=make('input');input.type='checkbox';input.disabled=true;input.setAttribute('aria-label',text);label.append(input);card.append(label);}
    if(id!=='keyboard')card.append(make('small','连接后读取启用状态','device-awaiting'));
    const link=make('a','进入'+(id==='keyboard'?'按键映射':title+'设置')+' →','overview-link');link.href='#'+id;card.append(link);grid.append(card);
  }
  status.append(grid);
  const inputs=q('#diagnostics');inputs.querySelector('.section-heading').remove();
  const inputGroup=make('details','','status-inputs');inputGroup.id='input-status';inputGroup.open=true;
  inputGroup.append(make('summary','输入状态 · 按键与旋钮'));
  while(inputs.firstChild)inputGroup.append(inputs.firstChild);inputs.remove();status.append(inputGroup);
  q('#profiles').before(status);
  const demo=q('#experiment-device');demo.id='experiment-demo';demo.querySelector('summary').textContent='临时测试 · 模拟演示';
  demo.querySelector('p').textContent='仅供页面测试，不连接真实设备。测试结束后可移除此入口。';
  q('#experimental .experimental-heading p').textContent='校准滑轨、调整高级参数，或导入导出配置。';
  // Short guidance retains the three distinct template actions.
  q('#capacity').textContent='预设 4 个 · 自定义最多 8 个';
  q('#profiles .section-heading p').textContent='选择要编辑的模板。预设可另存为自定义模板。';
  q('#profiles > .hint').textContent='点选预设会恢复默认设置，预设不能覆盖或改名。“保存到当前模板”可保存自定义模板；选中预设时会另存为新模板。要让设备使用该模板，先点“设为活动模板”，再应用并保存。';
  q('#manual-update-help').textContent='物理进入方法：按住 BOOT 接入 USB，再松开按键。出现 RPI-RP2 磁盘后，复制适用于本设备的 UF2 文件；完成后重新连接网页，核对固件版本。';
  q('#experiment-hall > summary').textContent='滑轨 · 校准、输出范围与响应';
  // Replace channel-count jargon in page copy, preserving device field identifiers.
  const walker=document.createTreeWalker(document.body,NodeFilter.SHOW_TEXT);
  while(walker.nextNode())walker.currentNode.nodeValue=walker.currentNode.nodeValue.replaceAll('12 路按键','12 个按键').replaceAll('十二路','12 个输入').replaceAll('三路','三个').replaceAll('输入测试','设备状态');
  for(const node of document.querySelectorAll('[aria-label],[title]'))for(const attr of ['aria-label','title']){const value=node.getAttribute(attr);if(value)node.setAttribute(attr,value.replaceAll('12 路按键','12 个按键').replaceAll('输入测试','设备状态'));}
  q('.copy-preview-note strong').textContent='文案预览 · 第三轮修订';
  q('.copy-preview-note span').textContent='可查看设备状态布局、跳转设置和试调倍率。设备操作未接入。';
  document.title='FluxRail · 文案预览（第三轮修订）';
  // Fourth-round copy refinement; keep the device-data label and action controls.
  q('.status-panel .eyebrow').textContent='SETTINGS';
  q('.status-panel h2').firstChild.nodeValue='设置与保存 ';
  q('#device-summary').textContent='改好后点“应用并保存”，断电后也会保留。';
  q('#draft-state').textContent='暂无修改';
  q('#save-state').textContent='保存状态：连接后查看';
  // Remove the four heading badges requested in the latest preview annotations.
  for(const id of ['keyboard','hall','knobs','profiles'])q('#'+id+' > .section-heading > .badge').remove();
}
