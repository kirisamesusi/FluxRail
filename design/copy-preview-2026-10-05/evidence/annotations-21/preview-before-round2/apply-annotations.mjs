// Only transforms the independent review shell. No firmware or production Web changes.
export function applyAnnotations() {
  const q = selector => { const el = document.querySelector(selector); if (!el) throw Error('Missing annotation target: ' + selector); return el; };
  const make = (tag, text = '', className = '') => { const el = document.createElement(tag); el.textContent = text; if (className) el.className = className; return el; };
  const group = id => [...document.querySelectorAll('#experimental-content > details')].find(el => el.querySelector(id));
  // 1: put the layout guide below the key cards.
  q('#key-editor').after(q('#keyboard .layout-guide'));
  // 2/3: keep the single-key colour entry, omit brightness-limit copy.
  q('#brightness-text').remove();
  q('#lighting > .hint').textContent = '单独设置每颗按键的颜色，请展开“实验性功能”。';
  // 4: remove the repeated interaction instruction on both preview dials.
  document.querySelectorAll('#knob-fields .knob-preview').forEach(panel => panel.lastElementChild.remove());
  // 5/6: local ruler + exact numeric input. Values are a proposed UI, never device defaults.
  const ruler = (label, title, id) => {
    label.replaceChildren(); label.className = 'gain-control';
    label.append(make('span', title, 'gain-title'));
    const body = make('div', '', 'gain-body');
    const scale = make('div', '', 'gain-scale');
    const ends = make('div', '', 'gain-endpoints'); ends.append(make('span', '低'), make('span', '高'));
    const slider = make('input'); slider.type = 'range'; slider.min = '0'; slider.max = '10'; slider.step = 'any'; slider.value = '5';
    slider.id = id + '-ruler'; slider.dataset.localGain = id; slider.setAttribute('aria-label', title + '刻度尺');
    slider.setAttribute('aria-valuetext', '倍率 5');
    const ticks = make('div', '', 'gain-ticks'); ticks.setAttribute('aria-hidden', 'true');
    for (const value of ['0.1', '1', '2', '3', '4', '5', '6', '7', '8', '9', '10']) ticks.append(make('span', value));
    scale.append(ends, slider, ticks);
    const exact = make('div', '', 'gain-exact'); const name = make('span', '输入数值');
    const input = make('input'); input.type = 'number'; input.min = '0.1'; input.max = '10'; input.step = 'any'; input.value = '5';
    input.id = id + '-number'; input.dataset.localGain = id; input.setAttribute('aria-label', title + '输入数值');
    const error = make('small', '', 'gain-error'); error.id = id + '-error'; error.setAttribute('role', 'status'); input.setAttribute('aria-describedby', error.id);
    exact.append(name, input); body.append(scale, exact); label.append(body, error);
    for (let p = label.parentElement; p; p = p.parentElement) if (p.tagName === 'FIELDSET') p.disabled = false;
  };
  ruler(q('#hall-fields > label:nth-child(3)'), '中心倍率', 'hall-gain');
  document.querySelectorAll('#knob-fields .knob-settings').forEach((el, i) => ruler(el.querySelector('label:last-child'), '倍率', 'knob-gain-' + (i + 1)));
  // 7: remove input frequency badge.
  q('#diagnostics .section-heading > .badge').remove();
  // 8/9: use mode consistently, including accessibility text.
  const walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT);
  while (walker.nextNode()) walker.currentNode.nodeValue = walker.currentNode.nodeValue.replaceAll('身份', '模式');
  for (const el of document.querySelectorAll('[aria-label],[title],[placeholder]')) for (const attr of ['aria-label','title','placeholder']) {
    const value = el.getAttribute(attr); if (value) el.setAttribute(attr, value.replaceAll('身份', '模式'));
  }
  // 10/11/12: mode selection lives visibly in Profiles, recovery gets its own notice.
  const mode = q('#update .integration-mode'); mode.classList.add('startup-mode-card');
  mode.prepend(make('h3', '启动模式'));
  const buttons = [...mode.querySelectorAll(':scope > button')]; buttons[0].remove();
  mode.querySelector('p').textContent = '应用并保存启动模式后，将在下次上电时启用。';
  const recovery = make('div', '', 'io4-recovery');
  recovery.append(make('strong', '从 IO4 返回普通模式'), make('p', '按住 BT10，再复位设备或重新接通电源。进入普通模式后，即可重新连接网页。'));
  mode.append(recovery); q('#profiles .section-heading').after(mode);
  // 13: restart sits beside Apply & Save; click only opens a local confirmation.
  const reboot = buttons[1]; reboot.id = 'restart-preview'; reboot.textContent = '重启设备'; reboot.disabled = false;
  q('.status-panel > .toolbar').append(reboot);
  const dialog = q('#confirmation-dialog'); dialog.id = 'restart-confirmation';
  dialog.setAttribute('aria-labelledby', 'restart-title'); dialog.setAttribute('aria-describedby', 'restart-description');
  dialog.replaceChildren();
  const title = make('h2', '重启设备'); title.id = 'restart-title';
  const description = make('p', '重启后，未保存的修改会丢失，设备将使用已保存的设置。确定要重启吗？'); description.id = 'restart-description';
  const note = make('p', '本页只演示确认弹窗，不会重启设备。', 'hint');
  const actions = make('div', '', 'toolbar dialog-actions');
  const cancel = make('button', '取消'); cancel.id = 'restart-cancel'; cancel.type = 'button';
  const confirm = make('button', '确认重启', 'primary'); confirm.id = 'restart-confirm'; confirm.type = 'button';
  actions.append(cancel, confirm); dialog.append(title, description, note, actions);
  // 14: emphasise the firmware version line.
  q('#current-firmware').classList.add('current-version');
  // 15/21: calibration joins the Hall group; explain each parameter below its label.
  const hallGroup = group('#hall-advanced-fields'); hallGroup.id = 'experiment-hall';
  q('#experiment-calibration > summary').textContent = '滑轨校准';
  hallGroup.append(q('#experiment-calibration'));
  hallGroup.querySelector(':scope > .hint').textContent = '调整滑轨的输出范围和响应。下方可展开“滑轨校准”，按实际行程建立校准记录。';
  const fieldCopy = [
    ['输出下界（归一化）', '输出范围的最小值，取值 0～1。中心倍率小于 1 时，实际输出只覆盖这个范围的中央一段。'],
    ['输出上界（归一化）', '输出范围的最大值，取值 0～1，须大于下界。'],
    ['静止滤波（微秒）', '静止或慢速移动时的平滑程度。值越大，抖动越少，跟随也越慢；设为 0 可关闭此滤波。'],
    ['移动滤波（微秒）', '快速移动时的平滑程度。值越小，跟随越快；不能大于静止滤波。'],
    ['滤波过渡速度（毫米/秒）', '达到这个速度时，使用移动滤波。低于此速度时，在静止和移动滤波之间逐渐过渡，不会限制滑块速度。'],
    ['活动门槛（毫米）', '移动多远才算正在操作滑轨，用于输入来源切换，不是位置死区。实际门槛还会参考校准测得的噪声。'],
    ['活动保持（微秒）', '确认滑轨正在移动后，保持活动状态的时间。'],
    ['进入信号（ADC count）', '三路传感器经校准后的信号总和达到这个值，才开始接受滑轨位置。值越高，要求的信号越强。'],
    ['退出信号（ADC count）', '已接受位置后，三路校准信号总和低于这个值便停止接受。不能大于进入信号，两个门槛可减少状态反复切换。'],
    ['曲线容差（ADC count）', '允许传感器读数偏离校准曲线的程度。超过这个值，当前位置会被判为无效；放宽后也可能接受更多异常读数。'],
    ['最大有效间隔（微秒）', '有效采样间隔超过此值，或采样出现中断时，会以新位置重新建立参考，不沿用此前的滤波状态。']
  ];
  [...q('#hall-advanced-fields').children].forEach((label, i) => {
    const input = label.querySelector('input'); const [name, help] = fieldCopy[i];
    label.replaceChildren(make('span', name, 'parameter-name'), input, make('small', help, 'parameter-help'));
    label.classList.add('parameter-field'); const id = 'hall-parameter-help-' + i; label.lastElementChild.id = id; input.setAttribute('aria-describedby', id);
  });
  // 16: IO4 uses exactly the same physical arrangement as ordinary mapping.
  const ioGrid = q('.io4-key-grid'), ordinary = q('#key-grid .physical-keys-main');
  ioGrid.classList.add('physical-keys-main');
  for (const key of ordinary.children) {
    const id = key.dataset.source; const select = q('#io4-binding-' + id); select.closest('.io4-key-card').dataset.source = id; ioGrid.append(select.closest('.io4-key-card'));
  }
  const scroller = make('div', '', 'io4-key-scroller'); ioGrid.before(scroller); scroller.append(ioGrid);
  // 17: merge applying + saving IO4; use the existing Profiles entry to activate a template.
  q('#io4-mapping-apply').textContent = '应用并保存 IO4 设置'; q('#io4-mapping-save').remove();
  q('#io4-mapping-activate').remove(); q('#io4-mapping-refresh').textContent = '重新读取';
  q('#io4-mapping-editor .toolbar > a').remove();
  q('#io4-mapping-editor .io4-editor-body > p.hint').textContent = '在普通模式下连接设备，为当前模板设置 IO4 按键。改好后点“应用并保存 IO4 设置”。需要启用这个模板时，请到“模板管理”点“设为活动模板”，再应用并保存。普通模式的键位会保留。';
  // 18: one update location; bring the alternate hardware-entry method here.
  const updateGroup = group('#uf2-result');
  const manual = make('details', '', 'experiment-group manual-update'); manual.id = 'manual-update';
  manual.append(make('summary', '手动进入下载模式'), make('p', '按住 BOOT，按下并松开 SW1 / RUN，然后松开 BOOT。也可以按住 BOOT 接入 USB 后松开。电脑出现 RPI-RP2 磁盘后，将适用于本设备的 UF2 文件复制进去；完成后重新连接网页，核对固件版本。', 'hint'));
  manual.append(q('#uf2-result')); q('#update').append(manual); updateGroup.remove();
  // 19/20: GP11 is the Test light; remove its controls and brightness copy.
  const light = q('#light-advanced-fields'); [...light.children].slice(3).forEach(el => el.remove());
  const lightGroup = group('#light-advanced-fields'); lightGroup.querySelector('summary').textContent = '灯光 · 单键颜色';
  lightGroup.querySelector(':scope > .hint').textContent = '切换灯效时会沿用已经设置的单键颜色。';
  // Correct links after moving mode selection and calibration.
  document.querySelectorAll('a[href="#update"]').forEach(a => { if (!a.dataset.module) a.href = '#profiles'; });
  const hints = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT);
  while (hints.nextNode()) hints.currentNode.nodeValue = hints.currentNode.nodeValue.replaceAll('下方“运动”分组', '本组“滑轨校准”').replaceAll('在更新主区', '在模板管理中');
  q('.copy-preview-note strong').textContent = '文案预览 · 批注修订版';
  q('.copy-preview-note span').textContent = '可以切换页面、展开说明、试调倍率和查看重启确认。设备操作未接入。';
  document.title = 'FluxRail · 文案预览（批注修订版）';
}
