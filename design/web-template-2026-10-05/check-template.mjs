// Offline template QA; Playwright is an explicit external checker dependency.
import assert from 'node:assert/strict';
import {readFile,writeFile,mkdir,access} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {fileURLToPath,pathToFileURL} from 'node:url';
const root=new URL('./',import.meta.url),evidence=process.argv[3]?pathToFileURL(process.argv[3].replace(/[\\/]?$/,'/')):new URL('evidence/',root);
let exists=false;try{await access(new URL('template-check.json',evidence));exists=true;}catch(error){if(error.code!=='ENOENT')throw error;}
if(exists)throw Error('Preserve the frozen report. Supply a new absolute output directory as the third argument.');
await mkdir(evidence,{recursive:true});
const preview=new URL('../copy-preview-2026-10-05/',root);
const sameFiles=['index.html','style.css','preview.css','shell.js','assets/fluxrail-mascot-brand.png','assets/fluxrail-mascot-controller.png','assets/fluxrail-favicon.ico','assets/fluxrail-icon-180.png','assets/fluxrail-icon-192.png','assets/fluxrail-icon-32.png'];
for(const name of sameFiles)assert.deepEqual(await readFile(new URL(name,root)),await readFile(new URL(name,preview)),name);
const baseline=JSON.parse((await readFile(new URL('baseline/production-web-before-preview.json',root),'utf8')).replace(/^\uFEFF/,''));
for(const file of baseline)assert.equal(createHash('sha256').update(await readFile(new URL('../../'+file.path,root))).digest('hex'),file.sha256);
const {chromium}=await import(pathToFileURL(process.argv[2]).href),browser=await chromium.launch({channel:'msedge',headless:true});
const report={scope:'Fixed template assembly only',date_local:'2026-10-05',same_preview_files:sameFiles,production_files_unchanged:baseline.length,hardware_tested:false,compiled:false,production_web_tests_run:false,views:[]};
try{
 const page=await browser.newPage(),errors=[],requests=[];page.on('pageerror',e=>errors.push(String(e)));page.on('request',r=>requests.push({method:r.method(),path:new URL(r.url()).pathname}));
 const modules=['device-status','keyboard','lighting','hall','knobs','profiles','update','experimental'];
 for(const width of [1274,390,320]){
  await page.setViewportSize({width,height:width>600?1291:844});await page.goto('http://localhost:4176/',{waitUntil:'networkidle'});await page.reload({waitUntil:'networkidle'});
  assert.deepEqual(await page.locator('.module-nav [data-module]').evaluateAll(nodes=>nodes.map(node=>node.dataset.module)),modules);
  assert.equal(await page.locator('.module-panel:visible').getAttribute('id'),'device-status');
  assert.equal(await page.locator('.unified-device-header').evaluate(el=>getComputedStyle(el).borderWidth),'0px');
  assert.equal(await page.locator('.unified-device-header').evaluate(el=>getComputedStyle(el).boxShadow),'none');
  assert.equal(await page.locator('#identity').count(),1);assert.equal(await page.locator('#key-grid .key-card').count(),12);
  assert.equal(await page.locator('#update .update-steps').count(),0);assert.equal(await page.locator('#update #manual-update-help').count(),0);
  for(const id of ['keyboard','hall','knobs','profiles'])assert.equal(await page.locator('#'+id+' > .section-heading > .badge').count(),0);
  for(const id of modules){
   await page.locator(`.module-nav [data-module="${id}"]`).click();await page.waitForFunction(id=>!document.getElementById(id).hidden,id);
   assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,id+'/'+width);report.views.push({width,module:id,horizontal_overflow:false});
   if(id==='hall'||id==='knobs')for(const gain of id==='hall'?['hall-gain']:['knob-gain-1','knob-gain-2']){
    await page.locator('#'+gain+'-number').fill('1.5');assert.equal(await page.locator('#'+gain+'-ruler').inputValue(),'1.5');
    await page.locator('#'+gain+'-ruler').focus();await page.keyboard.press('ArrowRight');assert.equal(await page.locator('#'+gain+'-number').inputValue(),'2');
   }
   if(['device-status','update','keyboard'].includes(id)&&width!==320){await page.evaluate(()=>window.scrollTo({top:0,behavior:'instant'}));await page.screenshot({path:fileURLToPath(new URL(id+'-'+width+'.png',evidence))});}
  }
  await page.locator('#io4-mode-switch').click();await page.locator('#io4-mode-cancel').click();assert.equal(await page.locator('#io4-mode-switch').getAttribute('aria-checked'),'false');
  await page.locator('#io4-mode-switch').click();await page.locator('#io4-mode-confirm').click();assert.equal(await page.locator('#io4-mode-switch').getAttribute('aria-checked'),'true');
  assert.equal(await page.locator('#io4-mode-switch').evaluate(el=>getComputedStyle(el).color),'rgb(150, 87, 121)');await page.locator('#io4-mode-switch').click();
  await page.locator('#restart-preview').click();await page.keyboard.press('Escape');assert.equal(await page.locator('#restart-confirmation').evaluate(el=>el.open),false);
  await page.locator('.module-nav [data-module="update"]').click();await page.locator('#integration-bootloader').click();
  await page.screenshot({path:fileURLToPath(new URL('uf2-dialog-'+width+'.png',evidence))});
  await page.locator('#uf2-download-cancel').click();assert.equal(await page.locator('#uf2-download-confirmation').evaluate(el=>el.open),false);
  await page.locator('#integration-bootloader').click();await page.locator('#uf2-download-confirm').click();assert.equal(await page.locator('#notice').textContent(),'这里只演示下载模式确认，未操作设备。');
 }
 assert.deepEqual(errors,[]);assert.equal(requests.some(r=>r.path.startsWith('/src/')||r.method!=='GET'),false);report.browser_errors=errors;report.production_modules_loaded=false;report.passed=true;
 await writeFile(new URL('template-check.json',evidence),JSON.stringify(report,null,2)+'\n');console.log('Fixed template assembly passed: 24 views, 10 exact preview files, 74 production files unchanged.');
}finally{await browser.close();}
