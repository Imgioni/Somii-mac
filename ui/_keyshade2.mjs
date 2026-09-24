import fs from 'node:fs';
let g = fs.readFileSync('gen.mjs', 'utf8');

const start = g.indexOf('  const kw = oct / 7, bw = kw * 0.62, bh = h * 0.62;');
const end   = g.indexOf('  return `<div id="keybed"');
if (start < 0 || end < 0) { console.log('MISS'); process.exit(1); }

const neu = `  const kw = oct / 7, bw = kw * 0.62, bh = h * 0.62;
  const base = 36;                                     // C2

  // A pressed key is shaded by an overlay cut to the key's real outline. Tinting the hit
  // rectangle instead paints a grey slab: a white key's rectangle is the full height of the
  // bed and runs straight across the black keys sitting on top of it. A white key is drawn
  // in two parts - the full-width front, and the narrow tongue that shows between the black
  // keys - with one gradient shared across both so the join carries no seam.
  const GRAD = 'linear-gradient(180deg,rgba(0,0,0,.30) 0%,rgba(0,0,0,.13) 28%,'
    + 'rgba(0,0,0,.10) 72%,rgba(0,0,0,.22) 100%)';
  const shade = (top) => 'background-image:' + GRAD + ';background-size:100% ' + r1(h)
    + 'px;background-position:0 -' + r1(top) + 'px;background-repeat:no-repeat;';

  let press = '', hits = '';
  for (let o = 0; o < 5; o++) {
    // where the black keys of this octave actually sit, so the white tongues can dodge them
    const blacks = BLACK.map(([semi, at]) => {
      const l = o * oct + at * kw - bw / 2;
      return { semi, l, r: l + bw };
    });

    WHITE.forEach((semi, i) => {
      const n = base + o * 12 + semi;
      const L = o * oct + i * kw, R = L + kw;

      // the front of the key, below the black keys: always the full width
      press += \`<div data-press="\${n}" style="position:absolute;\`
        + \`left:\${r1(L)}px;top:\${r1(bh)}px;width:\${r1(kw)}px;height:\${r1(h - bh)}px;\`
        + \`z-index:1;opacity:0;pointer-events:none;\`
        + \`border-radius:0 0 \${r1(kw * 0.10)}px \${r1(kw * 0.10)}px;\` + shade(bh) + \`"></div>\`;

      // the tongue between the neighbouring black keys, if any of it shows
      let tl = L, tr = R;
      for (const b of blacks) {
        if (b.r > tl && b.l <= L) tl = Math.max(tl, b.r);      // black overlapping on the left
        if (b.l < tr && b.r >= R) tr = Math.min(tr, b.l);      // black overlapping on the right
      }
      if (tr - tl > 0.5)
        press += \`<div data-press="\${n}" style="position:absolute;\`
          + \`left:\${r1(tl)}px;top:0;width:\${r1(tr - tl)}px;height:\${r1(bh)}px;\`
          + \`z-index:1;opacity:0;pointer-events:none;\` + shade(0) + \`"></div>\`;

      hits += \`<div data-note="\${n}" style="position:absolute;\`
        + \`left:\${r1(L)}px;top:0;width:\${r1(kw)}px;height:\${h}px;\`
        + \`z-index:2;cursor:pointer;"></div>\`;
    });

    for (const b of blacks) {
      const n = base + o * 12 + b.semi;
      // a black key is already dark, so it only deepens - no highlight, which would read as glare
      press += \`<div data-press="\${n}" style="position:absolute;\`
        + \`left:\${r1(b.l)}px;top:0;width:\${r1(bw)}px;height:\${r1(bh)}px;\`
        + \`z-index:1;opacity:0;pointer-events:none;border-radius:0 0 \${r1(bw * 0.16)}px \${r1(bw * 0.16)}px;\`
        + \`background:linear-gradient(180deg,rgba(0,0,0,.50),rgba(0,0,0,.26) 55%,rgba(0,0,0,.40));\`
        + \`box-shadow:inset 0 2px 6px rgba(0,0,0,.45);"></div>\`;
      hits += \`<div data-note="\${n}" style="position:absolute;\`
        + \`left:\${r1(b.l)}px;top:0;width:\${r1(bw)}px;height:\${r1(bh)}px;\`
        + \`z-index:3;cursor:pointer;"></div>\`;
    }
  }
`;

g = g.slice(0, start) + neu + g.slice(end);
fs.writeFileSync('gen.mjs', g);
console.log('key shading cut to the real key outline');
