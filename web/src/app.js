import { applyAndSave } from './workflows.js';
import { mountGainControl } from './gain-control.js';
import { mountConfirmation } from './confirmation.js';
import { templateAction } from './template-actions.js';
import { mountModuleNavigation } from './layout.js';
import { SerialClient } from './connection/client.js';
import { Device } from './device.js';
import { keys, fromEvent, fromCharacter, actionLabel, actionShort } from './keyboard.js';
import { hallFields, knobFields, assert } from './config.js';
import { archive, exportSnapshot, importSnapshot, parseArchive } from './snapshot.js';
import { FirmwareUpdate } from './update/controller.js';
import { attachManualKnob, projectPreview } from './knob-preview.js';
import { mountIntegration, pollIntegration, lockIntegration, hasIo4Draft, hasModeDraft, getModeDraft, resetModeDraft, hasCalibrationJob, getMotion } from './integration.js';
import { createPalette, rgbToHex, hexToRgb } from './colors.js';
import { changeList, keyName, knobName, effectName } from './history.js';
import { loadLines, pickLine } from './mascot.js';
/** 单页DOM编排层。网页草稿、设备RAM和已保存Flash是三个不同状态：
 * change只改draft；apply等待公共两核ACK再读回；save独立维护并等待最终ACK。
 * 所有设备字段用textContent展示，不将设备名称/文件内容注入HTML。
 * 运动/校准/升级适配复用统一CDC v1，由integration.js按能力接线。
 * 设计见 design/web-v2-2026-10-11：设备状态首页集中按键、灯光、滑轨、旋钮和模板。
 */
const $ = id => document.getElementById(id);
const confirmChange = mountConfirmation($('confirmation-dialog'));
let client = null, device = null, busy = false, connecting = false, listening = null, importCandidate = null, recording = false, records = [], dropped = 0, pollTimer = null;
let advancedSource = 7, selection = { type: 'key', id: 2 }, renaming = false, savedActive = null;
const palettes = { global: createPalette('global'), key: createPalette('key') };
const firmwareUpdate = new FirmwareUpdate({ result: $('uf2-result'), version: $('version-result'), current: $('current-firmware'), getConnection: () => client?.alive && device?.ready ? { hello: device.hello, simulated: !!client.port.simulated } : null });

// ---------- 小工具 ----------
function h(tag, attrs = {}, ...children) {
  const e = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs)) {
    if (v === false || v == null) continue;
    if (k === 'class') e.className = v;
    else if (k === 'text') e.textContent = v;
    else if (k === 'value' || k === 'checked' || k === 'disabled') e[k] = v;
    else if (k.startsWith('on')) e.addEventListener(k.slice(2), v);
    else e.setAttribute(k, v === true ? '' : String(v));
  }
  for (const c of children.flat()) if (c != null && c !== false) e.append(c);
  return e;
}
const element = (tag, text = '', className = '') => h(tag, { text, class: className });
const clamp01 = v => Math.max(0, Math.min(1, v));
const hhmm = () => { const d = new Date(); return String(d.getHours()).padStart(2, '0') + ':' + String(d.getMinutes()).padStart(2, '0'); };
const BT = [7, 6, 5, 4, 3, 2, 1, 0, 8, 9]; // 面板从左到右：BT8～BT5 | BT4～BT1，下排 BT9 / BT10

// ---------- 气泡与历史操作 ----------
const history = []; let flash = null, flashTimer = null, lastBubbleKey = '', loggedBase = null, logTimer = null;
const ICON = {
  ok: '<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="#2A7FA8" stroke-width="3" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M5 12.5l4.5 4.5L19 7.5"/></svg>',
  info: '<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="#2A7FA8" stroke-width="3" stroke-linecap="round" aria-hidden="true"><path d="M12 11v6"/><path d="M12 6.5v.01"/></svg>',
  warn: '<svg width="18" height="18" viewBox="0 0 24 24" fill="#C2456F" aria-hidden="true"><circle cx="12" cy="12" r="6"/></svg>',
  err: '<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="#B23A48" stroke-width="3" stroke-linecap="round" aria-hidden="true"><path d="M12 5v9"/><path d="M12 18.5v.01"/></svg>'
};
function addHistory(text) {
  if (history[0]?.text === text) return;
  history.unshift({ time: hhmm(), text, fresh: true }); history.length = Math.min(history.length, 40);
  renderHistory();
}
function renderHistory() {
  $('history').replaceChildren(...(history.length ? history.map(entry => { const row = h('div', { class: 'logrow' + (entry.fresh ? ' new' : '') }, h('time', { text: entry.time }), h('span', { text: entry.text })); entry.fresh = false; return row; }) : [h('div', { class: 'log-empty', text: '还没有操作记录' })]));
}
/** kind: ok / info / warn / err。成功和提示约5秒后淡出回到常驻状态；出错不自动消失。 */
function say(kind, text, { ms = 5000, log = true } = {}) {
  const at = text.indexOf('。');
  flash = { kind, title: at < 0 ? text : text.slice(0, at), body: at < 0 ? '' : text.slice(at + 1) };
  clearTimeout(flashTimer);
  if (kind !== 'err' && ms) flashTimer = setTimeout(() => { flash = null; renderBubble(); }, ms);
  if (log) addHistory(text);
  renderBubble();
}
function notice(text, error = false) { say(error ? 'err' : 'ok', text); }
function clearFlash() { clearTimeout(flashTimer); flash = null; }
function draftChanges() { return device?.draft && device.profile ? changeList(device.profile, device.draft) : []; }
function pendingCount() { return draftChanges().length + (hasModeDraft() ? 1 : 0) + (hasIo4Draft() ? 1 : 0); }
function deviceUnsaved() { return !!client?.alive && !!device?.ready && !!(device.latestEnvelope || device.client.last)?.dirty; }
function baseBubble() {
  if (connecting) return { kind: 'info', title: '正在连接…', body: '请在浏览器弹窗里选择 FluxRail。' };
  if (!device) return { kind: 'info', title: '尚未连接设备', body: '连接后可读取和修改设置。' };
  if (!client?.alive) return { kind: 'info', title: '已断开连接', body: '重新连接后会读回设备当前设置。' };
  if (!device.ready || device.revision !== client.revision) return { kind: 'err', title: '需要重新读取设置', body: '读回未完成或设备版本已变化，请重新读取后再编辑。', action: '重新读取设置' };
  const n = pendingCount();
  if (n) return { kind: 'warn', title: '有 ' + n + ' 处修改还没应用', body: '点右上角“应用并保存”写入设备，或点“还原修改”放弃。', action: '还原修改' };
  if (deviceUnsaved()) return { kind: 'warn', title: '设置已应用，还没保存', body: '断电或重启后会丢失。点“应用并保存”写入设备。' };
  return { kind: 'ok', title: '设置和设备一致', body: '修改后点“应用并保存”写入设备' };
}
function renderBubble() {
  const m = flash ?? baseBubble(), key = JSON.stringify(m);
  if (key === lastBubbleKey) return;
  lastBubbleKey = key;
  const bubble = $('bubble'); bubble.dataset.kind = m.kind;
  const title = h('div', { class: 'bt' }, h('span', { class: 'bi' }), h('span', { text: m.title }));
  title.querySelector('.bi').innerHTML = ICON[m.kind]; // 固定的内置SVG，不含设备或用户文本
  bubble.replaceChildren(title, ...(m.body ? [h('div', { class: 'bb', text: m.body })] : []));
  if (m.action) bubble.append(h('div', { class: 'ba' }, h('button', { type: 'button', id: 'bubble-action', text: m.action, onclick: () => action(readSettings, null) })));
}
function scheduleLog() {
  // 连续拖动只记最后结果：停手约0.6秒后，与上次记录时的草稿比较，写出“改了什么”。
  clearTimeout(logTimer);
  logTimer = setTimeout(() => {
    if (!device?.draft || !loggedBase) return;
    const entries = changeList(loggedBase, device.draft);
    loggedBase = structuredClone({ config: device.draft.config, motion: device.draft.motion });
    entries.slice(0, 4).forEach(entry => addHistory(entry.text + '（未应用）'));
    if (entries.length > 4) addHistory('另有 ' + (entries.length - 4) + ' 项修改（未应用）');
  }, 600);
}
function resetLogBase() { clearTimeout(logTimer); loggedBase = device?.draft ? structuredClone({ config: device.draft.config, motion: device.draft.motion }) : null; }

