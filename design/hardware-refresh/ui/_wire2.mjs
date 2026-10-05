import fs from 'node:fs';

// ---- geminus.js: resync must read toggles with getValue, not getChoiceIndex ----
let j = fs.readFileSync('geminus.js', 'utf8');
j = j.replace(
`      const v = el.dataset.juceType === 'slider'
        ? st.getNormalisedValue()
        : st.getChoiceIndex() / (parseInt(el.dataset.steps || '3', 10) - 1);`,
`      const t = el.dataset.juceType;
      const v = t === 'slider' ? st.getNormalisedValue()
              : t === 'toggle' ? (st.getValue() ? 1 : 0)
              : st.getChoiceIndex() / (parseInt(el.dataset.steps || '3', 10) - 1);`);
fs.writeFileSync('geminus.js', j);

// ---- gen.mjs: global strip ----------------------------------------------------
let g = fs.readFileSync('gen.mjs', 'utf8');
let n = 0;
const sub = (a, b) => { if (!g.includes(a)) { console.log('MISS:', a.slice(0, 64)); return; } g = g.replace(a, b); n++; };

// MANUAL is per layer, one button each - unambiguous
sub(`  o += button({ x: 46, y: B + 12, w: 26, on: false, label: 'LOWER' });
  o += button({ x: 82, y: B + 12, w: 26, on: false, label: 'UPPER' });`,
`  o += button({ x: 46, y: B + 12, w: 26, on: false, label: 'LOWER', id: 'lower.manual' });
  o += button({ x: 82, y: B + 12, w: 26, on: false, label: 'UPPER', id: 'upper.manual' });`);

sub(`  o += knob({ x: 152, y: B + 8, d: 32, v: 'cream', val: 0.42, ticks: 11, label: '', lsize: 7.4 });`,
    `  o += knob({ x: 152, y: B + 8, d: 32, v: 'cream', val: 0.42, ticks: 11, label: '', lsize: 7.4, id: 'perf.tempo' });`);

// HOLD is per layer
sub(`  o += button({ x: 222, y: B + 12, w: 26, on: false, label: 'LOWER' });
  o += button({ x: 258, y: B + 12, w: 26, on: false, label: 'UPPER' });`,
`  o += button({ x: 222, y: B + 12, w: 26, on: false, label: 'LOWER', id: 'lower.hold' });
  o += button({ x: 258, y: B + 12, w: 26, on: false, label: 'UPPER', id: 'upper.hold' });`);

// KEYBOARD MODE: three keys, one choice (SINGLE | DUAL | SPLIT)
sub(`  o += button({ x: 316, y: B + 12, w: 26, on: true, label: 'SINGLE' });
  o += button({ x: 352, y: B + 12, w: 26, on: false, label: 'DUAL' });
  o += button({ x: 388, y: B + 12, w: 26, on: false, label: 'SPLIT' });`,
`  o += button({ x: 316, y: B + 12, w: 26, on: true,  label: 'SINGLE', id: 'perf.keyboardMode', domId: 'perf.keyboardMode__0', value: 0, steps: 3 });
  o += button({ x: 352, y: B + 12, w: 26, on: false, label: 'DUAL',   id: 'perf.keyboardMode', domId: 'perf.keyboardMode__1', value: 1, steps: 3 });
  o += button({ x: 388, y: B + 12, w: 26, on: false, label: 'SPLIT',  id: 'perf.keyboardMode', domId: 'perf.keyboardMode__2', value: 2, steps: 3 });`);

// LAYER: which layer plays in SINGLE (UPPER | LOWER)
sub(`  o += button({ x: 458, y: B + 14, w: 26, on: false, label: 'LOWER', dark: true });
  o += button({ x: 500, y: B + 14, w: 26, on: true, label: 'UPPER', dark: true });`,
`  o += button({ x: 458, y: B + 14, w: 26, on: false, label: 'LOWER', dark: true, id: 'perf.singleLayer', domId: 'perf.singleLayer__1', value: 1, steps: 2 });
  o += button({ x: 500, y: B + 14, w: 26, on: true,  label: 'UPPER', dark: true, id: 'perf.singleLayer', domId: 'perf.singleLayer__0', value: 0, steps: 2 });`);

fs.writeFileSync('gen.mjs', g);
console.log(`global strip: ${n}/5 groups wired`);
