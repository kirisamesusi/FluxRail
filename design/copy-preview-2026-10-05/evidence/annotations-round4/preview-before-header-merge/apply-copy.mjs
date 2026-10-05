import { readFile, writeFile } from 'node:fs/promises';
import { fileURLToPath, pathToFileURL } from 'node:url';
import path from 'node:path';
import { applyAnnotations } from './apply-annotations.mjs';
import { applyRound3 } from './apply-round3.mjs';
const root = fileURLToPath(new URL('../', import.meta.url));
const edits = JSON.parse((await readFile(path.join(root, 'copy-replacements.json'), 'utf8')).replace(/^\uFEFF/, ''));
const previewEdits = JSON.parse(await readFile(path.join(root, 'preview-edits.json'), 'utf8'));
const evidence = path.join(root, 'evidence', previewEdits.revision);
await (await import('node:fs/promises')).mkdir(evidence, { recursive: true });
const source = await readFile(path.join(root, 'tools/source-shell.html'), 'utf8');
const { chromium } = await import(pathToFileURL(process.argv[2]).href);
const browser = await chromium.launch({ channel:'msedge', headless:true });
try {
  const page = await browser.newPage();
  await page.route('**/*', route => route.abort());
  await page.setContent(source.replace(/<script\b[^>]*>[\s\S]*?<\/script>/gi, ''));
  const applied = await page.evaluate(({ edits, previewEdits }) => {
    const report = [];
    for (const edit of edits) {
      let count = 0;
      const names = [...edit.from.matchAll(/\{([^{}]+)\}/g)].map(match => match[1]);
      const replacementNames = [...edit.to.matchAll(/\{([^{}]+)\}/g)].map(match => match[1]);
      // Fill placeholders only when every new variable is present in the source phrase.
      const template = names.length && replacementNames.every(name => names.includes(name))
        ? new RegExp(edit.from.split(/\{[^{}]+\}/).map(part => part.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')).join('(.+?)') + '$') : null;
      const rewrite = value => {
        if (value.includes(edit.from)) return value.split(edit.from).join(edit.to);
        if (!template) return value;
        return value.replace(template, (...args) => {
          const values = Object.fromEntries(names.map((name, i) => [name, args[i + 1]]));
          return edit.to.replace(/\{([^{}]+)\}/g, (_, name) => values[name]);
        });
      };
      const walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT);
      const texts = []; while(walker.nextNode()) texts.push(walker.currentNode);
      for(const node of texts) {
        if (['SCRIPT','STYLE'].includes(node.parentElement.tagName)) continue;
        const next = rewrite(node.nodeValue);
        if (next !== node.nodeValue) { node.nodeValue = next; count++; }
      }
      for(const el of document.querySelectorAll('[aria-label],[title],[placeholder]')) for(const attr of ['aria-label','title','placeholder']) {
        const value=el.getAttribute(attr);
        if (value) { const next = rewrite(value); if (next !== value) { el.setAttribute(attr, next); count++; } }
      }
      report.push({ ...edit, matches_in_preview:count });
    }
    for (const edit of previewEdits.changes) {
      const target = document.querySelector(edit.selector);
      if (!target) throw Error('Missing preview target: ' + edit.selector);
      if (edit.remove) target.remove();
      else target.textContent = edit.text;
    }
    const script=document.createElement('script');script.src='shell.js';script.defer=true;document.body.append(script);
    return report;
  }, { edits, previewEdits });
  await page.evaluate(applyAnnotations);
  await page.evaluate(applyRound3);
  await writeFile(path.join(root, 'index.html'), await page.content(), 'utf8');
  await writeFile(path.join(evidence, 'applied-copy.json'), JSON.stringify(applied, null, 2));
  await writeFile(path.join(evidence,'page-copy.txt'), await page.evaluate(() => [...document.querySelectorAll('.module-panel')].map(panel => `【${panel.querySelector('h2').textContent}】\n${panel.textContent}`).join('\n\n')));
  console.log(JSON.stringify({ proposed:edits.length, matched:applied.filter(e=>e.matches_in_preview).length, other_runtime_texts:applied.filter(e=>!e.matches_in_preview).length }));
} finally { await browser.close(); }
