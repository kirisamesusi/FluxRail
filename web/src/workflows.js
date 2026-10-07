import { assert } from './config.js';

/** Compose the existing ACK + readback operations; never retry an uncertain write. */
export async function applyAndSave(device, { startupMode = null, validateDraft = () => {}, isCurrent = () => true, hasIo4Draft = () => false } = {}) {
  device.checkWritable();
  assert(isCurrent() && !hasIo4Draft(), '请先处理 IO4 草稿');
  assert(device.hello.capabilities.flash_save, '此固件不支持保存');
  validateDraft();
  let applied = false;
  try {
    if (device.localDirty) { await device.apply(); applied = true; }
    assert(isCurrent() && !hasIo4Draft(), '设备或草稿已变化，请重新读回');
    if (startupMode && startupMode !== device.meta?.startup_usb_mode) {
      assert(device.hello.capabilities.usb_cold_switch, '此固件不支持启动模式');
      await device.mutate('set_startup_usb_mode', { mode: startupMode }); applied = true;
    }
  } catch (error) {
    throw new Error((applied ? '部分设置已应用，但未保存。' : '应用未完成，未开始保存。') + '请重新读取设置核对结果。' + error.message, { cause: error });
  }
  assert(isCurrent() && !hasIo4Draft(), '设备或草稿已变化，请重新读回');
  try { await device.mutate('save_config'); }
  catch (error) { throw new Error('设置已应用，但保存未确认；结果可能未知，请重新读取设置核对，勿直接重试。' + error.message, { cause: error }); }
}

export function confirmationGuard(device, isCurrent = () => true) {
  const revision = device.revision, selected = device.selected, client = device.client;
  return () => {
    assert(isCurrent() && device.client === client && revision === device.revision && selected === device.selected, '确认期间设备或模板已变化，请重新读回');
    device.checkWritable();
  };
}

export async function confirmedCommand(device, op, confirm, { isCurrent = () => true, hasDraft = () => false, calibrationBusy = () => false } = {}) {
  const check = () => {
    device.checkWritable();
    assert(!device.localDirty && !hasDraft(), '先应用并保存或重新读取网页草稿');
    assert(!calibrationBusy(), '校准进行中，请先结束或取消');
    assert(op !== 'reboot_to_saved_mode' || !device.client.last?.dirty, '设备设置尚未保存，请先应用并保存');
    assert(device.hello.capabilities[op === 'enter_bootloader' ? 'bootloader_command' : 'usb_cold_switch'], '此固件不支持此操作');
  };
  check(); const guard = confirmationGuard(device, isCurrent);
  const boot = op === 'enter_bootloader';
  if (!await confirm({ title: boot ? '进入 UF2 下载模式' : '重启设备', confirmLabel: boot ? '确认进入下载模式' : '确认重启', description: boot
    ? '请先保存需要保留的设置。确认后连接将断开，进入下载模式不代表更新完成。'
    : '重启后，未保存的修改会丢失，设备将使用已保存的设置。确定要重启吗？',
    ...(!boot ? { note: '点击“确认重启”后，设备将重启并断开网页连接。' } : {}),
    ...(boot ? { steps: ['等待电脑出现 RPI-RP2 磁盘。', '手动复制匹配此设备的 FluxRail UF2 到磁盘，Web Serial 不传送固件。', '完成后重新连接，核对固件版本并检查功能。'], fallback: '按住键盘上的 BOOT 接入 USB 后松开。' } : {}) })) return false;
  guard(); check();
  try {
    await device.client.request(op, { expected_revision: device.revision, ...(boot ? {} : { target: device.meta.startup_usb_mode }) }, 4000);
  } catch (error) {
    device.ready = false;
    throw error;
  }
  return true;
}
