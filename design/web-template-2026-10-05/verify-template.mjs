import {readFile} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {fileURLToPath} from 'node:url';
import path from 'node:path';
const root=fileURLToPath(new URL('./',import.meta.url));
const manifest=JSON.parse(await readFile(path.join(root,'manifest.json'),'utf8'));
let checked=0;
for(const file of manifest.files){
  const resolved=path.resolve(root,file.path);
  if(!resolved.startsWith(root))throw Error('File outside template: '+file.path);
  const data=await readFile(resolved);
  if(data.length!==file.bytes||createHash('sha256').update(data).digest('hex')!==file.sha256)throw Error('Template changed: '+file.path);
  checked++;
}
console.log('Fixed template verified: '+checked+' files. This is file integrity, not device validation.');
