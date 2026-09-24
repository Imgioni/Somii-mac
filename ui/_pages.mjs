import fs from 'node:fs';
let g = fs.readFileSync('gen.mjs', 'utf8');

const page = `
// ------------------------------------------------------------------ pop-overs
// The modulation matrix: 8 sources x 8 destinations per layer, bipolar amount.
// Every cell is an ordinary parameter, so it binds like any panel control.
const MTX_SRC = [
  ['dds2', 'DDS 2'], ['lfo2', 'LFO 2'], ['env1', 'ENV 1'], ['vel', 'VELOCITY'],
  ['at', 'AFTERTOUCH'], ['expr', 'EXPRESSION'], ['ribbon', 'RIBBON'], ['note', 'NOTE NO.']
];
const MTX_DST = [
  ['lfo1', 'LFO 1 RATE'], ['xmod', 'CROSS MOD'], ['wave', 'WAVE MOD'], ['mix', 'OSC MIX'],
  ['hpf', 'HPF'], ['res', 'RESONANCE'], ['env1', 'ENV 1 DECAY'], ['dly', 'DELAY TIME']
];

function matrixPage(layer) {
  const L = layer === 0 ? 'upper' : 'lower';
  const x0 = 150, y0 = 92, cw = 104, ch = 74;
  let o = '';
  o += txt(24, 58, 300, (layer === 0 ? 'UPPER' : 'LOWER') + ' LAYER', { size: 10, align: 'left', ls: 0.14, color: INK2 });

  // destination headings across the top, sources down the side
  MTX_DST.forEach((d, c) => {
    o += txt(x0 + c * cw - 8, y0 - 26, cw + 16, d[1], { size: 7.4, color: INK2 });
  });
  MTX_SRC.forEach((s, r) => {
    o += txt(18, y0 + r * ch + 22, 124, s[1], { size: 8.4, align: 'right' });
  });

  // one small bipolar knob per cross-point
  MTX_SRC.forEach((s, r) => {
    MTX_DST.forEach((d, c) => {
      const id = L + '.mtx.' + s[0] + '.' + d[0];
      o += knob({ x: x0 + c * cw + cw / 2 - 13, y: y0 + r * ch + 8, d: 26, v: 'cream',
                  val: 0.5, ticks: 5, id: id });
    });
  });
  return o;
}

function settingsPage() {
  let o = '';
  const col = (x, title) => txt(x, 62, 240, title, { size: 10, align: 'left', ls: 0.12, color: INK2 });

  col(28, 'TUNING');
  o += knob({ x: 40,  y: 96, d: 46, v: 'cream', val: 0.5, ticks: 11, label: 'FINE TUNE', ly: 156, id: 'global.fineTune' });
  o += knob({ x: 140, y: 96, d: 46, v: 'cream', val: 0.5, ticks: 11, label: 'TRANSPOSE', ly: 156, id: 'global.transpose' });

  col(300, 'KEYBOARD');
  o += knob({ x: 312, y: 96, d: 46, v: 'cream', val: 0.47, ticks: 11, label: 'SPLIT POINT', ly: 156, id: 'perf.splitPoint' });
  o += sw3({ x: 420, y: 96, h: 44, pos: 0, opts: ['UPPER', 'LOWER', 'BOTH'], label: 'MOD LAYER', ly: 156, id: 'perf.modLayer' });

  col(560, 'MIDI');
  o += knob({ x: 572, y: 96, d: 46, v: 'cream', val: 0, ticks: 16, label: 'CHANNEL', ly: 156, id: 'global.midiChannel' });
  o += button({ x: 676, y: 104, w: 30, on: true, label: 'CC RX', id: 'global.ccRx' });
  o += button({ x: 726, y: 104, w: 30, on: true, label: 'CLOCK RX', id: 'global.clockRx' });

  o += txt(28, 210, 760, 'SHIFT REVEALS SECONDARY FUNCTIONS  ·  CTRL-DRAG FOR FINE  ·  DOUBLE-CLICK RESETS',
           { size: 7.6, align: 'left', color: INK2 });
  return o;
}
`;

// insert the pages just before the panel assembly
const anchor = "let body = '';";
g = g.replace(anchor, page + '\n' + anchor);

// build the pop-overs into the page, and give the strip openers
g = g.replace(
  "body += wordmark(546, PR);",
  "body += wordmark(546, PR);\n" +
  "body += popover('pop-matrix', 'MODULATION MATRIX', 120, 60, 1010, 700,\n" +
  "                matrixPage(0) + '<div style=\"position:absolute;left:0;top:352px;width:1010px;height:1px;background:#A9A6A0\"></div>');\n" +
  "body += popover('pop-settings', 'SETTINGS', 240, 200, 820, 260, settingsPage());"
);

fs.writeFileSync('gen.mjs', g);
console.log('matrix + settings pages inserted');
