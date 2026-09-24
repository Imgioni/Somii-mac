// Non-destructive sprite framing. The original PNG bytes/alpha are embedded unchanged.
// SVG viewBoxes remove transparent margins and isolate variants from the supplied sheets.
import fs from 'node:fs';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import assert from 'node:assert/strict';
const source = new URL('../docs/reference/assets/New assets/', import.meta.url);
const target = new URL('../ui/', import.meta.url);
let count = 0;
const rasterSprites = [];

function sprite(name, file, box, { body = '', transform = '' } = {}) {
  const png = fs.readFileSync(new URL(encodeURIComponent(file), source));
  assert.equal(png.subarray(1, 4).toString(), 'PNG');
  const width = png.readUInt32BE(16), height = png.readUInt32BE(20);
  const [x, y, w, h] = box;
  assert(x >= 0 && y >= 0 && w > 0 && h > 0 && x + w <= width && y + h <= height, name);
  const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="${box.join(' ')}" preserveAspectRatio="none">${body}<image width="${width}" height="${height}" href="data:image/png;base64,${png.toString('base64')}"${transform ? ` transform="${transform}"` : ''}/></svg>`;
  fs.writeFileSync(new URL(`new-${name}.svg`, target), svg);
  rasterSprites.push(name);
  count++;
}

for (const [name, file] of [
  ['button-light', 'Light pushbutton.png'], ['button-light-down', 'Light pushbutton pushed.png'],
  ['button-dark', 'Dark pushbutton.png'], ['button-dark-down', 'Dark pushbutton pushed.png']
]) sprite(name, file, [307, 310, 640, 634]);
for (const [name, color] of [['grey', 'white'], ['orange', 'orange'], ['dark', 'black']])
  sprite(`fader-${name}`, `Fader cap ${color}.png`, [258, 410, 739, 526]);
sprite('rail', 'Fader rail.png', [432, 115, 156, 1310]);
for (const state of ['on', 'off']) sprite(`led-${state}`, `Indicator LED ${state}.png`, [414, 402, 426, 420]);
sprite('ribbon-strip', 'Ribbon strip.png', [24, 265, 2124, 188]);
sprite('ribbon-left', 'Ribbon left end.png', [258, 348, 891, 420]);
sprite('ribbon-right', 'Ribbon right end.png', [381, 222, 1047, 477]);
for (const [state, file] of [['left', 3], ['center', 1], ['right', 2]])
  sprite(`octave-${state}`, `Horizontal octave lever ${file}.png`, [225, 336, 999, 414]);

// All six bender frames keep the same socket position, scale and canvas size.
// Sheet 1 has clean transparency and includes the socket, unlike the separate base.
for (const [state, x] of [['center', 116], ['left', 820], ['right', 1524]]) {
  sprite(`bender-${state}`, 'Bender lever 1.png', [x, 80, 550, 250]);
  sprite(`bender-${state}-push`, 'Bender lever 1.png', [x, 374, 550, 250]);
}

// The three-position toggle is composed from 'Vertical toggle base.png' and
// 'Vertical toggle button.png' in crop-ui-assets.ps1 (new-switch-top/mid/bot.png).
for (const state of ['top', 'mid', 'bot']) fs.rmSync(new URL(`new-switch-${state}.svg`, target), { force: true });

// Separate the rotating top face from the stationary ribbed body. Coordinates
// are measured in the supplied sheet; the face is made circular in the UI,
// rotated there, then projected back into the original ellipse.
for (const [color, x, faceX] of [['cream', 174, 220], ['orange', 830, 875], ['dark', 1494, 1538]]) {
  sprite(`knob-${color}`, 'Standard rotary knob.png', [x, 76, 510, 565]);
  sprite(`knob-${color}-face`, 'Standard rotary knob.png', [faceX, 86, 410, 374]);
}
for (const [color, x, faceX] of [['cream', 502, 552], ['orange', 1113, 1162]]) {
  sprite(`small-${color}`, 'Small rotary knob 1.png', [x, 164, 390, 466]);
  sprite(`small-${color}-face`, 'Small rotary knob 1.png', [faceX, 174, 288, 260]);
}
execFileSync('powershell.exe', ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', fileURLToPath(new URL('./crop-ui-assets.ps1', import.meta.url))], { stdio: 'inherit' });
for (const name of rasterSprites) fs.rmSync(new URL(`new-${name}.svg`, target));
console.log(`Prepared ${rasterSprites.length} cropped PNG sprites and 3 composed toggle detents.`);
