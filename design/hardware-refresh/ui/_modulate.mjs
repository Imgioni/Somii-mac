import fs from 'node:fs';

// ---- the MODULATE pop-over: eight source amounts for one destination ----------
let g = fs.readFileSync('gen.mjs', 'utf8');

const page = `
// One shared MODULATE page. Its eight knobs are re-pointed at the routes for whichever
// control was right-clicked, so 8 x 32 destinations need eight controls, not 256.
function modulatePage() {
  let o = '';
  o += txt(22, 58, 420, 'RIGHT-CLICK ANY CONTROL TO ROUTE MODULATION TO IT', { size: 7.6, align: 'left', color: INK2 });
  o += txt(22, 74, 420, '-', { size: 11, align: 'left', ls: 0.1 });
  const rows = ['DDS 2', 'LFO 2', 'ENV 1', 'VELOCITY', 'AFTERTOUCH', 'EXPRESSION', 'RIBBON', 'NOTE NO.'];
  rows.forEach((r, i) => {
    const y = 104 + i * 52;
    o += txt(22, y + 16, 130, r, { size: 8.6, align: 'right' });
    o += knob({ x: 172, y: y, d: 34, v: 'cream', val: 0.5, ticks: 5,
                id: 'modslot' + i, domId: 'modslot' + i });
    o += txt(222, y + 16, 120, '-100 . . +100', { size: 6.6, align: 'left', color: INK2 });
  });
  return o;
}
`;
g = g.replace("let body = '';", page + "\nlet body = '';");
g = g.replace(
  "body += popover('pop-settings',",
  "body += popover('pop-modulate', 'MODULATE', Math.round((BASEW - 420) / 2), 120, 420, 540, modulatePage());\n" +
  "body += popover('pop-settings',"
);
fs.writeFileSync('gen.mjs', g);

// ---- JS: destination table + right-click routing -----------------------------
let j = fs.readFileSync('geminus.js', 'utf8');

const js = `
// ---- modulation routing ------------------------------------------------------
// Mirrors sgui::matrixDestSuffix in src/ui/UiHelpers.h: the first eight are the
// hard-wired matrix destinations (mtx.<src>.<name>), the rest are direct parameter
// mappings (mtxd.<src>.<n>). Keep in step with that table.
const MTX_FIXED = ['lfo1Rate', 'xmod', 'wave', 'mix', 'hpf', 'res', 'env1Decay', 'dlyTime'];
const DEST_SUFFIX = [
  'lfo1.rate', 'ddsMod.crossMod', 'ddsMod.pwmWave', 'mixer.mix', 'vcf.hpf', 'vcf.res', 'env1.decay', 'fx.delayTime',
  'dds2.tune', 'vcf.lpf', 'vcf.envAmt', 'vcf.lfo1Amt', 'vcf.dds2Amt', 'vca.envLevel', 'vca.lfo1Amt', 'vca.dds2Amt',
  'env1.attack', 'env1.sustain', 'env1.release', 'env2.attack', 'env2.decay', 'env2.sustain', 'env2.release',
  'lfo1.delay', 'lfo1.lrPhase', 'lfo2.rate', 'lfo2.delay', 'ddsMod.lfo1Amt', 'ddsMod.pwDetune', 'porta.time',
  'fx.delaySend', 'fx.delayFeedback'
];
const MTX_SRC_IDS = ['dds2', 'lfo2', 'env1', 'vel', 'at', 'expr', 'ribbon', 'note'];

const stateCache = new Map();
function cachedState(param) {
  if (!stateCache.has(param)) stateCache.set(param, makeState('slider', param));
  return stateCache.get(param);
}

// route id for source s modulating destination d, on the given layer
function routeId(layer, s, d) {
  return d < 8 ? layer + '.mtx.' + MTX_SRC_IDS[s] + '.' + MTX_FIXED[d]
               : layer + '.mtxd.' + MTX_SRC_IDS[s] + '.' + (d - 8 + 1);
}

function openModulate(param) {
  const dot = param.indexOf('.');
  const layer = param.slice(0, dot);
  if (layer !== 'upper' && layer !== 'lower') return false;
  const dest = DEST_SUFFIX.indexOf(param.slice(dot + 1));
  if (dest < 0) return false;

  const title = document.querySelectorAll('#pop-modulate div');
  for (let s = 0; s < 8; s++) {
    const el = document.getElementById('modslot' + s);
    if (!el) continue;
    const id = routeId(layer, s, dest);
    const list = byParam.get(el.dataset.param);
    if (list) { const i = list.indexOf(el); if (i >= 0) list.splice(i, 1); }
    el.dataset.param = id;
    el._state = cachedState(id);
    if (!byParam.has(id)) byParam.set(id, []);
    byParam.get(id).push(el);
    paint(el, readNorm(el));
  }
  // name the destination in the heading
  const heading = [...document.querySelectorAll('#pop-modulate div')]
    .find((d) => d.children.length === 0 && d.textContent.trim() === modHeading);
  if (heading) heading.textContent = param.toUpperCase();
  modHeading = param.toUpperCase();

  const pop = document.getElementById('pop-modulate');
  if (pop) pop.hidden = false;
  return true;
}
let modHeading = '-';
`;

j = j.replace('function bindPopovers() {', js + '\nfunction bindPopovers() {');

// right-click on any bound control opens MODULATE for it
j = j.replace(
  "  addEventListener('keydown', (e) => {\n    if (e.key === 'Escape') document.querySelectorAll('[id^=\"pop-\"]').forEach(close);\n  });",
  "  addEventListener('keydown', (e) => {\n    if (e.key === 'Escape') document.querySelectorAll('[id^=\"pop-\"]').forEach(close);\n  });\n\n" +
  "  // Right-click a control to route modulation to it (prompt section 5).\n" +
  "  for (const el of document.querySelectorAll('[data-juce-type][data-param]')) {\n" +
  "    if (el.id.startsWith('modslot')) continue;\n" +
  "    el.addEventListener('contextmenu', (e) => {\n" +
  "      if (openModulate(el.dataset.param)) e.preventDefault();\n" +
  "    });\n  }"
);

fs.writeFileSync('geminus.js', j);
console.log('MODULATE page + right-click routing added');
