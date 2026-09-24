import fs from 'node:fs';
let g = fs.readFileSync('gen.mjs','utf8'); let n=0;
const sub=(a,b,tag)=>{if(!g.includes(a)){console.log('MISS '+tag);return;} g=g.replace(a,b); n++;};

// WAVEFORM/RANGE captions: enough to clear the dial legends, not so much that they
// drop onto the sub-row of controls beneath them
sub("const rd = 46, rcy = bt + 66, rly = bt + 114;", "const rd = 46, rcy = bt + 66, rly = bt + 110;", 'rly');

// ENV 1 columns: the AH / DH shift pills were touching across the column gap
sub(`    EF(0.66, 'S', null, null, 'env1.sustain'), EF(0.30, 'R', 'orange', null, 'env1.release')
  ], 5);`,
`    EF(0.66, 'S', null, null, 'env1.sustain'), EF(0.30, 'R', 'orange', null, 'env1.release')
  ], 9);`, 'env1 gap');

// MANUAL: keep the pill where it was and hold its two captions tight instead, so the
// group stays clear of the PERFORMANCE heading below it
sub("o += button({ x: 46, y: B + 12, w: 26, on: false, label: 'LOWER', id: 'lower.manual' });",
    "o += button({ x: 46, y: B + 12, w: 26, on: false, label: 'LOWER', ly: B + 34, id: 'lower.manual' });", 'manual lower');
sub("o += button({ x: 82, y: B + 12, w: 26, on: false, label: 'UPPER', id: 'upper.manual' });",
    "o += button({ x: 82, y: B + 12, w: 26, on: false, label: 'UPPER', ly: B + 34, id: 'upper.manual' });", 'manual upper');
sub("o += txtInv(44, B + 58, 72, 'INIT PATCH', { size: 6.6 });",
    "o += txtInv(44, B + 50, 72, 'INIT PATCH', { size: 6.6 });", 'init patch');

fs.writeFileSync('gen.mjs', g);
console.log(n + '/6 applied');
