import fs from 'node:fs';
let n=0;
const sub=(file,a,b,tag)=>{let s=fs.readFileSync(file,'utf8');
  if(!s.includes(a)){console.log('MISS '+tag);return;} fs.writeFileSync(file,s.replace(a,b)); n++;};

// ---------------------------------------------------------------- lib.mjs
// One gap, used everywhere a label sits under a control.
sub('lib.mjs', "export const TEXT = 1.6;",
`export const TEXT = 1.6;

// The single label-to-control gap, and the height reserved for one line of label text.
// Both are derived from TEXT: the labels were scaled up without the slots they sit in,
// which is what put labels on top of their controls.
export const LGAP = 8;
export const lineH = (size) => (size || 8) * TEXT * 1.05;`, 'lib constants');

// sw3 detent labels: never closer together than a full line
sub('lib.mjs',
`  if (o.opts) {
    // detent labels sit level with the three cap positions
    for (let i = 0; i < o.opts.length; i++) {
      const ty = y + h * 0.16 + (1 - i / 2) * (h * 0.66) - 3.4;`,
`  if (o.opts) {
    // detent labels sit level with the three cap positions, but never closer than one
    // line apart - at TEXT > 1 the cap pitch alone runs the rows into each other
    const ss = o.ssize || 6.2, lh = lineH(ss), nOpt = o.opts.length;
    const pitch = Math.max(h * 0.33, lh + 2);
    for (let i = 0; i < nOpt; i++) {
      const ty = y + h / 2 + ((nOpt - 1) / 2 - i) * pitch - lh / 2;`, 'sw3 opts pitch');

// default label gap for the primitives that still use one
sub('lib.mjs', "  if (o.label) out += txt(x - 20, o.ly != null ? o.ly : y + h + 3, w + 40, o.label, { size: o.lsize || 7.6, color: o.labelColor || INK });",
              "  if (o.label) out += txt(x - 20, o.ly != null ? o.ly : y + h + LGAP, w + 40, o.label, { size: o.lsize || 7.6, color: o.labelColor || INK });", 'sw3 label gap');
sub('lib.mjs', "  if (o.label) out += txt(x - 18, o.ly != null ? o.ly : y + h + 3, w + 36, o.label, { size: o.lsize || 7.6, color: o.dark ? '#D8D6D0' : INK });",
              "  if (o.label) out += txt(x - 18, o.ly != null ? o.ly : y + h + LGAP, w + 36, o.label, { size: o.lsize || 7.6, color: o.dark ? '#D8D6D0' : INK });", 'button label gap');

// ---------------------------------------------------------------- gen.mjs
sub('gen.mjs',
`const LBL = s(206);                      // knob/switch label baseline
const SUB = s(218);                      // knob/switch secondary-label baseline`,
`const LBL = s(206);                      // knob/switch label baseline
// The shift pill clears a whole label line rather than a constant that predates TEXT.
const LROW = Math.ceil(8.5 * TEXT * 1.05);   // reserved height of one label row
const SUB = LBL + LROW + 4;              // knob/switch secondary-label baseline`, 'gen LBL/SUB');

// the DDS sub-row shares one baseline: put it under the TALLEST control in the row
sub('gen.mjs', "  const shy = bt + 126, shly = bt + 152;",
               "  const shy = bt + 126, shly = shy - 1 + 38 + LGAP;   // clears the h:38 mode switch", 'shly');

// voice-assign LED list
sub('gen.mjs',
`  ['SOLO', 'LEGATO', 'POLY 1', 'POLY 2'].forEach((s, i) => {
    o += led(600, B + 8 + i * 11, i === 2, 7);
    o += txt(611, B + 8 + i * 11, 44, s, { size: 6.4, align: 'left', color: INK2, weight: 600 });
  });`,
`  const vaPitch = Math.ceil(lineH(6.4)) + 2;
  ['SOLO', 'LEGATO', 'POLY 1', 'POLY 2'].forEach((s, i) => {
    o += led(600, B + 8 + i * vaPitch, i === 2, 7);
    o += txt(611, B + 8 + i * vaPitch, 44, s, { size: 6.4, align: 'left', color: INK2, weight: 600 });
  });`, 'voice assign list');

sub('gen.mjs', "  o += txt(x + 176, lb3 + 12, 120, 'BENDER AMOUNT', { size: 7.6 });",
               "  o += txt(x + 176, lb3 + LROW + 4, 120, 'BENDER AMOUNT', { size: 7.6 });", 'bender amount');

console.log(n + '/8 applied');
