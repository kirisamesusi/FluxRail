import assert from 'node:assert/strict';
import {readFile,writeFile,mkdir} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {fileURLToPath,pathToFileURL} from 'node:url';
const root=new URL('../',import.meta.url),evidence=new URL('evidence/uf2-confirmation/',root);await mkdir(evidence,{recursive:true});
const baseline=JSON.parse((await readFile(new URL('evidence/web-baseline.json',root),'utf8')).replace(/^\uFEFF/,''));
for(const file of baseline)assert.equal(createHash('sha256').update(await readFile(new URL('../../'+file.path,root))).digest('hex'),file.sha256);
const {chromium}=await import(pathToFileURL(process.argv[2]).href),browser=await chromium.launch({channel:'msedge',headless:true});
const report={scope:'Independent UF2 confirmation preview',production_files_unchanged:baseline.length,hardware_tested:false,views:[]};
try{
 const page=await browser.newPage(),errors=[],requests=[];page.on('pageerror',e=>errors.push(String(e)));page.on('request',r=>requests.push({method:r.method(),path:new URL(r.url()).pathname}));
 for(const width of [1274,390,320]){
  await page.setViewportSize({width,height:width>600?1291:844});await page.goto('http://localhost:4175/#update',{waitUntil:'networkidle'});
  await page.reload({waitUntil:'networkidle'});
  assert.equal(await page.locator('#update .update-steps').count(),0);assert.equal(await page.locator('#update #manual-update-help').count(),0);
  assert.equal(await page.locator('#update #uf2-result').count(),1);assert.equal(await page.locator('#current-firmware').count(),1);
  const entry=page.locator('#integration-bootloader'),dialog=page.locator('#uf2-download-confirmation');
  assert.equal(await entry.isDisabled(),false);assert.equal(await dialog.evaluate(el=>el.open),false);
  const initialNotice=await page.locator('#notice').textContent();await entry.click();
  assert.equal(await dialog.evaluate(el=>el.open),true);assert.ok((await dialog.textContent()).includes('未保存的修改会丢失'));
  assert.equal(await dialog.locator('ol li').count(),3);assert.equal(await dialog.getAttribute('aria-describedby'),'uf2-download-description');
  assert.equal(await page.evaluate(()=>document.activeElement.closest('dialog')?.id),'uf2-download-confirmation');
  assert.equal(await dialog.evaluate(el=>el.scrollWidth>el.clientWidth),false);
  await page.screenshot({path:fileURLToPath(new URL(`dialog-${width}.png`,evidence))});
  await page.locator('#uf2-download-cancel').click();assert.equal(await dialog.evaluate(el=>el.open),false);
  assert.equal(await page.locator('#notice').textContent(),initialNotice);assert.equal(await page.evaluate(()=>document.activeElement.id),'integration-bootloader');
  await entry.click();await page.keyboard.press('Escape');assert.equal(await dialog.evaluate(el=>el.open),false);assert.equal(await page.locator('#notice').textContent(),initialNotice);
  await entry.click();await dialog.locator('summary').click();assert.equal(await dialog.locator('#manual-update-help').isVisible(),true);
  await page.screenshot({path:fileURLToPath(new URL(`dialog-fallback-${width}.png`,evidence))});
  const requestCount=requests.length;await page.locator('#uf2-download-confirm').click();assert.equal(await dialog.evaluate(el=>el.open),false);
  assert.equal(await page.locator('#notice').textContent(),'这里只演示下载模式确认，未操作设备。');assert.equal(requests.length,requestCount);
  assert.equal(await page.evaluate(()=>document.activeElement.id),'integration-bootloader');
  assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
  await page.screenshot({path:fileURLToPath(new URL(`update-${width}.png`,evidence))});
  report.views.push({width,cancel_and_escape_preserve_notice:true,confirm_local_only:true,focus_returned:true,horizontal_overflow:false});
 }
 assert.deepEqual(errors,[]);assert.equal(requests.some(r=>r.path.startsWith('/src/')||r.method!=='GET'),false);report.browser_errors=errors;report.production_modules_loaded=false;report.passed=true;
 await writeFile(new URL('browser-check.json',evidence),JSON.stringify(report,null,2)+'\n');console.log('UF2 preview confirmation checked at 1274/390/320px; 74 production files unchanged.');
}finally{await browser.close();}
