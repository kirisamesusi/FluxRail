// Offline image-size/format export; web runtime remains dependency-free.
// Supply an installed Sharp module path as the first argument; no npm install required for Web.
import { createRequire } from 'node:module';
import { readFile, writeFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
const require = createRequire(import.meta.url);
const sharp = require(process.argv[2] || 'sharp');
const assets = fileURLToPath(new URL('../assets/', import.meta.url));
const source = await readFile(path.join(assets, 'fluxrail-mascot-icon-theme-source.png'));
await sharp(path.join(assets, 'fluxrail-mascot-theme-source.png')).resize(256, 256).png().toFile(path.join(assets, 'fluxrail-mascot-brand.png'));
await sharp(path.join(assets, 'fluxrail-mascot-controller-theme-source.png')).resize({ width: 800, withoutEnlargement: true }).png().toFile(path.join(assets, 'fluxrail-mascot-controller.png'));
for (const size of [32, 180, 192]) {
  await sharp(source).resize(size, size).png().toFile(path.join(assets, `fluxrail-icon-${size}.png`));
}
// ICO supports embedded PNG images. Keep 16/32/48 sizes in one Windows-compatible file.
const sizes = [16, 32, 48];
const images = await Promise.all(sizes.map(size => sharp(source).resize(size, size).png().toBuffer()));
const directory = Buffer.alloc(6 + 16 * sizes.length);
directory.writeUInt16LE(1, 2);
directory.writeUInt16LE(sizes.length, 4);
let offset = directory.length;
images.forEach((bytes, i) => {
  const entry = 6 + i * 16;
  directory[entry] = directory[entry + 1] = sizes[i];
  directory.writeUInt16LE(1, entry + 4);
  directory.writeUInt16LE(32, entry + 6);
  directory.writeUInt32LE(bytes.length, entry + 8);
  directory.writeUInt32LE(offset, entry + 12);
  offset += bytes.length;
});
await writeFile(path.join(assets, 'fluxrail-favicon.ico'), Buffer.concat([directory, ...images]));
console.log('Exported mascot brand/controller PNGs, 32/180/192px PNG icons and 16/32/48px ICO.');
