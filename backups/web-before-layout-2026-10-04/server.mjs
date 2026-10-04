// 无依赖本地静态服务器，只监听本机。用localhost满足Web Serial安全上下文。
// 不暴露整个工程/固件/参考资料，也不承担设备代理或配置持久化。
import http from 'node:http';
import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
const root=fileURLToPath(new URL('.',import.meta.url));
const port=Number(process.env.FLUXRAIL_WEB_PORT || 4174);
const mime={'.html':'text/html; charset=utf-8','.js':'text/javascript; charset=utf-8','.css':'text/css; charset=utf-8','.json':'application/json','.bin':'application/octet-stream'};
http.createServer(async(req,res)=>{
  try {
    if(req.method!=='GET'&&req.method!=='HEAD'){res.writeHead(405);res.end();return;}
    const url=new URL(req.url,'http://localhost');const route=decodeURIComponent(url.pathname);
    const allowed=route==='/'||route==='/index.html'||route==='/style.css'||route.startsWith('/src/')||route==='/tests/golden.js';
    const target=path.resolve(root,'.'+(route==='/'?'/index.html':route));
    if(!allowed||!target.startsWith(root)||!mime[path.extname(target)]){res.writeHead(404);res.end();return;}
    const body=await readFile(target);
    res.writeHead(200,{'Content-Type':mime[path.extname(target)],'Cache-Control':'no-store','X-Content-Type-Options':'nosniff','Content-Security-Policy':"default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' blob:; connect-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'"});
    res.end(req.method==='HEAD'?undefined:body);
  }catch{res.writeHead(404);res.end('Not found');}
}).listen(port,'127.0.0.1',()=>console.log(`FluxRail: http://localhost:${port}`));
