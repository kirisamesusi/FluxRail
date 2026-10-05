import http from 'node:http';
import path from 'node:path';
import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
const root = fileURLToPath(new URL('.', import.meta.url));
const mime = { '.html':'text/html; charset=utf-8', '.js':'text/javascript; charset=utf-8', '.css':'text/css; charset=utf-8', '.png':'image/png', '.ico':'image/vnd.microsoft.icon' };
const files = new Set(['/index.html','/style.css','/preview.css','/shell.js','/assets/fluxrail-mascot-brand.png','/assets/fluxrail-mascot-controller.png','/assets/fluxrail-icon-32.png','/assets/fluxrail-icon-180.png','/assets/fluxrail-icon-192.png','/assets/fluxrail-favicon.ico']);
http.createServer(async (req,res) => {
  try {
    if (!['GET','HEAD'].includes(req.method)) { res.writeHead(405); res.end(); return; }
    let route = new URL(req.url,'http://localhost').pathname;
    if (route === '/') route = '/index.html';
    if (route === '/favicon.ico') route = '/assets/fluxrail-favicon.ico';
    if (!files.has(route)) { res.writeHead(404); res.end(); return; }
    const body = await readFile(path.join(root, route.slice(1)));
    res.writeHead(200, {'Content-Type':mime[path.extname(route)],'Cache-Control':'no-store','X-Content-Type-Options':'nosniff','Content-Security-Policy':"default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self'; connect-src 'none'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'"});
    res.end(req.method === 'HEAD' ? undefined : body);
  } catch { res.writeHead(404); res.end(); }
}).listen(Number(process.env.FLUXRAIL_COPY_PORT || 4175), '127.0.0.1', () => console.log('FluxRail 文案预览：http://localhost:4175'));
