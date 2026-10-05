import fs from 'node:fs';
let g = fs.readFileSync('gen.mjs', 'utf8');

const a = g.indexOf('body += lowerLeft(44, 538);');
const b = g.indexOf('const shifted =');
if (a < 0 || b < 0) { console.log('markers not found'); process.exit(1); }

const q = String.fromCharCode(39);   // single quote, kept out of the template
const tail = [
  'body += lowerLeft(44, 538);',
  'body += vrule(408, 536, 430);',
  '// Wordmark is panel furniture, so it lives inside the scaled group.',
  'body += wordmark(546, PR);',
  '',
  '// Everything above is furniture and scales together.',
  'body = ' + q + '<div style="position:absolute;left:0;top:0;width:' + q + ' + BASEW + ' + q + 'px;height:986px;' + q + '',
  '     + ' + q + 'transform:scale(' + q + ' + FURN + ' + q + ');transform-origin:0 0;">' + q + ' + body + ' + q + '</div>' + q + ';',
  '',
  '// Ribbon and keybed keep their own size, so the furniture reads larger against them.',
  '// Both sit in the scaled coordinate space and share the same x range.',
  'const sc = (n) => Math.round(n * FURN);',
  'const KBX = sc(146), KBR = sc(PR);',
  'body += ribbonRow(sc(546), KBX, sc(PR - 330) - KBX - sc(30));',
  '',
  '// The keybed runs to the very bottom edge - no panel strip beneath it.',
  'body += keyboard(KBX, sc(616), KBR - KBX, H - sc(616));',
  '',
  ''
].join('\n');

g = g.slice(0, a) + tail + g.slice(b);
g = g.replace('function ribbonRow(y, rx, rw, rightEdge) {', 'function ribbonRow(y, rx, rw) {');
fs.writeFileSync('gen.mjs', g);
console.log('assembly tail rewritten');
