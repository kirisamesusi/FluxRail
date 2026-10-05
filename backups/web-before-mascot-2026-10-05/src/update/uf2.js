/** 本地检查UF2边界并从Flash payload读取FluxRail识别头和版本。没有上传通道。 */
import { assert } from '../config.js';
export const FIRMWARE_ID = 'FLUXRAIL-FW-V1';
export const FIRMWARE_BOARD = 'fluxrail-rp2040-4mib';
const magic = new Uint8Array(16);
magic.set(new TextEncoder().encode(FIRMWARE_ID));
const metadataSize = 88;

function semver(version) {
  assert(typeof version === 'string', '固件版本号无效');
  const match = /^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?$/.exec(version);
  assert(match && !(match[4]?.split('.').some(p => /^\d+$/.test(p) && p.length > 1 && p[0] === '0')), '固件版本号无效，需要 major.minor.patch 格式');
  return { core: match.slice(1, 4).map(BigInt), pre: match[4]?.split('.') ?? [] };
}
/** 返回文件版本相对当前版本的顺序，build metadata不参与优先级。 */
export function compareVersions(fileVersion, currentVersion) {
  const a = semver(fileVersion), b = semver(currentVersion);
  for (let i = 0; i < 3; i++) if (a.core[i] !== b.core[i]) return a.core[i] > b.core[i] ? 1 : -1;
  if (!a.pre.length || !b.pre.length) return a.pre.length === b.pre.length ? 0 : a.pre.length ? -1 : 1;
  for (let i = 0; i < Math.max(a.pre.length, b.pre.length); i++) {
    if (a.pre[i] === undefined || b.pre[i] === undefined) return a.pre[i] === b.pre[i] ? 0 : a.pre[i] === undefined ? -1 : 1;
    if (a.pre[i] === b.pre[i]) continue;
    const an = /^\d+$/.test(a.pre[i]), bn = /^\d+$/.test(b.pre[i]);
    if (an && bn) return BigInt(a.pre[i]) > BigInt(b.pre[i]) ? 1 : -1;
    if (an !== bn) return an ? -1 : 1;
    return a.pre[i] > b.pre[i] ? 1 : -1;
  }
  return 0;
}
function cstring(bytes, offset, size) {
  const field = bytes.subarray(offset, offset + size), end = field.indexOf(0);
  assert(end > 0 && field.subarray(0, end).every(b => b >= 32 && b <= 126) && field.subarray(end).every(b => b === 0), '固件识别头损坏');
  return new TextDecoder().decode(field.subarray(0, end));
}
function firmwareMetadata(ranges) {
  // 按Flash地址重排，允许合法乱序块；识别头跨块时只拼接连续地址。
  const groups = [];
  for (const r of ranges) {
    let group = groups.at(-1);
    if (!group || group.end !== r.start) { group = { start: r.start, end: r.start, parts: [] }; groups.push(group); }
    group.parts.push(r.bytes); group.end = r.end;
  }
  let found = null;
  for (const group of groups) {
    const bytes = new Uint8Array(group.end - group.start);
    let offset = 0;
    for (const part of group.parts) { bytes.set(part, offset); offset += part.length; }
    for (let i = 0; i <= bytes.length - magic.length; i++) {
      if (bytes[i] !== magic[0] || !magic.every((b, j) => bytes[i + j] === b)) continue;
      assert(!found, '固件不对：发现多个FluxRail识别头');
      assert(i + metadataSize <= bytes.length, '固件不对：FluxRail识别头不完整');
      const v = new DataView(bytes.buffer, i, metadataSize);
      assert(v.getUint32(16, true) === 1 && v.getUint32(84, true) === 0x46525746, '固件不对：识别头格式不支持或已损坏');
      const board = cstring(bytes, i + 20, 32), version = cstring(bytes, i + 52, 32);
      assert(board === FIRMWARE_BOARD, '固件不对：不是FluxRail RP2040 4MiB板型固件');
      semver(version);
      found = { id: FIRMWARE_ID, board, version, format: 1 };
    }
  }
  assert(found, '固件不对：未找到FluxRail识别头，请选择本项目新版正式UF2；旧版无识别头固件无法识别');
  return found;
}
export function validateUf2(bytes) {
  assert(bytes instanceof Uint8Array && bytes.length > 0 && bytes.length % 512 === 0 && bytes.length <= 8 * 1024 * 1024, 'UF2文件大小或块对齐无效');
  const v = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength), blocks = bytes.length / 512, seen = new Set(), ranges = [];
  for (let at = 0; at < bytes.length; at += 512) {
    const u = off => v.getUint32(at + off, true);
    assert(u(0) === 0x0a324655 && u(4) === 0x9e5d5157 && u(508) === 0x0ab16f30, 'UF2 magic无效');
    assert(u(8) === 0x2000 && u(28) === 0xe48bff56, '需要带RP2040 family ID的标准Flash UF2');
    const address = u(12), size = u(16), number = u(20);
    assert(size > 0 && size <= 476 && size % 4 === 0 && address % 4 === 0, 'UF2 payload或地址对齐无效');
    assert(address >= 0x10000000 && address + size <= 0x103fb000, 'UF2超出新板程序区或触及公共配置保留区');
    assert(u(24) === blocks && number < blocks && !seen.has(number), 'UF2块总数/序号不完整');
    seen.add(number); ranges.push({ start: address, end: address + size, bytes: bytes.subarray(at + 32, at + 32 + size) });
  }
  ranges.sort((a, b) => a.start - b.start);
  for (let i = 1; i < ranges.length; i++) assert(ranges[i].start >= ranges[i - 1].end, 'UF2地址重叠');
  return { blocks, payload_bytes: ranges.reduce((n, r) => n + r.end - r.start, 0), start: ranges[0].start, end: ranges.at(-1).end, firmware: firmwareMetadata(ranges) };
}
/** 选择UF2或重新握手后都重新比较，不把旧设备/模拟的版本当成当前版本。 */
export function assessFirmware(firmware, hello, { simulated = false, expectedSerial = null } = {}) {
  if (!hello) return { status: 'disconnected', message: '文件版本 ' + firmware.version + '。请连接设备读取当前固件版本后比较。' };
  if (simulated) return { status: 'simulation', message: '文件版本 ' + firmware.version + '。模拟会话不能用于比较实际设备版本，请连接真实设备。' };
  if (expectedSerial && hello.serial !== expectedSerial) return { status: 'different_device', message: '设备序号改变，不能确认原设备更新。请重新选择UF2，与当前设备比较。' };
  if (hello.product !== 'FluxRail' || hello.board !== 'FluxRail RP2040 4MiB' ||
      (hello.firmware_id !== undefined && hello.firmware_id !== firmware.id) ||
      (hello.firmware_board !== undefined && hello.firmware_board !== firmware.board) ||
      (hello.firmware_metadata_format !== undefined && hello.firmware_metadata_format !== firmware.format))
    return { status: 'wrong_device', message: '固件不对：文件与当前设备的固件身份或板型不匹配。' };
  let comparison;
  try { comparison = compareVersions(firmware.version, hello.firmware_version); }
  catch { return { status: 'unknown', message: '当前固件版本无法比较（' + String(hello.firmware_version) + '），文件版本 ' + firmware.version + '。' }; }
  const versions = '当前固件 ' + hello.firmware_version + '，文件版本 ' + firmware.version + '。';
  if (comparison === 0) return { status: 'same', message: versions + '版本相同，无需更新。' };
  if (comparison < 0) return { status: 'older', message: versions + '所选固件版本过低，不建议降级，请选择更新版本。' };
  return { status: 'newer', message: versions + '当前固件版本较低，可升级到所选版本。' };
}
