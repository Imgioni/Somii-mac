import fs from 'node:fs';
let g = fs.readFileSync('gen.mjs','utf8');
const bad = `const LBL = s(206);                      // knob/switch label baseline
// The shift pill clears a whole label line rather than a constant that predates TEXT.
const SUB = LBL + LROW + 4;              // knob/switch secondary-label baseline
const LROW = Math.ceil(8.5 * TEXT * 1.05);   // reserved height of one label row`;
const good = `const LBL = s(206);                      // knob/switch label baseline
// The shift pill clears a whole label line rather than a constant that predates TEXT:
// the captions were scaled by TEXT while the slot under them stayed 12px.
const LROW = Math.ceil(8.5 * TEXT * 1.05);   // reserved height of one label row
const SUB = LBL + LROW + 4;              // knob/switch secondary-label baseline`;
if (!g.includes(bad)) { console.log('MISS'); process.exit(1); }
fs.writeFileSync('gen.mjs', g.replace(bad, good));
console.log('declaration order fixed');
