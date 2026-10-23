import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { createPalette, rgbToHex, hexToRgb, DEFAULT_SLOTS } from '../src/colors.js';
import { parseLines, loadLines, pickLine, BUILTIN_LINES } from '../src/mascot.js';
import { changeList, keyName } from '../src/history.js';
import { asciiJson } from '../src/connection/client.js';
import { validName, validStoredName, utf8Length } from '../src/config.js';

const memory = () => { const m = new Map(); return { getItem: k => m.get(k) ?? null, setItem: (k, v) => m.set(k, v) }; };

test('色球：默认三色，新颜色依次顶掉三个球，一次拖动只占一个球，刷新后保留', () => {
  const store = memory(), p = createPalette('t', store);
  assert.deepEqual(p.slots, DEFAULT_SLOTS);
  p.set('#111111'); p.set('#222222'); p.end();
  assert.deepEqual(p.slots, ['#222222', DEFAULT_SLOTS[1], DEFAULT_SLOTS[2]]); assert.equal(p.next, 1);
  p.set('#333333'); p.end(); p.set('#444444'); p.end(); p.set('#555555'); p.end();
  assert.deepEqual(p.slots, ['#555555', '#333333', '#444444']);
  assert.deepEqual(createPalette('t', store).slots, p.slots);
  assert.deepEqual(createPalette('other', store).slots, DEFAULT_SLOTS);
  assert.throws(() => p.set('red'));
});
test('色球：存储损坏或不可用时回到默认三色且不抛错', () => {
  assert.deepEqual(createPalette('x', { getItem: () => '{bad', setItem() {} }).slots, DEFAULT_SLOTS);
  const broken = { getItem() { throw new Error('denied'); }, setItem() { throw new Error('denied'); } };
  const p = createPalette('x', broken); p.set('#123456'); p.end(); assert.equal(p.slots[0], '#123456');
  assert.deepEqual(createPalette('y', null).slots, DEFAULT_SLOTS);
  assert.equal(rgbToHex([143, 211, 240]), '#8FD3F0'); assert.deepEqual(hexToRgb('#8FD3F0'), [143, 211, 240]);
});
test('看板娘台词：忽略注释和空行，缺文件用内置，不连续重复', async () => {
  assert.deepEqual(parseLines('# c\n\n 你好 \r\n再见\n'), ['你好', '再见']);
  assert.deepEqual(await loadLines(async () => { throw new Error('x'); }), BUILTIN_LINES);
  assert.deepEqual(await loadLines(async () => '# only comment'), BUILTIN_LINES);
  const lines = ['a', 'b', 'c']; for (let i = 0; i < 30; i++) assert.notEqual(pickLine(lines, 'b', () => 1 / 3), 'b');
  assert.equal(pickLine(['only'], 'only'), 'only');
  const shipped = parseLines(await readFile(new URL('../assets/mascot-lines.txt', import.meta.url), 'utf8')); assert(shipped.length >= 3);
});
test('改动清单：一处修改对应一条，颜色/动作整体计数，自定义灯效与按键名', () => {
  const mk = () => ({ config: { keyboard: { bindings: Array.from({ length: 12 }, () => ({ action: { type: 'keyboard', usage: 4, modifiers: 0 } })), retrigger_gap_us: 0 }, scan: { press_debounce_us: 1, release_debounce_us: 1 }, lighting: { enabled: true, mode: 'static', per_key: false, brightness_level: 5, global_color: [1, 2, 3], key_colors: Array.from({ length: 12 }, () => [0, 0, 0]), period_ms: 1000, reverse: false } }, motion: null });
  const a = mk(), b = mk();
  assert.equal(changeList(a, b).length, 0);
  b.config.lighting.global_color = [9, 9, 9]; b.config.lighting.key_colors[2] = [255, 0, 0]; b.config.keyboard.bindings[0].action = { type: 'disabled' }; b.config.lighting.per_key = true;
  const list = changeList(a, b); assert.equal(list.length, 4);
  assert(list.some(x => x.text.includes('灯效模式') && x.text.includes('自定义')));
  assert(list.some(x => x.text.startsWith('BT3 按键颜色')));
  assert.equal(keyName(0), 'BT1'); assert.equal(keyName(10), 'P1');
});
test('协议线路保持纯ASCII：中文名以\\uXXXX转义，往返无损', () => {
  const line = asciiJson(JSON.stringify({ op: 'create_profile', payload: { name: '中文模板😀' } }));
  assert(!/[^\x00-\x7f]/.test(line)); assert.match(line, /\\u4e2d\\u6587/);
  assert.equal(JSON.parse(line).payload.name, '中文模板😀');
});
test('模板名：1～10个字符、UTF-8不超过31字节；旧ASCII长名仍可读回', () => {
  validName('测试'); assert.equal(utf8Length('一二三四五六七八九十'), 30);
  assert.throws(() => validName('')); assert.throws(() => validName('abcdefghijk'));
  validStoredName('a'.repeat(31)); assert.throws(() => validStoredName('a'.repeat(32)));
  assert.throws(() => validStoredName('一二三四五六七八九十一'));
});
test('色球固定：被固定的球不会被顶掉，状态随浏览器保存；全部固定时新颜色不进球', () => {
  const store = memory(), p = createPalette('pin', store);
  p.pin(2); assert.deepEqual(p.pinned, [false, false, true]);
  for (const hex of ['#111111', '#222222', '#333333']) { p.set(hex); p.end(); }
  assert.deepEqual(p.slots, ['#333333', '#222222', DEFAULT_SLOTS[2]]);
  p.pin(0); p.set('#444444'); p.end(); assert.deepEqual(p.slots, ['#333333', '#444444', DEFAULT_SLOTS[2]]);
  assert.deepEqual(createPalette('pin', store).pinned, [true, false, true]);
  p.pin(1); assert.equal(p.set('#555555'), -1); p.end(); assert.deepEqual(p.slots, ['#333333', '#444444', DEFAULT_SLOTS[2]]);
  p.pin(0, false); assert.equal(p.pin(0), true); assert.throws(() => p.pin(3));
  assert.deepEqual(createPalette('old', { getItem: () => JSON.stringify({ slots: DEFAULT_SLOTS, next: 0 }), setItem() {} }).pinned, [false, false, false]);
});
