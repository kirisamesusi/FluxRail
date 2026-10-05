import assert from 'node:assert/strict';
import {readFile,writeFile,mkdir} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {fileURLToPath,pathToFileURL} from 'node:url';
const root=new URL('../',import.meta.url),evidence=new URL('evidence/annotations-round3/',root);
await mkdir(evidence,{recursive:true});
const {chromium}=await import(pathToFileURL(process.argv[2]).href);
const baseline=JSON.parse((await readFile(new URL('evidence/web-baseline.json',root),'utf8')).replace(/^\uFEFF/,''));
for(const entry of baseline)assert.equal(createHash('sha256').update(await readFile(new URL('../../'+entry.path,root))).digest('hex'),entry.sha256,entry.path);
const browser=await chromium.launch({channel:'msedge',headless:true});
const report={scope:'Independent third-round shell',covered_comments:[1,2,3,4,5,6,7,8,9,10,11,12],production_files_unchanged:baseline.length,hardware_tested:false,compiled:false,views:[]};
try {
  const page=await browser.newPage(),errors=[],requests=[];
  page.on('pageerror',e=>errors.push(String(e)));page.on('request',r=>requests.push(new URL(r.url()).pathname));
  const modules=['device-status','keyboard','lighting','hall','knobs','profiles','update','experimental'];
  for(const width of [1440,1274,390]) {
    await page.setViewportSize({width,height:width===390?844:1291});await page.goto('http://localhost:4175/',{waitUntil:'networkidle'});
    assert.deepEqual(await page.locator('.module-nav [data-module]').evaluateAll(els=>els.map(el=>el.dataset.module)),modules);
    assert.equal(await page.locator('.module-panel:visible').getAttribute('id'),'device-status');
    assert.equal(await page.locator('#device-status #identity').count(),1);assert.equal(await page.locator('#identity dt').count(),7);
    assert.equal(await page.locator('#diagnostics,#experiment-device').count(),0);
    assert.equal(await page.locator('#device-status #input-status #input-test').count(),1);
    assert.equal(await page.locator('#device-status #knob-telemetry').count(),1);
    assert.equal(await page.locator('.device-module-grid input[type=checkbox]:disabled').count(),3);
    assert.equal(await page.locator('#experiment-demo #demo:disabled').count(),1);
    assert.equal(await page.locator('#experimental #identity').count(),0);
    assert.equal(await page.locator('#keyboard .badge').textContent(),'12 个按键');
    assert.equal(await page.locator('#capacity').textContent(),'预设 4 个 · 自定义最多 8 个');
    assert.equal(await page.locator('#experiment-hall > summary').textContent(),'滑轨 · 校准、输出范围与响应');
    assert.equal(await page.locator('#experiment-calibration').evaluate(el=>el.tagName),'DIV');
    const copy=await page.locator('body').textContent();assert.equal(copy.includes('输入测试'),false);assert.equal(copy.includes('三路'),false);assert.equal(copy.includes('12 路'),false);
    assert.equal((await page.locator('#manual-update-help').textContent()).includes('SW1 / RUN'),false);
    assert.ok((await page.locator('#manual-update-help').textContent()).startsWith('物理进入方法：'));
    assert.equal(await page.locator('.sidebar-startup > strong').evaluate(el=>getComputedStyle(el).fontSize),'13px');
    assert.equal(await page.locator('.connection .nav-label').evaluate(el=>getComputedStyle(el).fontSize),'13px');
    assert.equal(await page.locator('script').count(),1);assert.equal(await page.locator('script').getAttribute('src'),'shell.js');
    for(const id of modules){
      await page.locator(`.module-nav [data-module="${id}"]`).click();await page.waitForFunction(id=>!document.getElementById(id).hidden,id);
      assert.equal(await page.locator('.module-panel:visible').count(),1);
      assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,`overflow ${id}/${width}`);
      report.views.push({width,module:id,horizontal_overflow:false});
      if(['device-status','profiles','update'].includes(id))await page.screenshot({path:fileURLToPath(new URL(`${id}-${width}.png`,evidence)),fullPage:true});
      if(id==='experimental'){
        await page.locator('#experiment-hall > summary').click();await page.locator('#experiment-demo > summary').click();
        assert.equal(await page.locator('#experiment-calibration').isVisible(),true);
        await page.screenshot({path:fileURLToPath(new URL(`experimental-${width}.png`,evidence)),fullPage:true});
      }
    }
    await page.locator('.module-nav [data-module="device-status"]').click();
    for(const id of ['keyboard','hall','knobs','profiles']){
      await page.locator(`#device-status a[href="#${id}"]`).click();await page.waitForFunction(id=>!document.getElementById(id).hidden,id);
      assert.equal(await page.locator('.module-nav [aria-current=page]').getAttribute('data-module'),id);
      await page.locator('.module-nav [data-module="device-status"]').click();
    }
    const mode=page.locator('#io4-mode-switch'),dialog=page.locator('#io4-mode-confirmation');
    await mode.click();assert.equal(await dialog.evaluate(el=>el.open),true);assert.equal(await dialog.locator('.io4-recovery').count(),0);
    assert.ok((await dialog.locator('#io4-mode-description').textContent()).includes('BT10'));assert.ok((await dialog.locator('#io4-mode-description').textContent()).includes('无法连接'));
    await page.screenshot({path:fileURLToPath(new URL(`io4-dialog-${width}.png`,evidence))});
    await page.locator('#io4-mode-cancel').click();assert.equal(await mode.getAttribute('aria-checked'),'false');
    await mode.click();await page.keyboard.press('Escape');assert.equal(await mode.getAttribute('aria-checked'),'false');
    await mode.click();await page.locator('#io4-mode-confirm').click();assert.equal(await mode.getAttribute('aria-checked'),'true');
    await page.screenshot({path:fileURLToPath(new URL(`io4-selected-${width}.png`,evidence))});await mode.click();assert.equal(await mode.getAttribute('aria-checked'),'false');
    await page.locator('.module-nav [data-module="hall"]').click();
    await page.locator('#hall-gain-number').fill('1.5');assert.equal(await page.locator('#hall-gain-ruler').inputValue(),'1.5');
    await page.goto('http://localhost:4175/#diagnostics',{waitUntil:'networkidle'});assert.equal(await page.locator('.module-panel:visible').getAttribute('id'),'device-status');
    await page.goto('http://localhost:4175/#identity',{waitUntil:'networkidle'});assert.equal(await page.locator('.module-panel:visible').getAttribute('id'),'device-status');
    await page.goto('http://localhost:4175/#input-status',{waitUntil:'networkidle'});assert.equal(await page.locator('#input-status').evaluate(el=>el.open),true);
  }
  assert.deepEqual(errors,[]);assert.equal(requests.some(p=>p.startsWith('/src/')),false);
  report.browser_errors=errors;report.production_modules_loaded=false;report.passed=true;
  await writeFile(new URL('browser-check.json',evidence),JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify({passed:true,views:report.views.length,production_files_unchanged:baseline.length}));
} finally {await browser.close();}
