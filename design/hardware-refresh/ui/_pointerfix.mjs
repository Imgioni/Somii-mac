import fs from 'node:fs';
let n = 0;

// 1. The pop-over layer is an invisible full-page sheet. Without this it sits over the whole
//    panel and swallows every pointer event, so no control on the page can be clicked.
let g = fs.readFileSync('gen.mjs', 'utf8');
const a = "transform:scale(' + FURN + ');transform-origin:0 0;z-index:60;\">' + pops + '</div>';";
const b = "transform:scale(' + FURN + ');transform-origin:0 0;z-index:60;pointer-events:none;\">' + pops + '</div>';";
if (g.includes(a)) { g = g.replace(a, b); fs.writeFileSync('gen.mjs', g); n++; }
else console.log('MISS: gen.mjs wrapper');

// 2. The pop-over itself must still take clicks when it is shown - it is the modal scrim.
let l = fs.readFileSync('lib.mjs', 'utf8');
const c = "width:100%;height:100%;z-index:40;background:rgba(12,13,15,.55);'";
const d = "width:100%;height:100%;z-index:40;pointer-events:auto;background:rgba(12,13,15,.55);'";
if (l.includes(c)) { l = l.replace(c, d); fs.writeFileSync('lib.mjs', l); n++; }
else console.log('MISS: lib.mjs popover root');

console.log(n + '/2 pointer-events fixes applied');