// ---------- 录入 / 草稿 ----------
function cancelListening(showNotice = true) {
  if (listening === null) return;
  listening.button.classList.remove('listen'); listening.button.setAttribute('aria-pressed', 'false'); listening.button.textContent = '监听按键';
  listening = null; if (showNotice) say('info', '已退出键盘监听', { log: false });
}
function mark() {
  if (!device?.draft) return;
  device.localDirty = draftChanges().length > 0;
  cancelListening(false); updateKeys(); renderLight(); renderChips(); scheduleLog(); renderBubble(); controls();
}
function writable() { return !!(device?.ready && client?.alive && !busy && device.revision === client.revision && device.status.config_ready && !device.status.applying && device.status.maintenance === 'idle' && !hasCalibrationJob()); }
function canEdit() { return writable() && !hasIo4Draft(); }
function controls() {
  lockIntegration(device, busy);
  firmwareUpdate.render();
  $('uf2-file').disabled = busy;
  // 维护/写请求期间禁用编辑；出错或冲突后仅允许读回，避免用户继续覆盖未知状态。
  const connected = !!client?.alive, edit = canEdit(), caps = device?.hello?.capabilities || {}, profiles = edit && caps.profiles, templates = profiles && !hasModeDraft();
  if (!edit) cancelListening(false);
  $('connect').disabled = busy || connecting; $('connect').textContent = connecting ? '连接中…' : connected ? '断开' : '连接 FluxRail';
  $('demo').disabled = busy || connected;
  const pending = edit && (device.localDirty || hasModeDraft() || deviceUnsaved());
  $('apply').disabled = !edit || !(pending || !caps.flash_save && device.localDirty);
  $('apply').replaceChildren(...(pending ? [h('span', { class: 'dot', 'aria-hidden': 'true' })] : []), document.createTextNode(device?.hello && !caps.flash_save ? '应用设置（不支持保存）' : '应用并保存'));
  $('light-editor').disabled = !edit; lightLock();
  for (const id of ['key-advanced-editor', 'light-advanced-editor']) $(id).disabled = !edit;
  for (const id of ['hall-advanced-editor', 'knob-advanced-editor', 'routing-editor']) $(id).disabled = !profiles;
  for (const button of document.querySelectorAll('#profile-list .tpl')) button.disabled = !templates;
  $('create').disabled = !templates || !caps.flash_save || (device?.profiles?.length ?? 99) >= caps.max_profiles;
  for (const item of document.querySelectorAll('#inspector [data-edit]')) item.disabled = !edit;
  for (const item of document.querySelectorAll('#inspector [data-motion]')) item.disabled = !profiles;
  const dirty = !!device?.localDirty, selected = device?.selected, active = device?.meta?.active_profile;
  const set = (id, disabled) => { const e = $(id); if (e) e.disabled = disabled; };
  set('tpl-activate', !templates || dirty || selected === active); set('tpl-save', !templates || !caps.flash_save); set('tpl-rename', !templates || dirty || selected <= 4);
  set('tpl-delete', !templates || dirty || selected <= 4 || selected === active); set('tpl-restore', !templates || selected > 4);
  set('tpl-name-ok', !templates || dirty || selected <= 4);
  $('export').disabled = !edit || !caps.snapshot_transfer || !!device?.localDirty || hasModeDraft();
  $('import-file').disabled = !edit || !caps.snapshot_transfer || !!device?.localDirty || hasModeDraft();
  $('import-commit').disabled = !edit || !importCandidate || !!device?.localDirty || hasModeDraft() || importCandidate.revision !== device?.revision;
  $('record').disabled = busy || !connected; $('csv').disabled = records.length === 0;
  const sim = $('signal-source'); sim.disabled = !(connected && client.port.simulated);
  if (!sim.disabled && sim.value === 'device') sim.value = 'demo'; if (sim.disabled) sim.value = 'device';
  for (const item of document.querySelectorAll('#inspector [data-manual]')) item.hidden = sim.value !== 'manual';
  for (const knob of [$('knobL'), $('knobR'), $('slider')]) knob.classList.toggle('manual', manualEnabled());
}

