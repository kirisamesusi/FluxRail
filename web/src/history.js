import { actionText } from './keyboard.js';
import { hallFields, knobFields, pushFields } from './config.js';
import { gainView } from './sensitivity.js';
import { rgbToHex } from './colors.js';
/** 把“网页草稿”与“设备读回”的差异整理成人能读懂的改动清单。
 * 只比较 config / motion：模板名称、IO4 映射和启动模式另有各自的草稿和文案。
 * 颜色（三通道数组）和按键动作按一个整体算一处修改，所以“N 处修改”与历史操作一一对应。
 */
const modeNames = { static: '常亮', breathing: '呼吸', color_cycle: '同步换色', rainbow_wave: '彩虹流动', press: '按下即亮' };
export const keyName = id => id < 10 ? 'BT' + (id + 1) : 'P' + (id - 9);
export const knobName = id => id === 0 ? '左旋钮' : '右旋钮';
export const effectName = config => config.lighting.per_key ? '自定义' : modeNames[config.lighting.mode] ?? config.lighting.mode;
function flatten(value, path, out) {
  if (Array.isArray(value) && value.length === 3 && value.every(Number.isFinite)) out.set(path, value);
  else if (Array.isArray(value)) value.forEach((item, i) => flatten(item, path + '.' + i, out));
  else if (value && typeof value === 'object') {
    if (path.endsWith('.action')) out.set(path, value);
    else for (const key of Object.keys(value)) flatten(value[key], path ? path + '.' + key : key, out);
  } else out.set(path, value);
  return out;
}
const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);
const fixedLabel = new Map([...hallFields.map(([key, label]) => ['hall.' + key, label]), ...knobFields.map(([key, label]) => ['knob.' + key, label]), ...pushFields.map(([key, label]) => ['push.' + key, label])]);
const sensitivity = (gain, type) => { const view = gainView(gain, type); return view.legacy ? '旧配置（原倍率 ' + gain + '）' : view.value.toFixed(1); };
const show = value => Array.isArray(value) ? rgbToHex(value) : typeof value === 'boolean' ? (value ? '开' : '关') : typeof value === 'number' ? String(Number(value.toFixed(6))) : String(value);
const onOff = (name, on, yes = '已启用', no = '已停用') => name + (on ? yes : no);
/** 返回 [{ key, text }]，同一概念的多个字段合成一条（如灯效 = mode + per_key）。 */
export function changeList(base, draft) {
  const before = flatten({ config: base.config, motion: base.motion }, '', new Map());
  const after = flatten({ config: draft.config, motion: draft.motion }, '', new Map());
  const result = new Map();
  const add = (key, text) => { if (!result.has(key)) result.set(key, text); };
  for (const path of new Set([...before.keys(), ...after.keys()])) {
    const from = before.get(path), to = after.get(path);
    if (same(from, to)) continue;
    const p = path.split('.');
    if (path.startsWith('config.keyboard.bindings.') && p[4] === 'action') { const id = Number(p[3]); add(path, keyName(id) + ' 按键动作：' + actionText(from) + ' → ' + actionText(to)); }
    else if (path === 'config.lighting.enabled') add(path, to ? '键灯已启用' : '键灯已关闭');
    else if (path === 'config.lighting.mode' || path === 'config.lighting.per_key') add('light.effect', '灯效模式：' + effectName(base.config) + ' → ' + effectName(draft.config));
    else if (path === 'config.lighting.brightness_level') add(path, '灯光亮度：' + from + ' → ' + to);
    else if (path === 'config.lighting.global_color') add(path, '全局颜色：' + show(from) + ' → ' + show(to));
    else if (path.startsWith('config.lighting.key_colors.')) add(path, keyName(Number(p[3])) + ' 按键颜色：' + show(from) + ' → ' + show(to));
    else if (path === 'config.lighting.period_ms') add(path, '灯光动画周期 ms：' + from + ' → ' + to);
    else if (path === 'config.lighting.reverse') add(path, onOff('灯光动画', to, '已反向', '已取消反向'));
    else if (path === 'config.keyboard.retrigger_gap_us') add(path, '重触发间隔 µs：' + from + ' → ' + to);
    else if (path === 'config.scan.press_debounce_us') add(path, '按下消抖 µs：' + from + ' → ' + to);
    else if (path === 'config.scan.release_debounce_us') add(path, '释放消抖 µs：' + from + ' → ' + to);
    else if (path === 'motion.hall.enabled') add(path, onOff('滑轨', to));
    else if (path === 'motion.hall.invert') add(path, onOff('滑轨', to, '已反转输出方向', '已取消反转'));
    else if (path === 'motion.hall.center_gain') add(path, '滑轨中心灵敏度：' + sensitivity(from, 'f32') + ' → ' + sensitivity(to, 'f32'));
    else if (path.startsWith('motion.hall.')) add(path, '滑轨 ' + (fixedLabel.get('hall.' + p[2]) ?? p[2]) + '：' + show(from) + ' → ' + show(to));
    else if (path.startsWith('motion.knobs.')) {
      const name = knobName(Number(p[2])), field = p[3];
      if (field === 'enabled') add(path, onOff(name, to));
      else if (field === 'invert') add(path, onOff(name, to, '已反转输出方向', '已取消反转'));
      else if (field === 'axis') add(path, name + ' 输出轴：' + (from ? 'Y' : 'X') + ' → ' + (to ? 'Y' : 'X'));
      else if (field === 'gain') add(path, name + '灵敏度：' + sensitivity(from, 'f64') + ' → ' + sensitivity(to, 'f64'));
      else if (field === 'push') add(path, name + ' PUSH 参数 ' + (fixedLabel.get('push.' + p[4]) ?? p[4]) + '：' + show(from) + ' → ' + show(to));
      else add(path, name + ' ' + (fixedLabel.get('knob.' + field) ?? field) + '：' + show(from) + ' → ' + show(to));
    }
    else if (path === 'motion.routing.idle_us') add(path, '接管静止滞回 µs：' + from + ' → ' + to);
    else if (path === 'motion.routing.simultaneous_hall_first') add(path, onOff('同时起动滑轨优先', to, '已开启', '已关闭'));
    else add(path, path + '：' + show(from) + ' → ' + show(to));
  }
  return [...result].map(([key, text]) => ({ key, text }));
}
