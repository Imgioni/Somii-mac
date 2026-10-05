import fs from 'node:fs';
let n = 0;
const sub = (file, a, b, tag) => {
  let s = fs.readFileSync(file, 'utf8');
  if (!s.includes(a)) { console.log('MISS ' + tag); return; }
  fs.writeFileSync(file, s.replace(a, b)); n++;
};

// ---------------------------------------------------------------- 1. CHORUS
// I and II are independent bits of one 4-way parameter (OFF / I / II / I+II), which is
// how the hardware works: pressing a lit one clears its own bit, and both can be lit.
sub('lib.mjs',
  "    type: (o.value != null || o.cycle) ? 'combo' : 'toggle',",
  "    type: (o.value != null || o.bit != null || o.cycle) ? 'combo' : 'toggle',",
  'lib button type');

sub('lib.mjs',
  "      o.value != null ? { value: o.value, steps: o.steps || 2 }\n      : o.cycle ? { steps: o.steps || 3 } : {})",
  "      o.value != null ? { value: o.value, steps: o.steps || 2 }\n      : o.bit != null ? { bit: o.bit, steps: o.steps || 4 }\n      : o.cycle ? { steps: o.steps || 3 } : {})",
  'lib button data');

sub('gen.mjs',
  "domId: 'fx.chorus__1', value: 1, steps: 4 });",
  "domId: 'fx.chorus__1', bit: 1, steps: 4 });",
  'chorus I');
sub('gen.mjs',
  "domId: 'fx.chorus__2', value: 2, steps: 4 });",
  "domId: 'fx.chorus__2', bit: 2, steps: 4 });",
  'chorus II');

// ---------------------------------------------------------------- 2. MATRIX SIZE
sub('gen.mjs',
  "  const x0 = 150, y0 = 92, cw = 104, ch = 74;",
  "  const x0 = 200, y0 = 104, cw = 140, ch = 96;",
  'matrix grid');
sub('gen.mjs',
  "  o += txt(24, 58, 300, (layer === 0 ? 'UPPER' : 'LOWER') + ' LAYER', { size: 10, align: 'left', ls: 0.14, color: INK2 });",
  "  o += txt(26, 62, 320, (layer === 0 ? 'UPPER' : 'LOWER') + ' LAYER', { size: 12, align: 'left', ls: 0.14, color: INK2 });",
  'matrix layer label');
sub('gen.mjs',
  "    o += txt(x0 + c * cw - 8, y0 - 26, cw + 16, d[1], { size: 7.4, color: INK2 });",
  "    o += txt(x0 + c * cw - 8, y0 - 32, cw + 16, d[1], { size: 9.2, color: INK2 });",
  'matrix dest headings');
sub('gen.mjs',
  "    o += txt(18, y0 + r * ch + 22, 124, s[1], { size: 8.4, align: 'right' });",
  "    o += txt(22, y0 + r * ch + 28, 168, s[1], { size: 10.4, align: 'right' });",
  'matrix source labels');
sub('gen.mjs',
  "      o += knob({ x: x0 + c * cw + cw / 2 - 13, y: y0 + r * ch + 8, d: 26, v: 'cream',",
  "      o += knob({ x: x0 + c * cw + cw / 2 - 17, y: y0 + r * ch + 8, d: 34, v: 'cream',",
  'matrix knobs');
sub('gen.mjs',
  "popover('pop-matrix', 'MODULATION MATRIX', Math.round((BASEW - 1010) / 2), 250, 1010, 700,\n                matrixPage(0) + '<div style=\"position:absolute;left:0;top:352px;width:1010px;height:1px;background:#A9A6A0\"></div>');",
  "popover('pop-matrix', 'MODULATION MATRIX', Math.round((BASEW - 1350) / 2), 60, 1350, 910,\n                matrixPage(0) + '<div style=\"position:absolute;left:0;top:465px;width:1350px;height:1px;background:#A9A6A0\"></div>');",
  'matrix popover');

console.log(n + '/9 applied');