// ---------- 实验性功能页的字段 ----------
function field(container, label, object, key, type = 'number', options = {}) {
  // 直接引用当前完整草稿中的字段，保留未编辑的高级参数。只在change记录最后值，
  // 不向设备发滑块input洪泛，也不通过localStorage伪造设备掉电保存。
  const row = element('label', label, type === 'checkbox' ? 'check-field' : ''); let input;
  if (type === 'select') {
    input = element('select'); for (const [value, text] of options.values) { const o = element('option', text); o.value = value; input.append(o); } input.value = object[key];
  } else {
    input = element('input'); input.type = type;
    if (type === 'checkbox') input.checked = object[key];
    else { input.value = typeof object[key] === 'number' && options.step < 1 ? Number(object[key].toFixed(6)) : object[key]; if (options.min !== undefined) input.min = options.min; if (options.max !== undefined) input.max = options.max; input.step = options.step ?? 1; }
  }
  input.addEventListener('change', () => {
    if (!input.checkValidity()) { input.reportValidity(); return; }
    object[key] = type === 'checkbox' ? input.checked : type === 'select' && options.numeric ? Number(input.value) : type === 'select' ? input.value : input.valueAsNumber;
    mark();
  }); row.append(input); container.append(row); return input;
}
const hallHelp = { range_min: '输出范围的最小值，取值 0～1，须小于上界。', range_max: '输出范围的最大值，取值 0～1，须大于下界。', quiet_tau_us: '静止或慢速移动时的平滑程度。值越大，抖动越少，跟随也越慢；设为 0 可关闭此滤波。', moving_tau_us: '快速移动时的平滑程度。值越小，跟随越快；不能大于静止滤波。', fast_speed_mm_s: '达到这个速度时，使用移动滤波。低于此速度时逐渐过渡，不限制滑块速度。', activity_mm: '移动多远才算正在操作滑轨，用于来源切换，不是位置死区。实际门槛还会参考校准噪声。', activity_hold_us: '确认滑轨正在移动后，保持活动状态的时间（微秒）。', enter_signal: '三个传感器经校准后的信号总和达到此值，才开始接受位置，单位 ADC count。', exit_signal: '三路校准信号总和低于此值便停止接受位置。不能大于进入信号；单位 ADC count。', curve_tolerance: '允许读数偏离校准曲线的程度（ADC count）。放宽可能接受更多异常读数。', max_gap_us: '有效采样间隔超过此值（微秒），或采样中断时，以新位置重新建立参考，不沿用旧滤波状态。' };
function numbers(container, object, descriptors) {
  for (const [key, label, min, max, type] of descriptors) {
    const input = field(container, label, object, key, 'number', { min, max, step: type.startsWith('u') ? 1 : 'any' });
    if (container.id === 'hall-advanced-fields') { const row = input.parentElement; row.classList.add('parameter-field'); const help = element('small', hallHelp[key], 'parameter-help'); help.id = 'hall-help-' + key; input.setAttribute('aria-describedby', help.id); row.append(help); }
  }
}
function renderAdvancedBinding() {
  const select = $('advanced-binding-source'); select.replaceChildren();
  for (const id of BT.slice().sort((a, b) => a - b)) { const option = element('option', keyName(id)); option.value = id; select.append(option); } select.value = advancedSource;
  const f = $('advanced-binding-fields'); f.replaceChildren(); const binding = device.draft.config.keyboard.bindings[advancedSource];
  const row = element('label', '手填 US 单字符'), text = element('input'); text.type = 'text'; text.maxLength = 2; text.setAttribute('aria-label', 'US单字符'); row.append(text); f.append(row);
  const set = element('button', '录入字符'); set.type = 'button'; set.onclick = () => { try { setBinding(advancedSource, fromCharacter(text.value)); notice('字符已录入网页草稿，应用后生效'); } catch (error) { notice(error.message, true); } }; f.append(set);
  if (binding.action.type === 'keyboard') { field(f, 'HID usage', binding.action, 'usage', 'number', { min: 4, max: 231 }); field(f, '修饰掩码 0～255', binding.action, 'modifiers', 'number', { min: 0, max: 255 }); }
  else f.append(element('p', '当前为鼠标动作或禁用；录入字符后可编辑 HID 字段。', 'hint'));
}
$('advanced-binding-source').onchange = () => { cancelListening(); advancedSource = Number($('advanced-binding-source').value); renderAdvancedBinding(); };
function renderEditors() {
  const c = device.draft.config;
  renderAdvancedBinding();
  const scan = $('scan-editor'); scan.replaceChildren(); field(scan, '重触发间隔 µs', c.keyboard, 'retrigger_gap_us', 'number', { min: 0, max: 100000 });
  for (const key of ['press_debounce_us', 'release_debounce_us']) field(scan, key === 'press_debounce_us' ? '按下消抖 µs' : '释放消抖 µs', c.scan, key, 'number', { min: 0, max: 20000 });
  const la = $('light-advanced-fields'); la.replaceChildren(); field(la, '周期 ms', c.lighting, 'period_ms', 'number', { min: 100, max: 60000 }); field(la, '动画反向', c.lighting, 'reverse', 'checkbox');
  const ha = $('hall-advanced-fields'), ka = $('knob-advanced-fields'), r = $('routing-fields');
  for (const container of [ha, ka, r]) container.replaceChildren();
  if (device.draft.motion) {
    const m = device.draft.motion; numbers(ha, m.hall, hallFields.filter(([key]) => key !== 'center_gain'));
    for (let id = 0; id < 2; id++) {
      // 灵敏度（倍率）在首页检查器里设置；PUSH 防误触不在网页显示，底层数据原样保留并随保存带回。
      const af = element('div', '', 'lf'); numbers(af, m.knobs[id], knobFields.filter(([key]) => key !== 'gain'));
      ka.append(h('div', {}, element('h4', '旋钮 ' + (id + 1) + ' · 进阶参数'), af));
    }
    field(r, '接管静止滞回 µs', m.routing, 'idle_us', 'number', { min: 0, max: 100000 }); field(r, '同时起动滑轨优先（工程起点）', m.routing, 'simultaneous_hall_first', 'checkbox');
  } else ka.append(element('p', '此固件未提供旋钮与滑轨的进阶参数。', 'pgnote'));
}

// ---------- 设备面板 ----------
const knobSVG = side => `<svg viewBox="0 0 160 160" aria-hidden="true"><defs><linearGradient id="rg${side}" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#CBEAF8"/><stop offset="1" stop-color="#FAD3E2"/></linearGradient><radialGradient id="cap${side}" cx="50%" cy="32%" r="75%"><stop offset="0" stop-color="#FFFFFF"/><stop offset="1" stop-color="#FFF0F6"/></radialGradient></defs><circle cx="80" cy="80" r="73" fill="none" stroke="#9CCBE6" stroke-width="3" stroke-dasharray="1 8" stroke-linecap="round"/><circle cx="80" cy="80" r="58" fill="none" stroke="url(#rg${side})" stroke-width="18"/><circle cx="80" cy="80" r="66.5" fill="none" stroke="#fff" stroke-opacity=".7" stroke-width="2"/><circle cx="80" cy="80" r="45" fill="url(#cap${side})" stroke="#fff" stroke-width="3"/><circle cx="80" cy="80" r="45" fill="none" stroke="#F6D5E2" stroke-width="1.5"/><g class="mn"><circle cx="80" cy="22" r="11" fill="#8FD3F0" stroke="#fff" stroke-width="4.5"/></g><g class="ma"><rect x="76" y="41" width="8" height="14" rx="4" fill="#F7A8C4"/></g><text x="80" y="87" text-anchor="middle" font-size="14" font-weight="800" fill="#1F3A4A" font-family="inherit">${side === 'L' ? '左旋钮' : '右旋钮'}</text></svg>`;
$('knobL').innerHTML = knobSVG('L'); $('knobR').innerHTML = knobSVG('R'); // 固定内置SVG，不含设备文本
const keyButtons = new Map();
function buildKeys() {
  const make = (id, wide) => {
    const button = h('button', { type: 'button', class: 'key' + (wide ? ' wide' : ''), 'data-id': id, onclick: () => select({ type: 'key', id }) }, element('b', keyName(id)), element('span', '—'), h('i', { class: 'kdot', hidden: true }));
    keyButtons.set(id, button); return button;
  };
  $('keysL').replaceChildren(...BT.slice(0, 4).map(id => make(id))); $('keysR').replaceChildren(...BT.slice(4, 8).map(id => make(id))); $('keysB').replaceChildren(...BT.slice(8).map(id => make(id, true)));
}
function updateKeys() {
  const c = device?.draft?.config, custom = !!c && c.lighting.per_key && c.lighting.enabled;
  for (const [id, button] of keyButtons) {
    const a = c?.keyboard.bindings[id].action, selected = selection.type === 'key' && selection.id === id;
    button.classList.toggle('sel', selected); button.classList.toggle('dis', a?.type === 'disabled'); button.setAttribute('aria-pressed', String(selected));
    button.setAttribute('aria-label', keyName(id) + '：' + (a ? actionLabel(a) : '未读取'));
    button.querySelector('span').textContent = a ? actionShort(a) : '—';
    const dot = button.querySelector('.kdot'); dot.hidden = !(custom && a.type !== 'disabled'); if (!dot.hidden) dot.style.background = rgbToHex(c.lighting.key_colors[id]);
  }
  for (const [name, el] of [['knobL', $('knobL')], ['knobR', $('knobR')], ['slider', $('slider')]]) { const on = selection.type === name; el.classList.toggle('sel', on); el.setAttribute('aria-pressed', String(on)); }
}
function renderPhysical() {
  // get_status只含状态位图（约2Hz，有运动遥测时约10Hz），不能据快照推断全部down/up事件或实时吞键。
  const mask = client?.alive && device?.ready ? device.status.physical_mask : 0;
  for (const [id, button] of keyButtons) { const down = !!(mask & (1 << id)); if (button.classList.contains('down') !== down) button.classList.toggle('down', down); }
}

