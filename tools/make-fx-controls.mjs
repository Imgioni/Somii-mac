// Draws the FX tab's flat digital controls as SVG files in ui/ (the FX tab is the one place the
// UI uses generated artwork instead of photographed parts - see ui/fxskins.js).
//   fx-knob-<style>.svg          knob caps, pointer at 12 o'clock; the page rotates them and draws
//                                 the value arc live around them
//   fx-power-glyph.svg           the power glyph, used as a mask the page colours
//   fx-toggle-up/down.svg        mini bat switch (hardware-style skins)
//   flat-btn-off/on.svg          flat square keys used by the pop-over pages
// Run from the repository root:  node tools/make-fx-controls.mjs
import fs from 'node:fs';
import { KNOB_STYLES } from '../ui/fxskins.js';

const out = new URL('../ui/', import.meta.url);
const svg = (w, h, body) => `<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}">${body}</svg>\n`;
const circle = (r, fill, extra = '') => `<circle cx="100" cy="100" r="${r}" fill="${fill}" ${extra}/>`;
const pointer = (y1, y2, colour, width) => `<line x1="100" y1="${y1}" x2="100" y2="${y2}" stroke="${colour}" stroke-width="${width}" stroke-linecap="round"/>`;
// n small marks around a radius (grip teeth, skirt ticks)
const around = (n, r1, r2, colour, width) => Array.from({ length: n }, (_, i) => {
  const a = (i / n) * Math.PI * 2, s = Math.sin(a), c = Math.cos(a);
  return `<line x1="${(100 + s * r1).toFixed(2)}" y1="${(100 - c * r1).toFixed(2)}" x2="${(100 + s * r2).toFixed(2)}" y2="${(100 - c * r2).toFixed(2)}" stroke="${colour}" stroke-width="${width}"/>`;
}).join('');

// The caps are the 002's own knobs drawn flat (no gradients): the cream UPPER cap, the orange LOWER
// cap and the black global cap, plus a slate one for the SUPER SIX faces. A skirt of ticks marks the
// grip; the pointer is the printed line on the real caps.
const cap = (rim, face, line) => circle(84, rim) + around(48, 76, 84, face, 2.5) + circle(72, face) + circle(58, 'none', 'stroke="' + rim + '" stroke-opacity=".35" stroke-width="2"') + pointer(28, 64, line, 8);
const KNOBS = {
  // the house cap: flat graphite disc, a lighter face, a white line
  house: circle(80, '#15171B') + circle(76, '#30343C') + circle(66, '#383C45') + pointer(32, 68, '#EEF0F6', 8),
  bone: cap('#B9B2A6', '#E7E2DA', '#23252A'),
  coal: cap('#0E0E0F', '#2A2A2C', '#E7E2DA'),
  ember: cap('#B8401A', '#F65A27', '#23252A'),
  slate: cap('#122830', '#2F5A6B', '#FFE8D1')
};
for (const style of KNOB_STYLES) {
  if (!KNOBS[style]) throw new Error('no drawing for knob style ' + style);
  fs.writeFileSync(new URL('fx-knob-' + style + '.svg', out), svg(200, 200, KNOBS[style]));
}

// power key: the IEC power glyph as a mask; the page paints it (and the ring behind it) in the
// face's or the theme's colours, so one file serves every skin and theme
fs.writeFileSync(new URL('fx-power-glyph.svg', out), svg(64, 64, '<path d="M32 17v14" stroke="#000" stroke-width="5" stroke-linecap="round" fill="none"/>'
  + '<path d="M21.5 23.5a14 14 0 1 0 21 0" stroke="#000" stroke-width="5" stroke-linecap="round" fill="none"/>'));

// flat square keys for the pop-over pages: just an edge; the page fills them from the active theme
// (ui/gen.mjs: .pop img[src$=flat-btn-*]), so they follow GEMINI / SUPER SIX
fs.writeFileSync(new URL('flat-btn-off.svg', out), svg(64, 64, '<rect x="2" y="2" width="60" height="60" rx="9" fill="none" stroke="#FFFFFF" stroke-opacity=".22" stroke-width="2"/>'));
fs.writeFileSync(new URL('flat-btn-on.svg', out), svg(64, 64, '<rect x="2" y="2" width="60" height="60" rx="9" fill="none" stroke="#FFFFFF" stroke-opacity=".35" stroke-width="2"/>'));

// mini bat switch: a slot and the lever's tip at the top or bottom
const toggle = (up) => svg(40, 72, `<rect x="6" y="4" width="28" height="64" rx="14" fill="#2A2A2B"/><rect x="6" y="4" width="28" height="64" rx="14" fill="none" stroke="#4A4A4B" stroke-width="2"/>`
  + `<circle cx="20" cy="${up ? 20 : 52}" r="11" fill="#E9E9E6"/><rect x="17" y="${up ? 20 : 36}" width="6" height="16" fill="#E9E9E6"/>`);
fs.writeFileSync(new URL('fx-toggle-up.svg', out), toggle(true));
fs.writeFileSync(new URL('fx-toggle-down.svg', out), toggle(false));
console.log('wrote ' + KNOB_STYLES.length + ' knob caps, ' + 'a power glyph, 2 flat keys, 2 toggles to ui/');
