// One-off visual/HTTP evidence for the 2026-10-05 mascot change. No serial/device access.
import assert from 'node:assert/strict';
import { access, readFile, writeFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { fileURLToPath, pathToFileURL } from 'node:url';
import path from 'node:path';
const { chromium } = await import(pathToFileURL(process.argv[2]).href);
const evidence = fileURLToPath(new URL('.', import.meta.url));
// Stop before touching screenshots when this evidence set has already been frozen.
try {
  await access(path.join(evidence, 'browser-http-check.json'));
  throw new Error('Evidence already exists; use a new dated directory instead of overwriting it.');
} catch (error) { if (error.code !== 'ENOENT') throw error; }
const base = 'http://localhost:4174';
const browser = await chromium.launch({ channel: 'msedge', headless: true });
const report = { date_local: '2026-10-05', scope: 'Web static assets, responsive layout and disconnected navigation only', browser: await browser.version(), hardware_tested: false, compiled: false, views: [], http: [] };
try {
  const page = await browser.newPage();
  const errors = [];
  page.on('pageerror', error => errors.push(String(error)));
  page.on('console', message => { if (message.type() === 'error') errors.push(message.text()); });
  for (const [width, height] of [[1440, 980], [980, 900], [768, 900], [390, 844], [320, 844]]) {
    await page.setViewportSize({ width, height });
    await page.goto(base, { waitUntil: 'networkidle' });
    await page.locator('.brand-mark img').evaluate(img => img.decode());
    await page.locator('.hero-art').evaluate(img => img.decode());
    const state = await page.evaluate(() => {
      const brand = document.querySelector('.brand');
      const brandRect = brand.getBoundingClientRect();
      const last = brand.lastElementChild.getBoundingClientRect();
      return { overflow: document.documentElement.scrollWidth > innerWidth, brandFits: last.right <= brandRect.right + 1, images: [...document.images].map(img => ({ src: img.getAttribute('src'), width: img.naturalWidth, height: img.naturalHeight, loaded: img.complete && img.naturalWidth > 0 })), icons: [...document.querySelectorAll('link[rel~="icon"],link[rel="apple-touch-icon"]')].map(link => link.getAttribute('href')) };
    });
    assert.equal(state.overflow, false, `horizontal overflow at ${width}`);
    assert.equal(state.brandFits, true, `brand overflows at ${width}`);
    assert.ok(state.images.every(img => img.loaded));
    assert.equal(state.icons.length, 4);
    report.views.push({ width, height, ...state });
    if (width === 1440 || width === 390) await page.screenshot({ path: path.join(evidence, `layout-${width}.png`), fullPage: true });
    if (width === 1440) await page.locator('.brand').screenshot({ path: path.join(evidence, 'brand.png') });
  }
  await page.locator('[data-module="keyboard"]').click();
  await page.waitForFunction(() => !document.getElementById('keyboard').hidden);
  assert.equal(await page.locator('#connection-state').textContent(), '未连接设备');
  assert.deepEqual(errors, []);
  report.navigation_passed = true;
  report.browser_errors = errors;
  for (const route of ['/assets/fluxrail-mascot-brand.png', '/assets/fluxrail-mascot-controller.png', '/assets/fluxrail-icon-32.png', '/assets/fluxrail-icon-180.png', '/assets/fluxrail-icon-192.png', '/assets/fluxrail-favicon.ico', '/favicon.ico', '/assets/fluxrail-lace-bunny.png']) {
    for (const method of ['GET', 'HEAD']) {
      const response = await page.request.fetch(base + route, { method });
      assert.equal(response.status(), 200);
      const mime = response.headers()['content-type'];
      assert.equal(mime, route.endsWith('.ico') ? 'image/vnd.microsoft.icon' : 'image/png');
      assert.equal(response.headers()['x-content-type-options'], 'nosniff');
      assert.ok(response.headers()['content-security-policy']);
      if (method === 'HEAD') assert.equal((await response.body()).length, 0);
      report.http.push({ route, method, status: response.status(), mime });
    }
  }
  for (const route of ['/assets/fluxrail-mascot-aqua.png', '/assets/fluxrail-mascot-icon-source.png', '/assets/fluxrail-mascot-controller-source.png', '/assets/fluxrail-mascot-theme-source.png', '/assets/fluxrail-mascot-icon-theme-source.png', '/assets/fluxrail-mascot-controller-theme-source.png', '/assets/MASCOT.md', '/tools/export-mascot.mjs', '/LICENSE', '/firmware/FluxRail/FluxRail.ino', '/assets/not-listed.png']) {
    const response = await page.request.get(base + route);
    assert.equal(response.status(), 404);
    report.http.push({ route, method: 'GET', status: response.status() });
  }
  const ico = await readFile(new URL('../../assets/fluxrail-favicon.ico', import.meta.url));
  assert.equal(ico.readUInt16LE(0), 0);
  assert.equal(ico.readUInt16LE(2), 1);
  assert.equal(ico.readUInt16LE(4), 3);
  report.ico_sizes = [];
  for (let i = 0; i < 3; i++) {
    const entry = 6 + i * 16, length = ico.readUInt32LE(entry + 8), offset = ico.readUInt32LE(entry + 12);
    assert.ok(offset + length <= ico.length);
    assert.equal(ico.subarray(offset, offset + 8).toString('hex'), '89504e470d0a1a0a');
    report.ico_sizes.push(ico[entry]);
  }
  const backup = JSON.parse(await readFile(new URL('../../../backups/web-before-mascot-2026-10-05/BACKUP-MANIFEST.json', import.meta.url), 'utf8'));
  const repository = fileURLToPath(new URL('../../../', import.meta.url));
  let unchanged = 0;
  for (const entry of backup.files.filter(entry => entry.path.startsWith('src/') || entry.path.startsWith('tests/'))) {
    const bytes = await readFile(path.join(repository, 'web', entry.path));
    assert.equal(createHash('sha256').update(bytes).digest('hex'), entry.sha256);
    unchanged++;
  }
  report.unchanged_existing_src_and_test_files = unchanged;
  report.passed = true;
  await writeFile(path.join(evidence, 'browser-http-check.json'), JSON.stringify(report, null, 2), { flag: 'wx' });
  console.log(JSON.stringify({ passed: true, views: report.views.map(view => view.width), http_checks: report.http.length, unchanged_existing_src_and_test_files: unchanged }));
} finally { await browser.close(); }