// ---------- 灯光 ----------
function isLight(hex) { const n = parseInt(hex.slice(1), 16); return ((n >> 16) * 299 + ((n >> 8) & 255) * 587 + (n & 255) * 114) / 1000 > 235; }
function colorBalls(container, palette, { label, get, set }) {
  const slots = [0, 1, 2].map(i => h('button', { type: 'button', class: 'cs slot', onclick: () => { set(hexToRgb(palette.slots[i])); paint(); } }));
  // 每个球下面一个小按钮：固定后，调色盘选的新颜色不会顶掉这个球（设置只存浏览器本地）。
  const pins = [0, 1, 2].map(i => h('button', { type: 'button', class: 'pinbtn', onclick: () => { palette.pin(i); paint(); } }));
  const picker = h('input', { type: 'color', 'aria-label': label + '调色盘，选一个新颜色' });
  picker.addEventListener('input', () => { palette.set(picker.value); set(hexToRgb(picker.value)); paint(); });
  picker.addEventListener('change', () => palette.end());
  const wrap = h('label', { class: 'cs custom', title: '调色盘，选一个新颜色' }, picker);
  container.replaceChildren(...slots.map((b, i) => h('span', { class: 'slotwrap' }, b, pins[i])), h('span', { class: 'slotwrap' }, wrap));
  function paint() {
    const current = get(), now = current ? rgbToHex(current) : null, pinned = palette.pinned;
    slots.forEach((b, i) => { const hex = palette.slots[i]; b.style.background = hex; b.classList.toggle('white', isLight(hex)); b.setAttribute('aria-pressed', String(hex === now)); b.setAttribute('aria-label', label + ' ' + hex + (pinned[i] ? '（已固定）' : '')); });
    pins.forEach((b, i) => { b.textContent = pinned[i] ? '已固定' : '固定'; b.classList.toggle('on', pinned[i]); b.setAttribute('aria-pressed', String(pinned[i])); b.setAttribute('aria-label', label + '第' + (i + 1) + '个色球：' + (pinned[i] ? '取消固定' : '固定，不被新颜色顶掉')); });
    picker.value = (now ?? '#8FD3F0').toLowerCase();
  }
  paint();
  return { paint, setDisabled(disabled) { slots.forEach(b => { b.disabled = disabled; }); picker.disabled = disabled; wrap.classList.toggle('dis', disabled); } };
}
let globalBalls = null, inspectorBalls = null;
/** 灯光相关控件的可编辑状态；busy/连接状态变化时由 controls() 调用，不必重绘。 */
function lightLock() {
  const c = device?.draft?.config.lighting, edit = canEdit(), custom = !!c?.per_key, off = !c?.enabled;
  $('light-mode').disabled = !edit || off; $('light-brightness').disabled = !edit || off; $('light-enabled').disabled = !edit;
  globalBalls?.setDisabled(!edit || off || custom); $('global-hex').classList.toggle('dim', !edit || off || custom);
  inspectorBalls?.setDisabled(!edit || off || !custom); $('key-hex')?.classList.toggle('dim', !edit || off || !custom);
}
function renderLight() {
  const c = device?.draft?.config.lighting, edit = canEdit();
  $('light-enabled').checked = !!c?.enabled; $('light-mode').value = c ? (c.per_key ? 'custom' : c.mode) : 'static';
  $('light-brightness').value = c?.brightness_level ?? 0; $('brightness-text').textContent = c ? c.brightness_level : '—';
  globalBalls ??= colorBalls($('global-colors'), palettes.global, { label: '全局颜色', get: () => device?.draft?.config.lighting.global_color, set: rgb => { device.draft.config.lighting.global_color = rgb; mark(); } });
  globalBalls.paint(); $('global-hex').textContent = c ? rgbToHex(c.global_color) : '';
  lightLock();
  const note = $('light-note'); note.hidden = !(c?.per_key && c.mode !== 'static');
  if (!note.hidden) note.textContent = '这份配置在“自定义”之外还带有「' + effectName({ lighting: { ...c, per_key: false } }) + '」动画（旧配置，原值保留）；换一种灯效会清除它。';
}
$('light-enabled').onchange = () => { device.draft.config.lighting.enabled = $('light-enabled').checked; mark(); };
$('light-mode').onchange = () => {
  const l = device.draft.config.lighting, value = $('light-mode').value;
  // “自定义”＝ per_key：逐键颜色生效，全局颜色不可编辑；其他灯效 per_key=false。二者互斥，
  // 选“自定义”时动画恢复为常亮，不保留“某种动画 + 逐键颜色”的组合。
  if (value === 'custom') { l.per_key = true; l.mode = 'static'; } else { l.per_key = false; l.mode = value; }
  mark(); renderInspector({ keepFocus: true });
};
$('light-brightness').oninput = () => { device.draft.config.lighting.brightness_level = Number($('light-brightness').value); mark(); };

// ---------- 设备信号（旋钮 / 滑轨指示） ----------
const sim = { knob: [{ raw: 0, after: 0 }, { raw: 0, after: 0 }], hall: { p: .5 }, t0: performance.now(), last: performance.now() };
const shown = { knob: [{ raw: 0, after: 0 }, { raw: 0, after: 0 }], hall: { p: .5, out: .5 } };
let targetNow = null;
const signalMode = () => $('signal-source').value;
const manualEnabled = () => !!(client?.alive && client.port.simulated && signalMode() === 'manual');
function simulatedTargets(now) {
  const m = device.draft?.motion, mode = signalMode(), dt = (now - sim.last) / 1000; sim.last = now;
  if (mode === 'demo') {
    const t = (now - sim.t0) / 1000;
    sim.knob[0].raw = 1.2 * Math.sin(t * .7); sim.knob[1].raw = .9 * Math.sin(t * .45 + 1.6) + (t * .05) % 1; sim.hall.p = .5 + .38 * Math.sin(t * .55 + .8);
  } else if (dt < 0) sim.last = now;
  const knobs = [0, 1].map(i => { const config = m?.knobs[i]; const p = config ? projectPreview(sim.knob[i].raw, config) : null; return { valid: true, raw: sim.knob[i].raw, after: p ? p.scaledTurns : sim.knob[i].raw }; });
  const hall = m?.hall, gain = hall ? hall.center_gain : 1, out = clamp01(.5 + (sim.hall.p - .5) * gain);
  return { knobs, hall: { valid: true, p: sim.hall.p, out: hall?.invert ? 1 - out : out } };
}
function computeTargets(now) {
  if (!client?.alive || !device?.ready) return null;
  if (client.port.simulated) return simulatedTargets(now);
  if (!device.hello.capabilities.motion_telemetry) return null;
  const m = getMotion(); if (!m?.sample_ready) return null;
  return { knobs: [0, 1].map(i => { const k = m.knobs?.[i]; return k ? { valid: !!k.valid, raw: k.raw_turns, after: k.scaled_turns } : { valid: false }; }), hall: { valid: !!m.hall?.valid, p: m.hall?.p, out: m.hall?.output } };
}
const transforms = new WeakMap();
function setTransform(node, value) { if (transforms.get(node) !== value) { transforms.set(node, value); node.style.transform = value; } }
function paintKnob(button, state, target) {
  const on = !!target?.valid; button.classList.toggle('off', !on);
  if (!on) return;
  // 位置为累计圈数，不回绕；接近目标时逐帧缓动，转动平滑而不是一格一格地跳。
  for (const key of ['raw', 'after']) { const gap = target[key] - state[key]; state[key] = Math.abs(gap) > 4 ? target[key] : Math.abs(gap) < .0005 ? target[key] : state[key] + gap * .22; }
  setTransform(button.querySelector('.mn'), 'rotate(' + state.raw * 360 + 'deg)'); setTransform(button.querySelector('.ma'), 'rotate(' + state.after * 360 + 'deg)');
}
function paintSlider(target) {
  const slider = $('slider'), on = !!target?.valid; slider.classList.toggle('off', !on);
  if (!on) return;
  for (const [key, source] of [['p', target.p], ['out', target.out]]) { const gap = source - shown.hall[key]; shown.hall[key] = Math.abs(gap) < .0005 ? source : shown.hall[key] + gap * .22; }
  const thumb = $('track').querySelector('.thumb'), fill = $('track').querySelector('.fill'), pin = $('track').querySelector('.pin');
  thumb.style.left = clamp01(shown.hall.p) * 100 + '%'; fill.style.width = clamp01(shown.hall.out) * 100 + '%'; pin.style.left = clamp01(shown.hall.out) * 100 + '%';
}
function paintReadout() {
  const nat = $('ro-native'), aft = $('ro-after'); if (!nat) return;
  const t = targetNow, put = (node, text, none) => { if (node.textContent !== text) node.textContent = text; node.classList.toggle('none', none); };
  if (selection.type === 'slider') { const v = t?.hall; if (v?.valid) { put(nat, (v.p * 100).toFixed(1) + '%', false); put(aft, v.out.toFixed(3), false); } else { put(nat, '暂无有效位置', true); put(aft, '暂无有效位置', true); } }
  else { const v = t?.knobs[selection.type === 'knobL' ? 0 : 1]; if (v?.valid) { put(nat, v.raw.toFixed(3) + ' 圈', false); put(aft, v.after.toFixed(3) + ' 圈', false); } else { put(nat, '暂无有效位置', true); put(aft, '暂无有效位置', true); } }
}
function frame(now) {
  targetNow = computeTargets(now);
  paintKnob($('knobL'), shown.knob[0], targetNow?.knobs[0]); paintKnob($('knobR'), shown.knob[1], targetNow?.knobs[1]); paintSlider(targetNow?.hall);
  paintReadout(); requestAnimationFrame(frame);
}
// 手动测试：只在模拟演示 + “手动测试”下，滚轮转旋钮、拖动滑轨；真实设备只跟随硬件。
[['knobL', 0], ['knobR', 1]].forEach(([id, index]) => attachManualKnob($(id), { enabled: manualEnabled, rotate: delta => { sim.knob[index].raw += delta; } }));
{
  let drag = false; const move = event => { const r = $('track').getBoundingClientRect(); sim.hall.p = clamp01((event.clientX - r.left) / r.width); };
  $('slider').addEventListener('pointerdown', event => { if (!manualEnabled()) return; drag = true; move(event); $('slider').setPointerCapture?.(event.pointerId); });
  $('slider').addEventListener('pointermove', event => { if (drag && manualEnabled()) move(event); });
  $('slider').addEventListener('pointerup', () => { drag = false; }); $('slider').addEventListener('pointercancel', () => { drag = false; });
}
$('signal-source').onchange = () => { if (signalMode() === 'demo') sim.t0 = performance.now(); controls(); };

