import fs from 'node:fs';

// ---- gen.mjs: give every key a press overlay shaped like the key, not a flat slab ----
let g = fs.readFileSync('gen.mjs', 'utf8');
const old = g.slice(g.indexOf('  const kw = oct / 7, bw = kw * 0.62, bh = h * 0.62;'),
                    g.indexOf('  return `<div id="keybed"'));
if (!old.includes('let hits')) { console.log('MISS keyboard body'); process.exit(1); }

const neu = `  const kw = oct / 7, bw = kw * 0.62, bh = h * 0.62;
  const base = 36;                                     // C2
  // A pressed key is shaded by an overlay cut to the shape that is actually visible.
  // Tinting the hit rectangle instead paints a grey slab: a white key's rectangle runs
  // the full height of the bed, straight across the black keys sitting on top of it.
  const WPRESS = 'background:linear-gradient(180deg,rgba(0,0,0,.30),rgba(0,0,0,.11) 35%,rgba(0,0,0,.22));'
    + 'box-shadow:inset 0 2px 5px rgba(0,0,0,.28);';
  const BPRESS = 'background:linear-gradient(180deg,rgba(0,0,0,.40),rgba(0,0,0,.18) 45%,rgba(255,255,255,.06));'
    + 'box-shadow:inset 0 2px 6px rgba(0,0,0,.45);';
  let press = '', hits = '';
  for (let o = 0; o < 5; o++) {
    WHITE.forEach((semi, i) => {
      const n = base + o * 12 + semi;
      // only the part below the black keys is exposed, so the shading stops there
      press += \`<div data-press="\${n}" style="position:absolute;\`
        + \`left:\${r1(o * oct + i * kw)}px;top:\${r1(bh)}px;width:\${r1(kw)}px;height:\${r1(h - bh)}px;\`
        + \`z-index:1;opacity:0;pointer-events:none;border-radius:0 0 \${r1(kw * 0.10)}px \${r1(kw * 0.10)}px;\`
        + WPRESS + \`"></div>\`;
      hits += \`<div data-note="\${n}" style="position:absolute;\`
        + \`left:\${r1(o * oct + i * kw)}px;top:0;width:\${r1(kw)}px;height:\${h}px;\`
        + \`z-index:2;cursor:pointer;"></div>\`;
    });
    for (const [semi, at] of BLACK) {
      const n = base + o * 12 + semi;
      press += \`<div data-press="\${n}" style="position:absolute;\`
        + \`left:\${r1(o * oct + at * kw - bw / 2)}px;top:0;width:\${r1(bw)}px;height:\${r1(bh)}px;\`
        + \`z-index:1;opacity:0;pointer-events:none;border-radius:0 0 \${r1(bw * 0.16)}px \${r1(bw * 0.16)}px;\`
        + BPRESS + \`"></div>\`;
      hits += \`<div data-note="\${n}" style="position:absolute;\`
        + \`left:\${r1(o * oct + at * kw - bw / 2)}px;top:0;width:\${r1(bw)}px;height:\${r1(bh)}px;\`
        + \`z-index:3;cursor:pointer;"></div>\`;
    }
  }
`;
g = g.replace(old, neu);
g = g.replace('overflow:hidden;touch-action:none;">${keys}${hits}</div>`;',
              'overflow:hidden;touch-action:none;">${keys}${press}${hits}</div>`;');
fs.writeFileSync('gen.mjs', g);

// ---- geminus.js: drive the overlay, not the hit rectangle ----
let j = fs.readFileSync('geminus.js', 'utf8');
const oldLit = `  // dim the key under the finger so the press is visible
  const lit = (n, on) => {
    for (const k of bed.querySelectorAll('[data-note="' + n + '"]'))
      k.style.background = on ? 'rgba(0,0,0,.30)' : '';
  };`;
if (!j.includes(oldLit)) { console.log('MISS lit'); process.exit(1); }
const newLit = `  // shade the key under the finger; the overlay is shaped to the key, the hit rect is not
  const lit = (n, on) => {
    for (const k of bed.querySelectorAll('[data-press="' + n + '"]'))
      k.style.opacity = on ? '1' : '0';
  };`;
j = j.replace(oldLit, newLit);
fs.writeFileSync('geminus.js', j);
console.log('key press shading reworked');
