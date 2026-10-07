// Export only browser runtime files; never publish the repository or test evidence.
import { copyFile, mkdir, readdir, lstat } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const web = fileURLToPath(new URL('../', import.meta.url));
const target = process.argv[2];
if (!target) throw new Error('Usage: node web/deploy/prepare-site.mjs <new-output-directory>');
const output = path.resolve(target);
// mkdir without recursive deliberately refuses an existing output directory.
await mkdir(output);
const assets = [
  'fluxrail-lace-bunny.png', 'fluxrail-mascot-brand.png',
  'fluxrail-mascot-controller.png', 'fluxrail-icon-32.png',
  'fluxrail-icon-180.png', 'fluxrail-icon-192.png', 'fluxrail-favicon.ico',
];
const files = ['index.html', 'style.css', 'tests/golden.js', ...assets.map(name => `assets/${name}`)];
async function sourceFiles(directory = 'src') {
  for (const entry of await readdir(path.join(web, directory), { withFileTypes: true })) {
    const relative = `${directory}/${entry.name}`;
    if (entry.isDirectory()) await sourceFiles(relative);
    else if (entry.isFile() && entry.name.endsWith('.js')) files.push(relative);
    else throw new Error(`Unexpected source entry: ${relative}`);
  }
}
await sourceFiles();
for (const relative of files) {
  const source = path.join(web, relative);
  if (!(await lstat(source)).isFile()) throw new Error(`Expected regular file: ${relative}`);
  const destination = path.join(output, relative);
  await mkdir(path.dirname(destination), { recursive: true });
  await copyFile(source, destination);
}
await copyFile(path.join(web, '../LICENSE'), path.join(output, 'LICENSE.txt'));
console.log(`Exported ${files.length + 1} runtime/license files to ${output}`);