// ---------- 选中与检查器 ----------
function select(next) {
  cancelListening(false); renaming = false; selection = next; updateKeys(); renderProfiles(); renderInspector();
}
const lockNote = () => client?.alive ? null : h('p', { class: 'note warn', text: '连接设备后才能修改。' });
let heroTimer = null, heroLine = '', heroUntil = 0, mascotLines = null, lastLine = '';
function paintMascot() { const b = $('mascot-bubble'); if (!b) return; if (Date.now() < heroUntil) { b.textContent = heroLine; b.hidden = false; } else b.hidden = true; }
async function mascotClick() {
  mascotLines ??= await loadLines(async () => { const r = await fetch('assets/mascot-lines.txt', { cache: 'no-store' }); if (!r.ok) throw new Error('missing'); return r.text(); });
  heroLine = lastLine = pickLine(mascotLines, lastLine); heroUntil = Date.now() + 5000; paintMascot();
  clearTimeout(heroTimer); heroTimer = setTimeout(paintMascot, 5100);
}
function hero() {
  return h('div', { class: 'hero' }, h('div', { class: 'mbub', id: 'mascot-bubble', role: 'status', hidden: true }), h('button', { type: 'button', class: 'herobtn', id: 'hero-button', 'aria-label': '和看板娘说话', onclick: mascotClick }, h('img', { src: 'assets/fluxrail-v2-hero.png', alt: '', width: 800, height: 503, decoding: 'async' })));
}
function selectField(id, label, options, value, extra = {}) {
  return h('div', { class: 'fld' }, h('label', { for: id, text: label }), h('select', { id, class: 'sel-in big', ...extra }, options.map(([v, t]) => h('option', { value: v, text: t, ...(String(v) === String(value) ? { selected: true } : {}) }))));
}
function checkRow(id, label, checked, onchange, extra = {}) {
  return h('div', { class: 'row' }, h('label', { for: id, text: label }), h('input', { id, type: 'checkbox', checked, onchange: event => onchange(event.target.checked), ...extra }));
}
function readoutBox() {
  return h('div', { class: 'readout', role: 'group', 'aria-label': '设备实时读数' }, h('div', { class: 'two' },
    h('div', {}, h('div', { class: 'nm' }, h('i', { class: 'ldot' }), '原生'), h('div', { class: 'num none', id: 'ro-native', text: '暂无有效位置' })),
    h('div', {}, h('div', { class: 'nm' }, h('i', { class: 'lbar' }), '灵敏度后'), h('div', { class: 'num none', id: 'ro-after', text: '暂无有效位置' }))));
}
function gainField(object, key, id, label) {
  const holder = h('div', { class: 'fld' });
  if (object) mountGainControl(holder, { object, key, id, label, onChange: () => mark() });
  else { mountGainControl(holder, { object: { g: 1 }, key: 'g', id, label, onChange: () => {} }); for (const e of holder.querySelectorAll('input')) { e.disabled = true; if (e.type === 'number') { e.value = ''; e.placeholder = '待读取'; } } }
  for (const e of holder.querySelectorAll('input')) if (object) e.dataset.motion = '';
  return holder;
}
const modeActions = [['disabled', '禁用'], ['mouse:1', '鼠标 · 左键'], ['mouse:2', '鼠标 · 右键'], ['mouse:4', '鼠标 · 中键']];
const actionKey = a => a.type === 'disabled' ? 'disabled' : a.type === 'mouse_button' ? 'mouse:' + a.button : 'key:' + a.usage + ':' + (a.modifiers || 0);
function keyInspector() {
  const id = selection.id, binding = device?.draft?.config.keyboard.bindings[id], a = binding?.action, lighting = device?.draft?.config.lighting;
  const options = [...modeActions, ...keys.map(k => ['key:' + k.usage + ':0', '键盘 · ' + k.label])];
  if (a && !options.some(([v]) => v === actionKey(a))) options.push([actionKey(a), a.type === 'keyboard' ? '键盘 · ' + actionShort(a) : actionLabel(a)]);
  const custom = !!lighting?.per_key, colorsOn = !!device && custom && lighting.enabled;
  const balls = h('div', { class: 'sw-row', id: 'key-colors', role: 'group', 'aria-label': '按键灯光颜色' }); const hex = h('span', { class: 'hexv', id: 'key-hex' });
  const ui = colorBalls(balls, palettes.key, { label: keyName(id) + ' 按键颜色', get: () => device?.draft?.config.lighting.key_colors[id], set: rgb => { device.draft.config.lighting.key_colors[id] = rgb; hex.textContent = rgbToHex(rgb); mark(); } });
  hex.textContent = lighting ? rgbToHex(lighting.key_colors[id]) : '';
  const listen = h('button', { type: 'button', class: 'rbtn', id: 'key-listen', 'aria-pressed': 'false', 'data-edit': '', text: '监听按键' });
  listen.onclick = () => {
    cancelListening(false); listening = { id, setAction: next => setBinding(id, next), button: listen };
    listen.classList.add('listen'); listen.setAttribute('aria-pressed', 'true'); listen.textContent = '正在录入：按下目标键（Esc 取消）';
    say('info', '正在录入按键：按下目标键；Esc 取消，切换焦点退出。长按重复输入将被忽略。', { ms: 0, log: false });
  };
  listen.addEventListener('blur', () => cancelListening());
  const node = [
    h('div', {}, element('div', 'SELECTED', 'eyebrow'), element('h2', keyName(id) + ' · ' + (a ? actionShort(a) : '未读取'))),
    selectField('key-action', '按键动作', a ? options : [['', '—']], a ? actionKey(a) : '', { 'data-edit': '' }),
    listen,
    h('div', { class: 'fld' }, element('span', '按键灯光', 'lb'), balls, hex,
      element('div', custom ? '灯效为“自定义”，可以单独设置这个按键的颜色。' : '按键灯光和灯效互斥。灯效模式选“自定义”后才能设置。', 'note small')),
    lockNote(), hero()];
  inspectorBalls = ui; queueMicrotask(() => { lightLock(); const sel = $('key-action'); if (sel) sel.onchange = () => { const [type, v1, v2] = sel.value.split(':'); setBinding(id, type === 'disabled' ? { type: 'disabled' } : type === 'mouse' ? { type: 'mouse_button', button: Number(v1) } : { type: 'keyboard', usage: Number(v1), modifiers: Number(v2 || 0) }); }; });
  return node;
}
function knobInspector() {
  const index = selection.type === 'knobL' ? 0 : 1, knob = device?.draft?.motion?.knobs[index], disabledAttr = knob ? {} : { disabled: true };
  return [
    h('div', {}, element('div', 'SELECTED', 'eyebrow'), element('h2', knobName(index))), readoutBox(),
    checkRow('knob-enabled', '启用旋钮输入', !!knob?.enabled, v => { knob.enabled = v; mark(); }, { 'data-motion': '', ...disabledAttr }),
    checkRow('knob-invert', '反转输出方向', !!knob?.invert, v => { knob.invert = v; mark(); }, { 'data-motion': '', ...disabledAttr }),
    selectField('knob-axis', '输出轴', [[0, 'X'], [1, 'Y']], knob?.axis ?? index, { 'data-motion': '', ...disabledAttr }),
    gainField(knob, 'gain', 'knob-gain-' + (index + 1), '灵敏度'), lockNote(), hero()];
}
function sliderInspector() {
  const hall = device?.draft?.motion?.hall, disabledAttr = hall ? {} : { disabled: true };
  return [
    h('div', {}, element('div', 'SELECTED', 'eyebrow'), element('h2', '滑轨')), readoutBox(),
    checkRow('hall-enabled', '启用滑轨输入', !!hall?.enabled, v => { hall.enabled = v; mark(); }, { 'data-motion': '', ...disabledAttr }),
    checkRow('hall-invert', '反转输出方向', !!hall?.invert, v => { hall.invert = v; mark(); }, { 'data-motion': '', ...disabledAttr }),
    gainField(hall, 'center_gain', 'hall-gain', '中心灵敏度'),
    element('div', '灵敏度影响输出位置覆盖范围。', 'note small'), lockNote(), hero()];
}
function templateInspector() {
  const p = device?.profile, preset = p ? device.selected <= 4 : false, active = p && device.selected === device.meta?.active_profile;
  if (!p) return [h('div', {}, element('div', 'TEMPLATE', 'eyebrow'), element('h2', '模板')), lockNote(), hero()];
  const rename = renaming && !preset;
  const nameInput = h('input', { id: 'tpl-name', 'aria-label': '模板名称', value: p.name, autocomplete: 'off', spellcheck: 'false', placeholder: '1～10个字符' });
  nameInput.addEventListener('keydown', event => { if (event.key === 'Enter') { event.preventDefault(); $('tpl-name-ok').click(); } if (event.key === 'Escape') { renaming = false; renderInspector(); } });
  return [
    h('div', {}, element('div', preset ? 'PRESET TEMPLATE' : 'CUSTOM TEMPLATE', 'eyebrow'), element('h2', p.name)),
    element('div', (preset ? '预设模板，不能改名，也不能覆盖。' : '自定义模板，可以改名、更新或删除。') + (active ? ' 当前使用中。' : ''), 'note'),
    h('button', { type: 'button', class: 'rbtn main', id: 'tpl-activate', text: active ? '当前使用中' : '设为活动模板', onclick: () => runTemplateAction('activate', {}, '活动模板已切换；需要掉电保留时，请保存到设备。') }),
    h('button', { type: 'button', class: 'rbtn', id: 'tpl-save', text: preset ? '新建模板后保存' : '保存到当前模板', onclick: () => runTemplateAction('save', {}, preset ? '已新建并保存当前设置；活动模板未切换。' : '当前设置已保存到自定义模板，设备保存状态已读回。') }),
    ...(preset ? [h('button', { type: 'button', class: 'rbtn', id: 'tpl-restore', text: '恢复预置模板', onclick: () => runTemplateAction('restore', {}, '预设默认设置已恢复；需要掉电保留时，请保存到设备。') })]
      : [rename ? h('div', { class: 'nameedit' }, nameInput, h('button', { type: 'button', id: 'tpl-name-ok', text: '确定', onclick: () => runTemplateAction('rename', { name: nameInput.value }, '模板名称已修改并保存。') })) : h('button', { type: 'button', class: 'rbtn', id: 'tpl-rename', text: '修改名称', onclick: () => { renaming = true; renderInspector(); $('tpl-name')?.focus(); $('tpl-name')?.select(); } }),
        h('button', { type: 'button', class: 'rbtn danger', id: 'tpl-delete', text: '删除自定义模板', onclick: () => runTemplateAction('delete', {}, '自定义模板已删除，设备保存状态已读回。') }),
        ...(active ? [element('div', '使用中的模板不能删除，先切换到其他模板。', 'note small')] : [])]),
    element('div', '点选预设会恢复默认设置，预设不能覆盖或改名。要让设备使用该模板，请点“设为活动模板”，再应用并保存。', 'note small'), lockNote(), hero()];
}
function renderInspector({ keepFocus = false } = {}) {
  const focusId = keepFocus ? document.activeElement?.id : null, box = $('inspector');
  cancelListening(false);
  const content = selection.type === 'key' ? keyInspector() : selection.type === 'slider' ? sliderInspector() : selection.type === 'tpl' ? templateInspector() : knobInspector();
  box.replaceChildren(...content.filter(Boolean)); paintMascot(); controls();
  if (focusId) $(focusId)?.focus({ preventScroll: true });
}
function setBinding(id, action) {
  // 菜单、监听和实验室字符录入共享唯一binding；不创建第二套键位配置。
  device.draft.config.keyboard.bindings[id].action = action; mark(); renderInspector({ keepFocus: true }); renderAdvancedBinding();
}
document.addEventListener('keydown', event => {
  if (!listening) return; event.preventDefault(); event.stopPropagation(); if (event.key === 'Escape') { cancelListening(); return; }
  try { const next = fromEvent(event); if (next) { const target = listening; cancelListening(false); target.setAction(next); say('ok', '键位已录入网页草稿，应用后生效。', { log: false }); } } catch (e) { notice(e.message, true); }
}, true);
window.addEventListener('blur', () => cancelListening());

