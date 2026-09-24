import fs from 'node:fs';
let g = fs.readFileSync('gen.mjs', 'utf8');
let n = 0;
const sub = (a, b, tag) => { if (!g.includes(a)) { console.log('MISS ' + tag); return; } g = g.replace(a, b); n++; };

// Geometry measured off octave.png, not assumed. The black keys straddle the white-key
// seams; the old table put them a quarter of a white key to the left, so every black key's
// hit area and press shading sat beside the key in the photograph instead of on it.
sub(
`  const WHITE = [0, 2, 4, 5, 7, 9, 11];
  const BLACK = [[1, 0.68], [3, 1.72], [6, 3.66], [8, 4.70], [10, 5.74]];
  const kw = oct / 7, bw = kw * 0.62, bh = h * 0.62;`,
`  const WHITE = [0, 2, 4, 5, 7, 9, 11];
  const WSEAM = [0, 0.962, 1.940, 2.946, 3.938, 4.944, 5.965, 6.985];   // white-key seams
  const BLACK = [[1, 0.955], [3, 2.078], [6, 3.916], [8, 5.031], [10, 6.154]];
  const kw = oct / 7, bw = kw * 0.52, bh = h * 0.569;`,
  'geometry table');

// white keys follow the measured seams rather than an even seven-way split
sub('      const L = o * oct + i * kw, R = L + kw;',
    '      const L = o * oct + WSEAM[i] * kw, R = o * oct + WSEAM[i + 1] * kw, ww = R - L;',
    'white span');
sub(`        + \`left:\${r1(L)}px;top:\${r1(bh)}px;width:\${r1(kw)}px;height:\${r1(h - bh)}px;\`
        + \`z-index:1;opacity:0;pointer-events:none;\`
        + \`border-radius:0 0 \${r1(kw * 0.10)}px \${r1(kw * 0.10)}px;\` + shade(bh) + \`"></div>\`;`,
    `        + \`left:\${r1(L)}px;top:\${r1(bh)}px;width:\${r1(ww)}px;height:\${r1(h - bh)}px;\`
        + \`z-index:1;opacity:0;pointer-events:none;\`
        + \`border-radius:0 0 \${r1(ww * 0.10)}px \${r1(ww * 0.10)}px;\` + shade(bh) + \`"></div>\`;`,
    'white front');
sub(`        + \`left:\${r1(L)}px;top:0;width:\${r1(kw)}px;height:\${h}px;\`
        + \`z-index:2;cursor:pointer;"></div>\`;`,
    `        + \`left:\${r1(L)}px;top:0;width:\${r1(ww)}px;height:\${h}px;\`
        + \`z-index:2;cursor:pointer;"></div>\`;`,
    'white hit');

fs.writeFileSync('gen.mjs', g);
console.log(n + '/4 applied');
