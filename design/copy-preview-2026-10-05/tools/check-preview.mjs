import assert from 'node:assert/strict';
import { readFile, writeFile, mkdir } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
const root=fileURLToPath(new URL('../',import.meta.url)), repo=path.resolve(root,'../..');
const edits=JSON.parse(await readFile(path.join(root,'preview-edits.json'),'utf8'));
const evidence=path.join(root,'evidence',edits.revision);
await mkdir(evidence,{recursive:true});
const { chromium }=await import(pathToFileURL(process.argv[2]).href);
const baseline=JSON.parse((await readFile(path.join(root,'evidence/web-baseline.json'),'utf8')).replace(/^\uFEFF/,''));
const changed=[];
for(const entry of baseline) if(createHash('sha256').update(await readFile(path.join(repo,entry.path))).digest('hex')!==entry.sha256) changed.push(entry.path);
assert.deepEqual(changed,[],'existing web files changed since copy-review began');
assert.equal(createHash('sha256').update(await readFile(path.join(root,'style.css'))).digest('hex'),baseline.find(e=>e.path==='web/style.css').sha256);
const browser=await chromium.launch({channel:'msedge',headless:true});
const report={date_local:'2026-10-05',scope:'Isolated copy shell only',review_model:'gpt-6-luna',hardware_tested:false,compiled:false,unchanged_web_files:baseline.length,views:[],modules:[],details:0};
try {
  const page=await browser.newPage(),errors=[],requests=[];
  page.on('pageerror',e=>errors.push(String(e)));
  page.on('request',r=>requests.push(new URL(r.url()).pathname));
  for(const width of [1440,390]) {
    await page.setViewportSize({width,height:width===1440?980:844});
    await page.goto('http://localhost:4175',{waitUntil:'networkidle'});
    await page.locator('.hero-art').evaluate(img=>img.decode());
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,`overflow at ${width}`);
    assert.equal(await page.locator('script').count(),1);
    assert.equal(await page.locator('script').getAttribute('src'),'shell.js');
    assert.equal(await page.locator('button:not([disabled])').count(),0);
    assert.equal(await page.locator('#apply').textContent(),'应用并保存');
    assert.equal(await page.locator('.status-panel #save').count(),0);
    await page.screenshot({path:path.join(evidence,`preview-${width}.png`),fullPage:true});
    report.views.push({width,overflow:false});
    for(const id of ['profiles','keyboard','lighting','hall','knobs','diagnostics','update','experimental']) {
      await page.locator(`[data-module="${id}"]`).click();
      await page.waitForFunction(id=>!document.getElementById(id).hidden,id);
      assert.equal(await page.locator('.module-panel:visible').count(),1);
      assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,`overflow in ${id} at ${width}`);
      if(width===1440)report.modules.push(id);
    }
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,`expanded panel overflow at ${width}`);
    for(const detail of await page.locator('#experimental-content details').all()) {
      await detail.locator(':scope > summary').click();
      assert.equal(await detail.evaluate(el=>el.open),true);
      if(width===1440)report.details++;
    }
    await page.screenshot({path:path.join(evidence,`expanded-${width}.png`),fullPage:true});
  }
  assert.ok(requests.every(r=>r==='/'||['/shell.js','/preview.css','/style.css'].includes(r)||r.startsWith('/assets/')),'unexpected production requests');
  assert.deepEqual(errors,[]);
  report.browser_errors=errors;report.production_modules_loaded=false;report.passed=true;
  report.shared_action='应用并保存';
  await writeFile(path.join(evidence,'preview-check.json'),JSON.stringify(report,null,2));
  console.log(JSON.stringify(report));
} finally {await browser.close();}