// ---------- 模板列表、横幅、设备信息 ----------
function renderProfiles() {
  const list = $('profile-list');
  if (!device?.profiles?.length) { list.replaceChildren(element('p', client?.alive ? '此固件仅提供运行配置' : '连接后读取模板', 'note small')); return; }
  const active = device.meta?.active_profile;
  list.replaceChildren(...device.profiles.map(p => {
    const sel = selection.type === 'tpl' && p.profile_id === device.selected;
    return h('button', { type: 'button', class: 'tpl' + (p.profile_id === active ? ' act' : '') + (sel ? ' sel' : ''), 'data-profile': p.profile_id, 'aria-pressed': String(p.profile_id === device.selected), disabled: !canEdit() || !device.hello.capabilities.profiles, onclick: () => selectProfile(p.profile_id) }, element('span', p.name), element('em', p.profile_id === active ? '使用中' : ''));
  }));
}
let chipKey = '';
function renderChips() {
  const c = !!device?.hello, active = device?.profiles?.find(p => p.profile_id === device.meta?.active_profile);
  const activeText = c ? '活动模板 · ' + (active ? active.name : '运行配置') + (device.meta && savedActive !== null && savedActive !== device.meta.active_profile ? '（未保存）' : '') : '活动模板：—';
  const parts = [c ? device.hello.board : '设备型号：连接后读取', c ? '固件 ' + device.hello.firmware_version : '固件：—', activeText], key = JSON.stringify([c, parts]);
  if (key === chipKey) return; chipKey = key;
  $('chips').replaceChildren(element('span', parts[0], 'chip' + (c ? '' : ' off')), element('span', parts[1], 'chip' + (c ? '' : ' off')), element('span', parts[2], 'chip p' + (c ? '' : ' off')));
}
function renderIdentity() {
  const hello = device?.hello, dl = $('identity'); dl.replaceChildren();
  const rows = hello ? [['设备型号', hello.product + ' / ' + hello.board], ['固件 / 协议', hello.firmware_version + ' / CDC v1'], ['唯一序号', hello.serial], ['当前 USB 模式', hello.usb_mode], ['启动模式（设备 RAM）', device.meta?.startup_usb_mode || '此固件未提供'], ['活动模板', device.profiles?.find(p => p.profile_id === device.meta?.active_profile)?.name ?? (device.meta?.active_profile ?? '模块1运行配置')], ['设置修订号', device.revision]]
    : ['设备型号', '固件 / 协议', '唯一序号', '当前 USB 模式', '启动模式（设备 RAM）', '活动模板', '设置修订号'].map(label => [label, '连接后查看']);
  for (const [label, value] of rows) dl.append(element('dt', label), element('dd', String(value)));
  $('identity-state').textContent = !hello ? '' : !client?.alive ? '上次读回' : '';
}

