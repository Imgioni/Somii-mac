import assert from 'node:assert/strict';
import fs from 'node:fs';
import { A, button, knob, sw3, fader } from '../ui/lib.mjs';
const root = new URL('../ui/', import.meta.url);
const html = fs.readFileSync(new URL('index.html', root), 'utf8');
const generated = [html, button({ id: 'test', x: 0, y: 0, dark: true, on: true }),
  knob({ id: 'small', x: 0, y: 0, d: 38 }), knob({ id: 'normal', x: 0, y: 0, d: 56 }),
  sw3({ id: 'rocker', x: 0, y: 0, rocker: true }), fader({ id: 'fader', x: 0, y: 0, h: 100 })].join('');
const paths = new Set([...generated.matchAll(/(?:src|data-on|data-off|data-src[012]|data-ledon|data-ledoff)="([^"<>]+\.(?:png|svg))"/g)].map(m => m[1]));
for (const path of paths) assert(fs.existsSync(new URL(path, root)), `Missing sprite: ${path}`);
for (const path of paths) if (path.startsWith('new-') && path.endsWith('.svg')) {
  const svg = fs.readFileSync(new URL(path, root), 'utf8');
  assert(svg.includes('data:image/png;base64,'), `Sprite must be self-contained: ${path}`);
  assert(svg.includes('viewBox='), path);
}
assert.notEqual(A.btnDark, A.btnDarkOn);
assert(!generated.includes('clip-path:inset(24%'), 'Old button artifact clipping still present');
assert(generated.includes(A.knob.cream), 'Current knob artwork is referenced');
assert(generated.includes(A.cap.grey), 'Current fader artwork is referenced');
const switchStates = ['top', 'mid', 'bot'].map(state => fs.readFileSync(new URL(`new-switch-${state}.png`, root)).toString('base64'));
assert.equal(new Set(switchStates).size, 3, 'Toggle detents must have distinct artwork');
assert(generated.includes('data-optbase='), 'Toggle option labels need state binding');
for (const side of ['left', 'center', 'right']) for (const push of ['', '-push'])
  assert(fs.existsSync(new URL(`new-bender-${side}${push}.png`, root)));
const ids = [...html.matchAll(/\bid="([^"]+)"/g)].map(m => m[1]);
assert.equal(new Set(ids).size, ids.length, 'Duplicate UI ids');
console.log(`Verified ${paths.size} sprite references, six bender poses, current control artwork and unique UI ids.`);
if (process.argv.includes('--binaries')) {
  const release = new URL(`../${process.env.GEMINUS_BUILD_DIR || 'build-vs'}/Geminus_artefacts/Release/`, import.meta.url);
  const current = ['index.html', 'geminus.js', ...fs.readdirSync(root).filter(f => f.startsWith('new-') && /\.(png|svg)$/.test(f))];
  for (const file of ['VST3/Somii.vst3/Contents/x86_64-win/Somii.vst3', 'CLAP/Somii.clap', 'Standalone/Somii.exe']) {
    const binary = fs.readFileSync(new URL(file, release));
    for (const resource of current)
      assert(binary.includes(fs.readFileSync(new URL(resource, root))), `Stale build: ${file} does not embed current ${resource}`);
    console.log(`Current HTML, JS and all new artwork verified inside ${file}`);
  }
}
