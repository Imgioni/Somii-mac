import fs from 'node:fs';
let g = fs.readFileSync('gen.mjs', 'utf8');
let n = 0;
const sub = (a, b) => { if (!g.includes(a)) { console.log('MISS:', a.slice(0, 64)); return; } g = g.replace(a, b); n++; };

// The whole bottom-left block is per layer on the hardware, so like the global strip
// it follows the LAYER keys. OCTAVE is a five-way choice, so the switch that sat beside
// the LED row now drives it (the LEDs stay as the read-out).

// ---- PERFORMANCE band ------------------------------------------------------
sub(`    { w: wSw3(40), r: (bx) => sw3({ x: bx, y: c1 - 20, h: 40, pos: 1, opts: ['OCT +', '0', 'OCT -'], label: 'TRANSPOSE', ly: l1 }) },`,
    `    { w: wSw3(40), r: (bx) => sw3({ x: bx, y: c1 - 20, h: 40, pos: 1, opts: ['OCT +', '0', 'OCT -'], label: 'OCTAVE', ly: l1, id: 'octave', layered: true, steps: 5 }) },`);

sub(`    { w: wFader, r: (bx) => fader({ x: bx + 4, y: c1 - 44, h: 88, sw: SWD, cap: 'grey', val: 0.2, label: 'PORTAMENTO', ly: l1 }) },`,
    `    { w: wFader, r: (bx) => fader({ x: bx + 4, y: c1 - 44, h: 88, sw: SWD, cap: 'grey', val: 0.2, label: 'PORTAMENTO', ly: l1, id: 'porta.time', layered: true }) },`);

// GLIDE has no parameter of its own - portamento is off at 0 - so it stays a legend.

// ---- LFO 2 band ------------------------------------------------------------
sub(`    { w: wRotary(34, 15), r: (bx) => rotary({ x: bx + (wRotary(34, 15) - 34) / 2, y: c2 - 17, d: 34, sel: 0, steps: ['SIN', 'SAW', 'S&H', 'SQR', 'TRI', 'NSE'], label: 'WAVE', ly: lb2, ssize: 5.8 }) },
    { w: wKnob(40), r: (bx) => knob({ x: bx + kpad(40), y: c2 - 20, d: 40, v: 'cream', val: 0.5, ticks: 11, label: 'RATE', ly: lb2 }) },
    { w: wKnob(40), r: (bx) => knob({ x: bx + kpad(40), y: c2 - 20, d: 40, v: 'cream', val: 0.12, ticks: 11, label: 'DELAY', ly: lb2 }) },`,
`    { w: wRotary(34, 15), r: (bx) => rotary({ x: bx + (wRotary(34, 15) - 34) / 2, y: c2 - 17, d: 34, sel: 0, steps: ['SIN', 'RSAW', 'S&H', 'SQR', 'SAW', 'NSE'], label: 'WAVE', ly: lb2, ssize: 5.8, id: 'lfo2.wave', layered: true }) },
    { w: wKnob(40), r: (bx) => knob({ x: bx + kpad(40), y: c2 - 20, d: 40, v: 'cream', val: 0.5, ticks: 11, label: 'RATE', ly: lb2, id: 'lfo2.rate', layered: true }) },
    { w: wKnob(40), r: (bx) => knob({ x: bx + kpad(40), y: c2 - 20, d: 40, v: 'cream', val: 0.12, ticks: 11, label: 'DELAY', ly: lb2, id: 'lfo2.delay', layered: true }) },`);

// The upper switch of that column is the oscillator destination; the lower one selects
// which layer the performance section drives, which is perf.portaLayer (BOTH|LOWER|UPPER).
sub(`      r: (bx) => sw3({ x: bx, y: c2 - 42, h: 34, pos: 1, opts: ['DDS 2', '1+2', 'DDS 1'], label: '', ly: 0 })
             + sw3({ x: bx, y: c2 + 4, h: 34, pos: 2, opts: ['BOTH', 'UPPER', 'LOWER'], label: 'DESTINATION', ly: lb2 })`,
`      r: (bx) => sw3({ x: bx, y: c2 - 42, h: 34, pos: 1, opts: ['DDS 2', '1+2', 'DDS 1'], label: '', ly: 0, id: 'dest.osc', layered: true })
             + sw3({ x: bx, y: c2 + 4, h: 34, pos: 2, opts: ['UPPER', 'LOWER', 'BOTH'], label: 'DESTINATION', ly: lb2, id: 'perf.portaLayer' })`);

// ---- DEPTH band ------------------------------------------------------------
sub(`  const DF = (v, lbl, cap) => ({
    w: wFader,
    r: (bx) => fader({ x: bx + 4, y: c3 - 46, h: 92, sw: SWD, cap: cap || 'grey', val: v, label: lbl, ly: lb3 })
  });`,
`  const DF = (v, lbl, cap, id) => ({
    w: wFader,
    r: (bx) => fader({ x: bx + 4, y: c3 - 46, h: 92, sw: SWD, cap: cap || 'grey', val: v, label: lbl, ly: lb3,
                       id: id, layered: id ? true : false })
  });`);

sub(`    DF(0.30, 'LFO 2 RATE'), DF(0.20, 'DDS'), DF(0.40, 'VCF'), DF(0.10, 'VCA'),
    { w: 14, r: (bx) => vrule(bx + 7, c3 - 52, 108) },
    DF(0.50, 'DDS', 'orange'), DF(0.60, 'VCF', 'orange'),
    { w: 8, r: () => '' },
    { w: wSw3(34), r: (bx) => sw3({ x: bx, y: c3 - 17, h: 34, pos: 2, opts: ['ON', 'AT+T', 'TRIG'], label: 'TRIG', ly: lb3 }) }`,
`    DF(0.30, 'LFO 2 RATE', null, 'lfo2.rateMod'),
    DF(0.20, 'DDS', null, 'lfo2.ddsAmt'),
    DF(0.40, 'VCF', null, 'lfo2.vcfAmt'),
    DF(0.10, 'VCA', null, 'lfo2.vcaAmt'),
    { w: 14, r: (bx) => vrule(bx + 7, c3 - 52, 108) },
    DF(0.50, 'DDS', 'orange', 'bender.ddsAmt'),
    DF(0.60, 'VCF', 'orange', 'bender.vcfAmt'),
    { w: 8, r: () => '' },
    { w: wSw3(34), r: (bx) => sw3({ x: bx, y: c3 - 17, h: 34, pos: 2, opts: ['ON', 'AT+T', 'TRIG'], label: 'TRIG', ly: lb3, id: 'lfo2.trigger', layered: true }) }`);

fs.writeFileSync('gen.mjs', g);
console.log(`${n}/7 performance-block groups wired`);
