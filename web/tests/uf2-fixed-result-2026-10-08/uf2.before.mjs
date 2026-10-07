import test from 'node:test';
import assert from 'node:assert/strict';
import { validateUf2, compareVersions, assessFirmware, FIRMWARE_ID, FIRMWARE_BOARD } from '../src/update/uf2.js';
import { FirmwareUpdate } from '../src/update/controller.js';

function metadata(version = '0.10.0', board = FIRMWARE_BOARD) {
  const b = new Uint8Array(88), v = new DataView(b.buffer), encoder = new TextEncoder();
  b.set(encoder.encode(FIRMWARE_ID)); v.setUint32(16, 1, true);
  b.set(encoder.encode(board), 20); b.set(encoder.encode(version), 52); v.setUint32(84, 0x46525746, true);
  return b;
}
function uf2(payload, { gap = 0 } = {}) {
  const count = Math.ceil(payload.length / 256), b = new Uint8Array(count * 512), v = new DataView(b.buffer);
  for (let n = 0; n < count; n++) {
    for (const [at, value] of [[0, 0x0a324655], [4, 0x9e5d5157], [8, 0x2000], [12, 0x10000000 + n * 256 + (n ? gap : 0)], [16, 256], [20, n], [24, count], [28, 0xe48bff56], [508, 0x0ab16f30]]) v.setUint32(n * 512 + at, value, true);
    b.set(payload.subarray(n * 256, (n + 1) * 256), n * 512 + 32);
  }
  return b;
}
function image(version = '0.10.0', offset = 48) { const b = new Uint8Array(512); b.set(metadata(version), offset); return uf2(b); }
const firmware = { id: FIRMWARE_ID, board: FIRMWARE_BOARD, version: '0.10.0', format: 1 };
const hello = { product: 'FluxRail', board: 'FluxRail RP2040 4MiB', serial: 'device-a', firmware_version: '0.9.0-integrated-software', firmware_id: FIRMWARE_ID, firmware_board: FIRMWARE_BOARD, firmware_metadata_format: 1 };

