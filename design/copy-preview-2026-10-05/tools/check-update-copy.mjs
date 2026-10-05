import assert from 'node:assert/strict';
import {readFile,writeFile} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {pathToFileURL} from 'node:url';
const {chromium}=await import(pathToFileURL(process.argv[2]).href);
const root=new URL('../',import.meta.url);
const baseline=JSON.parse((await readFile(new URL('evidence/web-baseline.json',root),'utf8')).replace(/^\uFEFF/,''));
for(const entry of baseline)assert.equal(createHash('sha256').update(await readFile(new URL('../../'+entry.path,root))).digest('hex'),entry.sha256);
const browser=await chromium.launch({channel:'msedge',headless:true});
try {
  const page=await browser.newPage({viewport:{width:1440,height:1000}}),errors=[];
  page.on('pageerror',e=>errors.push(String(e)));
  await page.goto('http://localhost:4175/#update',{waitUntil:'networkidle'});
  const step=await page.locator('.update-steps > div:nth-child(2) p').textContent();
  assert.equal(step,'进入后电脑会显示 RPI-RP2 磁盘，操作方法见下方说明。');
  assert.equal(step.includes('BOOT'),false);
  assert.ok((await page.locator('#manual-update-help').textContent()).includes('按住 BOOT'));
  assert.equal(await page.locator('#manual-update').count(),0);
  assert.equal(await page.locator('#integration-bootloader').count(),1);
  assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
  assert.deepEqual(errors,[]);
  await page.screenshot({path:new URL('evidence/annotations-round2/update-copy-final-1440.png',root).pathname.replace(/^\/(\w:)/,'$1'),fullPage:true});
  await writeFile(new URL('evidence/annotations-round2/update-copy-final.json',root),JSON.stringify({passed:true,scope:'Final update copy refinement',production_files_unchanged:baseline.length,step02:step,hardware_tested:false,browser_errors:errors},null,2)+'\n');
  console.log('Final update copy passed; '+baseline.length+' production files unchanged.');
} finally {await browser.close();}
