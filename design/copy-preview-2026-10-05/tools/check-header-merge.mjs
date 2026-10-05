import assert from 'node:assert/strict';
import {readFile,writeFile,mkdir} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {fileURLToPath,pathToFileURL} from 'node:url';
const root=new URL('../',import.meta.url),evidence=new URL('evidence/header-merge/',root);await mkdir(evidence,{recursive:true});
const baseline=JSON.parse((await readFile(new URL('evidence/web-baseline.json',root),'utf8')).replace(/^\uFEFF/,''));
for(const entry of baseline)assert.equal(createHash('sha256').update(await readFile(new URL('../../'+entry.path,root))).digest('hex'),entry.sha256);
const {chromium}=await import(pathToFileURL(process.argv[2]).href),browser=await chromium.launch({channel:'msedge',headless:true});
const report={scope:'Independent preview header consolidation',production_files_unchanged:baseline.length,hardware_tested:false,views:[]};
try{
 const page=await browser.newPage(),errors=[],requests=[];page.on('pageerror',e=>errors.push(String(e)));page.on('request',r=>requests.push(new URL(r.url()).pathname));
 for(const width of [1274,390,320]){
  await page.setViewportSize({width,height:width>600?1291:844});await page.goto('http://localhost:4175/#device-status',{waitUntil:'networkidle'});
  assert.equal(await page.locator('.unified-device-header').count(),1);assert.equal(await page.locator('.status-panel,.header-tag').count(),0);
  assert.equal(await page.locator('h1').count(),1);assert.equal(await page.locator('.unified-device-header h2').count(),0);
  for(const id of ['refresh','apply','restart-preview','notice','device-summary','draft-state','save-state','demo-label'])assert.equal(await page.locator('.unified-device-header #'+id).count(),1,id);
  assert.equal(await page.locator('#notice').getAttribute('aria-live'),'polite');
  assert.equal(await page.locator('#refresh').isDisabled(),true);assert.equal(await page.locator('#apply').isDisabled(),true);
  assert.equal(await page.locator('.hero-art').evaluate(el=>el.complete&&el.naturalWidth>0),true);
  for(const id of ['device-status','profiles','keyboard','lighting','hall','knobs','update','experimental']){
    await page.locator(`.module-nav [data-module="${id}"]`).click();await page.waitForFunction(id=>!document.getElementById(id).hidden,id);
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,`${id}/${width}`);
  }
  await page.locator('.module-nav [data-module="device-status"]').click();
  await page.locator('.unified-device-header').screenshot({path:fileURLToPath(new URL(`header-${width}.png`,evidence))});
  if(width===1274)await page.screenshot({path:fileURLToPath(new URL('device-status-1274.png',evidence))});
  await page.locator('#restart-preview').click();assert.equal(await page.locator('#restart-confirmation').evaluate(el=>el.open),true);
  await page.locator('#restart-cancel').click();assert.equal(await page.locator('#restart-confirmation').evaluate(el=>el.open),false);
  await page.locator('#io4-mode-switch').click();await page.locator('#io4-mode-confirm').click();assert.ok((await page.locator('.unified-device-header #notice').textContent()).includes('IO4'));
  assert.equal(await page.locator('#io4-mode-switch').evaluate(el=>getComputedStyle(el).color),'rgb(150, 87, 121)');
  await page.locator('#io4-mode-switch').click();assert.ok((await page.locator('#notice').textContent()).includes('普通模式'));
  report.views.push({width,header_height:await page.locator('.unified-device-header').evaluate(el=>el.getBoundingClientRect().height),modules_checked:8,horizontal_overflow:false,local_confirmations_work:true});
 }
 assert.deepEqual(errors,[]);assert.equal(requests.some(p=>p.startsWith('/src/')),false);report.browser_errors=errors;report.production_modules_loaded=false;report.passed=true;
 await writeFile(new URL('browser-check.json',evidence),JSON.stringify(report,null,2)+'\n');console.log('Unified header passed at 1274, 390 and 320px; 74 production files unchanged.');
}finally{await browser.close();}
