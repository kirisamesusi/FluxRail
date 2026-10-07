import { assert } from '../config.js';
import { validateUf2, assessFirmware, FIRMWARE_ID, FIRMWARE_BOARD } from './uf2.js';

/** 文件选择只进行本地检查；每次连接/断开都更新当前版本和比较结果。 */
export class FirmwareUpdate {
  constructor({ result, version, current, getConnection }) {
    Object.assign(this, { result, version, current, getConnection });
    this.candidate = null;
    this.expectedSerial = null;
    this.error = null;
    this.fileSummary = '';
    this.pendingText = '选择本项目正式FluxRail UF2，自动读取版本。';
  }
  async check(file) {
    // 先清除旧候选。无效文件/取消选择不能继续显示上个文件检查通过。
    this.candidate = null; this.expectedSerial = null; this.error = null; this.fileSummary = '';
    this.pendingText = file ? '正在读取UF2识别头与版本…' : '选择本项目正式FluxRail UF2，自动读取版本。';
    this.render();
    if (!file) return;
    try {
      assert(file.size <= 8 * 1024 * 1024, 'UF2文件过大');
      const checked = validateUf2(new Uint8Array(await file.arrayBuffer()));
      this.candidate = checked;
      this.fileSummary = file.name + '\nFluxRail固件识别通过 · 版本 ' + checked.firmware.version +
        '\n型号 ' + checked.firmware.board + '\n' + checked.blocks + '块 / ' + checked.payload_bytes + '字节';
      this.render();
    } catch (error) {
      this.error = '固件版本不对：请重新检查固件文件';
      this.render();
      throw error;
    }
  }
  render() {
    const { hello = null, simulated = false } = this.getConnection() ?? {};
    const identified = hello?.firmware_id === FIRMWARE_ID && hello?.firmware_board === FIRMWARE_BOARD && hello?.firmware_metadata_format === 1;
    this.current.textContent = hello ? (simulated ? '模拟固件版本：' : '当前固件版本：') + hello.firmware_version +
      (simulated ? '（仅模拟）' : identified ? '（FluxRail识别通过）' : hello.firmware_id === undefined ? '（旧固件未提供识别头）' : '（固件身份不匹配）') : '当前固件版本：未连接设备';
    if (this.error) { this.show(this.error, true); return; }
    if (!this.candidate) { this.show(this.pendingText, false); return; }
    if (hello && !simulated && !this.expectedSerial) this.expectedSerial = hello.serial;
    const assessment = assessFirmware(this.candidate.firmware, hello, { simulated, expectedSerial: this.expectedSerial });
    this.show(this.fileSummary + '\n' + assessment.message + ' 文件检查不会刷写设备。', ['older', 'wrong_device', 'different_device', 'unknown'].includes(assessment.status));
  }
  show(text, error) {
    this.version.textContent = '选择UF2后自动识别固件并比较版本。';
    this.version.classList.toggle('error', false);
    this.result.textContent = text;
    this.result.classList.toggle('error', error);
  }
}
