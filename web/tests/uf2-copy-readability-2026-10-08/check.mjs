import assert from 'node:assert/strict';
import { chromium } from 'file:///C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright/index.mjs';
import { access, writeFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
const out = new URL('./', import.meta.url);
try { await access(new URL('browser-check.json', out)); throw Error('Evidence exists'); } catch (error) { if (error.code !== 'ENOENT') throw error; }
function uf2(address = 0x10000000) {
  const bytes = new Uint8Array(512), view = new DataView(bytes.buffer), encoder = new TextEncoder();
  for (const [offset, value] of [[0, 0x0a324655], [4, 0x9e5d5157], [8, 0x2000], [12, address], [16, 256], [20, 0], [24, 1], [28, 0xe48bff56], [508, 0x0ab16f30]]) view.setUint32(offset, value, true);
  bytes.set(encoder.encode('FLUXRAIL-FW-V1'), 32); view.setUint32(48, 1, true);
  bytes.set(encoder.encode('fluxrail-rp2040-4mib'), 52); bytes.set(encoder.encode('0.10.2'), 84); view.setUint32(116, 0x46525746, true);
  return Buffer.from(bytes);
}
const browser = await chromium.launch({ channel: 'msedge', headless: true });
const page = await browser.newPage(), errors = [], report = { date_local: '2026-10-08', hardware_tested: false, checks: [] };
page.on('pageerror', error => errors.push(String(error)));
try {
  for (const width of [1274, 390, 320]) {
    await page.setViewportSize({ width, height: 844 }); await page.goto('http://localhost:4174/#update');
    const hint = page.locator('#version-result'), result = page.locator('#uf2-result'), file = page.locator('#uf2-file');
    const unchanged = async () => { assert.equal(await hint.textContent(), '选择UF2后自动识别固件并比较版本。'); assert.equal(await hint.evaluate(node => node.classList.contains('error')), false); };
    await unchanged();
    await file.setInputFiles({ name: 'invalid-test-fixture.uf2', mimeType: 'application/octet-stream', buffer: uf2(0x103fb000) });
    await result.getByText('固件版本不对：请重新检查固件文件', { exact: true }).waitFor(); await unchanged();
    assert.match(await page.locator('#notice').textContent(), /UF2检查未通过/);
    assert.equal((await result.textContent()).match(/固件版本不对/g).length, 1);
    await page.locator('#update').screenshot({ path: fileURLToPath(new URL('invalid-' + width + '.png', out)) });
    await file.setInputFiles({ name: 'valid-test-fixture.uf2', mimeType: 'application/octet-stream', buffer: uf2() });
    await result.getByText(/识别通过/).waitFor(); await unchanged();
    assert.match(await page.locator('#notice').textContent(), /UF2检查通过/);
    assert.equal(await page.locator('#notice').evaluate(node => node.classList.contains('error')), false);
    for (const node of [result, page.locator('#notice')]) {
      const style = await node.evaluate(el => ({ size: getComputedStyle(el).fontSize, weight: getComputedStyle(el).fontWeight }));
      assert.equal(style.size, '14px'); assert.equal(style.weight, '600');
    }
    assert.match(await page.locator('#update-cap').textContent(), /固件与型号/);
    assert.match(await result.textContent(), /0\.10\.2[\s\S]*型号[\s\S]*请连接设备/);
    await page.locator('#update').screenshot({ path: fileURLToPath(new URL('valid-' + width + '.png', out)) });
    await file.setInputFiles([]); await result.getByText(/选择本项目/).waitFor(); await unchanged();
    assert.equal(await page.locator('#notice').textContent(), '已清空 UF2 文件选择。');
    await page.evaluate(async () => {
      const { confirmedCommand } = await import('/src/workflows.js');
      const { mountConfirmation } = await import('/src/confirmation.js');
      const confirm = mountConfirmation(document.querySelector('#confirmation-dialog'));
      const device = { revision: 1, selected: 4, localDirty: false, meta: { startup_usb_mode: 'NORMAL_NKRO' }, hello: { capabilities: { bootloader_command: true } }, client: {}, checkWritable() {} };
      await confirmedCommand(device, 'enter_bootloader', async options => { confirm(options); return null; });
    });
    assert.match(await page.locator('#confirmation-dialog [data-guidance]').textContent(), /手动复制匹配此设备的/);
    await page.locator('#confirmation-dialog').screenshot({ path: fileURLToPath(new URL('download-' + width + '.png', out)) });
    await page.locator('#confirmation-dialog [data-cancel]').click();
    assert.equal(await page.evaluate(() => document.documentElement.scrollWidth > innerWidth), false);
    report.checks.push('invalid-to-valid-to-clear resets header; simplified error, model label, 14px/600 styles, device wording and fixed hint; no overflow at ' + width);
  }
  assert.deepEqual(errors, []); report.passed = true; report.browser = await browser.version();
  await writeFile(new URL('browser-check.json', out), JSON.stringify(report, null, 2)); console.log(JSON.stringify(report, null, 2));
} finally { await browser.close(); }