test('读取真实payload识别头，支持跨块、乱序块、非零byteOffset', () => {
  const b = image('0.10.0', 248), reverse = new Uint8Array(1024);
  reverse.set(b.subarray(512), 0); reverse.set(b.subarray(0, 512), 512);
  const padded = new Uint8Array(1040); padded.set(reverse, 8);
  assert.deepEqual(validateUf2(padded.subarray(8, 1032)).firmware, firmware);
});
test('其他固件、旧文件、错误板型、截断/损坏/重复识别头拒绝', () => {
  assert.throws(() => validateUf2(uf2(new Uint8Array(256))), /未找到FluxRail/);
  for (const [offset, value] of [[16, 2], [84, 0], [52, 0]]) {
    const b = metadata(); b[offset] = value; assert.throws(() => validateUf2(uf2(b)));
  }
  assert.throws(() => validateUf2(uf2(metadata('0.10.0', 'other-board'))), /板型/);
  assert.throws(() => validateUf2(uf2(metadata('invalid'))), /版本号/);
  const truncated = new Uint8Array(256); truncated.set(metadata().subarray(0, 16), 240);
  assert.throws(() => validateUf2(uf2(truncated)), /不完整/);
  const double = new Uint8Array(256); double.set(metadata(), 0); double.set(metadata(), 96);
  assert.throws(() => validateUf2(uf2(double)), /多个/);
});
test('文件头/UF2 padding的伪识别头不能替代Flash payload，跨空洞拒绝', () => {
  const padding = uf2(new Uint8Array(256)); padding.set(metadata(), 288);
  assert.throws(() => validateUf2(padding), /未找到FluxRail/);
  const payload = new Uint8Array(512); payload.set(metadata(), 248);
  assert.throws(() => validateUf2(uf2(payload, { gap: 4 })), /未找到FluxRail|不完整/);
});
test('UF2损坏、地址重叠、块号重复、保留区、大小错误拒绝', () => {
  for (const [offset, value] of [[0, 0], [8, 1], [28, 0], [12, 0x103fb000], [16, 480], [24, 3], [512 + 12, 0x10000000], [512 + 20, 0]]) {
    const b = image(); new DataView(b.buffer).setUint32(offset, value, true); assert.throws(() => validateUf2(b));
  }
  assert.throws(() => validateUf2(new Uint8Array(0)));
  assert.throws(() => validateUf2(image().subarray(0, 513)));
});
test('版本按数值和SemVer预发布顺序比较，兼容旧集成版本', () => {
  for (const [a, b] of [['0.10.0', '0.9.0-integrated-software'], ['1.10.0', '1.9.0'], ['1.0.0', '1.0.0-rc.1'], ['1.0.0-rc.10', '1.0.0-rc.2'], ['1.0.0-beta', '1.0.0-2'], ['1.0.0-rc.1', '1.0.0-rc']]) {
    assert.equal(compareVersions(a, b), 1); assert.equal(compareVersions(b, a), -1);
  }
  assert.equal(compareVersions('1.0.0+one', '1.0.0+two'), 0);
  assert.equal(compareVersions('0.10.0', '0.10.0'), 0);
  for (const v of ['1.0', '01.0.0', '1.0.0-01', '1.0.0-', 'x']) assert.throws(() => compareVersions(v, '1.0.0'));
});
test('版本相同/可升级/低版本，以及未知版本、模拟、设备变更分别提示', () => {
  const check = (patch = {}, options = {}) => assessFirmware(firmware, { ...hello, ...patch }, options);
  assert.equal(check().status, 'newer');
  assert.equal(check({ firmware_version: '0.10.0' }).status, 'same');
  assert.equal(check({ firmware_version: '1.0.0' }).status, 'older');
  assert.match(check({ firmware_version: '1.0.0' }).message, /版本过低/);
  assert.equal(check({ firmware_version: 'unknown' }).status, 'unknown');
  assert.equal(check({}, { simulated: true }).status, 'simulation');
  assert.equal(check({}, { expectedSerial: 'device-b' }).status, 'different_device');
  for (const patch of [{ firmware_id: 'other' }, { firmware_board: 'other' }, { firmware_metadata_format: 2 }, { board: 'other' }]) assert.equal(check(patch).status, 'wrong_device');
  assert.equal(assessFirmware(firmware, null).status, 'disconnected');
  const legacy = { ...hello }; delete legacy.firmware_id; delete legacy.firmware_board; delete legacy.firmware_metadata_format;
  assert.equal(assessFirmware(firmware, legacy).status, 'newer');
});
function fixture() {
  let connection = null;
  const node = () => ({ textContent: '', classList: { toggle(name, enabled) { this[name] = enabled; } } });
  const result = node(), version = node(), current = node();
  const controller = new FirmwareUpdate({ result, version, current, getConnection: () => connection });
  return { controller, result, version, current, connect: c => { connection = c; controller.render(); } };
}
const file = (b = image()) => ({ name: 'FluxRail.uf2', size: b.length, async arrayBuffer() { return b.buffer; } });
test('先选择文件再连接、重连同版及断线清除当前版本，不发设备请求', async () => {
  // 以当前Web为准：result保留文件详情，version显示当前会话的比较提示。
  const f = fixture(); f.controller.render();
  assert.equal(f.version.textContent, '选择UF2后自动识别固件并比较版本。');
  await f.controller.check(file()); assert.match(f.version.textContent, /请连接设备/);
  const summary = f.result.textContent;
  assert.match(summary, /FluxRail\.uf2[\s\S]*识别通过[\s\S]*0\.10\.0[\s\S]*板型[\s\S]*2块 \/ 512字节/);
  f.connect({ hello }); assert.match(f.current.textContent, /0.9.0/); assert.match(f.version.textContent, /可升级/);
  assert.equal(f.result.textContent, summary);
  f.connect(null); assert.match(f.current.textContent, /未连接设备/); assert.match(f.version.textContent, /请连接设备/);
  assert.equal(f.result.textContent, summary);
  f.connect({ hello: { ...hello, firmware_version: '0.10.0' } }); assert.match(f.version.textContent, /版本相同/);
  assert.equal(f.result.textContent, summary);
  f.connect({ hello: { ...hello, serial: 'device-b' } }); assert.match(f.version.textContent, /设备序号改变/);
  assert.equal(f.version.classList.error, true); assert.equal(f.result.textContent, summary);
  await f.controller.check(file()); assert.match(f.version.textContent, /可升级/);
  assert.equal(f.version.classList.error, false); assert.equal(f.result.textContent, summary);
  assert.match(f.version.textContent, /文件检查不会刷写设备/);
});
test('无效/过大文件和取消选择会清空旧候选；模拟不参与硬件版本比较', async () => {
  const f = fixture(); f.connect({ hello, simulated: true }); await f.controller.check(file());
  assert.match(f.current.textContent, /模拟固件版本[\s\S]*仅模拟/);
  assert.match(f.version.textContent, /模拟会话[\s\S]*请连接真实设备/);
  assert.doesNotMatch(f.version.textContent, /可升级|版本相同/);
  assert.match(f.result.textContent, /FluxRail\.uf2[\s\S]*识别通过[\s\S]*板型/);
  await assert.rejects(f.controller.check(file(uf2(new Uint8Array(256))))); assert.equal(f.controller.candidate, null);
  assert.equal(f.controller.expectedSerial, null);
  assert.match(f.result.textContent, /固件不对/); assert.doesNotMatch(f.result.textContent, /识别通过/);
  assert.equal(f.version.textContent, f.result.textContent); assert.equal(f.version.classList.error, true);
  f.connect(null); assert.match(f.current.textContent, /未连接设备/);
  assert.equal(f.version.textContent, f.result.textContent); assert.equal(f.version.classList.error, true);
  await assert.rejects(f.controller.check({ size: 8 * 1024 * 1024 + 1 })); assert.match(f.result.textContent, /文件过大/);
  assert.equal(f.controller.candidate, null); assert.equal(f.version.classList.error, true);
  assert.equal(f.version.textContent, f.result.textContent);
  await f.controller.check(null); assert.equal(f.controller.candidate, null); assert.equal(f.controller.error, null);
  assert.equal(f.version.classList.error, false);
  assert.match(f.result.textContent, /选择本项目正式FluxRail UF2/);
  assert.equal(f.version.textContent, '选择UF2后自动识别固件并比较版本。');
});
