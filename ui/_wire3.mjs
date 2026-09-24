import fs from 'node:fs';
let g = fs.readFileSync('gen.mjs', 'utf8');
let n = 0;
const sub = (a, b) => { if (!g.includes(a)) { console.log('MISS:', a.slice(0, 60)); return; } g = g.replace(a, b); n++; };

// VOICE ASSIGN - mode is a 4-way choice shown as one key plus four LEDs; the key steps it.
sub(`  o += button({ x: 564, y: B + 12, w: 26, on: true, label: 'MODE' });`,
    `  o += button({ x: 564, y: B + 12, w: 26, on: true, label: 'MODE', id: 'voice.mode', layered: true, cycle: true, steps: 4 });`);
sub(`  o += button({ x: 664, y: B + 12, w: 26, on: false, label: 'UNISON' });
  o += button({ x: 700, y: B + 12, w: 26, on: true, label: 'BINAURAL' });`,
`  o += button({ x: 664, y: B + 12, w: 26, on: false, label: 'UNISON', id: 'voice.unison', layered: true });
  o += button({ x: 700, y: B + 12, w: 26, on: true, label: 'BINAURAL', id: 'voice.binaural', layered: true });`);

// ARPEGGIATOR / SEQUENCER
sub(`  o += button({ x: 760, y: B + 14, w: 26, on: false, label: 'ON' });
  o += knob({ x: 798, y: B + 10, d: 30, v: 'cream', val: 0.5, ticks: 9, label: 'CLK DIV', lsize: 7.4 });
  o += button({ x: 846, y: B + 14, w: 26, on: false, label: 'SYNC' });
  o += button({ x: 882, y: B + 14, w: 26, on: false, label: 'RANGE' });
  o += button({ x: 918, y: B + 14, w: 26, on: false, label: 'MODE' });`,
`  o += button({ x: 760, y: B + 14, w: 26, on: false, label: 'ON', id: 'arp.on', layered: true });
  o += knob({ x: 798, y: B + 10, d: 30, v: 'cream', val: 0.5, ticks: 9, label: 'CLK DIV', lsize: 7.4, id: 'arp.clockDiv', layered: true, hitType: 'combo', hitData: { steps: 16 } });
  o += button({ x: 846, y: B + 14, w: 26, on: false, label: 'SYNC', id: 'arp.sync', layered: true });
  o += button({ x: 882, y: B + 14, w: 26, on: false, label: 'RANGE', id: 'arp.range', layered: true, cycle: true, steps: 4 });
  o += button({ x: 918, y: B + 14, w: 26, on: false, label: 'MODE', id: 'arp.mode', layered: true, cycle: true, steps: 5 });`);

// CHORUS: two keys, one four-way choice (OFF | I | II | I+II)
sub(`  o += button({ x: 1608, y: B + 14, w: 26, on: true, label: 'I' });
  o += button({ x: 1644, y: B + 14, w: 26, on: false, label: 'II' });`,
`  o += button({ x: 1608, y: B + 14, w: 26, on: true,  label: 'I',  id: 'fx.chorus', layered: true, domId: 'fx.chorus__1', value: 1, steps: 4 });
  o += button({ x: 1644, y: B + 14, w: 26, on: false, label: 'II', id: 'fx.chorus', layered: true, domId: 'fx.chorus__2', value: 2, steps: 4 });`);

// DELAY
sub(`  o += knob({ x: 1704, y: B + 10, d: 30, v: 'cream', val: 0.36, ticks: 11, label: 'TIME', lsize: 7.4 });
  o += knob({ x: 1756, y: B + 10, d: 30, v: 'cream', val: 0.44, ticks: 11, label: 'FEEDBACK', lsize: 7.4 });
  o += button({ x: 1806, y: B + 14, w: 26, on: false, label: 'FREEZE' });`,
`  o += knob({ x: 1704, y: B + 10, d: 30, v: 'cream', val: 0.36, ticks: 11, label: 'TIME', lsize: 7.4, id: 'fx.delayTime', layered: true });
  o += knob({ x: 1756, y: B + 10, d: 30, v: 'cream', val: 0.44, ticks: 11, label: 'FEEDBACK', lsize: 7.4, id: 'fx.delayFeedback', layered: true });
  o += button({ x: 1806, y: B + 14, w: 26, on: false, label: 'FREEZE', id: 'fx.freeze', layered: true });`);

fs.writeFileSync('gen.mjs', g);
console.log(`${n}/6 strip groups wired as layer-following`);
