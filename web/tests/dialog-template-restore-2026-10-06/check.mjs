import assert from 'node:assert/strict';
import { chromium } from 'file:///C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright/index.mjs';
import { mkdir, writeFile, access } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
const out = new URL('./verified-final/', import.meta.url), report = { date_local: '2026-10-06', hardware_tested: false, checks: [] };
try { await access(new URL('browser-check.json', out)); throw Error('Evidence exists'); } catch (error) { if (error.code !== 'ENOENT') throw error; }
await mkdir(out, { recursive: true });
const browser = await chromium.launch({ channel: 'msedge', headless: true });
const page = await browser.newPage(), errors = [];
page.on('pageerror', error => errors.push(String(error)));
await page.addInitScript(() => {
  Object.defineProperty(navigator, 'serial', { configurable: true, value: { requestPort: async () => {
    const { DemoHost, demoPort } = await import('/src/demo.js');
    const host = new DemoHost(), original = host.handle.bind(host);
    host.device.profiles.forEach(profile => profile.io4_bindings = Array(12).fill(0));
    host.mode = 'NORMAL_NKRO';
    host.handle = request => {
      const envelope = result => ({ protocol_version: 1, id: request.id, ok: true, revision: host.device.revision, applied_revision: host.device.revision, saved_revision: host.saved, dirty: host.device.revision !== host.saved, result });
      if (['set_startup_usb_mode', 'reboot_to_saved_mode', 'enter_bootloader'].includes(request.op)) {
        host.requests.push(structuredClone(request));
        if (request.op === 'set_startup_usb_mode') { host.mode = request.payload.mode; host.device.revision++; }
        return envelope({});
      }
      const response = original(request);
      if (request.op === 'hello') {
        response.result.scope = 'device_schema2'; response.result.serial = 'DIALOG-TEST-FIXTURE-ONLY';
        Object.assign(response.result.capabilities, { io4: true, usb_cold_switch: true, bootloader_command: true });
      }
      if (request.op === 'get_device') { response.result.schema = 2; response.result.startup_usb_mode = host.mode; }
      return response;
    };
    const port = demoPort(host); port.simulated = false; window.testHost = host; return port;
  } } });
});
const settle = () => page.waitForFunction(() => !document.getElementById('refresh').disabled);
const writes = () => page.evaluate(() => testHost.requests.filter(r => !['hello', 'get_status', 'get_device', 'list_profiles', 'get_profile'].includes(r.op)).map(r => r.op));
try {
  await page.goto('http://localhost:4174'); await page.locator('#connect').click(); await settle();
  await page.locator('#apply').click(); await settle();
  const initialWrites = await writes();
  const dialog = page.locator('#confirmation-dialog');
  for (const width of [1274, 390, 320]) {
    await page.setViewportSize({ width, height: 844 });
    const before = await writes();
    await page.locator('#io4-mode-switch').click();
    assert.equal(await dialog.locator('[data-title]').textContent(), '启用 IO4 启动模式？');
    assert(await dialog.locator('[data-warning]').isVisible());
    assert.equal(await dialog.locator('[data-warning-title]').textContent(), '启用后，配置网页将无法连接设备。');
    assert.equal(await dialog.locator('[data-confirm]').textContent(), '了解，选择 IO4');
    assert.equal(await dialog.locator('[data-warning]').evaluate(e => getComputedStyle(e, '::before').content), '"!"');
    assert.match(await dialog.locator('[data-note]').textContent(), /不会写入设备/);
    assert.equal(await dialog.evaluate(e => e.scrollWidth > e.clientWidth), false);
    await dialog.screenshot({ path: fileURLToPath(new URL('io4-' + width + '.png', out)) });
    await page.keyboard.press('Escape'); await settle(); assert.deepEqual(await writes(), before);
    assert.equal(await page.locator('#io4-mode-switch').getAttribute('aria-checked'), 'false');
    await page.locator('#restart-device').click();
    assert.equal(await dialog.locator('[data-title]').textContent(), '重启设备');
    assert.equal(await dialog.locator('[data-description]').textContent(), '重启后，未保存的修改会丢失，设备将使用已保存的设置。确定要重启吗？');
    assert(!(await dialog.locator('[data-warning]').isVisible()));
    assert.match(await dialog.locator('[data-note]').textContent(), /设备将重启并断开/);
    if (width === 1274) assert((await dialog.boundingBox()).height < 260, 'original compact restart dialog');
    await dialog.screenshot({ path: fileURLToPath(new URL('restart-' + width + '.png', out)) });
    await dialog.locator('[data-cancel]').click(); await settle(); assert.deepEqual(await writes(), before);
    report.checks.push('IO4/restart presentation, no overflow, Escape/cancel no writes at ' + width);
  }
  await page.locator('#io4-mode-switch').click(); await dialog.locator('[data-confirm]').click(); await settle();
  assert.equal(await page.locator('#io4-mode-switch').getAttribute('aria-checked'), 'true');
  assert.deepEqual(await writes(), initialWrites);
  await page.locator('#refresh').click(); assert(!(await dialog.locator('[data-warning]').isVisible())); assert(!(await dialog.locator('[data-note]').isVisible()));
  await dialog.locator('[data-confirm]').click(); await settle();
  await page.locator('#restart-device').click(); await dialog.locator('[data-confirm]').click(); await settle();
  assert.deepEqual(await writes(), [...initialWrites, 'reboot_to_saved_mode']);
  await page.locator('[data-module="update"]').click();
  await page.locator('#integration-bootloader').click();
  assert(!(await dialog.locator('[data-warning]').isVisible())); assert(!(await dialog.locator('[data-note]').isVisible()));
  assert.equal(await dialog.locator('.uf2-dialog-steps li').count(), 3);
  await dialog.locator('[data-cancel]').click(); await settle();
  await page.locator('[data-module="profiles"]').click(); await page.locator('#create').click();
  assert(await dialog.locator('[data-name-row]').isVisible());
  assert(!(await dialog.locator('[data-warning]').isVisible())); assert(!(await dialog.locator('[data-note]').isVisible()));
  assert.equal(await page.evaluate(() => document.activeElement.getAttribute('aria-label')), '新模板名称');
  await dialog.locator('[data-cancel]').click(); await settle();
  assert.deepEqual(await writes(), [...initialWrites, 'reboot_to_saved_mode']);
  report.checks.push('IO4 confirm only changes draft; other dialogs clear warning/note; confirmed reboot writes once; UF2 guidance preserved');
  report.checks.push('template name dialog and initial input focus preserved; cancellation no writes');
  assert.deepEqual(errors, []); report.passed = true; report.browser = await browser.version();
  await writeFile(new URL('browser-check.json', out), JSON.stringify(report, null, 2));
  console.log(JSON.stringify(report, null, 2));
} finally { await browser.close(); }
