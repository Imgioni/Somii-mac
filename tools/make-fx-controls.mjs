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

// a disc lit from the top left: a radial gradient between two tones, and a dark pointer
const glowDisc = (hi, lo) => '<defs><radialGradient id="g" cx="38%" cy="32%" r="75%"><stop offset="0" stop-color="' + hi + '"/><stop offset="1" stop-color="' + lo + '"/></radialGradient></defs>'
  + circle(80, 'url(#g)') + pointer(30, 66, '#12143A', 9);
const KNOBS = {
  // the house cap: flat graphite disc, a lighter face, a white line
  house: circle(80, '#15171B') + circle(76, '#30343C') + circle(66, '#383C45') + pointer(32, 68, '#EEF0F6', 8),
  // Rev-style: a plain white disc, navy line
  ocean: circle(78, '#F4F6FF') + circle(78, 'none', 'stroke="#C9D3EE" stroke-width="3"') + pointer(30, 64, '#142C5C', 8),
  // Ambient-style: concentric hairline rings round a black centre, a white dot
  rings: [94, 86, 78, 70, 62].map((r) => circle(r, 'none', 'stroke="#1A1A1A" stroke-width="2"')).join('') + circle(52, '#141414') + '<circle cx="100" cy="66" r="7" fill="#FFFFFF"/>',
  // console-module: knurled black knob, white line
  tuba: circle(88, '#0B0B0B') + around(56, 80, 88, '#2C2C2C', 3.5) + circle(70, '#171717') + circle(70, 'none', 'stroke="#262626" stroke-width="3"') + pointer(20, 62, '#F4F4F2', 7),
  // FF-style: slate disc, pale line
  fab: circle(78, '#1B2027') + circle(74, '#39414C') + circle(64, '#303741') + pointer(34, 66, '#E6EBF2', 7),
  // Saturn-style: a white cap with a soft grey edge, dark line
  white: circle(80, '#8E8A89') + circle(76, '#F4F1EE') + circle(64, 'none', 'stroke="#E2DEDA" stroke-width="3"') + pointer(30, 62, '#2B2525', 8),
  // VintageVerb-style: solid glowing discs (violet, azure, cyan), a dark line
  violet: glowDisc('#B07CFF', '#7A42E6'), azure: glowDisc('#8C95FF', '#5560E8'), cyan: glowDisc('#5FD6FF', '#1E9FE6'),
  // its big DECAY: concentric rings from violet to cyan round a deep centre, a dark notch
  vdecay: [['#8E5CFF', 94], ['#7C6CFF', 80], ['#6A80FF', 66], ['#4F9BFF', 52], ['#3CC0FF', 38]].map(([c, r]) => circle(r, 'none', 'stroke="' + c + '" stroke-width="8"')).join('')
    + circle(26, '#24186E') + '<rect x="95" y="2" width="10" height="44" rx="3" fill="#0B0E2A"/>'
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
