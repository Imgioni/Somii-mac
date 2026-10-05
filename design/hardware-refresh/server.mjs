import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
// The approved result is knobs only, on the actual production panel.
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../../ui');
http.createServer((req,res)=>{
  let file;
  try {const p=new URL(req.url,'http://localhost').pathname;file=path.resolve(root,'.'+decodeURIComponent(p==='/'?'/index.html':p));}
  catch {res.writeHead(400).end();return;}
  if(!file.startsWith(root+path.sep)||!fs.existsSync(file)||!fs.statSync(file).isFile()){res.writeHead(404).end();return;}
  const mime={'.html':'text/html; charset=utf-8','.png':'image/png','.svg':'image/svg+xml','.jpg':'image/jpeg','.webp':'image/webp','.woff2':'font/woff2','.js':'text/javascript','.mjs':'text/javascript','.css':'text/css','.json':'application/json'};
  res.writeHead(200,{'Content-Type':mime[path.extname(file)]||'text/plain','Cache-Control':'no-store'});
  fs.createReadStream(file).pipe(res);
}).listen(5180,'127.0.0.1',()=>console.log('Hardware refresh: http://127.0.0.1:5180'));
