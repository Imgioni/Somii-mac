import fs from 'node:fs';
let g = fs.readFileSync('gen.mjs', 'utf8');
const old = "    body { margin: 0; font-family: 'Barlow Condensed','Arial Narrow',Arial,sans-serif; }";
if (!g.includes(old)) { console.log('MISS'); process.exit(1); }
const neu = [
  "    body { margin: 0; font-family: 'Barlow Condensed','Arial Narrow',Arial,sans-serif; }",
  "    /* A panel is not a document: dragging a knob must never leave a text selection behind. */",
  "    body { -webkit-user-select: none; user-select: none; -webkit-tap-highlight-color: transparent; }"
].join('\n');
g = g.replace(old, neu);
fs.writeFileSync('gen.mjs', g);
console.log('user-select disabled on the panel');
