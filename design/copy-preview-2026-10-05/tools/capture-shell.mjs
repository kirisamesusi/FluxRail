// Read-only capture: the existing Web runs against its isolated demo, never a device.
import http from 'node:http';
import path from 'node:path';
import { readFile, writeFile, mkdir, copyFile } from 'node:fs/promises';
import { fileURLToPath, pathToFileURL } from 'node:url';
const out = fileURLToPath(new URL('../', import.meta.url));
const source = path.resolve(out, '../../web');
const { chromium } = await import(pathToFileURL(process.argv[2]).href);
const server = http.createServer(async (req, res) => {
  try {
    const route = decodeURIComponent(new URL(req.url, 'http://localhost').pathname);
    const target = path.resolve(source, '.' + (route === '/' ? '/index.html' : route));
    if (!target.startsWith(source + path.sep)) throw Error('outside source');
    const body = await readFile(target);
    const mime = { '.html': 'text/html; charset=utf-8', '.js': 'text/javascript', '.css': 'text/css', '.png': 'image/png', '.ico': 'image/vnd.microsoft.icon' };
    res.writeHead(200, { 'Content-Type': mime[path.extname(target)] || 'application/octet-stream' });
    res.end(body);
  } catch { res.writeHead(404); res.end(); }
});
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const browser = await chromium.launch({ channel: 'msedge', headless: true });
try {
  const page = await browser.newPage();
  const errors = [];
  page.on('pageerror', e => errors.push(String(e)));
  await page.addInitScript(() => {
    Object.defineProperty(navigator, 'serial', { value: { requestPort() { throw Error('Real serial access is prohibited in copy review'); } }, configurable: false });
  });
  await page.goto(`http://127.0.0.1:${server.address().port}/#experiment-device`);
  await page.locator('#demo').click();
  await page.waitForFunction(() => document.querySelectorAll('.key-card').length === 12);
  await page.waitForFunction(() => document.querySelector('#connection-state').textContent === '模拟会话');
  await page.evaluate(() => {
    document.querySelectorAll('script').forEach(el => el.remove());
    document.querySelectorAll('input, select, textarea').forEach(el => {
      if (el.tagName === 'SELECT') [...el.options].forEach(o => o.toggleAttribute('selected', o.selected));
      else if (el.type === 'checkbox' || el.type === 'radio') el.toggleAttribute('checked', el.checked);
      else if (el.type !== 'file') el.setAttribute('value', el.value);
    });
    document.querySelectorAll('details').forEach(el => el.open = false);
    document.querySelectorAll('.module-panel').forEach(el => el.hidden = el.id !== 'profiles');
    document.querySelectorAll('[data-module]').forEach(el => el.toggleAttribute('aria-current', el.dataset.module === 'profiles'));
    document.querySelector('[data-module="profiles"]').setAttribute('aria-current', 'page');
    // Freeze all device and simulation controls. Navigation and native details remain active.
    document.querySelectorAll('button, input, select, textarea, fieldset').forEach(el => el.disabled = true);
    document.querySelectorAll('[role="slider"]').forEach(el => { el.removeAttribute('tabindex'); el.setAttribute('aria-disabled', 'true'); });
    document.querySelectorAll('.down').forEach(el => el.classList.remove('down'));
    for (const el of document.querySelectorAll('.physical-state, .test-key small')) el.textContent = '等待连接';
    const initial = {
      'connection-state': '文案预览', 'demo-label': '', 'device-summary': '连接后查看当前模板、固件版本和保存状态。',
      'draft-state': '还没有修改设置', 'save-state': '连接后查看保存状态',
      'notice': '先连接设备，再修改设置。改好后应用到设备，需要断电保留时再保存。',
      'hall-cap': '滑轨位置', 'knob-cap': '双旋钮', 'capacity': '4 个预设 · 最多 8 个自定义',
      'current-firmware': '当前版本：连接后查看', 'calibration-state': '连接后查看校准记录',
      'calibration-availability': '连接设备后可开始校准。', 'status-data': '连接后查看按键和传感器状态。',
      'io4-mapping-profile': '连接后读取当前模板', 'io4-mapping-status': '请在普通 USB 模式下连接设备，读取后编辑 IO4 键位。',
      'import-preview': '选择配置文件后，查看将要导入的内容。导入会替换设备当前配置，包括全部模板和滑轨校准；保存后才能在断电后保留。',
      'update-cap': '选择 UF2 文件，检查板型和固件版本。', 'version-result': '连接设备后，可与当前版本比较。',
      'diagnostic-summary': '连接设备后查看按键状态。页面约每半秒更新一次，短按可能看不到。',
      'integration-motion-summary': '连接设备后查看滑轨与旋钮状态。'
    };
    for (const [id, text] of Object.entries(initial)) document.getElementById(id).textContent = text;
    document.querySelectorAll('#identity dd').forEach(el => el.textContent = '连接后查看');
    document.querySelectorAll('[id^="integration-knob-"]').forEach(el => el.textContent = '连接后查看圈数和按压状态');
    document.querySelectorAll('.io4-key-card small').forEach(el => el.textContent = '连接后读取');
    const ribbon = document.createElement('div'); ribbon.className = 'copy-preview-note';
    ribbon.innerHTML = '<strong>文案预览 · 第一稿</strong><span>这页只供检查文案。可切换页面、展开说明；连接和保存等按钮暂不可用。</span>';
    document.querySelector('.workspace-header').before(ribbon);
    document.title = 'FluxRail · 文案预览（第一稿）';
    const extra = document.createElement('link'); extra.rel = 'stylesheet'; extra.href = 'preview.css'; document.head.append(extra);
    const script = document.createElement('script'); script.src = 'shell.js'; script.defer = true; document.body.append(script);
  });
  await writeFile(path.join(out, 'tools/source-shell.html'), await page.content(), 'utf8');
  await copyFile(path.join(source, 'style.css'), path.join(out, 'style.css'));
  await mkdir(path.join(out, 'assets'), { recursive: true });
  for (const asset of ['fluxrail-mascot-brand.png', 'fluxrail-mascot-controller.png', 'fluxrail-icon-32.png', 'fluxrail-icon-180.png', 'fluxrail-icon-192.png', 'fluxrail-favicon.ico']) await copyFile(path.join(source, 'assets', asset), path.join(out, 'assets', asset));
  await writeFile(path.join(out, 'evidence/capture.json'), JSON.stringify({ date: '2026-10-05', real_device_access: false, source_files_modified: false, captured_dynamic_keys: 12, page_errors: errors }, null, 2));
  if (errors.length) throw Error(errors.join('\n'));
  console.log('Captured 8 modules and dynamic forms as a static shell. No device access.');
} finally { await browser.close(); server.close(); }