// ---------- 状态与读回 ----------
function renderStatus() {
  // 断开后保留上次配置供辨认，但清除实时按下态，避免旧快照冒充当前设备状态。
  const alive = !!client?.alive;
  $('connection-pill').classList.toggle('on', alive && !!device?.ready);
  $('connection-state').textContent = connecting ? '连接中' : alive ? (client.port.simulated ? '模拟会话' : device?.ready ? '已连接并完成读回' : '读回未完成') : device?.hello ? '已断开 / 结果待读回' : '未连接';
  if (!alive) { renderPhysical(); renderIdentity(); renderChips(); renderBubble(); controls(); return; }
  if (!device?.ready) { renderIdentity(); renderBubble(); controls(); return; }
  const e = device.latestEnvelope || device.envelope, s = device.status;
  if (!e.dirty) savedActive = device.meta?.active_profile ?? null; // 设备RAM与已保存一致时，活动模板即已保存的活动模板
  const text = JSON.stringify({ revision: client.revision, applied_revision: e.applied_revision, saved_revision: e.saved_revision, dirty: e.dirty, ...s }, null, 2);
  if ($('status-data').textContent !== text) $('status-data').textContent = text;
  renderIdentity(); renderChips(); renderPhysical(); renderBubble(); controls();
}
function render() {
  // 仅完整读回成功后重建编辑器。不会在周期轮询时覆盖用户草稿或抢走输入焦点。
  const hello = device.hello;
  $('demo-label').textContent = client.port.simulated ? '模拟演示 · 不连接硬件' : '';
  $('update-cap').textContent = hello.capabilities.bootloader_command ? '设备支持进入 UF2 下载模式。请手动复制文件，完成后重新连接并核对版本。' : '此固件只支持板上BOOT手动进入UF2。';
  $('calibration-state').textContent = device.meta ? `全局校准：${device.meta.calibration_valid ? '有效共享校准记录' : '无有效校准'} · generation ${device.meta.calibration_generation}` : '全局校准存储未提供';
  importCandidate = null; $('import-file').value = ''; $('import-preview').textContent = '导入schema1/2完整设备快照。全部模板、活动槽、启动身份和全局机械校准都会纳入候选。';
  device.latestEnvelope = null; if (selection.type === 'tpl' && !hello.capabilities.profiles) selection = { type: 'key', id: 2 };
  renderEditors(); resetLogBase(); renderProfiles(); updateKeys(); renderLight(); renderInspector(); renderStatus();
}
async function action(task, success, failureText = null, returnFocus = null) {
  if (busy) return; const origin = returnFocus ?? document.activeElement; cancelListening(false); if (flash?.kind === 'err') clearFlash(); busy = true; controls();
  try { await task(); if (success) notice(success); }
  catch (error) { say('err', failureText ?? `${error.code ? error.code + '：' : ''}${error.message}。配置/保存出错需读回核对结果。`); }
  finally { busy = false; renderStatus(); controls(); if ((returnFocus || document.activeElement === document.body) && origin?.isConnected && !origin.disabled) origin.focus({ preventScroll: true }); }
}
function schedulePoll() {
  // 自调度：前次请求完成后再等500ms（有运动遥测时100ms），忙时跳过，不积累轮询队列。
  // 状态记录最多600条，先丢最旧，并单独记录丢弃数；不阻塞实时USB输入。
  clearTimeout(pollTimer); pollTimer = setTimeout(async () => {
    if (!client?.alive) return;
    if (!busy) { const current = device; try { await current.poll(); await pollIntegration(current); if (current !== device) return; renderStatus(); if (recording) { if (records.length === 600) { records.shift(); dropped++; } records.push({ host_time: new Date().toISOString(), revision: client.revision, ...device.status }); $('record-state').textContent = `${records.length}/600条 · 已移除最早记录 ${dropped} 条；主机轮询快照`; controls(); } } catch (error) { say('err', '诊断读回失败：' + error.message, { log: false }); } }
    schedulePoll();
  }, device?.hello?.capabilities.motion_telemetry ? 100 : 500);
}
async function connect(simulated = false) {
  connecting = true; renderStatus();
  await action(async () => {
    assert(!client?.alive, '已有连接'); let port;
    if (simulated) { const { createDemoPort } = await import('./demo.js'); port = await createDemoPort(); }
    else { assert(window.isSecureContext && navigator.serial, '请在桌面Chrome/Edge的localhost或HTTPS打开；当前浏览器无Web Serial'); port = await navigator.serial.requestPort(); }
    client = new SerialClient(port, error => { clearTimeout(pollTimer); cancelListening(false); say('err', '连接失败'); renderStatus(); });
    device = new Device(client); resetModeDraft(device); savedActive = null; history.length = 0; renderHistory();
    importCandidate = null; $('import-file').value = ''; $('import-preview').textContent = '等待当前设备配置读回';
    $('status-data').textContent = '等待当前设备 get_status'; records = []; recording = false; dropped = 0; $('record').textContent = '开始状态记录'; $('record-state').textContent = '最多保留 600 条当前会话记录；超出后移除最早记录。';
    renderIdentity(); await client.open();
    try { await device.handshake(); connecting = false; render(); schedulePoll(); } catch (error) { await client.close(); throw error; }
  }, simulated ? '模拟会话已连接。全部结果仅用于界面/协议软件演示。' : '已完成能力握手和配置读回。编辑只形成网页草稿。', '连接失败');
  connecting = false; renderStatus();
}
async function disconnect() {
  await action(async () => {
    const parts = [device?.localDirty || hasIo4Draft() || hasModeDraft() ? '未应用的网页修改会丢失。' : '', client?.last?.dirty ? '设备仍在使用未保存的设置，断电或重启后会丢失。' : ''].filter(Boolean);
    if (parts.length && !await confirmChange({ title: '断开连接', description: parts.join('') + '断开不会保存。', confirmLabel: '仍然断开' })) return;
    clearTimeout(pollTimer); await client.close(); $('connection-state').textContent = '未连接';
    say('ok', '已断开；设备RAM变化未自动保存，重连会重新读回。');
  }, null);
}
$('connect').onclick = () => client?.alive ? disconnect() : connect(false);
$('demo').onclick = () => { location.hash = '#device-status'; connect(true); };
async function readSettings() {
  const current = device, observedRevision = client.revision, selected = current.selected;
  if (current.localDirty || hasIo4Draft() || hasModeDraft()) {
    const scopes = [current.localDirty ? '普通设置' : null, hasIo4Draft() ? 'IO4 映射' : null, hasModeDraft() ? '启动模式' : null].filter(Boolean).join('、');
    if (!await confirmChange({ title: '重新读取设置', description: '将舍弃未应用的网页修改：' + scopes + '。设备 RAM 中已应用但未保存的设置仍保留。', confirmLabel: '舍弃并重新读取' })) return false;
    // An uncertain prior operation may have invalidated ready; refresh is the recovery path.
    assert(device === current && client === current.client && client.alive && current.selected === selected, '确认期间连接或模板已变化');
    assert(client.revision === observedRevision, '确认期间修订已变化，请再次读取');
  }
  await current.refresh(); resetModeDraft(current); render(); notice('设置已重新读取；设备 RAM 中未保存设置仍保留。'); return true;
}
$('apply').onclick = () => action(async () => {
  const current = device, mode = getModeDraft(); say('info', '正在应用并保存，请等待设备确认。', { ms: 0, log: false });
  if (!current.hello.capabilities.flash_save) { validateTemplateDraft(); await current.apply(); render(); notice('设置已应用并读回；此固件不支持保存。'); return; }
  await applyAndSave(current, { startupMode: mode, validateDraft: validateTemplateDraft, isCurrent: () => device === current && !hasCalibrationJob(), hasIo4Draft });
  resetModeDraft(current); render(); notice('设置已应用并保存，当前模板和设备保存状态已读回。');
}, null);
function validateTemplateDraft() { assert([...document.querySelectorAll('fieldset input, #inspector input')].every(i => i.checkValidity()), '存在未修正的无效输入'); }
function runTemplateAction(kind, options = {}, success = '') {
  return action(async () => {
    const current = device; assert(!hasModeDraft() && !hasIo4Draft() && !hasCalibrationJob(), '请先处理启动模式、IO4 草稿或校准任务');
    const changed = await templateAction(current, kind, confirmChange, { ...options, validateDraft: validateTemplateDraft, isCurrent: () => current === device && !hasIo4Draft() && !hasModeDraft() && !hasCalibrationJob() });
    renaming = false;
    if (changed) { if (kind === 'select') selection = { type: 'tpl' }; render(); notice(success); } else renderProfiles();
  }, null, null, null);
}
function selectProfile(id) { return runTemplateAction('select', { id }, id <= 4 ? '已恢复并读取预设默认设置；活动模板未切换。' : '已读取自定义模板；活动模板未切换。'); }
$('create').onclick = () => runTemplateAction('create', {}, '已新建并保存当前设置；活动模板未切换。');
function download(name, text, type) { const url = URL.createObjectURL(new Blob([text], { type })); const a = element('a'); a.href = url; a.download = name; a.hidden = true; document.body.append(a); a.click(); a.remove(); setTimeout(() => URL.revokeObjectURL(url), 30000); }
$('export').onclick = () => action(async () => { device.checkWritable(); const bytes = await exportSnapshot(client); download('FluxRail-config.json', JSON.stringify(archive(bytes, device.hello), null, 2), 'application/json'); }, '已导出设备完整快照与身份；未执行Flash保存。');
$('import-file').onchange = () => action(async () => {
  // 只在本地检查并呈现拟变更，不会在选择文件时发送begin/commit或save。
  importCandidate = null; device.checkWritable(); const file = $('import-file').files[0]; if (!file) return; assert(file.size <= 20000, '配置文件过大');
  const parsed = parseArchive(await file.text()); importCandidate = { ...parsed, revision: device.revision }; const d = parsed.device;
  $('import-preview').textContent = `拟导入 ${d.profiles.length} 个模板，活动ID ${d.active_profile}，启动身份 ${d.startup_usb_mode}\n` + d.profiles.map(p => `${p.profile_id}: ${p.name}`).join('\n') + `\n全局校准 ${d.calibration.valid ? '有效记录' : '无效/空'}。文件设备 ${parsed.archive.serial || '未知'}；当前设备 ${device.hello.serial}。\n跨安装校准可能不适用。网页仅检查结构/字段/范围，固件commit执行完整曲线语义检查。将替换全部RAM配置，未自动保存；确认这些拟变更后点击导入。`;
}, '配置文件检查完成，请查看导入预览。');
$('import-commit').onclick = () => action(async () => { device.checkWritable(); assert(!device.localDirty && importCandidate, '请重新选择文件并预览'); try { await importSnapshot(client, importCandidate.bytes, importCandidate.revision); await device.refresh(); render(); } catch (error) { device.ready = false; throw error; } }, '固件校验并确认完整导入，RAM读回完成。需要掉电保留时再保存。');
$('record').onclick = () => { recording = !recording; $('record').textContent = recording ? '停止状态记录' : '开始状态记录'; };
$('csv').onclick = () => { const columns = ['host_time', 'revision', 'physical_mask', 'config_ready', 'applying', 'maintenance', 'save_result', 'valid_slots', 'protocol_requests', 'protocol_rejected', 'overlong_lines']; const csv = columns.join(',') + '\r\n' + records.map(r => columns.map(k => '"' + String(r[k] ?? '').replaceAll('"', '""') + '"').join(',')).join('\r\n'); download('FluxRail-status.csv', csv, 'text/csv;charset=utf-8'); };
$('uf2-file').onchange = () => action(async () => { const file = $('uf2-file').files[0]; await firmwareUpdate.check(file); notice(file ? 'UF2检查通过，请查看固件更新区的识别与版本比较结果。' : '已清空 UF2 文件选择。'); }, null, 'UF2检查未通过，请查看固件更新区的提示。');
// 浏览器尽力提示未保存离开；不自动save，不假定beforeunload一定可阻止关闭。
window.addEventListener('beforeunload', event => { if (device?.localDirty || hasIo4Draft() || hasModeDraft() || client?.last?.dirty) { event.preventDefault(); event.returnValue = ''; } });
mountIntegration({ getDevice: () => device, getClient: () => client, runAction: action, onControlsChange: controls, refresh: async () => { await device.refresh(); resetModeDraft(device); render(); }, confirm: confirmChange, notice, readSettings, hasModeDraft, calibrationBusy: hasCalibrationJob });
buildKeys(); renderIdentity(); renderChips(); renderProfiles(); renderHistory(); updateKeys(); renderLight(); renderInspector(); renderBubble(); controls();
mountModuleNavigation({ onNavigate: () => cancelListening(false) });
requestAnimationFrame(frame);
