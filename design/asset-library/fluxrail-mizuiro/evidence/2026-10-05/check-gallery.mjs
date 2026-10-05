import assert from 'node:assert/strict';
import { access, writeFile } from 'node:fs/promises';
import { fileURLToPath, pathToFileURL } from 'node:url';
import path from 'node:path';
const evidence=fileURLToPath(new URL('.',import.meta.url));
try { await access(path.join(evidence,'gallery-check.json')); throw new Error('Evidence already exists.'); }
catch(error) { if(error.code!=='ENOENT')throw error; }
const { chromium }=await import(pathToFileURL(process.argv[2]).href);
const browser=await chromium.launch({channel:'msedge',headless:true});
const report={date_local:'2026-10-05',scope:'offline asset library only',browser:await browser.version(),views:[],groups:{},page_errors:[],network_requests:[]};
try {
  const page=await browser.newPage();
  page.on('pageerror',error=>report.page_errors.push(String(error)));
  page.on('request',request=>{if(/^https?:/.test(request.url()))report.network_requests.push(request.url());});
  for(const [width,height,name] of [[1440,1000,'desktop'],[390,844,'mobile']]) {
    await page.setViewportSize({width,height});
    await page.goto(new URL('../../index.html',import.meta.url).href,{waitUntil:'networkidle'});
    await page.evaluate(()=>Promise.all([...document.images].map(img=>img.decode())));
    const state=await page.evaluate(()=>({loaded:[...document.images].filter(img=>img.complete&&img.naturalWidth>0).length,visible:document.querySelectorAll('.card:not([hidden])').length,overflow:document.documentElement.scrollWidth>innerWidth}));
    assert.equal(state.loaded,18);assert.equal(state.visible,3);assert.equal(state.overflow,false);
    report.views.push({width,height,...state});
    await page.screenshot({path:path.join(evidence,`gallery-${name}.png`),fullPage:true});
  }
  for(const [group,count] of Object.entries({recommended:3,original:4,exploration:3,export:6,reference:2,all:18})) {
    await page.locator(`[data-filter="${group}"]`).click();
    assert.equal(await page.locator('.card:not([hidden])').count(),count);
    report.groups[group]=count;
  }
  assert.deepEqual(report.page_errors,[]);assert.deepEqual(report.network_requests,[]);
  report.passed=true;
  await writeFile(path.join(evidence,'gallery-check.json'),JSON.stringify(report,null,2),{flag:'wx'});
  console.log(JSON.stringify(report));
} finally {await browser.close();}
