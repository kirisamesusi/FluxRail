import assert from 'node:assert/strict';
import { readFile, writeFile, mkdir } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
const root=fileURLToPath(new URL('../',import.meta.url)), repo=path.resolve(root,'../..');
const evidence=path.join(root,'evidence/annotations-21'); await mkdir(evidence,{recursive:true});
const {chromium}=await import(pathToFileURL(process.argv[2]).href);
const baseline=JSON.parse((await readFile(path.join(root,'evidence/web-baseline.json'),'utf8')).replace(/^\uFEFF/,''));
for(const entry of baseline) assert.equal(createHash('sha256').update(await readFile(path.join(repo,entry.path))).digest('hex'),entry.sha256,entry.path);
const browser=await chromium.launch({channel:'msedge',headless:true});
const report={date_local:'2026-10-05',scope:'Independent annotated preview',production_files_unchanged:baseline.length,hardware_tested:false,compiled:false,views:[],covered_comments:Array.from({length:21},(_,i)=>i+1)};
try {
  const page=await browser.newPage(),errors=[],requests=[];
  page.on('pageerror',e=>errors.push(String(e))); page.on('request',r=>requests.push(new URL(r.url()).pathname));
  for(const width of [1440,1285,390]) {
    await page.setViewportSize({width,height:width===390?844:1000});
    await page.goto('http://localhost:4175',{waitUntil:'networkidle'});
    assert.deepEqual(await page.locator('button:not([disabled])').evaluateAll(els=>els.map(el=>el.id)),['restart-preview','restart-cancel','restart-confirm']);
    assert.equal(await page.locator('script').count(),1); assert.equal(await page.locator('script').getAttribute('src'),'shell.js');
    assert.equal(await page.locator('#brightness-text').count(),0);
    assert.equal(await page.locator('#diagnostics .section-heading > .badge').count(),0);
    assert.equal(await page.locator('#profiles .integration-mode').count(),1);
    assert.equal(await page.locator('#update .integration-mode').count(),0);
    assert.equal(await page.locator('#experiment-hall #experiment-calibration').count(),1);
    assert.equal(await page.locator('#light-advanced-fields label').count(),3);
    assert.equal(await page.locator('.parameter-help').count(),11);
    assert.equal(await page.locator('#experimental-content #uf2-result').count(),0);
    assert.equal(await page.locator('#update #uf2-result').count(),1);
    assert.equal(await page.locator('#io4-mapping-editor .toolbar button').count(),2);
    assert.equal(await page.locator('#io4-mapping-editor .toolbar a').count(),0);
    assert.equal(await page.locator('body').textContent().then(text=>text.includes('身份')),false);
    assert.equal(await page.locator('#keyboard').evaluate(el=>el.querySelector('#key-editor').compareDocumentPosition(el.querySelector('.layout-guide'))&Node.DOCUMENT_POSITION_FOLLOWING),4);
    assert.deepEqual(await page.locator('#key-grid .physical-keys-main > [data-source]').evaluateAll(els=>els.map(el=>el.dataset.source)),await page.locator('.io4-key-grid > [data-source]').evaluateAll(els=>els.map(el=>el.dataset.source)));
    for(const id of ['profiles','keyboard','lighting','hall','knobs','diagnostics','update','experimental']) {
      await page.locator(`[data-module="${id}"]`).click(); await page.waitForFunction(id=>!document.getElementById(id).hidden,id);
      assert.equal(await page.locator('.module-panel:visible').count(),1);
      assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,`overflow ${id}/${width}`);
      if(id==='hall'||id==='knobs') {
        const ids=id==='hall'?['hall-gain']:['knob-gain-1','knob-gain-2'];
        for(const gain of ids) {
          const input=page.locator('#'+gain+'-number'),slider=page.locator('#'+gain+'-ruler');
          assert.equal(await input.inputValue(),'5');
          await input.fill(''); await input.pressSequentially('1.5'); assert.equal(await input.inputValue(),'1.5'); assert.equal(await slider.inputValue(),'1.5');
          await slider.focus(); await page.keyboard.press('ArrowRight'); assert.equal(await input.inputValue(),'2');
          await input.fill('0.1'); assert.equal(await slider.inputValue(),'0');
          await slider.focus(); await page.keyboard.press('ArrowRight'); assert.equal(await input.inputValue(),'1');
          await page.keyboard.press('Home'); assert.equal(await input.inputValue(),'0.1');
          await page.keyboard.press('End'); assert.equal(await input.inputValue(),'10');
          await input.fill('11'); assert.equal(await input.getAttribute('aria-invalid'),'true');
          assert.ok((await page.locator('#'+gain+'-error').textContent()).includes('0.1～10'));
          await input.fill('5');
        }
      }
      if([1440,390].includes(width)&&['profiles','hall','knobs','update'].includes(id)) {
        await page.evaluate(()=>window.scrollTo({top:0,behavior:'instant'}));
        await page.screenshot({path:path.join(evidence,`${id}-${width}.png`),fullPage:true});
      }
    }
    for(const details of await page.locator('#experimental-content details').all()) {
      if(!await details.evaluate(el=>el.open)) await details.locator(':scope > summary').click();
      assert.equal(await details.evaluate(el=>el.open),true);
    }
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,`expanded overflow/${width}`);
    if([1440,390].includes(width)) {
      await page.locator('#experiment-hall').screenshot({path:path.join(evidence,`hall-advanced-${width}.png`)});
      await page.locator('#io4-mapping-editor').screenshot({path:path.join(evidence,`io4-${width}.png`)});
    }
    await page.locator('[data-module="profiles"]').click();
    await page.locator('#restart-preview').click(); assert.equal(await page.locator('#restart-confirmation').evaluate(el=>el.open),true);
    assert.ok((await page.locator('#restart-description').textContent()).includes('未保存的修改会丢失'));
    if(width===1440) await page.screenshot({path:path.join(evidence,'restart-dialog-1440.png')});
    await page.locator('#restart-cancel').click(); assert.equal(await page.locator('#restart-confirmation').evaluate(el=>el.open),false);
    await page.locator('#restart-preview').click(); await page.keyboard.press('Escape'); assert.equal(await page.locator('#restart-confirmation').evaluate(el=>el.open),false);
    await page.locator('#restart-preview').click(); await page.locator('#restart-confirm').click();
    assert.equal(await page.locator('#notice').textContent(),'这里只演示重启确认，未操作设备。');
    report.views.push({width,modules:8,overflow:false,three_rulers_passed:true,restart_dialog_passed:true});
  }
  assert.ok(requests.every(r=>r==='/'||['/shell.js','/preview.css','/style.css','/favicon.ico'].includes(r)||r.startsWith('/assets/')));
  assert.deepEqual(errors,[]); report.browser_errors=errors;report.production_modules_loaded=false;report.passed=true;
  await writeFile(path.join(evidence,'browser-check.json'),JSON.stringify(report,null,2));console.log(JSON.stringify(report));
} finally {await browser.close();}
