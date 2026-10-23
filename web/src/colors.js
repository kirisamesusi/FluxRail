/** 色球：三个可复用颜色 + 一个调色盘。全局颜色与按键颜色各一套，互不影响。
 * 三个球的内容只是网页本地的使用习惯，存于浏览器 localStorage（读写都包 try/catch，
 * 隐私窗口或被禁用时退回内存），不写入设备，也不进模板或配置快照。
 * 从调色盘选新颜色时，依次顶掉第一个、第二个、第三个球，然后回到第一个；
 * 同一次拖动选色（input 事件序列）只占一个球，change 时才前进。
 */
export const DEFAULT_SLOTS = ['#8FD3F0', '#F7A8C4', '#FFFFFF'];
const HEX = /^#[0-9a-fA-F]{6}$/;
export const rgbToHex = color => '#' + color.map(v => v.toString(16).padStart(2, '0')).join('').toUpperCase();
export const hexToRgb = hex => [1, 3, 5].map(i => parseInt(hex.slice(i, i + 2), 16));
function defaultStorage() { try { return globalThis.localStorage ?? null; } catch { return null; } }
export function createPalette(name, storage = defaultStorage()) {
  const key = 'fluxrail.palette.' + name;
  let slots = [...DEFAULT_SLOTS], next = 0, session = null, pinned = [false, false, false];
  try {
    const saved = JSON.parse(storage?.getItem(key) ?? 'null');
    if (saved && Array.isArray(saved.slots) && saved.slots.length === 3 && saved.slots.every(v => typeof v === 'string' && HEX.test(v)) && [0, 1, 2].includes(saved.next)) {
      slots = saved.slots.map(v => v.toUpperCase()); next = saved.next;
      if (Array.isArray(saved.pinned) && saved.pinned.length === 3 && saved.pinned.every(v => typeof v === 'boolean')) pinned = [...saved.pinned];
    }
  } catch { /* 存储不可用或内容损坏：使用默认三色 */ }
  function save() { try { storage?.setItem(key, JSON.stringify({ slots, next, pinned })); } catch { /* 无法保存时仍可使用本次会话 */ } }
  return {
    get slots() { return [...slots]; },
    get next() { return next; },
    get pinned() { return [...pinned]; },
    /** 固定/取消固定某个球：被固定的球不会被新颜色顶掉。 */
    pin(index, value = !pinned[index]) { if (![0, 1, 2].includes(index)) throw new Error('色球序号应为 0～2'); pinned[index] = !!value; save(); return pinned[index]; },
    /** 选色过程中的一次预览：同一次拖动始终写入同一个球。返回被写入的球序号。 */
    set(hex) {
      assertHex(hex);
      if (session === null) {
        // 从“下一个球”开始找第一个没被固定的球；三个都固定时新颜色只用于当前选择，不进球。
        for (let i = 0; i < 3; i++) { const c = (next + i) % 3; if (!pinned[c]) { session = c; break; } }
        if (session === null) return -1;
      }
      slots[session] = hex.toUpperCase();
      return session;
    },
    /** 选色结束（change）：下一次新颜色顶掉下一个球。 */
    end() {
      if (session === null) return;
      next = (session + 1) % 3; session = null; save();
    }
  };
}
function assertHex(hex) { if (typeof hex !== 'string' || !HEX.test(hex)) throw new Error('颜色格式应为 #RRGGBB'); }
