/** 看板娘台词：assets/mascot-lines.txt，一行一句，# 开头为注释，空行忽略。
 * 文件缺失或没有有效行时使用内置的几句；点击不会连续重复同一句。
 */
export const BUILTIN_LINES = ['改完记得点“应用并保存”哦。', '今天也辛苦啦。', '旋钮转一转，就知道灵敏度合不合适啦。', '喝口水再继续吧。', '断电之前别忘了保存哦。', '我在这里陪你调试。'];
export function parseLines(text) {
  return String(text ?? '').split(/\r?\n/).map(line => line.trim()).filter(line => line && line[0] !== '#');
}
export async function loadLines(fetchText) {
  try {
    const lines = parseLines(await fetchText());
    return lines.length ? lines : BUILTIN_LINES;
  } catch { return BUILTIN_LINES; }
}
export function pickLine(lines, last = '', random = Math.random) {
  if (!lines.length) return '';
  let pick = lines[Math.floor(random() * lines.length)];
  for (let tries = 0; lines.length > 1 && pick === last && tries < 8; tries++) pick = lines[Math.floor(random() * lines.length)];
  if (lines.length > 1 && pick === last) pick = lines[(lines.indexOf(pick) + 1) % lines.length];
  return pick;
}
