import assert from 'node:assert/strict';
import {readFile,writeFile,mkdir} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {fileURLToPath,pathToFileURL} from 'node:url';
const root=new URL('../',import.meta.url),evidence=new URL('evidence/annotations-round4/',root);
await mkdir(evidence,{recursive:true});
const baseline=JSON.parse((await readFile(new URL('evidence/web-baseline.json',root),'utf8')).replace(/^\uFEFF/,''));
for(const entry of baseline)assert.equal(createHash('sha256').update(await readFile(new URL('../../'+entry.path,root))).digest('hex'),entry.sha256);
const {chromium}=await import(pathToFileURL(process.argv[2]).href);
const browser=await chromium.launch({channel:'msedge',headless:true});
const report={scope:'Two fourth-round presentation edits',production_files_unchanged:baseline.length,hardware_tested:false,checks:[]};
try{
 const page=await browser.newPage(),errors=[];page.on('pageerror',e=>errors.push(String(e)));
 for(const width of [1274,390]){
  await page.setViewportSize({width,height:width===390?844:1291});await page.goto('http://localhost:4175/#device-status',{waitUntil:'networkidle'});
  assert.equal((await page.locator('.status-panel h2').textContent()).trim(),'设置与保存');
  assert.equal(await page.locator('#device-summary').textContent(),'改好后点“应用并保存”，断电后也会保留。');
  assert.equal(await page.locator('#draft-state').textContent(),'暂无修改');
  assert.equal(await page.locator('#save-state').textContent(),'保存状态：连接后查看');
  assert.equal(await page.locator('#demo-label').count(),1);
  await page.locator('.status-panel').screenshot({path:fileURLToPath(new URL(`save-panel-${width}.png`,evidence))});
  const mode=page.locator('#io4-mode-switch');await mode.click();await page.locator('#io4-mode-confirm').click();
  assert.equal(await mode.getAttribute('aria-checked'),'true');
  const selected=await mode.evaluate(el=>({color:getComputedStyle(el).color,background:getComputedStyle(el).backgroundImage,track:getComputedStyle(el.querySelector('.switch-track')).backgroundColor}));
  assert.equal(selected.color,'rgb(150, 87, 121)');assert.ok(selected.background.includes('255, 245, 250'));
  await mode.hover();assert.equal(await mode.evaluate(el=>getComputedStyle(el).color),'rgb(150, 87, 121)');
  await mode.screenshot({path:fileURLToPath(new URL(`io4-selected-${width}.png`,evidence))});
  await mode.click();assert.equal(await mode.getAttribute('aria-checked'),'false');
  assert.equal(await mode.evaluate(el=>getComputedStyle(el).color),'rgb(75, 114, 141)');
  assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
  report.checks.push({width,selected,hover_stays_pink:true,off_returns_blue:true,horizontal_overflow:false});
 }
 assert.deepEqual(errors,[]);report.browser_errors=errors;report.passed=true;
 await writeFile(new URL('browser-check.json',evidence),JSON.stringify(report,null,2)+'\n');console.log('Two edits checked at 1274 and 390px; 74 production files unchanged.');
}finally{await browser.close();}
