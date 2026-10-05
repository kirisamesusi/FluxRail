import assert from 'node:assert/strict';
import {readFile,writeFile,access} from 'node:fs/promises';
import {pathToFileURL} from 'node:url';
import path from 'node:path';
import crypto from 'node:crypto';
const {chromium}=await import(pathToFileURL(process.argv[2]).href);
const browser=await chromium.launch({channel:'msedge',headless:true});
try{
for(const folder of ["design/asset-library/fluxrail-mizuiro/collections/full-body-2026-10-05","design/asset-library/fluxrail-mizuiro/collections/full-body-white-2026-10-05"]){
const root=path.resolve(folder),evidence=path.join(root,'evidence'),output=path.join(evidence,'gallery-check.json');
try{await access(output);throw Error('Evidence exists: '+output);}catch(e){if(e.code!=='ENOENT')throw e;}
const catalog=JSON.parse(await readFile(path.join(root,'catalog.json'),'utf8'));
for(const asset of catalog.assets){const b=await readFile(path.join(root,asset.path));assert.equal(crypto.createHash('sha256').update(b).digest('hex'),asset.sha256);}
const report={passed:false,scope:'offline full-body art collection only',browser:await browser.version(),views:[],page_errors:[],network_requests:[]};
const page=await browser.newPage();
page.on('pageerror',e=>report.page_errors.push(String(e)));
page.on('request',r=>{if(/^https?:/.test(r.url()))report.network_requests.push(r.url());});
for(const [width,height,name] of [[1440,950,'desktop'],[390,844,'mobile']]){
await page.setViewportSize({width,height});
await page.goto(pathToFileURL(path.join(root,'index.html')).href,{waitUntil:'networkidle'});
await page.evaluate(()=>Promise.all([...document.images].map(i=>i.decode())));
const state=await page.evaluate(()=>({loaded:[...document.images].filter(i=>i.complete&&i.naturalWidth===1024&&i.naturalHeight===1536).length,overflow:document.documentElement.scrollWidth>innerWidth}));
assert.equal(state.loaded,3);assert.equal(state.overflow,false);report.views.push({width,height,...state});
await page.screenshot({path:path.join(evidence,'gallery-'+name+'.png'),fullPage:true});
}
assert.deepEqual(report.page_errors,[]);assert.deepEqual(report.network_requests,[]);report.passed=true;
await writeFile(output,JSON.stringify(report,null,2)+'\n',{flag:'wx'});console.log(JSON.stringify({folder,...report}));
await page.close();
}
}finally{await browser.close();}

