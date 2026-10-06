// Generates ui/index.html - the Geminus panel. Run: node gen.mjs
// Layout follows the real Super Gemini (docs/reference/ui_prompt_v2.md).
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import {
  A, INK, INK2, ORANGE, ONBLACK_INK, ONBLACK_INK2, txt, txtInv, vrule, hrule, blackPanel, well, sw2h,
  knob, rotary, wRotary, fader, hfader, sw3, wSw3, button, led, ledBound, ledLadder, sect, popover,
  keyButton, hit, TEXT, lineH, wText, r1, esc, FONT, BTN_ASPECT, STYLE
} from './lib.mjs';
import { FX, DIVS, NP, CARVE_BEATS, CARVE_SHAPERS, CARVE_HINTS, FX_MOD_SOURCES, FX_EXT } from './fxdefs.mjs';
import { SKINS, SKIN_OF } from './fxskins.js';

const OUT = path.dirname(fileURLToPath(import.meta.url));   // decodes spaces (the project folder is '002 by SPKR')

// ---- parameters (for validation and defaults) ------------------------------
const PARAMS = new Map();
for (const line of fs.readFileSync(path.join(OUT, '..', 'params.tsv'), 'utf8').trim().split('\n')) {
  const [id, type, def, steps, range] = line.split('\t');
  PARAMS.set(id, { type, def: parseFloat(def), steps: parseInt(steps, 10), range });
}
const choices = (id) => (PARAMS.get(id) || {}).range.split('|');

// ---------------------------------------------------------------- geometry
const W = 3400, H = 1330;
// no end cheeks (user, 2026-09-17): the panel runs to a 24px margin on both sides
const PL = 24, PR = W - 24;

// ---- desktop layout ---------------------------------------------------------
// Desktop mode re-flows the SAME controls (no duplicated parameter bindings) into a
// laptop-shaped canvas at 100 % size, one engine at a time. Every section is wrapped in
// a card: in the full panel the wrapper is inert; in desktop mode it becomes an
// absolutely placed box and its content is shifted by (-sx, -sy) into it.
// Anything in a .fp-wrap that is not a card is hidden in desktop mode.
const DKCARD = new Map();                        // name -> { w, h }
// k > 1 enlarges a card in desktop mode (never below 1: readability is the point).
const dk = (name, sx, sy, w, h, extraCls, k = 1) => {
  DKCARD.set(name, { w: Math.round(w * k), h: Math.round(h * k) });
  return '<div class="dk' + (extraCls ? ' ' + extraCls : '') + '" data-dk="' + name + '" style="--sx:' + r1(-sx) + 'px;--sy:' + r1(-sy) + 'px;--k:' + k + '">'
    + '<div class="dk-in">';
};
const dkEnd = '</div></div>';
const DK_SRC_Y = 2000;                           // desktop-only card content lives below the full canvas
const TOPBAR = 62;
const ROWH = 322, ROW1 = TOPBAR + 8, ROW2 = ROW1 + ROWH + 10;
const STRIP = ROW2 + ROWH + 12, STRIPH = 168;
const BOTTOM = STRIP + STRIPH + 6;               // performance block, ribbon, keys
const KBX = 700, KBY = 1000, KBH = H - KBY;
const RIBY = BOTTOM + 22;

// per-row vertical map (relative to the row top)
const CY = 152;                                  // knob / switch centre
const FTOP = 44, FH = 200;                       // fader top and travel
const LBL = 262;                                 // label baseline (knobs, switches, faders)
const SUB = LBL + lineH(8.5) + 3;                // shift legend
const KD = 76;                                   // standard knob
const SWH = 64;                                  // three-position switch
const SWD = 13;                                  // fader slot width

// section widths; the row is laid out cumulatively so a width change never needs hand-nudging
const LEFTCOL = 150;
// Give the oscillator modules the room they need while keeping the hardware order
// and the overall row width unchanged.
const SECW = { lfo: 330, mod: 620, osc: 680, mix: 118, vcf: 430, vca: 280, env: 470, dly: 100 };
// sections are justified: the gap takes whatever width the sections leave
const SGAP = (PR - PL - LEFTCOL - Object.values(SECW).reduce((a, b) => a + b, 0)) / (Object.keys(SECW).length - 1);
const S = (() => {
  const out = {}; let x = PL + LEFTCOL;
  for (const [k, w] of Object.entries(SECW)) { out[k] = { x, w }; x += w + SGAP; }
  out.__end = x - SGAP;
  return out;
})();
if (S.__end > PR) console.warn('row overflow: ' + S.__end + ' > ' + PR);

const kpad = (d) => Math.max(7, d * 0.2);
const widest = (...v) => Math.max(...v.filter((n) => typeof n === 'number' && isFinite(n)));
const wFader = SWD + 26;                          // rail, cap and tick ladder
const wKnob = (d) => d + 2 * kpad(d);

// lay a row of items out centred inside a section
function pack(sec, items, gap, name) {
  const total = items.reduce((a, i) => a + i.w, 0) + gap * (items.length - 1);
  if (total > sec.w + 0.5) console.warn('  overflow ' + (name || '?') + ': cluster ' + Math.round(total) + ' > section ' + Math.round(sec.w));
  let cx = sec.x + (sec.w - total) / 2;
  let out = '';
  for (const it of items) { out += it.r(cx); cx += it.w + gap; }
  return out;
}
const GAP = (w) => ({ w, r: () => '' });
const ONBLACK = { lc: ONBLACK_INK2, tick: 'var(--otick)', labelColor: ONBLACK_INK };

// The 3rd Wave's filter section [its manual pp.53-54]: a 4-pole low-pass with SATURATION on its
// output, and above it the 2-pole state-variable filter that feeds it. CUTOFF, RESONANCE and ENV
// AMT belong to an independent parameter bank, retained when switching styles.
function thirdWaveVcf(g, y, P, kv) {
  let o = '';
  // Geometry: a 46 px left column holds STYLE (row 1) and the two state-variable keys (row 2);
  // the knobs take the rest. Desktop mode shows this card on its own and enlarges it, so the rows
  // use the whole height rather than sitting in the top two thirds.
  const kd = 52, col = 50, kx = g.x + col, kw = g.w - col - 8;
  const r1y = y + 60, r2y = y + 208;
  const lblOf = (ky) => ky + kd + 12;
  const place = (items, rowY, areaX, areaW) => {
    const pitch = areaW / items.length;
    return items.map(([label, id, val, minmax], i) =>
      knob({ x: areaX + i * pitch + (pitch - kd) / 2, y: rowY, d: kd, v: kv, val, ticks: 11,
             label, lsize: 6.4, ly: lblOf(rowY), lw: pitch - 4, id: P(id), domId: P(id) + '__3w', minmax })).join('');
  };

  // ── the 4-pole low-pass, with SATURATION on its output ──
  o += txt(kx, y + 28, 220, 'LOW-PASS FILTER', { size: 6.6, align: 'left', color: INK2, weight: 600 });
  o += place([['SAT', 'vcf.saturation', 0.0],
              ['CUTOFF', 'tw.cutoff', 1.0],
              ['RES', 'tw.res', 0.0],
              ['ENV AMT', 'tw.envAmt', 0.5, ['−', '+']],
              ['VELOCITY', 'vcf.velocity', 0.0],
              ['KEY AMT', 'tw.keytrack', 0.0]], r1y, kx, kw);
  o += button({ x: kx + kw - 48, y: y + 15, w: 28, led: true, ld: 6,
                label: 'RES COMP', lsize: 5.6, ly: y + 38, lw: 64,
                id: P('tw.resComp'), title: 'Preserve bass as low-pass resonance increases' });

  // ── the 2-pole state-variable filter that feeds it ──
  o += txt(kx, y + 178, 260, 'STATE-VARIABLE FILTER', { size: 6.6, align: 'left', color: INK2, weight: 600 });
  // ON and BAND sit on the same line as the knobs they belong to, not stacked in the margin
  o += button({ x: g.x + 10, y: r2y + 4, w: 28, led: true, ld: 6, label: 'ON', lsize: 6.2, ly: r2y + 32, lw: 46,
                id: P('svf.on'), domId: P('svf.on') + '__3w',
                title: 'Run the state-variable filter before the low-pass' });
  o += button({ x: g.x + 10, y: r2y + 67, w: 28, led: true, ld: 6, label: 'BAND', lsize: 6.2, ly: r2y + 95, lw: 46,
                id: P('svf.band'), domId: P('svf.band') + '__3w', title: 'Band-pass mode' });
  o += place([['MODE', 'svf.mode', 0.0, ['LP', 'HP']],
              ['CUTOFF', 'svf.cutoff', 1.0],
              ['RES', 'svf.res', 0.0],
              ['ENV AMT', 'svf.envAmt', 0.5, ['−', '+']],
              ['VELOCITY', 'svf.velocity', 0.0],
              ['KEY AMT', 'svf.keytrack', 0.0]], r2y, kx, kw);
  o += txt(kx, lblOf(r2y) + 13, kw / 6, 'LP - NOTCH - HP', { size: 5.6, color: INK2, weight: 600 });
  return o;
}

// ---------------------------------------------------------------- layer row
function layerRow(y, upper) {
  let o = '';
  const engine = upper ? 'upper' : 'lower';
  const group = (name, x, w) => dk(engine + '-' + name, x, y, w, ROWH);
  const endGroup = () => bracketsDone() + dkEnd;
  const P = (s) => (upper ? 'upper.' : 'lower.') + s;
  const cap = upper ? 'grey' : 'orange';         // UPPER cream/grey caps, LOWER orange, as the hardware
  const kv = upper ? 'cream' : 'orange';
  const ly = y + LBL, sy = y + SUB, cy = y + CY, fy = y + FTOP;
  const ky = (d) => cy - d / 2;
  const rails = new Map();
  // Brackets identify related controls using the same endpoints in both layers.
  // Keep them above the rails, clear of the section headings and fader caps.
  const brackets = [
    ['lfo1.rate', 'lfo1.delay', 'lfo1.lrPhase'],
    ['ddsMod.pwDetune', 'ddsMod.drift', 'ddsMod.pwmWave'],
    ['ddsMod.lfo1Amt', 'ddsMod.env1Amt'],
    ['vcf.lpf', 'vcf.res'],
    ['vcf.envAmt', 'vcf.lfo1Amt', 'vcf.dds2Amt'],
    ['vca.envLevel', 'vca.lfo1Amt', 'vca.dds2Amt']
  ];
  const bracketsDone = () => {
    let b = '';
    for (let i = brackets.length - 1; i >= 0; i--) {
      const ids = brackets[i];
      if (!ids.every((id) => rails.has(id))) continue;
      brackets.splice(i, 1);
      const left = rails.get(ids[0]), right = rails.get(ids.at(-1));
      b += '<svg aria-hidden="true" style="position:absolute;left:' + r1(left) + 'px;top:' + (fy - 15)
        + 'px;width:' + r1(right - left) + 'px;height:8px;overflow:visible;pointer-events:none" viewBox="0 0 '
        + r1(right - left) + ' 8"><path d="M0 8V0H' + r1(right - left) + 'V8" fill="none" stroke="' + INK2 + '" stroke-width="1"/></svg>';
    }
    return b;
  };

  const F = (val, label, sub, id, cp, shiftId) => {
    const iw = widest(wFader, wText(label, 7.6), wText(sub, 6.8));
    return { w: iw, r: (x) => {
      const railX = x + (iw - SWD) / 2 - 5;
      rails.set(id, railX + SWD / 2);
      return fader({ x: railX, y: fy, h: FH, sw: SWD, cap: cp || cap, val, label, lsize: 7.6, minmax: false, sub, ly, sy, id: P(id), shiftId: shiftId ? P(shiftId) : null });
    } };
  };
  const K = (d, val, label, sub, id, shiftId, minmax) => {
    const iw = widest(wKnob(d), wText(label, 8.5), wText(sub, 7));
    return { w: iw, r: (x) => knob({ x: x + (iw - d) / 2, y: ky(d), d, v: kv, val, label, sub, ly, sy, id: P(id), shiftId: shiftId ? P(shiftId) : null, minmax }) };
  };
  const R = (d, sel, steps, label, id) => {
    const iw = widest(wRotary(d), wText(label, 8.5));
    return { w: iw, r: (x) => rotary({ x: x + (iw - d) / 2, y: ky(d), d, v: kv, sel, steps, label, ly, id: P(id) }) };
  };
  const SW = (pos, opts, label, id) => {
    const iw = widest(wSw3(SWH), wText(label, 7.6));
    // legend centred on lever + option text, so a long legend cannot reach the neighbour
    return { w: iw, r: (x) => sw3({ x: x + (iw - wSw3(SWH)) / 2, y: ky(SWH), h: SWH, pos, opts, label: '', title: label, id: P(id) })
      + txt(x - 6, ly, iw + 12, label, { size: 7.6 }) };
  };
  const SWCOL = (a, b) => {
    const iw = widest(wSw3(50), wText(a.label, 7.6), wText(b.label, 7.6));
    const off = (iw - wSw3(50)) / 2;
    return { w: iw,
      r: (x) => sw3({ x: x + off, y: cy - 66, h: 50, pos: a.pos, opts: a.opts, label: '', id: P(a.id) })
             + txt(x, cy - 10, iw, a.label, { size: 7.6 })
             + sw3({ x: x + off, y: cy + 22, h: 50, pos: b.pos, opts: b.opts, label: '', id: P(b.id) })
             + txt(x, ly, iw, b.label, { size: 7.6 }) };
  };
  // A functional cluster owns its controls, caption and bracket as one unit.
  const cluster = (items, caption, gap = 5) => {
    const w = items.reduce((n, it) => n + it.w, 0) + gap * (items.length - 1);
    return { w, r: (x) => pack({ x, w }, items, gap, caption)
      + hrule(x + 4, y + 301, w - 8, INK2, 1)
      + vrule(x + 4, y + 295, 6, INK2) + vrule(x + w - 4, y + 295, 6, INK2)
      + txt(x, y + 306, w, caption, { size: 5.6, color: INK2, weight: 600 }) };
  };

  // -- left column: layer name, MASTER VOLUME (upper) / DETUNE (lower)
  o += txt(PL, y + 2, 120, upper ? 'UPPER' : 'LOWER', { size: 13, align: 'left', ls: 0.14 });
  o += '<img id="' + (upper ? 'led-upper' : 'led-lower') + '" src="' + A.ledOff + '" alt="" style="position:absolute;left:' + (PL + 104) + 'px;top:' + (y + 6) + 'px;width:12px;height:12px;">';
  if (upper) {
    o += knob({ x: PL + 30, y: ky(72), d: 72, v: 'cream', val: 0.8, ticks: 11, label: 'MASTER VOLUME', ly, id: 'global.masterVolume', minmax: ['0', '+4dB'] });
  } else {
    o += knob({ x: PL + 34, y: ky(64), d: 64, v: 'orange', val: 0.5, ticks: 11, label: 'DETUNE', ly, sub: 'PERF DETUNE', sy, id: 'perf.lowerDetune', shiftId: 'perf.perfDetune', minmax: ['-7', '+7'] });
  }

  // -- LFO 1
  o += group('lfo', S.lfo.x, S.lfo.w);
  let g = S.lfo;
  o += sect(g.x, y + 2, g.w, 'LFO 1');
  const LFO1STACK = { w: wRotary(44), r: (x) => {
    // MODE and WAVE share one narrow column. The controls keep their native
    // size; only their placement changes so LFO1 does not consume needless width.
    const sx = x + (wRotary(44) - 44) / 2;
    return rotary({ x: sx, y: y + 48, d: 44, v: kv, sel: 0,
      steps: ['FREE', 'ONCE', 'RESET'], label: 'MODE', ly: y + 122, id: P('lfo1.mode') })
      + rotary({ x: sx, y: y + 156, d: 44, v: kv, sel: 0,
        steps: ['TRI', 'SAW', 'S&H', 'SQR', 'HF', 'TRK'], label: 'WAVE', ly: y + 230, id: P('lfo1.wave') })
      // SPKR addition (not on the hardware): 1 = every voice's LFO 1 in step, 2 = each voice its own
      + sw2h({ x: sx + 22, y: y + 268, h: 30, opts: ['1', '2'], label: 'VOICE PHASE', ly: y + 286,
               id: P('lfo1.phaseMode'), title: '1: all voices share one LFO 1  ·  2: each voice card runs its own' });
  } };
  o += pack(g, [
    F(0.67, 'RATE', null, 'lfo1.rate'),
    F(0.0, 'DELAY', null, 'lfo1.delay'),
    F(0.0, 'LR PHASE', 'SPREAD', 'lfo1.lrPhase'),
    GAP(4),
    LFO1STACK
  ], 5, 'lfo');
  o += endGroup();

  // -- DDS MODULATOR
  o += group('mod', S.mod.x, S.mod.w);
  g = S.mod;
  o += sect(g.x, y + 2, g.w, 'DDS MODULATOR');
  o += pack(g, [
    cluster([
      F(0.0, 'LFO 1', null, 'ddsMod.lfo1Amt'),
      F(0.0, 'ENV 1', null, 'ddsMod.env1Amt'),
      SW(1, ['DDS 2', '1+2', 'DDS 1'], 'DEST', 'ddsMod.dest')
    ], 'PITCH MODULATION'),
    GAP(4),
    cluster([
      SW(0, ['ON', '1/2', 'OFF'], 'SUPER', 'ddsMod.super'),
      F(0.0, 'PW/DET', 'DRIFT', 'ddsMod.pwDetune', null, 'ddsMod.drift'),
      F(0.0, 'DRIFT', null, 'ddsMod.drift', 'dark'),
      F(0.0, 'PWM', null, 'ddsMod.pwmWave'),
      SW(0, ['ENV 1', 'LFO 1', 'MAN'], 'PWM SRC', 'ddsMod.pwmSource')
    ], 'PULSE WIDTH / DETUNE / WAVE', 10),
    GAP(4),
    F(0.0, 'CROSS MOD', 'RING MOD', 'ddsMod.crossMod'),
  ], 5, 'mod');
  o += endGroup();

  // -- OSCILLATORS (two black insets)
  o += group('osc', S.osc.x, S.osc.w);
  g = S.osc;
  o += sect(g.x, y + 2, g.w, 'OSCILLATORS');
  const IW = (g.w - 10) / 2, bt = y + 30, bh = ROWH - 36, b2 = g.x + IW + 10;
  o += blackPanel(g.x, bt, IW, bh);
  o += blackPanel(b2, bt, IW, bh);
  o += txtInv(g.x + 8, bt + 7, 26, '1', { size: 8, bg: 'var(--badge)' });
  o += txtInv(b2 + 8, bt + 7, 26, '2', { size: 8, bg: 'var(--badge)' });
  o += txt(g.x + 40, bt + 8, 80, 'DDS 1', { size: 9, align: 'left', color: ONBLACK_INK });
  o += txt(b2 + 40, bt + 8, 80, 'DDS 2', { size: 9, align: 'left', color: ONBLACK_INK });
  const rd = 56, rcy = bt + 96, rly = bt + 150;
  const RB = (sel, steps, label, id) => ({
    w: IW / 2,
    r: (x) => rotary(Object.assign({ x: x + (IW / 2 - rd) / 2, y: rcy - rd / 2, d: rd, v: kv, sel, steps, label, ly: rly, lsize: 7.6, ssize: 6.6, waveSymbols: label === 'WAVEFORM', rangeOutline: label === 'RANGE', id: P(id) }, ONBLACK))
  });
  o += pack({ x: g.x, w: IW }, [RB(1, ['SIN', 'SAW', 'SQR', 'TRI', 'NSE', 'ALT'], 'WAVEFORM', 'dds1.wave'),
                                RB(3, ["64'", "32'", "16'", "8'", "4'", "2'"], 'RANGE', 'dds1.range')], 0, 'osc1');
  o += pack({ x: b2, w: IW }, [RB(2, ['SIN', 'SAW', 'SQR', 'TRI', 'NSE', 'PLS'], 'WAVEFORM', 'dds2.wave'),
                               RB(3, ['LFO', "32'", "16'", "8'", "4'", "2'"], 'RANGE', 'dds2.range')], 0, 'osc2');
  // lower shelf inside each inset
  const shy = bt + 204, shly = shy + 54;
  const ALT_DISPLAY = (ch) => ({ w: 146, r: (x) => {
    const dom = P('dds1.alt' + ch + '-display');
    return button({ x: x + 53, y: shy - 4, w: 40, led: false, label: 'ALT ' + ch,
      ly: shy + 30, lsize: 7.4, onDark: true, action: 'alt' + ch,
      domId: P('dds1.alt' + ch + '-open'), title: 'Choose alternative wave ' + ch })
      + txt(x, shy + 52, 146, 'W1 ORGAN 8-4', { id: dom, size: 5.8, color: ONBLACK_INK2,
        weight: 600, style: 'overflow:hidden;text-overflow:ellipsis;' });
  } });
  // CUSTOM (SPKR) took ALT B's place: it opens the sample page for this layer's DDS 1
  const CUSTOM = { w: 146, r: (x) => button({ x: x + 53, y: shy - 4, w: 40, led: false, label: 'CUSTOM',
      ly: shy + 30, lsize: 7.4, onDark: true, action: 'custom',
      domId: P('dds1.custom-open'), title: 'Custom wave: drop an audio file to play it from DDS 1' })
    + txt(x, shy + 52, 146, 'NO SAMPLE', { id: P('dds1.custom-display'), size: 5.8, color: ONBLACK_INK2,
      weight: 600, style: 'overflow:hidden;text-overflow:ellipsis;' }) };
  o += pack({ x: g.x, w: IW }, [ALT_DISPLAY('A'), CUSTOM], 8, 'osc1b');
  let modeX, subX;
  o += pack({ x: b2, w: IW }, [
    { w: 100, r: (x) => knob({ x: x + 32, y: shy - 6, d: 36, v: kv, val: 0.5, ticks: 15, tick: 'var(--otick)', label: 'TUNE', lsize: 7, ly: shly, labelColor: ONBLACK_INK, id: P('dds2.tune'), scaleValues: Array.from({ length: 15 }, (_, i) => i > 7 ? '+' + (i - 7) : String(i - 7)), lc2: ONBLACK_INK2 }) },
    { w: 96, r: (x) => { modeX = x + 18.5; return sw3(Object.assign({ x: x + 8, y: shy - 6, h: 46, pos: 2, opts: ['SYNC', 'RING', 'NORM'], label: '', ssize: 6.6, id: P('dds2.mode') }, ONBLACK)) + txt(x, shly, 96, 'MODE', { size: 7, color: ONBLACK_INK }); } },
    { w: 96, r: (x) => { subX = x + 18.5; return sw3(Object.assign({ x: x + 8, y: shy - 6, h: 46, pos: 2, opts: ['SIN', 'SQR', 'OFF'], label: '', ssize: 6.6, id: P('dds2.mode'), domId: P('dds2.mode') + '__sub' }, ONBLACK)) + txt(x, shly, 96, 'SUB OSC', { size: 7, color: ONBLACK_INK }); } }
  ], 10, 'osc2b');
  // MODE and SUB OSC are the same three-state parameter. The stepped leader
  // makes their paired states explicit without implying an extra audio route.
  o += '<svg aria-hidden="true" style="position:absolute;left:' + r1(modeX) + 'px;top:' + (shy - 18)
    + 'px;width:' + r1(subX - modeX) + 'px;height:10px;overflow:visible;pointer-events:none" viewBox="0 0 '
    + r1(subX - modeX) + ' 10"><path d="M0 10V4L4 0H' + r1(subX - modeX - 4) + 'L' + r1(subX - modeX)
    + ' 4V10m-4 -4l4 4l4 -4" fill="none" stroke="' + ONBLACK_INK2 + '" stroke-width="1.2"/></svg>';
  o += endGroup();

  // -- MIXER
  o += group('mix', S.mix.x, S.mix.w);
  g = S.mix;
  o += sect(g.x, y + 2, g.w, 'MIXER');
  o += pack(g, [K(60, 0.0, 'MIX', 'PAN', 'mixer.mix', 'mixer.pan', ['DDS 1', 'DDS 2'])], 0, 'mix');
  o += endGroup();

  // -- VCF. Two layouts in the same box: the Super Gemini's, and the 3rd Wave's (VCF STYLE = 3W,
  // a SPKR addition). The STYLE key swaps them; CSS shows one and hides the other per layer.
  o += dk(engine + '-vcf', S.vcf.x, y, S.vcf.w, ROWH, null, 1.15);
  g = S.vcf;
  o += sect(g.x, y + 2, g.w, 'VCF');
  // STYLE sits outside both layouts: it has to be reachable whichever one is showing
  o += button({ x: g.x + 8, y: y + 34, w: 32, led: true, ld: 8, label: 'STYLE', lsize: 6.2, ly: y + 66, lw: 48,
                id: P('vcf.style'), cycle: true, steps: 2,
                title: 'Filter style: SG (Super Gemini ladder) or 3W (3rd Wave)' });
  o += '<div class="vcf-sg-' + engine + '">';
  o += pack(g, [
    SW(0, ['2', '1', 'OFF'], 'DRIVE', 'vcf.drive'),
    F(0.0, 'HPF', null, 'vcf.hpf'),
    cluster([F(1.0, 'LPF', null, 'vcf.lpf'), F(0.0, 'RES', null, 'vcf.res')], 'FILTER'),
    SWCOL({ pos: 0, opts: ['ENV 2', '1+2', 'ENV 1'], label: 'ENV SRC', id: 'vcf.envSource' },
          { pos: 0, opts: ['ON', '1/2', 'OFF'], label: 'KEYTRACK', id: 'vcf.keytrack' }),
    cluster([F(0.0, 'ENV', null, 'vcf.envAmt'), F(0.0, 'LFO 1', null, 'vcf.lfo1Amt'),
      F(0.0, 'DDS 2', null, 'vcf.dds2Amt', 'dark')], 'CUTOFF MODULATION', 3)
  ], 3, 'vcf');
  // the brackets link the Super Gemini faders, so they belong inside that layout - drawn here
  // rather than in endGroup(), or they stay on screen over the 3rd Wave one
  o += bracketsDone();
  o += '</div><div class="vcf-3w-' + engine + '">';
  o += thirdWaveVcf(g, y, P, kv);
  o += '</div>';
  o += endGroup();

  // -- VCA
  o += group('vca', S.vca.x, S.vca.w);
  g = S.vca;
  o += sect(g.x, y + 2, g.w, 'VCA');
  o += pack(g, [
    SWCOL({ pos: 0, opts: ['ON', '1/2', 'OFF'], label: 'DYNAMICS', id: 'vca.dynamics' },
          { pos: 0, opts: ['GATE+R', 'GATE', 'ENV 2'], label: 'ENV', id: 'vca.envMode' }),
    cluster([F(0.8, 'ENV LEVEL', null, 'vca.envLevel'),
      F(0.0, 'LFO 1', null, 'vca.lfo1Amt'),
      F(0.0, 'DDS 2', null, 'vca.dds2Amt', 'dark')], 'AMPLITUDE')
  ], 5, 'vca');
  o += endGroup();

  // -- ENVELOPES (two black insets)
  o += group('env', S.env.x, S.env.w);
  g = S.env;
  o += sect(g.x, y + 2, g.w, 'ENVELOPES');
  const EW = (g.w - 10) / 2, ebt = y + 30, ebh = ROWH - 36, e2 = g.x + EW + 10;
  o += blackPanel(g.x, ebt, EW, ebh);
  o += blackPanel(e2, ebt, EW, ebh);
  o += txtInv(g.x + 8, ebt + 7, 26, '1', { size: 8, bg: 'var(--badge)' });
  o += txtInv(e2 + 8, ebt + 7, 26, '2', { size: 8, bg: 'var(--badge)' });
  o += txt(g.x + 40, ebt + 8, 80, 'ENV 1', { size: 9, align: 'left', color: ONBLACK_INK });
  o += txt(e2 + 40, ebt + 8, 80, 'ENV 2', { size: 9, align: 'left', color: ONBLACK_INK });
  const efy = ebt + 46, efh = FTOP + FH - (ebt + 46 - y), ely = ly, esy = ely + lineH(8.4) + 3;
  const EF = (v, lbl, sub, id, shiftId) => ({
    w: 30,
    r: (x) => fader({ x: x + 8, y: efy, h: efh, sw: 11, cap: cap, val: v, scale: true, tickW: 8, minmax: false, label: lbl, lsize: 8.4, ly: ely, sy: esy, sub, labelColor: ONBLACK_INK, onDark: true, id: P(id), shiftId: shiftId ? P(shiftId) : null })
  });
  const ESW = (pos, opts, label, id, extra) => ({
    w: wSw3(46, 40),
    r: (x) => sw3(Object.assign({ x, y: efy + 10, h: 46, pos, opts, label, lsize: 7, ly: efy + 66, ssize: 6.4, id: P(id) }, ONBLACK)) + (extra ? extra(x) : '')
  });
  o += pack({ x: g.x, w: EW }, [
    { w: 86, r: (x) => sw3({ x: x + 13, y: efy, h: 46, pos: 0, opts: ['ON','1/2','OFF'], label: '', title: 'KEYTRACK', id: P('env1.keytrack'), ...ONBLACK })
      + txt(x, efy + 55, 86, 'KEYTRACK', { size: 6.2, color: ONBLACK_INK })
      + sw3({ x: x + 13, y: efy + 88, h: 46, pos: 0, opts: ['LOOP','INV','NORM'], label: '', title: 'MODE', id: P('env1.mode'), ...ONBLACK })
      + txt(x, efy + 143, 86, 'MODE', { size: 6.2, color: ONBLACK_INK })
      + '<img id="' + P('loopLed') + '" src="' + A.ledOff + '" alt="" style="position:absolute;left:' + (x + 16) + 'px;top:' + (ely - 24) + 'px;width:10px;height:10px;">'
      + txt(x + 31, ely - 26, 44, 'LOOP', { size: 6.2, align: 'left', color: ONBLACK_INK2 }) },
    EF(0.0, 'A', 'AH', 'env1.attack', 'env1.attackHold'), EF(0.67, 'D', 'DH', 'env1.decay', 'env1.decayHold'),
    EF(0.0, 'S', null, 'env1.sustain'), EF(0.67, 'R', null, 'env1.release')
  ], 4, 'env1');
  o += pack({ x: e2, w: EW }, [
    GAP(20),
    EF(0.0, 'A', null, 'env2.attack'), EF(0.67, 'D', 'DH', 'env2.decay', 'env2.decayHold'),
    EF(1.0, 'S', null, 'env2.sustain'), EF(0.5, 'R', null, 'env2.release'),
    GAP(20)
  ], 14, 'env2');
  o += vrule(g.x + 91, efy - 4, 170, 'var(--otick)');
  o += endGroup();

  // -- DLY send
  o += group('dly', S.dly.x, S.dly.w);
  g = S.dly;
  o += sect(g.x, y + 2, g.w, 'DLY');
  o += pack(g, [F(0.0, 'SEND', null, 'fx.delaySend')], 0, 'dly');
  o += endGroup();

  o += '<div class="engine-overlays">';
  // section separators
  for (const k of ['mod', 'osc', 'mix', 'vcf', 'vca', 'env', 'dly']) o += vrule(S[k].x - SGAP / 2, y + 6, ROWH - 12);
  o += vrule(S.lfo.x - SGAP / 2, y + 6, ROWH - 12);
  o += '</div>';
  return o;
}

// ------------------------------------------------------------- global strip
function globalStrip(y) {
  let o = '';
  // One desktop card per hardware section; the separators between them stay full-panel only.
  const card = (name, w, k) => { o += dk('strip-' + name, x, y, w, STRIPH, null, k); };
  const T = y + 4;                                // section title top
  const B = y + 60;                               // key top (LED sits above)
  const BW = 42, BP = 58;                         // key width, pitch
  const LBLY = B + BW * BTN_ASPECT + 8;           // key label baseline
  const SUBY = LBLY + lineH(7.6) + 2;
  let x = PL;
  const title = (w, t) => { o += sect(x, T, w, t); };
  const STRIP_FIXED = 120 + 96 + 110 + 172 + 134 + 262 + 660 + (148 + 16 * 51 + 14 + 42 + 6) + 112 + 110 + (104 + 2 * 84);
  const DIV = (PR - PL - STRIP_FIXED) / 10;          // ten dividers between eleven sections
  const div = () => { x += DIV / 2 - 1; o += vrule(x, y + 6, STRIPH - 12); x += DIV / 2 + 1; };
  const key = (dx, opts) => button(Object.assign({ x: x + dx, y: B, w: BW, ly: LBLY }, opts));

  // MANUAL (SHIFT: INIT PATCH)
  card('manual', 120);
  title(120, 'MANUAL');
  o += key(6, { dark: true, label: 'LOWER', id: 'lower.manual', sub: 'INIT', sy: SUBY, title: 'MANUAL lower - Shift: init patch' });
  o += key(6 + BP, { label: 'UPPER', id: 'upper.manual', sub: 'INIT', sy: SUBY, title: 'MANUAL upper - Shift: init patch' });
  x += 120; o += dkEnd; div();

  // TEMPO
  card('tempo', 96);
  title(96, 'TEMPO');
  o += knob({ x: x + 18, y: B - 6, d: 56, v: 'cream', val: 0.33, ticks: 11, label: '', id: 'perf.tempo', minmax: ['30', '300'], title: 'TEMPO 30-300 BPM' });
  o += '<img id="led-tempo" src="' + A.ledOff + '" alt="" style="position:absolute;left:' + (x + 80) + 'px;top:' + (B - 4) + 'px;width:10px;height:10px;">';
  x += 96; o += dkEnd; div();

  // HOLD
  card('hold', 110);
  title(110, 'HOLD');
  o += key(6, { dark: true, label: 'LOWER', id: 'lower.hold' });
  o += key(6 + BP, { label: 'UPPER', id: 'upper.hold' });
  x += 110; o += dkEnd; div();

  // KEYBOARD (all three dark on the hardware)
  card('keyboard', 172);
  title(172, 'KEYBOARD');
  o += key(6, { dark: true, label: 'SINGLE', id: 'perf.keyboardMode', domId: 'perf.keyboardMode__0', value: 0, steps: 3 });
  o += key(6 + BP, { dark: true, label: 'DUAL', id: 'perf.keyboardMode', domId: 'perf.keyboardMode__1', value: 1, steps: 3 });
  o += key(6 + 2 * BP, { dark: true, label: 'SPLIT', id: 'perf.keyboardMode', domId: 'perf.keyboardMode__2', value: 2, steps: 3, sub: '(NOTE)', sy: SUBY, title: 'SPLIT - Shift: set split point from the next key' });
  x += 172; o += dkEnd; div();

  // LAYER - the black inset. LOWER dark, UPPER light, as the hardware.
  card('layer', 134);
  o += blackPanel(x - 6, y + 4, 140, STRIPH - 8);
  o += sect(x + 4, T, 120, 'LAYER', { color: ONBLACK_INK, ruleColor: 'var(--otick)' });
  o += key(10, { dark: true, label: 'LOWER', onDark: true, id: 'perf.singleLayer', domId: 'perf.singleLayer__1', value: 1, steps: 2, title: 'Edit / play the LOWER layer' });
  o += key(10 + BP, { label: 'UPPER', onDark: true, id: 'perf.singleLayer', domId: 'perf.singleLayer__0', value: 0, steps: 2, title: 'Edit / play the UPPER layer' });
  x += 134; o += dkEnd; div();

  // VOICE ASSIGN (per layer, follows LAYER)
  card('voice', 262);
  title(262, 'VOICE ASSIGN');
  o += key(6, { label: 'MODE', id: 'voice.mode', layered: true, cycle: true, steps: 4, sub: 'U. SIZE', sy: SUBY, shiftId: 'voice.unisonSize', title: 'Voice mode - Shift: unison size' });
  o += '<div class="shift-hide">' + ledLadder({ x: x + 62, y: B - 12, items: ['SOLO', 'LEGATO', 'POLY 1', 'POLY 2'], id: 'voice.mode', layered: true, sel: 2, lw: 64, pitch: 20 }) + '</div>';
  o += ledLadder({ x: x + 62, y: B - 12, items: ['1 HALF', '2 ALL', '3 OCT', '4 5TH+OCT'], id: 'voice.unisonSize', layered: true, sel: 1, lw: 64, pitch: 20 })
        .replace(/<img /g, '<img class="shift-only" ').replace(/<div style="position:absolute;left/g, '<div class="shift-only" style="position:absolute;left').replace(/<div id="upper\.voice\.unisonSize__led/g, '<div class="shift-only" id="upper.voice.unisonSize__led');
  o += key(142, { label: 'UNISON', id: 'voice.unison', layered: true });
  o += key(142 + 66, { label: 'BINAURAL', id: 'voice.binaural', layered: true });
  x += 262; o += dkEnd; div();

  // ARPEGGIATOR / SEQUENCER (per layer)
  card('arp', 660);
  title(660, 'ARPEGGIATOR / SEQUENCER');
  const arpKey = (dx, opts) => key(dx + (dx >= 446 ? 100 : dx >= 336 ? 60 : dx >= 190 ? 30 : 0), opts);
  o += key(4, { label: 'ON', id: 'arp.on', layered: true });
  // SYNC on: CLK DIV detents. SYNC off: a free RATE knob in the same place, marked in ms at
  // its shortest, middle and longest settings (the other is hidden - see .arp-free in the CSS).
  o += '<div class="arp-sync-only">' + rotary({ x: x + 100, y: B - 10, d: 56, v: 'cream', sel: 4, steps: ['1/1', '1/2', '1/4', '1/8', '1/16', '1/32', '1/4T', '1/8T'], label: 'CLK DIV', ly: LBLY + 26, ssize: 6, id: 'arp.clockDiv', layered: true }) + '</div>';
  {
    const kd = 56, kx = x + 100, ky = B - 10, rad = kd / 2 + kpad(kd) + 14;
    let free = knob({ x: kx, y: ky, d: kd, v: 'cream', val: 0.5, ticks: 11, label: 'RATE', ly: LBLY + 26, id: 'arp.rate', layered: true, title: 'Arp step length with SYNC off (20 ms to 2 s)' });
    // ends sit beside the knob (clear of the RATE legend), the centre value just above it
    [['20 MS', -118, rad, 'right'], ['200 MS', 0, rad - 11, 'center'], ['2000 MS', 118, rad, 'left']].forEach(([t, deg, rr, al]) => {
      const ang = deg * Math.PI / 180, px = kx + kd / 2 + Math.sin(ang) * rr, py = ky + kd / 2 - Math.cos(ang) * rr - 7;
      free += txt(al === 'right' ? px - 60 : al === 'left' ? px : px - 30, py, 60, t, { size: 6, align: al, color: INK2, weight: 600, ls: 0.02 });
    });
    o += '<div class="arp-free-only">' + free + '</div>';
  }
  o += arpKey(190, { label: 'SYNC', id: 'arp.sync', layered: true, sub: 'EXT CLK', sy: SUBY, shiftId: 'global.clockRx', title: 'LFO 1 / delay follow the clock - Shift: host clock receive' });
  o += arpKey(190 + BP, { label: 'RANGE', id: 'arp.range', layered: true, cycle: true, steps: 4, sub: 'SWING', sy: SUBY, shiftId: 'arp.swing', title: 'Arp range - Shift: swing' });
  o += '<div class="shift-hide">' + ledLadder({ x: x + 220 + BP + BW + 6, y: B - 12, items: ['1', '2', '3', '4'], id: 'arp.range', layered: true, sel: 0, lw: 14, pitch: 17, d: 9, lsize: 6 }) + '</div>';
  o += ledLadder({ x: x + 220 + BP + BW + 6, y: B - 12, items: ['OFF', '1', '2', '3', '4'], id: 'arp.swing', layered: true, sel: 0, lw: 24, pitch: 15, d: 8, lsize: 5.6 })
        .replace(/<img /g, '<img class="shift-only" ').replace(/<div style="position:absolute;left/g, '<div class="shift-only" style="position:absolute;left').replace(/<div id="upper\.arp\.swing__led/g, '<div class="shift-only" id="upper.arp.swing__led');
  o += arpKey(336, { label: 'MODE', id: 'arp.mode', layered: true, cycle: true, steps: 5, sub: 'LOAD', sy: SUBY, title: 'Arp mode - Shift: load sequence' });
  o += ledLadder({ x: x + 396 + BW + 6, y: B - 14, items: ['UP', 'DOWN', 'U&D', 'RANDOM', 'SEQ'], id: 'arp.mode', layered: true, sel: 0, lw: 52, pitch: 16, d: 9, lsize: 5.8 });
  o += arpKey(446, { label: 'SEQ REC', action: 'seqRec', domId: 'act-seqRec', sub: 'STORE', sy: SUBY, title: 'Record steps from the keys - Shift: store sequence' });
  o += arpKey(446 + BP, { dark: true, label: 'TRACK', action: 'openSeq', domId: 'act-track', title: 'Open the step sequencer' });
  x += 660; o += dkEnd; div();

  // MOD AMOUNT / MOD ASSIGN
  card('mod', 148 + 16 * 51 + 14 + 42 + 6, 1.12);
  title(148, 'MOD AMOUNT');
  o += knob({ x: x + 14, y: B - 6, d: 56, v: 'cream', val: 0.5, ticks: 11, label: '', id: 'modamt', domId: 'modamt', shiftId: 'global.fineTune', title: 'Modulation amount: -100 to +100 percent  -  Shift: global fine tune' });
  o += txt(x - 4, B + 62, 92, 'AMOUNT', { size: 7.6 });
  o += txtInv(x - 4, B + 82, 92, 'FINE ADJ', { size: 6.6 });
  o += key(104, { label: 'CLEAR', action: 'mtxClear', domId: 'act-mtxClear', title: 'Clear the selected routing - Shift: clear all for the source' });
  x += 148;
  const mx = x + 6, MP = 51, MW = 34;
  o += sect(mx, T, 16 * MP - 12, 'MOD ASSIGN');
  o += txt(mx - 4, B - 30, 8 * MP, 'SOURCES', { size: 6.6, align: 'left', color: INK2, weight: 600 });
  o += txt(mx + 8 * MP, B - 30, 8 * MP, 'DESTINATIONS', { size: 6.6, align: 'left', color: INK2, weight: 600 });
  const srcs = ['DDS 2', 'LFO 2', 'ENV 1', 'VEL', 'AT', 'EXPR', 'RIBN', 'NOTE'];
  const dsts = ['LFO 1', 'X MOD', 'WAVE', 'MIX', 'HPF', 'RES', 'ENV1-D', 'DLY TIME'];
  srcs.forEach((s, i) => {
    o += button({ x: mx + i * MP, y: B, w: MW, ly: LBLY, label: s, lsize: 6.4, action: 'mtxSrc', domId: 'mtxsrc' + i, title: 'Source ' + (i + 1) + ': ' + s });
  });
  dsts.forEach((s, i) => {
    o += button({ x: mx + (8 + i) * MP, y: B, w: MW, ly: LBLY, label: s, lsize: 6.4, dark: true, action: 'mtxDst', domId: 'mtxdst' + i, title: 'Destination ' + 'ABCDEFGH'[i] + ': ' + s });
  });
  o += key(16 * MP + 14, { label: 'MATRIX', action: 'openMatrix', domId: 'act-matrix', led: false, title: 'Open the full modulation matrix' });
  x += 16 * MP + 14 + BW + 6; o += dkEnd; div();

  // EDIT: A/B compare + SHIFT
  card('edit', 112);
  title(112, 'EDIT');
  o += key(4, { label: 'A / B', action: 'ab', domId: 'act-ab', title: 'Compare with the stored sound (Shift: store current as the other)' });
  o += key(4 + BP, { dark: true, label: 'SHIFT', action: 'shift', domId: 'act-shift', title: 'Secondary functions (latching)' });
  x += 112; o += dkEnd; div();

  // CHORUS (per layer)
  card('chorus', 110);
  title(110, 'CHORUS');
  o += key(6, { label: 'I', id: 'fx.chorus', layered: true, domId: 'upper.fx.chorus__1', bit: 1, steps: 4 });
  o += key(6 + BP, { label: 'II', id: 'fx.chorus', layered: true, domId: 'upper.fx.chorus__2', bit: 2, steps: 4 });
  o += hrule(x + 6, LBLY + 24, BP + BW, INK2, 1);
  o += txt(x, LBLY + 30, 110, 'CHORUS MODES', { size: 5.8, color: INK2, weight: 600 });
  x += 110; o += dkEnd; div();

  // DELAY (per layer)
  const delayPitch = 84, DLW = 104 + 2 * delayPitch;     // item centres at 52, 52+p, 52+2p
  x += Math.max(0, (PR - x - DLW) / 2);
  card('delay', DLW);
  title(DLW, 'DELAY');
  o += knob({ x: x + 24, y: B - 6, d: 56, v: 'cream', val: 0.85, ticks: 11, label: 'TIME', lsize: 7.6, ly: B + 66, id: 'fx.delayTime', layered: true });
  o += knob({ x: x + 24 + delayPitch, y: B - 6, d: 56, v: 'cream', val: 0.3, ticks: 11, label: 'FEEDBACK', lsize: 7.6, ly: B + 66, id: 'fx.delayFeedback', layered: true });
  o += button({ x: x + 31 + delayPitch * 2, y: B, w: BW, ly: B + 66, label: 'FREEZE', id: 'fx.freeze', layered: true });
  o += dkEnd;
  return o;
}

// -------------------------------------------------------------- performance block (left of keys)
function perfBlock(x0, y0, w) {
  let o = '';
  const sec = { x: x0, w };
  // The performance panel follows the hardware's clean four-row hierarchy:
  // transpose/octave at the top, LFO2 and destination in the middle, then
  // the depth faders and bender amount below. Every row gets its own baseline.
  o += dk('perf', x0, y0 - 2, w, 358);
  o += sect(x0, y0, w, 'PERFORMANCE', { align: 'left', ruleColor: 'var(--rule)' });

  // ---- OCTAVE / TRANSPOSE / PORTAMENTO
  const c1 = y0 + 58;
  o += txt(x0 + 6, y0 + 25, 110, 'TRANSPOSED', { size: 6.6, align: 'left', color: INK2, weight: 600 });
  const OCT = { w: 156, r: (bx) => {
    let s = '';
    [-2, -1, 0, 1, 2].forEach((n, i) => {
      s += ledBound({ x: bx + 8 + i * 27, y: y0 + 43, d: 10, id: 'octave', layered: true, domId: 'upper.octave__led' + i, value: i, steps: 5, on: n === 0 });
      s += txt(bx - 2 + i * 27, y0 + 57, 30, (n > 0 ? '+' : '') + n, { size: 6.2, color: INK2, weight: 600, align: 'center' });
    });
    s += txt(bx, y0 + 78, 156, 'OCTAVE', { size: 7.6 });
    return s;
  } };
  const ROCK = { w: 58, r: (bx) => sw3({ x: bx, y: y0 + 35, h: 50, pos: 1, opts: ['OCT +', '', 'OCT -'], label: '', id: 'octave', layered: true, domId: 'upper.octave__rocker', rocker: true, hitData: { steps: 5, shiftparam: 'global.transpose' }, ssize: 6 }) };
  const PORTA = { w: 190, r: (bx) => hfader({ x: bx + 8, y: y0 + 39, w: 150, sw: SWD, cap: 'grey', val: 0.0, label: 'PORTAMENTO', ly: y0 + 78, id: 'porta.time', layered: true }) };
  const PL2 = { w: 58, r: (bx) => sw3({ x: bx, y: y0 + 35, h: 44, pos: 2, opts: ['UPPER', 'LOWER', 'BOTH'], label: 'LAYER', ly: y0 + 78, id: 'perf.portaLayer', ssize: 6, hitData: { map: '2,1,0' } }) };
  o += pack(sec, [OCT, GAP(8), ROCK, GAP(14), PORTA, GAP(8), PL2], 4, 'perf1');
  o += hrule(x0, y0 + 101, w, 'var(--rule)', 1);

  // ---- LFO 2 + DEST
  const l2y = y0 + 112;
  const c2 = l2y + 54, lb2 = l2y + 96;
  const lfoW = 330, destX = x0 + lfoW + 14, destW = w - lfoW - 14;
  o += sect(x0, l2y, lfoW, 'LFO 2');
  o += sect(destX, l2y, destW, 'DEST');
  o += vrule(destX - 8, l2y + 4, 96);
  o += pack({ x: x0, w: lfoW }, [
    { w: wRotary(46, 34), r: (bx) => rotary({ x: bx + (wRotary(46, 34) - 38) / 2, y: c2 - 19, d: 38, sel: 0, steps: ['SIN', 'RSAW', 'S&H', 'SQR', 'SAW', 'NSE'], label: 'WAVE', ly: lb2, ssize: 6.2, id: 'lfo2.wave', layered: true }) },
    { w: wKnob(52) + 10, r: (bx) => knob({ x: bx + kpad(52) + 10, y: c2 - 21, d: 42, v: 'cream', val: 0.67, ticks: 11, label: 'RATE', ly: lb2, id: 'lfo2.rate', layered: true, minmax: ['0.05', '50Hz'] })
                                    + '<img id="led-lfo2" src="' + A.ledOff + '" alt="" style="position:absolute;left:' + r1(bx + kpad(52) + 62) + 'px;top:' + r1(c2 - 26) + 'px;width:10px;height:10px;">' },
    { w: wKnob(52) + 10, r: (bx) => knob({ x: bx + kpad(52) + 10, y: c2 - 21, d: 42, v: 'cream', val: 0.0, ticks: 11, label: 'DELAY', ly: lb2, id: 'lfo2.delay', layered: true, minmax: ['0', '5s'] }) }
  ], 6, 'lfo2');
  o += pack({ x: destX, w: destW }, [
    { w: wSw3(44, 40), r: (bx) => sw3({ x: bx, y: c2 - 22, h: 44, pos: 1, opts: ['DDS 2', '1+2', 'DDS 1'], label: 'OSC', ly: lb2, id: 'dest.osc', layered: true, ssize: 6 }) },
    { w: wSw3(44, 44), r: (bx) => sw3({ x: bx, y: c2 - 22, h: 44, pos: 2, opts: ['UPPER', 'LOWER', 'BOTH'], label: 'LAYER', ly: lb2, id: 'perf.modLayer', ssize: 6, hitData: { map: '2,1,0' } }) }
  ], 4, 'dest');
  o += hrule(x0, y0 + 234, w, 'var(--rule)', 1);

  // ---- depth faders + trigger
  // band starts below the LFO 2 / DEST labels (which end at y0 + 226); labels below stay at y0 + 310
  const l3y = y0 + 244;
  const c3 = y0 + 282, lb3 = y0 + 310;
  const DF = (v, lbl, cp, id) => ({
    w: Math.max(wFader - 6, wText(lbl, 7.4)),
    r: (bx) => fader({ x: bx + 6, y: l3y, h: 54, sw: SWD, cap: cp || 'grey', val: v, label: lbl, lsize: 7.4, ly: lb3, id: id, layered: true, minmax: false })
  });
  const depthGroup = (items, caption) => {
    const gw = items.reduce((n, it) => n + it.w, 0) + 8 * (items.length - 1);
    return { w: gw, r: (bx) => pack({ x: bx, w: gw }, items, 8, caption)
      + hrule(bx + 3, lb3 + 24, gw - 6, INK2, 1)
      + vrule(bx + 3, lb3 + 18, 6, INK2) + vrule(bx + gw - 3, lb3 + 18, 6, INK2)
      + txt(bx, lb3 + 30, gw, caption, { size: 6, color: INK2, weight: 600 }) };
  };
  o += pack(sec, [
    { w: wSw3(50, 40), r: (bx) => sw3({ x: bx, y: c3 - 25, h: 50, pos: 0, opts: ['LFO2 ON', 'AT+TRIG', 'TRIG'], label: 'TRIG', ly: lb3, id: 'lfo2.trigger', layered: true, ssize: 5.8 })
                                    + txtInv(bx - 4, y0 + 238, 62, 'AT>BEND', { size: 5.6 }) },
    DF(0.0, 'LFO2 RATE', 'dark', 'lfo2.rateMod'),
    GAP(16),
    depthGroup([DF(0.2, 'DDS', null, 'lfo2.ddsAmt'),
      DF(0.0, 'VCF', null, 'lfo2.vcfAmt'),
      DF(0.0, 'VCA', null, 'lfo2.vcaAmt')], 'LFO 2 DEPTH'),
    GAP(24),
    depthGroup([DF(0.17, 'DDS', null, 'bender.ddsAmt'),
      DF(0.0, 'VCF', null, 'bender.vcfAmt')], 'BEND DEPTH')
  ], 6, 'depth');

  o += dkEnd;

  // ---- bender lever (its own desktop card, lever top level with the strip keys)
  const bw = 96, bhh = Math.round(bw * 250 / 550), bxx = x0 + (w - bw) / 2, byy = H - bhh - 16;
  o += dk('bender', bxx - 52, byy - 60, 200, STRIPH);
  o += '<div class="dk-only">' + sect(bxx - 52, byy - 56, 200, 'BENDER')
    + txt(bxx - 52, byy + bhh + 10, 200, 'PUSH = LFO 2', { size: 6.4, color: INK2, weight: 600 }) + '</div>';
  o += txt(bxx - 60, byy + bhh / 2 - 8, 50, '◁', { size: 8, color: INK2, align: 'right' });
  o += txt(bxx + bw + 10, byy + bhh / 2 - 8, 50, '▷', { size: 8, color: INK2, align: 'left' });
  o += txt(bxx + bw + 24, byy + 6, 150, 'PUSH = LFO 2', { size: 6.4, color: INK2, weight: 600, cls: 'fp-only' });
  o += '<img id="bender-img" src="' + A.bender + '" alt="" style="position:absolute;left:' + bxx + 'px;top:' + byy + 'px;width:' + bw + 'px;height:' + bhh + 'px;transform-origin:50% 100%;">';
  o += '<div id="bender-input" title="Bender: sideways = pitch, forward = push" style="position:absolute;left:' + (bxx - 20) + 'px;top:' + byy + 'px;width:' + (bw + 40) + 'px;height:' + bhh + 'px;cursor:grab;z-index:5;touch-action:none;"></div>';
  o += txt(bxx - 10, byy + bhh, bw + 20, 'BENDER', { size: 7.6, cls: 'fp-only' });
  o += dkEnd;
  return o;
}

// ------------------------------------------------------------------ ribbon
function ribbonRow(y, rx, rw) {
  let o = '';
  const rh = 56;
  const endW = rh * 2.2;
  const playableX = r1(rx + endW + 5), playableW = r1(rw - 2 * endW - 10);
  o += sect(rx, y - 33, rw, 'RIBBON  ·  ACROSS: BEND  ·  PUSH UP: AFTERTOUCH', { size: 9, ruleColor: 'var(--rule)' });
  o += '<img src="' + A.ribbon + '" alt="" style="position:absolute;left:' + rx + 'px;top:' + y + 'px;width:' + rw + 'px;height:' + rh + 'px;">';
  o += '<img src="' + A.ribbonLeft + '" alt="" style="position:absolute;left:' + rx + 'px;top:' + y + 'px;width:' + endW + 'px;height:' + rh + 'px;">';
  o += '<img src="' + A.ribbonRight + '" alt="" style="position:absolute;left:' + (rx + rw - endW) + 'px;top:' + y + 'px;width:' + endW + 'px;height:' + rh + 'px;">';
  // ACROSS bends from where the finger lands [p.77]; PUSH UP is aftertouch (user, 2026-10-05: most
  // keyboards have none). While touched: a tick where it landed, the bend as a span, the finger as a
  // bar, and a pressure bar rising from the strip - flat marks, drawn by ui/geminus.js
  const ty = y + 8, th = rh - 16, mk = 'position:absolute;top:' + ty + 'px;height:' + th + 'px;pointer-events:none;opacity:0;transition:opacity .12s;';
  o += '<div id="ribbon-span" style="' + mk + 'left:' + (rx + rw / 2) + 'px;width:0;background:' + ORANGE + ';opacity:0;"></div>';
  o += '<div id="ribbon-anchor" style="' + mk + 'left:' + (rx + rw / 2) + 'px;width:2px;margin-left:-1px;background:#F2EEE8;"></div>';
  o += '<div id="ribbon-marker" style="' + mk + 'left:' + (rx + rw / 2) + 'px;width:6px;margin-left:-3px;border-radius:3px;background:' + ORANGE + ';"></div>';
  o += '<div id="ribbon-at" style="position:absolute;left:' + (rx + rw / 2) + 'px;top:' + (ty - 4) + 'px;width:14px;margin-left:-7px;height:0;transform:translateY(-100%);border-radius:3px 3px 0 0;'
    + 'background:' + ORANGE + ';pointer-events:none;opacity:0;transition:opacity .12s;"></div>';
  o += '<div id="ribbon-readout" style="position:absolute;left:0;top:0;transform:translate(-50%,-100%);padding:5px 12px;border-radius:4px;background:#23252A;color:#F2EEE8;white-space:nowrap;'
    + 'font:700 15px ' + FONT + ';font-stretch:75%;letter-spacing:.1em;pointer-events:none;opacity:0;transition:opacity .12s;z-index:8"></div>';
  o += '<div id="ribbon-input" title="Ribbon: slide across to bend the pitch from where you land · push up for aftertouch" style="position:absolute;left:' + playableX + 'px;top:' + ty + 'px;width:' + playableW + 'px;height:' + th + 'px;cursor:pointer;touch-action:none;z-index:5;"></div>';
  for (let i = 1; i < 4; i++) {
    const mxp = playableX + (playableW * i / 4);
    o += '<div style="position:absolute;left:' + r1(mxp) + 'px;top:' + (y - 9) + 'px;width:1.5px;height:6px;background:' + INK2 + ';opacity:.8;"></div>';
  }
  return o;
}

// the product mark: Somii, by S·P·K·R (user, 2026-10-05; 002 before), centred under the rule beside the
// ribbon. Set on the font's own metrics (Bahnschrift condensed: at 64 px the cap height is 46 px and the
// baseline 51 px into a 64 px line; at 30 px 21 / 24; at 15 px 11 / 12), so that:
//   - the name's cap band sits on the ribbon's centre line (cap top = top + 5, baseline = top + 51)
//   - the divider spans exactly cap top to baseline
//   - BY starts at the name's cap top and S·P·K·R stands on the name's baseline
// `top` is the ribbon's top edge (the ribbon is 56 px tall, so its centre is top + 28 = the cap band's centre).
function wordmark(x, top) {
  return '<div style="position:absolute;left:' + x + 'px;top:' + top + 'px;width:470px;height:64px;display:flex;align-items:flex-start;justify-content:center;gap:22px;'
    + 'font-family:' + FONT + ';font-stretch:75%;color:' + INK + ';white-space:nowrap;pointer-events:none">'
    + '<span style="font-size:64px;font-weight:700;letter-spacing:.03em;line-height:64px;height:64px">Somii</span>'
    + '<span style="width:1.5px;height:46px;margin-top:5px;background:currentColor;opacity:.8"></span>'
    + '<span style="display:flex;flex-direction:column;align-items:flex-start">'
    + '<span style="font-size:15px;font-weight:600;letter-spacing:.3em;line-height:15px;height:15px;margin-top:4px;color:' + INK2 + '">BY</span>'
    + '<span style="font-size:30px;font-weight:700;letter-spacing:.04em;line-height:30px;height:30px;margin-top:8px">S·P·K·R</span></span></div>';
}

// ------------------------------------------------------------------ keyboard
// 5 octaves from C2 (60 keys). The octave render has a black key overlapping every white,
// so a lone top C cannot be cut from it.
function keyboard(x, y, w, h) {
  const oct = w / 5;
  let keys = '';
  for (let i = 0; i < 5; i++)
    keys += `<img src="${A.octave}" alt="" style="position:absolute;left:${r1(oct * i)}px;top:0;width:${r1(oct)}px;height:${h}px;">`;
  const WHITE = [0, 2, 4, 5, 7, 9, 11];
  const WSEAM = [0, 0.962, 1.940, 2.946, 3.938, 4.944, 5.965, 6.985];
  const BLACK = [[1, 0.955], [3, 2.078], [6, 3.916], [8, 5.031], [10, 6.154]];
  const kw = oct / 7, bw = kw * 0.52, bh = h * 0.569;
  const base = 36;
  const GRAD = 'linear-gradient(180deg,rgba(0,0,0,.30) 0%,rgba(0,0,0,.13) 28%,rgba(0,0,0,.10) 72%,rgba(0,0,0,.22) 100%)';
  const shade = (top) => 'background-image:' + GRAD + ';background-size:100% ' + r1(h) + 'px;background-position:0 -' + r1(top) + 'px;background-repeat:no-repeat;';
  let press = '', hits = '';
  for (let o = 0; o < 5; o++) {
    const blacks = BLACK.map(([semi, at]) => { const l = o * oct + at * kw - bw / 2; return { semi, l, r: l + bw }; });
    WHITE.forEach((semi, i) => {
      const n = base + o * 12 + semi;
      const L = o * oct + WSEAM[i] * kw, R = o * oct + WSEAM[i + 1] * kw, ww = R - L;
      press += `<div data-press="${n}" style="position:absolute;left:${r1(L)}px;top:${r1(bh)}px;width:${r1(ww)}px;height:${r1(h - bh)}px;z-index:1;opacity:0;pointer-events:none;border-radius:0 0 ${r1(ww * 0.10)}px ${r1(ww * 0.10)}px;${shade(bh)}"></div>`;
      let tl = L, tr = R;
      for (const b of blacks) {
        if (b.r > tl && b.l <= L) tl = Math.max(tl, b.r);
        if (b.l < tr && b.r >= R) tr = Math.min(tr, b.l);
      }
      if (tr - tl > 0.5)
        press += `<div data-press="${n}" style="position:absolute;left:${r1(tl)}px;top:0;width:${r1(tr - tl)}px;height:${r1(bh)}px;z-index:1;opacity:0;pointer-events:none;${shade(0)}"></div>`;
      hits += `<div data-note="${n}" style="position:absolute;left:${r1(L)}px;top:0;width:${r1(ww)}px;height:${h}px;z-index:2;cursor:pointer;"></div>`;
    });
    for (const b of blacks) {
      const n = base + o * 12 + b.semi;
      press += `<div data-press="${n}" style="position:absolute;left:${r1(b.l)}px;top:0;width:${r1(bw)}px;height:${r1(bh)}px;z-index:1;opacity:0;pointer-events:none;border-radius:0 0 ${r1(bw * 0.16)}px ${r1(bw * 0.16)}px;background:linear-gradient(180deg,rgba(0,0,0,.50),rgba(0,0,0,.26) 55%,rgba(0,0,0,.40));box-shadow:inset 0 2px 6px rgba(0,0,0,.45);"></div>`;
      hits += `<div data-note="${n}" style="position:absolute;left:${r1(b.l)}px;top:0;width:${r1(bw)}px;height:${r1(bh)}px;z-index:3;cursor:pointer;"></div>`;
    }
  }
  return `<div id="keybed" title="Keys: click or drag to play; Shift-drag splits the velocity by height" style="position:absolute;left:${x}px;top:${y}px;width:${w}px;height:${h}px;overflow:hidden;touch-action:none;">${keys}${press}${hits}</div>`;
}

// ------------------------------------------------------------------ top bar
const BAR_L = 1540;                              // fills the desktop header up to the VOICES / BPM block                              // width of the patch field + command links
function topBar() {
  let o = '';
  o += blackPanel(PL, 8, PR - PL, TOPBAR - 14);
  // patch field: click the name to browse; prev / next live inside its right end. SAVE lives only in the
  // browser (user, 2026-10-05: a one-click save here overwrote the file on top of the loaded patch)
  const fx = PL + 16, fy = 15, fw = 600, fh = 34;
  o += dk('bar-l', PL, 0, BAR_L, TOPBAR);
  o += '<div class="patch-field" style="position:absolute;left:' + fx + 'px;top:' + fy + 'px;width:' + fw + 'px;height:' + fh + 'px;background:rgba(0,0,0,.3);border-radius:3px;box-shadow:inset 0 1px 3px rgba(0,0,0,.6),inset 0 0 0 1px rgba(255,255,255,.1);"></div>';
  o += txt(fx + 12, fy + 9, 70, 'PATCH', { size: 8, align: 'left', color: 'var(--hi)', ls: 0.14 });
  const inW = [34, 34], inX = fx + fw - 6 - inW.reduce((s, v) => s + v, 0);
  o += txt(fx + 82, fy + 6, inX - fx - 92, 'INIT', { size: 11, align: 'left', color: 'var(--hi)', ls: 0.06, id: 'patch-name', style: 'overflow:hidden;text-overflow:ellipsis;' });
  o += '<div data-open="pop-patches" title="Open the patch browser" style="position:absolute;left:' + fx + 'px;top:' + fy + 'px;width:' + (inX - fx) + 'px;height:' + fh + 'px;cursor:pointer;z-index:6;"></div>';
  const inLink = (x, wd, label, action, opt) => keyButton(x, fy, wd, label, action, Object.assign({ link: true, size: label.length > 1 ? 8.4 : 9, h: fh, color: 'var(--barlink)' }, opt));
  o += inLink(inX, inW[0], '◀', 'patchPrev', { title: 'Previous patch in the folder' });
  o += inLink(inX + inW[0], inW[1], '▶', 'patchNext', { title: 'Next patch in the folder' });
  // menus, starting right after the patch field
  const barLinks = [
    ['SETTINGS', 'openSettings', {}],
    ['DESKTOP', 'toggleDesktopLayout', { domId: 'act-desktop-top', title: 'Desktop mode: larger controls for laptop screens, one layer at a time' }],
    ['MATRIX', 'openMatrix', { domId: 'act-matrix-top' }],
    ['SEQUENCE', 'openSeq', { domId: 'act-seq-top' }],
    ['FX', 'openFx', { domId: 'act-fx-top', title: 'Effects routing' }]
  ];
  {
    // spread evenly between the patch box and the end of the bar: equal space around every word
    const lx0 = fx + fw, span = BAR_L - (lx0 - PL);
    const widths = barLinks.map(([label]) => wText(label, 8.4, 0.08) + 24);
    // each slot has 12px of padding round its word, so the first gap gets that much extra
    const gap = (span - widths.reduce((s, v) => s + v, 0) - 12) / (barLinks.length + 1);
    let lx = lx0 + gap + 12;
    barLinks.forEach(([label, action, opt], i) => {
      o += inLink(lx, widths[i], label, action, opt);
      lx += widths[i] + gap;
    });
  }
  o += dkEnd;
  // right: voices, tempo, CPU-free meters
  const rx = PR - 420;
  o += dk('bar-r', rx, 0, 420, TOPBAR);
  o += txt(rx, 24, 90, 'VOICES', { size: 7.4, align: 'left', color: ONBLACK_INK2, ls: 0.1 });
  o += txt(rx + 70, 20, 80, '0', { size: 11, align: 'left', color: 'var(--hi)', id: 'voice-count' });
  o += txt(rx + 130, 24, 60, 'BPM', { size: 7.4, align: 'left', color: ONBLACK_INK2, ls: 0.1 });
  o += txt(rx + 174, 20, 90, '120', { size: 11, align: 'left', color: 'var(--hi)', id: 'bpm-readout' });
  // output meter
  const mx = rx + 260, my = 20, mw = 140, mh = 8;
  ['l', 'r'].forEach((ch, i) => {
    o += '<div style="position:absolute;left:' + mx + 'px;top:' + (my + i * 12) + 'px;width:' + mw + 'px;height:' + mh + 'px;background:rgba(0,0,0,.4);border-radius:2px;box-shadow:inset 0 1px 2px rgba(0,0,0,.6);"></div>';
    o += '<div data-meter="' + ch + '" style="position:absolute;left:' + mx + 'px;top:' + (my + i * 12) + 'px;width:' + mw + 'px;height:' + mh + 'px;border-radius:2px;background:linear-gradient(90deg,#6FB36A 0%,#D8C24A 70%,' + ORANGE + ' 100%);transform:scaleX(0);transform-origin:0 50%;"></div>';
  });
  o += txt(mx, my + 26, mw, 'OUTPUT', { size: 6, color: ONBLACK_INK2, weight: 600 });
  o += dkEnd;
  return o;
}

// ------------------------------------------------------------------ pop-overs
const MTX_SRC = [['dds2', 'DDS 2'], ['lfo2', 'LFO 2'], ['env1', 'ENV 1'], ['vel', 'VELOCITY'], ['at', 'AFTERTOUCH'], ['expr', 'EXPRESSION'], ['ribbon', 'RIBBON'], ['note', 'NOTE NO.']];
const MTX_DST = [['lfo1Rate', 'LFO 1 RATE'], ['xmod', 'CROSS MOD'], ['wave', 'WAVE MOD'], ['mix', 'OSC MIX'], ['hpf', 'HPF'], ['res', 'RESONANCE'], ['env1Decay', 'ENV 1 DECAY'], ['dlyTime', 'DELAY TIME']];

// Each route is a cell: idle cells sit back, a cell with an amount lights and carries a bar that
// grows from its centre (left = negative). Hovering a cell lights its source row and destination column.
function matrixPage(w, h) {
  let o = '';
  o += layerLink(w, 'mtx-layer-link');
  const x0 = 250, y0 = 128, cw = (w - x0 - 24) / 8, ch = (h - y0 - 24) / 8, G = 5;
  o += txt(30, 74, 600, '', { size: 8, align: 'left', color: INK2, weight: 600, id: 'mtx-count' });
  o += '<div id="mtx-geo" hidden data-x0="' + x0 + '" data-y0="' + y0 + '" data-cw="' + r1(cw) + '" data-ch="' + r1(ch) + '"></div>';
  MTX_DST.forEach((d, c) => o += txt(x0 + c * cw, y0 - 32, cw, d[1], { size: 8, color: INK2, ls: 0.1, id: 'mtx-col' + c, cls: 'mtx-head' }));
  MTX_SRC.forEach((s, r) => o += txt(24, y0 + r * ch + ch / 2 - 9, 206, s[1], { size: 8.6, align: 'right', color: INK2, ls: 0.08, id: 'mtx-row' + r, cls: 'mtx-head' }));
  const kd = 40;
  MTX_SRC.forEach((s, r) => MTX_DST.forEach((d, c) => {
    const dom = 'mtx.' + s[0] + '.' + d[0];
    const x = x0 + c * cw, y = y0 + r * ch, cx = x + cw / 2, cy = y + ch / 2 - 10;
    o += '<div id="' + dom + '__cell" class="mtx-cell" style="left:' + r1(x + G) + 'px;top:' + r1(y + G) + 'px;width:' + r1(cw - 2 * G) + 'px;height:' + r1(ch - 2 * G) + 'px"></div>';
    o += knob({ x: cx - kd / 2, y: cy - kd / 2, d: kd, v: 'cream', val: 0.5, ticks: 5, id: 'mtx.' + s[0] + '.' + d[0], layered: true, domId: dom });
    o += txt(cx - 50, cy + kd / 2 + 8, 100, '0', { size: 6.8, color: INK2, weight: 600, id: dom + '__val', cls: 'mtx-val' });
  }));
  return o;
}

// the layer switch of a page: a link beside the close button naming the layer you would switch TO
function layerLink(w, domId) {
  return keyButton(w - 200, 14, 130, 'LOWER', 'layerToggle', { link: true, domId, align: 'right', size: 9.4, title: 'Switch to the other layer' });
}

function seqPage(w, h) {
  let o = '';
  o += layerLink(w, 'seq-layer-link');
  // pages of 16, then the working-sequence readout
  const py = 66;
  o += txt(30, py + 8, 80, 'PAGE', { size: 7.4, align: 'left', color: INK2 });
  ['1-16', '17-32', '33-48', '49-64'].forEach((t, p) =>
    o += keyButton(96 + p * 96, py, 90, t, 'seqPage' + p, { link: true, size: 8.4, domId: 'seq-page' + p, title: 'Steps ' + (p * 16 + 1) + ' - ' + (p * 16 + 16) }));
  o += txt(500, py + 8, 800, '', { size: 8, align: 'left', color: INK, id: 'seq-info' });
  // step grid: 16 cards in four beats of four
  const gx = 30, gy = 118, BG = 22, chh = 214, cw = (w - 60 - 3 * BG) / 16;
  const inkC = ONBLACK_INK;
  for (let i = 0; i < 16; i++) {
    const x = gx + i * cw + Math.floor(i / 4) * BG, cx = x + 3, cwi = cw - 6;
    o += '<div id="seq-cell' + i + '" class="seq-cell" data-seqstep="' + i + '" style="left:' + r1(cx) + 'px;top:' + gy + 'px;width:' + r1(cwi) + 'px;height:' + chh + 'px;"></div>';
    o += '<div id="seq-play' + i + '" class="seq-play" style="left:' + r1(cx) + 'px;top:' + gy + 'px;width:' + r1(cwi) + 'px;"></div>';
    o += txt(cx + 12, gy + 12, cwi - 24, String(i + 1), { size: 6.6, color: inkC, weight: 600, align: 'left', id: 'seq-num' + i, style: 'z-index:6;opacity:.6;' });
    o += txt(cx, gy + 52, cwi, '—', { size: 15, color: inkC, id: 'seq-note' + i, cls: 'seq-note', style: 'z-index:6;' });
    o += txt(cx, gy + 92, cwi, '', { size: 6.2, color: inkC, weight: 600, id: 'seq-more' + i, style: 'z-index:6;opacity:.7;' });
    ['SLIDE', 'ACCENT', 'REST'].forEach((f, k) => {
      const fy = gy + 122 + k * 28;
      o += '<div id="seq-flag' + i + '_' + k + '" class="seq-flag" data-seqflag="' + k + '" data-seqstep="' + i + '" style="left:' + r1(cx + 6) + 'px;top:' + fy + 'px;width:' + r1(cwi - 12) + 'px;"></div>';
      o += txt(cx + 30, fy + 5, cwi - 42, f, { size: 5.8, color: inkC, weight: 700, align: 'left', style: 'z-index:8;', id: 'seq-flagtxt' + i + '_' + k, cls: 'seq-flagtxt' });
    });
  }
  // controls: four labelled groups on one line
  const cy = gy + chh + 34, ky = cy + 34;
  const L = (x, wd, label, action, opt) => keyButton(x, ky, wd, label, action, Object.assign({ link: true, size: 8.6 }, opt || {}));
  o += sect(30, cy, 390, 'SELECTED STEP', { align: 'left', size: 8.4 });
  o += L(26, 76, '▲ UP', 'seqUp', { title: 'Transpose step up a semitone' });
  o += L(110, 96, '▼ DOWN', 'seqDown', { title: 'Transpose step down a semitone' });
  o += L(214, 80, 'CLEAR', 'seqStepClear', { title: 'Empty this step' });
  o += L(302, 110, 'SET END', 'seqLength', { title: 'Sequence ends at the selected step' });
  o += sect(450, cy, 250, 'RECORD FROM KEYS', { align: 'left', size: 8.4 });
  o += L(446, 80, 'REC', 'seqRec', { domId: 'act-seqRecPop', title: 'Play notes: each is stored at the flashing step' });
  o += L(536, 80, 'STOP', 'seqRecStop');
  o += sect(730, cy, 180, 'SEQUENCE', { align: 'left', size: 8.4 });
  o += L(726, 130, 'CLEAR ALL', 'seqClear');
  const sx = 940;
  o += sect(sx, cy, w - 30 - sx, 'MEMORY', { align: 'left', size: 8.4 });
  for (let i = 0; i < 16; i++)
    o += button({ x: sx + i * 50, y: ky - 4, w: 36, led: false, label: String(i + 1), lsize: 6.4, id: 'seq.slot', layered: true, domId: 'upper.seq.slot__' + i, value: i, steps: 16 });
  o += L(sx + 16 * 50 + 20, 90, 'LOAD', 'seqLoad', { title: 'Copy the memory into the working sequence' });
  o += L(sx + 16 * 50 + 120, 100, 'STORE', 'seqStore', { title: 'Copy the working sequence into the memory' });
  o += tipRow(30, h - 46, w - 60, [['CLICK A STEP', 'SELECTS IT'], ['SLIDE', 'TIES INTO THE NEXT STEP'], ['ACCENT', 'ADDS LEVEL (DYNAMICS)'],
    ['REST', 'SKIPS THE STEP'], ['ARP MODE = SEQ, ON', 'PLAYS THE SEQUENCE'], ['HOLD', 'TRANSPOSES FROM C4']]);
  return o;
}

function altPage(w, h, channel) {
  let o = '';
  const names = choices('upper.dds1.altA');
  const cycles = JSON.parse(fs.readFileSync(path.join(OUT, 'wave-previews.json'), 'utf8'));
  const cols = 4, cw = (w - 60) / cols;
  const ch = channel || 'A';
  const y0 = 70;
  o += txt(30, y0, 500, 'ALT ' + ch + '  -  WAVE FOR CHANNEL ' + ch, { size: 9, align: 'left', ls: 0.1 });
  o += hrule(30, y0 + 24, w - 60, 'var(--rule)', 1);
  names.forEach((n, i) => {
    const c = i % cols, r = Math.floor(i / cols);
    const rowH = (h - 170) / 8, cardH = rowH - 10;
    const x = 30 + c * cw, y = y0 + 36 + r * rowH, cardW = cw - 12;
    const domId = 'upper.dds1.alt' + ch + '__' + i;
    o += well(x, y, cardW, cardH, 'background:rgba(255,255,255,.18);');
    o += txt(x + 12, y + 7, cardW - 24, n, { size: 7, align: 'left', color: INK, weight: 600 });
    // Min/max per screen column retains narrow pulse and noise detail.
    const cycle = cycles[i];
    let wavePath = '';
    for (let px = 0; px < 256; px++) {
      const samples = cycle.slice(px * 16, (px + 1) * 16);
      const lo = Math.min(...samples), hi = Math.max(...samples);
      wavePath += 'M' + px + ' ' + r1(16 - hi * 14) + 'V' + r1(16 - lo * 14) + ' ';
      if (px < 255) wavePath += 'M' + px + ' ' + r1(16 - cycle[px * 16] * 14) + 'L' + (px + 1) + ' ' + r1(16 - cycle[(px + 1) * 16] * 14) + ' ';
    }
    o += '<svg id="' + domId + '__v" aria-hidden="true" viewBox="0 0 256 32" preserveAspectRatio="none" style="position:absolute;left:' + (x + 12) + 'px;top:' + (y + 29) + 'px;width:' + (cardW - 24) + 'px;height:' + (cardH - 37) + 'px;pointer-events:none">'
      + '<path d="M0 16H256" stroke="#99958E" stroke-width=".5"/><path d="' + wavePath + '" fill="none" stroke="' + INK + '" stroke-width=".65"/></svg>';
    o += hit({ x, y, w: cardW, h: cardH, id: 'dds1.alt' + ch, layered: true, domId, type: 'combo', ctl: 'button', cursor: 'pointer', title: n,
      data: { value: i, steps: 32, wavecard: 'true' } });
  });
  o += tipRow(30, h - 44, w - 60, [['CLICK A WAVE', 'SELECTS IT'], ['TO HEAR IT', 'SET DDS 1 WAVEFORM TO ALT'], ['THE 32 WAVES', 'ARE THE FACTORY CYCLES']]);
  return o;
}

// DDS 1 CUSTOM: a sampler page (user, 2026-10-05: "more like Ableton's", with slicing). A toolbar
// (ON, the mode ONE SHOT | LOOP | SLICE, the file, LOAD / CLEAR), one big waveform you drag START /
// END / LOOP START on - in SLICE it shows each slice with the key that plays it, and clicking a slice
// plays it - then the controls in four groups: SAMPLE, SLICE, PITCH, OUTPUT. A group that does not
// apply to the current mode dims (ui/geminus.js sets cu-loop / cu-slice on the page).
const CU_W = 2300, CU_H = 900;
// a text segment bound to one value of a choice parameter (or a toggle when value is null)
function cuSeg(x, y, w, h, id, value, steps, label, domId, title) {
  const data = value == null ? { on: '', off: '' } : { value, steps, on: '', off: '' };
  return '<i id="' + domId + '__v" hidden></i>' + hit({ x, y, w, h, id, layered: true, domId, type: value == null ? 'toggle' : 'combo', ctl: 'button', cursor: 'pointer', title, data })
    .replace('data-juce-type', 'class="cu-seg" data-juce-type').replace(/"><\/div>$/, '">' + esc(label) + '</div>');
}
function customPage(w, h) {
  let o = '';
  o += layerLink(w, 'custom-layer-link');
  const x0 = 24, dw = w - 48;
  // ---- toolbar
  o += cuSeg(x0, 68, 84, 46, 'dds1.smpOn', null, 0, 'ON', 'custom-on', 'DDS 1 plays the sample instead of its waveform');
  ['ONE SHOT', 'LOOP', 'SLICE'].forEach((t, i) => { o += cuSeg(x0 + 104 + i * 150, 68, 146, 46, 'dds1.smpLoop', i, 3, t, 'custom-mode__' + i,
    ['Play START to END once per note', 'Repeat LOOP START to END while the note sounds', 'Chop it: each key from ROOT KEY up plays one slice'][i]); });
  o += txt(x0 + 600, 70, 900, 'NO SAMPLE', { size: 12, align: 'left', id: 'custom-name', style: 'overflow:hidden;text-overflow:ellipsis;' });
  o += txt(x0 + 600, 100, 900, '', { size: 7, align: 'left', color: INK2, weight: 600, id: 'custom-meta', ls: 0.12 });
  o += keyButton(w - 24 - 330, 68, 160, 'LOAD…', 'customBrowse', { h: 46, title: 'Choose an audio file' });
  o += keyButton(w - 24 - 160, 68, 160, 'CLEAR', 'customClear', { h: 46, title: 'Remove the sample from this layer' });
  // ---- the waveform
  const dy = 132, dh = 400;
  o += '<div id="custom-drop" title="Drop an audio file, or click to browse" style="position:absolute;left:' + x0 + 'px;top:' + dy + 'px;width:' + dw + 'px;height:' + dh + 'px;'
    + 'border-radius:8px;background:color-mix(in srgb,var(--pop-bg2) 60%,#000);box-shadow:inset 0 0 0 1px var(--rule);cursor:pointer;touch-action:none;z-index:5;overflow:hidden">'
    + '<canvas id="custom-canvas" width="' + dw * 2 + '" height="' + dh * 2 + '" style="position:absolute;inset:0;width:100%;height:100%;"></canvas>'
    + '<div id="custom-hint" style="position:absolute;inset:0;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:14px;pointer-events:none;'
    + 'font-family:' + FONT + ';font-stretch:75%;font-weight:700;text-transform:uppercase;letter-spacing:.08em;color:' + INK2 + ';">'
    + '<div style="font-size:34px;color:' + INK + '">DROP AN AUDIO FILE HERE</div><div style="font-size:17px">A ONE SHOT, A LOOP, OR A PIECE OF A SONG TO SLICE · WAV · AIFF · FLAC · OGG · MP3 · UP TO 90 S · OR CLICK TO BROWSE</div></div>'
    + '<input id="custom-file" type="file" accept=".wav,.aif,.aiff,.flac,.ogg,.mp3,audio/*" hidden></div>';
  // ---- controls: four groups on one line
  const cy = dy + dh + 36, ky = cy + 40, kd = 70, ly = ky + kd + 12;
  const vals = (x, id) => txt(x - 50, ly + 24, kd + 100, '', { size: 8.4, color: INK, id });
  const group = (cls, inner) => '<div class="cu-group ' + cls + '">' + inner + '</div>';
  let g = sect(x0, cy, 520, 'sample', { align: 'left' });
  g += knob({ x: x0 + 20, y: ky, d: kd, v: 'cream', val: 0, label: 'START', ly, id: 'dds1.smpStart', layered: true, domId: 'custom-start', title: 'Where the region starts' }) + vals(x0 + 20, 'custom-start-val');
  g += knob({ x: x0 + 190, y: ky, d: kd, v: 'cream', val: 1, label: 'END', ly, id: 'dds1.smpEnd', layered: true, domId: 'custom-end', title: 'Where the region ends' }) + vals(x0 + 190, 'custom-end-val');
  o += group('', g);
  o += group('cu-loop-only', knob({ x: x0 + 360, y: ky, d: kd, v: 'cream', val: 0, label: 'LOOP START', ly, id: 'dds1.smpLoopStart', layered: true, domId: 'custom-loop', title: 'LOOP jumps back here from END (START plays once)' }) + vals(x0 + 360, 'custom-loop-val'));
  const sx = x0 + 580;
  g = sect(sx, cy, 560, 'slice', { align: 'left' });
  ['AUTO', '4', '8', '16', '32', '64'].forEach((t, i) => { g += cuSeg(sx + i * 66, ky + 8, 62, 46, 'dds1.smpSlices', i, 6, t, 'custom-slices__' + i, i ? 'Cut the region into ' + t + ' equal slices' : 'Cut at the transients'); });
  g += txt(sx, ky + 66, 400, 'SLICES', { size: 7.6, align: 'left', color: INK });
  g += txt(sx, ly + 24, 400, '', { size: 8.4, align: 'left', color: INK, id: 'custom-slice-count' });
  g += knob({ x: sx + 440, y: ky, d: kd, v: 'cream', val: 0.5, label: 'SENSITIVITY', ly, id: 'dds1.smpSense', layered: true, domId: 'custom-sense', title: 'AUTO: how many transients cut a slice' }) + vals(sx + 440, 'custom-sense-val');
  o += group('cu-slice-only', g);
  const px = sx + 620;
  g = sect(px, cy, 470, 'pitch', { align: 'left' });
  g += knob({ x: px + 20, y: ky, d: kd, v: 'cream', val: 60 / 127, label: 'ROOT KEY', ly, id: 'dds1.smpRoot', layered: true, domId: 'custom-root', hitType: 'combo', hitData: { steps: 128 },
    title: 'The note the sample was recorded at. In SLICE: the key that plays the first slice' }) + vals(px + 20, 'custom-root-val');
  g += keyButton(px + 120, ky + 2, 50, '◀', 'customRootDown', { h: 40, title: 'Root key down a semitone' });
  g += keyButton(px + 176, ky + 2, 50, '▶', 'customRootUp', { h: 40, title: 'Root key up a semitone' });
  g += keyButton(px + 120, ky + 52, 106, 'LEARN', 'customLearn', { h: 40, domId: 'act-customLearn', title: 'The next key you play becomes the root key' });
  g += knob({ x: px + 300, y: ky, d: kd, v: 'cream', val: 0.5, label: 'FINE', ly, id: 'dds1.smpFine', layered: true, domId: 'custom-fine', title: 'Fine tune, -100 to +100 cents' }) + vals(px + 300, 'custom-fine-val');
  o += group('', g);
  const lx = px + 530;
  g = sect(lx, cy, w - 24 - lx, 'output', { align: 'left' });
  g += knob({ x: lx + 20, y: ky, d: kd, v: 'cream', val: 0.8, label: 'LEVEL', ly, id: 'dds1.smpLevel', layered: true, domId: 'custom-level', title: 'Sample volume (0 dB at 8, up to +4 dB)' }) + vals(lx + 20, 'custom-level-val');
  g += txt(lx + 140, ky + 6, w - 24 - lx - 140, '', { size: 7.4, align: 'left', color: INK2, weight: 600, id: 'custom-status', style: 'white-space:normal;line-height:1.35;' });
  o += group('', g);
  o += hrule(24, h - 66, w - 48, 'var(--rule)', 1);
  o += tipRow(24, h - 54, w - 48, [['DRAG THE FLAGS', 'START · END · LOOP START'], ['SLICE', 'ONE SLICE PER KEY FROM ROOT KEY UP'], ['CLICK A SLICE', 'HEARS IT'],
    ['AUTO', 'CUTS AT THE HITS · SENSITIVITY: HOW MANY'], ['STILL APPLY', 'SUPER, FILTER, ENVELOPES, FX']]);
  return o;
}

// Patch browser: a full-screen page inside the plugin, after Arturia's (user, 2026-10-05). A top bar
// (back, search, liked, count), a sidebar (ALL SOUNDS / LIKED / PACKS, your packs with their covers,
// the folder tools), the sound list or the pack grid in the middle, and on the right the sound you are
// on - its pack's cover (click it to change the image), name, type, pack, heart - with SAVE under it.
// A pack IS a folder inside the patch folder; its cover is a cover.png / .jpg in that folder.
// The layout is a CSS grid, so the page fills both the full panel and the desktop layout; in the full
// panel it is drawn at 1/PB_Z size and scaled up, so its type stays readable on the wide canvas.
const PB_Z = 1.4;
const HEART = '<svg viewBox="0 0 24 24" width="20" height="20" aria-hidden="true"><path d="M12 20.5 4.2 12.9A4.9 4.9 0 0 1 12 6.6a4.9 4.9 0 0 1 7.8 6.3Z" fill="currentColor" stroke="currentColor" stroke-width="1.6" stroke-linejoin="round"/></svg>';
const CHEV = '<svg viewBox="0 0 24 24" width="18" height="18" aria-hidden="true"><path d="M6 9.5 12 15.5 18 9.5" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"/></svg>';
function patchBrowser() {
  const act = (action, label, cls, id, title) => '<div class="link ' + cls + '" data-action="' + action + '" id="' + (id || 'act-' + action) + '"' + (title ? ' title="' + esc(title) + '"' : '') + '>' + label + '</div>';
  const field = (id, label, placeholder, menuId, menuTitle) => '<label class="pb-field"><span>' + label + '</span><input id="' + id + '" class="pb-input" placeholder="' + placeholder + '" spellcheck="false" maxlength="40">'
    + '<i class="pb-menu-btn" id="' + menuId + '" title="' + menuTitle + '">' + CHEV + '</i></label>';
  let o = '<div id="pop-patches" class="pop pb-pop-page" hidden><div class="pop-panel pb-page">';
  // ---- top bar
  o += '<header class="pb-top">'
    + '<div class="pb-back" data-close="pop-patches" title="Back to the panel (Esc)"><svg viewBox="0 0 24 24" width="22" height="22" aria-hidden="true"><path d="M14.5 6 8.5 12 14.5 18" fill="none" stroke="currentColor" stroke-width="2.4" stroke-linecap="round" stroke-linejoin="round"/></svg>Somii</div>'
    + '<div class="pb-title">SOUNDS</div>'
    + '<label class="pb-search"><svg viewBox="0 0 24 24" width="20" height="20" aria-hidden="true"><circle cx="10.5" cy="10.5" r="6" fill="none" stroke="currentColor" stroke-width="2.2"/><path d="m15 15 5 5" stroke="currentColor" stroke-width="2.2" stroke-linecap="round"/></svg>'
    + '<input id="patch-search" placeholder="Search names, types and packs" spellcheck="false"></label>'
    + '<div class="pb-chip" id="patch-f-fav" title="Show only the sounds you like">' + HEART + '<span>LIKED</span></div>'
    + '<div id="patch-clear" class="pb-textbtn" title="Show everything again">RESET FILTERS</div>'
    + '<div id="patch-count" class="pb-count"></div>'
    + '<div data-close="pop-patches" class="pop-close" title="Close (Esc)"><svg width="36" height="36" viewBox="0 0 36 36"><path d="M12.5 12.5 L23.5 23.5 M23.5 12.5 L12.5 23.5" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" fill="none"/></svg></div>'
    + '</header>';
  // ---- sidebar
  o += '<nav class="pb-side">'
    + '<div class="pb-nav" data-view="all">ALL SOUNDS<span id="pb-n-all"></span></div>'
    + '<div class="pb-nav" data-view="liked">LIKED<span id="pb-n-liked"></span></div>'
    + '<div class="pb-nav" data-view="packs">PACKS<span id="pb-n-packs"></span></div>'
    + '<div class="pb-side-h">YOUR PACKS</div><div id="patch-packlist" class="pb-packlist"></div>'
    + '<div class="pb-side-foot"><div id="patch-folder-path" class="pb-note" title=""></div><div class="pb-links">'
    + act('patchFolderUser', 'MY PATCHES', 'pb-l', null, 'Documents / Somii / Patches') + act('patchFolderChoose', 'OTHER FOLDER…', 'pb-l', null, 'Pick any folder that has .gpatch files')
    + act('patchOpen', 'OPEN FILE…', 'pb-l', null, 'Load any .gpatch file') + act('patchRefresh', 'REFRESH', 'pb-l', null, 'Re-read the folder')
    + act('initUpper', 'INIT UPPER', 'pb-l', null, 'Reset the upper layer to the init sound') + act('initLower', 'INIT LOWER', 'pb-l', null, 'Reset the lower layer to the init sound')
    + '</div></div></nav>';
  // ---- middle: the list, or the pack grid
  o += '<main class="pb-main">'
    + '<section class="pb-view" id="pb-view-list"><div id="patch-packhead" class="pb-packhead" hidden></div><div id="patch-types" class="pb-types"></div>'
    + '<div class="pb-grid pb-thead"><span class="pb-head" data-sort="fav" title="Sort by liked">' + HEART + '<span class="pb-arrow"></span></span>'
    + '<span class="pb-head" data-sort="name">NAME<span class="pb-arrow"></span></span><span class="pb-head" data-sort="type">TYPE<span class="pb-arrow"></span></span><span class="pb-head" data-sort="bank">PACK<span class="pb-arrow"></span></span></div>'
    + '<div id="patch-list" class="pb-list" tabindex="0"></div></section>'
    + '<section class="pb-view" id="pb-view-packs" hidden><div id="patch-packs" class="pb-packgrid"></div></section>'
    + '</main>';
  // ---- right: the sound you are on, and saving
  o += '<aside class="pb-detail">'
    + '<div id="patch-art" class="pb-art" title="Click to change this pack\'s image"></div>'
    + '<div class="pb-cur"><div id="patch-cur-name" class="pb-cur-name">INIT</div><div id="patch-cur-fav" class="pb-fav pb-cur-fav" title="Like this sound">' + HEART + '</div></div>'
    + '<div id="patch-cur-meta" class="pb-cur-meta"></div>'
    + '<div class="pb-row">' + act('patchPrev', 'PREV', 'pill', 'act-patchPrev2', 'Previous sound in this list (or arrow up)') + act('patchNext', 'NEXT', 'pill', 'act-patchNext2', 'Next sound in this list (or arrow down)') + '</div>'
    + '<div class="pb-save"><div class="pb-side-h">SAVE THIS SOUND</div>'
    + field('patch-name-input', 'NAME', 'Patch name', 'patch-name-menu', 'Sounds already in this list')
    + field('patch-type-input', 'TYPE', 'Lead, pad, or your own', 'patch-type-menu', 'Pick a type')
    + field('patch-bank-input', 'PACK', 'A pack, or a new name', 'patch-bank-menu', 'Pick a pack')
    + '<div class="pb-row">' + act('patchSave', 'SAVE', 'pill primary', 'act-patchSave2', 'Save into this pack under this name') + act('patchSaveAs', 'SAVE A COPY AS…', 'pill', null, 'Choose where to save and under what name') + '</div>'
    + '<div id="patch-status" class="pb-note pb-status" role="status"></div></div>'
    + '</aside>';
  return o + '</div></div>';
}

// A pop-over's foot legend, as key / meaning pairs in columns. The single 6.6 px line of
// middle-dot-separated clauses this replaces was there on five pages and was unreadable on all
// of them. Columns pack to the longest pair and never spread wider than the space allows.
function tipRow(x, y, w, pairs) {
  const widest = Math.max(...pairs.map(([k, v]) => Math.max(wText(k, 6.6), wText(v, 8))));
  const pitch = Math.min(Math.round(w / pairs.length), Math.round(widest + 48));
  return pairs.map(([k, v], i) => txt(x + i * pitch, y, pitch - 16, k, { size: 6.6, align: 'left', color: INK2, weight: 600 })
    + txt(x + i * pitch, y + 20, pitch - 16, v, { size: 8, align: 'left', color: INK, weight: 700 })).join('');
}

// ---- themes ------------------------------------------------------------------
// GEMINI (default) is the original hardware look. SUPER SIX recolours it. The photographed
// parts are not replaced: SVG filters re-map their luminance so that each asset's measured
// mid-tone lands exactly on the palette colour, keeping the render's shading and highlights.
//   cap1 = upper caps + light keys, cap2 = lower caps, capDark = black caps + dark keys,
//   insetTex = what happens to the dark inset texture ('flat' recolours it, 'hide' drops it)
const THEMES = {
  gemini: { base: '#E7E2DA', ink: '#23252A', ink2: '#4E5055', accent: '#F65A27', inset: '#424243', oink: '#E2E0DA', oink2: '#B4B2AC',
          rule: '#8A8B89', otick: '#9A9894', badge: '#E2E0DA', 'badge-ink': '#23252A', hi: '#F1EFE9', field: 'rgba(255,255,255,.5)', shade: '.45',
          sel: '#F65A27', osel: '#F65A27', opt: '#4E5055', link: '#23252A', pressed: '#F65A27',
          // pop-overs: the theme, darker - charcoal from the inset panels, cream ink, the orange accent
          'pop-bg': '#434345', 'pop-bg2': '#262627', 'pop-ink': '#E7E2DA', 'pop-ink2': '#A8A49C', 'pop-accent': '#F65A27',
          'pop-rule': 'rgba(231,226,218,.16)', 'pop-link': '#D8D2C8', 'pop-key': '#333335',
          'pop-upper': '#E7E2DA', 'pop-lower': '#F65A27',
          // the sample waveform, coloured by what is in it: lows, mids, highs
          'wave-lo': '#F65A27', 'wave-mid': '#F2A33A', 'wave-hi': '#E7E2DA' },   // the layers' cap colours: cream UPPER, orange LOWER
  // palette: base #568EA3, accent #826251, light #FFE8D1, text #FFFFFF, lines #68C3D4
  super6: { base: '#568EA3', ink: '#FFFFFF', ink2: '#FFE8D1', opt: '#68C3D4', accent: '#826251', inset: '#FFE8D1', oink: '#568EA3', oink2: '#68C3D4',
          rule: '#68C3D4', otick: '#568EA3', badge: '#568EA3', 'badge-ink': '#FFFFFF', hi: '#FFFFFF', field: 'rgba(0,0,0,.14)', shade: '.12',
          sel: '#FFFFFF', osel: '#826251', link: '#274957', pressed: '#FFFFFF',
          // pop-overs: the theme, darker - deep blue, white ink, cream secondary, the line colour as accent
          'pop-bg': '#2F5A6B', 'pop-bg2': '#18323D', 'pop-ink': '#FFFFFF', 'pop-ink2': '#D9C9B6', 'pop-accent': '#68C3D4',
          'pop-rule': 'rgba(104,195,212,.3)', 'pop-link': '#FFE8D1', 'pop-key': '#244654',
          'pop-upper': '#FFE8D1', 'pop-lower': '#C08A6C',
          'wave-lo': '#C08A6C', 'wave-mid': '#68C3D4', 'wave-hi': '#FFE8D1',
          cap1: '#FFE8D1', cap2: '#826251', capDark: '#826251', insetTex: 'hide' },
  // DARK (user, 2026-10-05: "ember is now dark mode, get rid of the old dark mode"): graphite panel, black
  // recessed wells darker than the panel (texture kept), GEMINI's own cream for UPPER and orange for LOWER and
  // for everything that is on. One neutral ramp: wells #141415 < panel #202022 < rules #4A4A4E < ink2 #A29E97
  // < ink #EEEAE3. The black keys and caps are lifted to #505056 so they stand off the graphite (at the
  // panel's own darkness they disappeared).
  dark: { base: '#202022', ink: '#EEEAE3', ink2: '#A29E97', opt: '#A29E97', accent: '#F65A27', 'accent-ink': '#FFFFFF', inset: '#141415',
          oink: '#E2E0DA', oink2: '#9A978F', rule: '#4A4A4E', otick: '#5E5E62', badge: '#2C2C2F', 'badge-ink': '#EEEAE3',
          hi: '#F1EFE9', field: 'rgba(238,234,227,.07)', shade: '.30',
          sel: '#F65A27', osel: '#F65A27', link: '#D8D2C8', pressed: '#F65A27',
          'pop-bg': '#2A2A2D', 'pop-bg2': '#18181A', 'pop-ink': '#EEEAE3', 'pop-ink2': '#A29E97', 'pop-accent': '#F65A27',
          'pop-rule': 'rgba(238,234,227,.14)', 'pop-link': '#D8D2C8', 'pop-key': '#1F1F21',
          'pop-upper': '#E7E2DA', 'pop-lower': '#F65A27',
          'wave-lo': '#F65A27', 'wave-mid': '#F2A33A', 'wave-hi': '#E7E2DA',
          cap1: '#E7E2DA', cap2: '#F65A27', capDark: '#505056', insetTex: 'flat', keys: '#DCD8D0',
          name: 'DARK', blurb: 'Graphite with the original cream and orange.' },

};
// the picker's names and lines (and the colours its swatch shows) for the themes that predate them
Object.assign(THEMES.gemini, { name: 'GEMINI', blurb: 'The original hardware: cream, charcoal, orange. Default.', 'accent-ink': '#FFFFFF',
  swatch: ['#E7E2DA', '#424243', '#E7E2DA', '#F65A27', '#2A2A2C', '#F65A27'] });
Object.assign(THEMES.super6, { name: 'SUPER SIX', blurb: 'Blue panel, cream caps, brown LOWER.', 'accent-ink': '#FFFFFF' });
const THEME_KEYS = ['gemini', 'super6', 'dark'];
// swatch: panel, well, UPPER cap, LOWER cap, black cap, accent
const swatchOf = (k) => THEMES[k].swatch || [THEMES[k].base, THEMES[k].inset, THEMES[k].cap1, THEMES[k].cap2, THEMES[k].capDark, THEMES[k].accent];
const CSS_KEYS = ['base', 'ink', 'ink2', 'opt', 'accent', 'accent-ink', 'inset', 'oink', 'oink2', 'rule', 'otick', 'badge', 'badge-ink', 'hi', 'field', 'shade', 'sel', 'osel', 'link', 'pressed', 'pop-bg', 'pop-bg2', 'pop-ink', 'pop-ink2', 'pop-accent', 'pop-rule', 'pop-link', 'pop-key', 'pop-upper', 'pop-lower', 'wave-lo', 'wave-mid', 'wave-hi'];

// SETTINGS: five columns on one grid, the THEME cards in a row under them. Every column starts on the same heading line, its controls
// centre on the same axis, and every knob prints its value under its label. The column maths
// derives from w, so both page margins stay equal and no heading rule runs off the panel (the
// hand-placed columns this replaces overlapped at DISPLAY and clipped THEME 100 px past the edge).
const SET_W = 2200, SET_H = 650;
function settingsPage(w) {
  let o = '';
  const M = 40, GUT = 32, N = 5;
  const CW = Math.round((w - 2 * M - GUT * (N - 1)) / N);
  const CX = (i) => M + i * (CW + GUT);
  const HALF = Math.round((CW - 20) / 2);          // two controls side by side inside a column
  const RX = (i) => CX(i) + CW - HALF;             // the column's right half
  const HY = 72;                                   // heading
  const KY = 126, KD = 60;                         // knob row 126…186, axis 156
  const LY = 200, VY = 226;                        // label, then the live value
  const BY = 136, BH = 40;                         // a 40 px key centres on the knob axis
  const col = (i, title) => { o += sect(CX(i), HY, CW, title, { align: 'left' }); };
  // knob + label + live read-out, stacked inside one half-column
  const dial = (x, label, readout, k) => knob(Object.assign({ x: x + (HALF - KD) / 2, y: KY, d: KD, v: 'cream', label, ly: LY, lw: HALF }, k))
    + txt(x, VY, HALF, '', { size: 8, color: INK2, weight: 600, id: readout });
  // square toggle with its legend beside it; stacked legends cannot collide the way centred ones did
  const sideToggle = (x, y, id, label, title) => button({ x, y, w: 36, id, title })
    + txt(x + 48, y + 7, HALF - 48, label, { size: 7.6, align: 'left', color: INK, weight: 700 });

  col(0, 'TUNING');
  o += dial(CX(0), 'FINE TUNE', 'finetune-readout', { val: 0.5, id: 'global.fineTune' });
  o += dial(RX(0), 'TRANSPOSE', 'transpose-readout', { val: 0.5, id: 'global.transpose' });

  col(1, 'KEYBOARD');
  o += dial(CX(1), 'SPLIT POINT', 'split-readout', { val: 0.47, id: 'perf.splitPoint' });
  o += keyButton(RX(1), BY, HALF, 'LEARN', 'splitLearn', { h: BH, title: 'Next key played sets the split point' });
  o += txt(RX(1), VY, HALF, 'NEXT KEY PLAYED', { size: 6.6, color: INK2, weight: 600 });

  col(2, 'MIDI');
  o += dial(CX(2), 'CHANNEL', 'midich-readout', { val: 0, id: 'global.midiChannel' });
  o += sideToggle(RX(2), KY, 'global.ccRx', 'CC RX', 'Receive MIDI CC');
  o += sideToggle(RX(2), KY + 48, 'global.clockRx', 'HOST CLOCK', 'Arp / seq follow the host tempo and transport');
  o += txt(CX(2), VY + 34, CW, 'LOWER LAYER = CHANNEL + 1', { size: 6.6, align: 'left', color: INK2, weight: 600 });

  col(3, 'ENGINE');
  o += txt(CX(3), 120, CW, 'OVERSAMPLING  2×', { size: 8, align: 'left' });
  o += txt(CX(3), 146, CW, 'BINAURAL VOICES  10 / 5 PER LAYER', { size: 7, align: 'left', color: INK2, weight: 600 });
  o += keyButton(CX(3), 188, HALF, 'PANIC', 'panic', { h: BH, title: 'All notes off' });
  o += keyButton(RX(3), 188, HALF, 'RESET ALL', 'resetAll', { h: BH, title: 'Both layers to init, performance to single upper' });

  col(4, 'DISPLAY');
  o += keyButton(CX(4), BY, CW, 'DESKTOP LAYOUT', 'toggleDesktopLayout', { h: BH, domId: 'act-desktop-layout', title: 'Larger controls for laptop screens: one engine at a time, no keyboard' });
  o += txt(CX(4), 192, CW, 'OFF', { size: 8, color: INK, weight: 700, id: 'desktop-layout-state' });
  o += txt(CX(4), 216, CW, 'FULL PANEL WITH KEYBOARD', { size: 6.6, color: INK2, weight: 600, id: 'desktop-layout-detail' });

  // THEME (user, 2026-10-05: "a better selection menu"): a row of cards under the columns, one per theme.
  // Each shows the theme itself in small: its panel with a recessed well, the three cap colours and the
  // accent, then its name and one line. The theme in use is outlined and says so.
  o += hrule(M, 300, w - 2 * M, 'var(--rule)', 1);
  o += sect(M, 326, w - 2 * M, 'THEME', { align: 'left' });
  const TG = 24, TW = Math.round((w - 2 * M - TG * (THEME_KEYS.length - 1)) / THEME_KEYS.length), TY = 372, TH = 150;
  THEME_KEYS.forEach((k, i) => {
    const [panel, well, c1, c2, c3, acc] = swatchOf(k), T = THEMES[k];
    const sw = '<svg class="tc-sw" viewBox="0 0 150 110" width="150" height="110" aria-hidden="true">'
      + '<rect x=".5" y=".5" width="149" height="109" rx="8" fill="' + panel + '" stroke="rgba(255,255,255,.14)"/>'
      + '<rect x="12" y="12" width="126" height="3" rx="1.5" fill="' + acc + '"/>'
      + '<rect x="84" y="28" width="54" height="68" rx="5" fill="' + well + '"/>'
      + '<rect x="96" y="40" width="4" height="44" rx="2" fill="' + c1 + '" opacity=".9"/><rect x="110" y="40" width="4" height="44" rx="2" fill="' + c2 + '" opacity=".9"/><rect x="124" y="40" width="4" height="44" rx="2" fill="' + c3 + '" stroke="rgba(255,255,255,.25)" stroke-width=".5"/>'
      + [[30, c1], [62, c2]].map(([cx, c]) => '<circle cx="' + cx + '" cy="48" r="13" fill="' + c + '"/>').join('')
      + '<circle cx="30" cy="84" r="9" fill="' + c3 + '" stroke="rgba(255,255,255,.25)"/><circle cx="62" cy="84" r="9" fill="' + acc + '"/></svg>';
    o += '<div class="theme-card" role="button" tabindex="0" data-action="theme_' + k + '" data-theme="' + k + '" id="act-theme-' + k + '" aria-pressed="false" title="' + esc(T.blurb) + '" style="left:' + (M + i * (TW + TG)) + 'px;top:' + TY + 'px;width:' + TW + 'px;height:' + TH + 'px">'
      + sw + '<span class="tc-name">' + esc(T.name) + '</span><span class="tc-blurb">' + esc(T.blurb) + '</span><span class="tc-on">IN USE</span></div>';
  });

  o += hrule(M, 552, w - 2 * M, 'var(--rule)', 1);
  o += tipRow(M, 574, w - 2 * M, [['SHIFT', 'SECONDARY FUNCTIONS'], ['CTRL-DRAG', 'FINE'], ['DOUBLE-CLICK', 'DEFAULT'],
    ['RIGHT-CLICK', 'ROUTE MODULATION'], ['WHEEL', 'STEP BY STEP']]);
  return o;
}

// FX rack page (docs/fx/FX_PROMPTS.md): routing column, three slot panels after the user's
// reference, a layer fader under each slot, and the grouped type picker.
// Knobs, faders and power keys are the photographed assets; the display is a screen (canvas).
const FXW = 3000, FXH = 1040;
// The picker's picture of each effect: a small drawing of what it does, in that module's own colours
// (house modules follow the theme). 64 x 64 viewBox, flat strokes and fills only.
function fxArt(f) {
  const k = SKINS[(f && SKIN_OF[f.id]) || 'house'], bg = k.bg, ink = k.ink, acc = k.accent, dim = k.ink2;
  const L = (d, c, w, o) => '<path d="' + d + '" fill="none" stroke="' + c + '" stroke-width="' + (w || 2.5) + '" stroke-linecap="round" stroke-linejoin="round"' + (o ? ' opacity="' + o + '"' : '') + '/>';
  const C = (x, y, r, c, o) => '<circle cx="' + x + '" cy="' + y + '" r="' + r + '" fill="' + c + '"' + (o ? ' opacity="' + o + '"' : '') + '/>';
  const R = (x, y, w, h, c, o) => '<rect x="' + x + '" y="' + y + '" width="' + w + '" height="' + h + '" rx="1" fill="' + c + '"' + (o ? ' opacity="' + o + '"' : '') + '/>';
  const wave = (fn, x0 = 8, x1 = 56) => { let d = ''; for (let x = x0; x <= x1; x += 1) d += (x === x0 ? 'M' : 'L') + x + ' ' + fn((x - x0) / (x1 - x0)).toFixed(1); return d; };
  const sin = (t, cyc) => Math.sin(t * Math.PI * 2 * cyc);
  let a = '';
  switch (f ? f.id : 'none') {
    case 'none': a = L('M20 20 44 44M44 20 20 44', dim, 3); break;
    case 'psdelay': for (let i = 0; i < 5; i++) a += C(12 + i * 10, 46 - i * 7, 6 - i, i ? ink : acc, 1 - i * 0.15); break;
    case 'tapeecho': for (let i = 0; i < 6; i++) { const hh = 18 * Math.pow(0.75, i); a += R(10 + i * 8, i % 2 ? 32 : 32 - hh, 5, hh, i ? ink : acc); } a += L('M8 32H56', dim, 1); break;
    case 'convolver': a += R(10, 12, 4, 40, acc); for (let i = 0; i < 18; i++) { const hh = 34 * Math.exp(-i / 6) * (0.5 + 0.5 * Math.abs(Math.sin(i * 2.7))); a += R(16 + i * 2.3, 52 - hh, 1.4, hh, ink); } break;
    case 'tuba': a += L('M22 52V24a10 10 0 0 1 20 0v28Z', ink, 2.5) + L('M28 44 30 30 32 44 34 30 36 44', acc, 2.5) + L('M24 56v4M30 56v4M34 56v4M40 56v4', dim, 2); break;
    case 'saturn': [[8, 26], [26, 14], [44, 22]].forEach(([x, y], i) => { a += R(x, y, 14, 52 - y, i === 1 ? acc : ink, i === 1 ? 1 : 0.85); a += L(wave((t) => y - 3 - 2 * sin(t, 2), x, x + 14), i === 1 ? acc : ink, 2); }); break;
    case 'distortion': a += L('M8 32H56', dim, 1) + L(wave((t) => 32 - Math.max(-1, Math.min(1, 2.4 * sin(t, 1.5))) * 16), acc, 3); break;
    case 'bitcrusher': a += L(wave((t) => 32 - Math.round(sin(t, 1) * 4) / 4 * 18), acc, 3) + L(wave((t) => 32 - sin(t, 1) * 18), dim, 1.2, 0.6); break;
    case 'vulf': a += L('M8 18 16 44 18 18 26 44 28 18 36 44 38 18 46 44 48 18 56 44', acc, 2.5) + L('M8 50H56', ink, 2); break;
    case 'faraday': a += L('M8 20H56', ink, 3) + L(wave((t) => 34 - Math.min(14, 22 * Math.abs(sin(t, 1.5)) * (0.6 + 0.4 * sin(t, 0.5))) * Math.sign(sin(t, 1.5))), acc, 2.5); break;
    case 'mbcomp': [[8, 30], [26, 18], [44, 26]].forEach(([x, y]) => { a += R(x, y, 13, 50 - y, ink, 0.85) + L('M' + (x + 6.5) + ' ' + (y - 9) + 'v6m-3-3 3 3 3-3', acc, 2); }); break;
    case 'phaser': a += L(wave((t) => 32 - sin(t, 1.5) * 15), acc, 2.5) + L(wave((t) => 32 - sin(t + 0.12, 1.5) * 15), ink, 2.5, 0.7); break;
    case 'flanger': for (let i = 0; i < 14; i++) { const x = 8 + 48 * Math.pow(i / 13, 1.6); a += L('M' + x.toFixed(1) + ' 12V52', i % 4 ? ink : acc, 1.8, i % 4 ? 0.7 : 1); } break;
    case 'stereopan': a += L('M10 44A22 22 0 0 1 54 44', dim, 2) + C(16, 34, 4, ink, 0.4) + C(24, 26, 4, ink, 0.6) + C(36, 23, 6, acc) + L('M10 50H18M46 50H54', ink, 2); break;
    case 'imager': [24, 18, 12].forEach((r, i) => { a += '<path d="M32 50L' + (32 - r * 1.1) + ' ' + (50 - r * 1.4) + 'A' + r * 1.8 + ' ' + r * 1.8 + ' 0 0 1 ' + (32 + r * 1.1) + ' ' + (50 - r * 1.4) + 'Z" fill="' + (i === 1 ? acc : ink) + '" opacity="' + (0.35 + i * 0.25) + '"/>'; }); break;
    case 'proq': a += L('M8 40C18 40 18 22 26 22S34 46 42 46 50 30 56 30', acc, 3) + C(26, 22, 4, ink) + C(42, 46, 4, ink); break;
    case 'filter': a += L('M8 40H30C36 40 38 22 41 22S44 34 47 44 52 54 56 56', acc, 3) + L('M8 48H56', dim, 1); break;
    case 'autochroma': a += L('M32 12 50 46H14Z', ink, 2.5) + ['#F65A27', acc, ink].map((c, i) => L('M38 ' + (30 + i * 5) + 'L56 ' + (22 + i * 10), c, 2.5)).join('') + L('M8 34H28', dim, 2); break;
    case 'valleyverb': [0, 1, 2, 3].forEach((i) => { a += R(10 + i * 4, 52 - 28 * Math.pow(0.8, i), 2.4, 28 * Math.pow(0.8, i), acc); }); for (let i = 0; i < 16; i++) { const hh = 22 * Math.exp(-i / 6); a += R(28 + i * 1.8, 52 - hh, 1.1, hh, ink, 0.7); } break;
    case 'nudestort': a += ['#F65A27', '#68C3D4', '#23252A', '#826251'].map((c, i) => '<path d="' + wave((t) => 16 + i * 10 + 6 * sin(t + i * 0.2, 1.2) * Math.cos(t * 3 + i), 6, 58) + 'L58 64H6Z" fill="' + c + '"/>').join(''); break;
    // CARVE: two beats of a drawn pump, its points marked; POISE: an uneven spectrum and the even line it leans to;
    // RIFT: a tunnel of rings with grains coming through it
    case 'carve': a += L('M8 52H56', dim, 1) + L('M8 50C12 26 16 18 30 16V50C34 26 38 18 56 16', acc, 3) + [[8, 50], [30, 16], [30, 50], [56, 16]].map(([x, y]) => C(x, y, 2.6, ink)).join(''); break;
    case 'poise': a += [30, 18, 34, 22, 40, 26, 44, 30, 36].map((y, i) => R(9 + i * 5.2, y, 3.6, 52 - y, ink, 0.35)).join('') + L('M8 30C20 34 44 34 56 30', acc, 3) + C(20, 33, 3, acc) + C(44, 33, 3, acc); break;
    case 'rift': for (let i = 4; i >= 1; i--) a += '<ellipse cx="34" cy="32" rx="' + i * 6.2 + '" ry="' + i * 6.8 + '" fill="none" stroke="' + ink + '" stroke-width="1.6" opacity="' + (0.25 + (4 - i) * 0.2) + '"/>'; a += [[14, 20, 2.4], [52, 14, 1.8], [12, 46, 1.6], [50, 48, 2.2], [24, 52, 1.4]].map(([x, y, r]) => C(x, y, r, acc)).join('') + C(34, 32, 4, acc); break;
    case 'parlour': a +='<rect x="16" y="16" width="32" height="32" fill="none" stroke="' + ink + '" stroke-width="2.5"/>' + L('M16 16 9 9M48 16 55 9M16 48 9 55M48 48 55 55', ink, 2) + [24, 32, 40].map((y) => L(wave((t) => y + 2.5 * sin(t, 1), 16, 48), ink, 1.6, 0.7)).join(''); break;
  }
  return '<svg viewBox="0 0 64 64" width="64" height="64" aria-hidden="true"><rect width="64" height="64" rx="7" fill="' + bg + '"/>' + a + '</svg>';
}

function fxPage(w, h) {
  let o = '';
  const S = 'stroke:var(--ink2);fill:none;stroke-width:1.5';
  // ---- routing column
  const cx = 40, cw = 250;
  o += sect(cx, 70, cw, 'ROUTING', { align: 'left', size: 9.4 });
  o += keyButton(cx, 112, 120, 'SERIAL', 'fxSerial', { link: true, cls: 'pill', size: 8.6, h: 40, title: 'FX 1 feeds FX 2 feeds FX 3' });
  o += keyButton(cx + 130, 112, 120, 'PARALLEL', 'fxParallel', { link: true, cls: 'pill', size: 8.6, h: 40, title: 'Each slot hears the dry sound; the results are mixed' });
  const bx = cx + 25, bw = 200, bh = 70, ys = [190, 300, 410, 520, 640];
  const box = (y, hh, top, nameId, id) => '<div' + (id ? ' id="' + id + '"' : '') + ' class="fx-route" style="position:absolute;left:' + bx + 'px;top:' + y + 'px;width:' + bw + 'px;height:' + hh + 'px;box-sizing:border-box;'
    + 'background:var(--cell);box-shadow:inset 0 0 0 1px var(--pop-rule);border-radius:6px;display:flex;flex-direction:column;align-items:center;justify-content:center;pointer-events:none;transition:box-shadow .12s;'
    + 'font:700 15px ' + FONT + ';font-stretch:75%;letter-spacing:.08em;color:var(--ink2)"><span>' + top + '</span>'
    + (nameId ? '<span id="' + nameId + '" style="font-size:19px;color:var(--ink)">NONE</span>' : '') + '</div>';
  o += box(ys[0], 44, 'INPUT');
  for (let i = 0; i < 3; i++) o += box(ys[i + 1], bh, 'EFFECT ' + (i + 1), 'fx-route-name' + i, 'fx-route' + i).replace('class="fx-route"', 'class="fx-route" data-fx-grip="' + i + '" data-fx-drop="' + i + '" title="Drag onto another effect to swap their order"').replace('pointer-events:none;', 'cursor:grab;touch-action:none;');
  o += box(ys[4], 44, 'OUTPUT');
  const mid = bx + bw / 2;
  // serial: one spine through every box
  const ser = '<path d="M' + mid + ' ' + (ys[0] + 44) + 'V' + ys[1] + 'M' + mid + ' ' + (ys[1] + bh) + 'V' + ys[2] + 'M' + mid + ' ' + (ys[2] + bh) + 'V' + ys[3]
    + 'M' + mid + ' ' + (ys[3] + bh) + 'V' + ys[4] + '" style="' + S + '"/>';
  // parallel: a bus down the left feeds every box, a bus down the right sums them
  const lx = bx - 14, rx = bx + bw + 14;
  const par = '<path d="M' + mid + ' ' + (ys[0] + 44) + 'V' + (ys[0] + 62) + 'H' + lx + 'V' + (ys[3] + bh / 2) + 'H' + bx
    + 'M' + lx + ' ' + (ys[1] + bh / 2) + 'H' + bx + 'M' + lx + ' ' + (ys[2] + bh / 2) + 'H' + bx
    + 'M' + (bx + bw) + ' ' + (ys[1] + bh / 2) + 'H' + rx + 'V' + (ys[4] - 22) + 'H' + mid + 'V' + ys[4]
    + 'M' + (bx + bw) + ' ' + (ys[2] + bh / 2) + 'H' + rx + 'M' + (bx + bw) + ' ' + (ys[3] + bh / 2) + 'H' + rx + '" style="' + S + '"/>';
  const svg = (cls, body) => '<svg class="' + cls + '" aria-hidden="true" width="' + w + '" height="' + h + '" style="position:absolute;left:0;top:0;pointer-events:none;overflow:visible">' + body + '</svg>';
  o += svg('fx-ser', ser) + svg('fx-par', par);
  o += txt(cx, 710, cw, '', { size: 6.6, align: 'left', color: INK2, weight: 600, id: 'fx-mode-note', style: 'text-transform:none;letter-spacing:.02em;white-space:normal;line-height:1.35' });

  // ---- slots: each is a self-contained "plug-in window"; ui/fx.js draws the loaded effect's own
  // face into it (skin, flat controls, layout) and the LAYER MIX fader along its bottom edge
  const x0 = cx + cw + 44, gap = 30, sw = Math.floor((w - x0 - 30 - 2 * gap) / 3), py = 70, ph = h - py - 36;
  for (let s = 0; s < 3; s++) {
    const x = x0 + s * (sw + gap);
    o += '<div class="fx-frame" id="fx' + s + '-frame" data-fx-drop="' + s + '" data-w="' + sw + '" data-h="' + ph + '" style="position:absolute;left:' + r1(x) + 'px;top:' + py + 'px;width:' + sw + 'px;height:' + ph + 'px"></div>';
  }

  // ---- type picker, opened from a slot's name: one column per kind of effect, each effect with its
  // picture and what it does (user, 2026-10-05: rows of uneven length left gaps and spilled over)
  const cols = ['TIME', 'DISTORTION', 'DYNAMICS', 'MODULATION', 'FILTER / EQ', 'EXPERIMENTAL'];
  const inGroup = (g) => FX.map((f, i) => ({ f, i })).filter((e) => e.f.group === g);
  const cg = 16, pw = w - 80, tw = Math.floor((pw - 48 - cg * (cols.length - 1)) / cols.length), th = 84, tg = 12;
  const deepest = Math.max(...cols.map((g) => inGroup(g).length)), ph2 = 84 + 40 + deepest * (th + tg) + 18;
  let p = '<div id="fx-picker" hidden style="position:absolute;left:' + r1((w - pw) / 2) + 'px;top:' + r1((h - ph2) / 2) + 'px;width:' + pw + 'px;height:' + ph2 + 'px;z-index:30;border-radius:8px;'
    + 'background:var(--pop-bg2);box-shadow:0 28px 56px -16px rgba(0,0,0,.75),0 0 0 1px var(--pop-rule)">';
  p += txt(24, 28, pw - 100, 'FX 1 · CHOOSE AN EFFECT', { size: 10, id: 'fx-picker-title', align: 'left', ls: 0.14 });
  p += '<div data-fx-close class="pop-close" title="Close" style="position:absolute;right:10px;top:10px;width:36px;height:36px;cursor:pointer;z-index:6"><svg width="36" height="36" viewBox="0 0 36 36"><path d="M12.5 12.5 L23.5 23.5 M23.5 12.5 L12.5 23.5" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" fill="none"/></svg></div>';
  const swatch = (f) => '<i class="fx-sw">' + fxArt(f) + '</i>';
  const tile = (x, y, val, f) => '<div class="fx-tile" data-fx-tile="' + val + '" title="' + esc(f ? f.blurb : 'Leave this slot empty') + '" style="left:' + r1(x) + 'px;top:' + r1(y) + 'px;width:' + tw + 'px;height:' + th + 'px">'
    + swatch(f) + '<span class="fx-tn">' + esc(f ? f.name : 'NONE') + '</span><span class="fx-tb">' + esc(f ? f.blurb : 'Leave this slot empty') + '</span></div>';
  // NONE: just the ✕ and the word, in the empty bottom-right corner (user, 2026-10-05: a box at the top right
  // read as a second close button)
  p += '<div class="fx-none" data-fx-tile="0" title="Leave this slot empty" style="right:30px;top:' + (124 + (deepest - 1) * (th + tg) + 14) + 'px">'
    + '<svg viewBox="0 0 24 24" width="30" height="30" aria-hidden="true"><rect x="1.5" y="1.5" width="21" height="21" rx="4" fill="none" stroke="currentColor" stroke-width="1.6"/><path d="M8 8l8 8M16 8l-8 8" stroke="currentColor" stroke-width="1.8" stroke-linecap="round"/></svg><span>NONE</span></div>';
  cols.forEach((g, c) => {
    const x = 24 + c * (tw + cg);
    p += '<div class="section-heading" style="position:absolute;left:' + r1(x) + 'px;top:96px;width:' + tw + 'px;height:20px;display:flex;align-items:center;gap:10px;pointer-events:none;font:700 14px ' + FONT + ';font-stretch:75%;letter-spacing:.16em;color:var(--ink2)">'
      + '<span>' + g + '</span><span style="flex:1;height:1px;background:var(--pop-rule)"></span></div>';
    inGroup(g).forEach((e, k) => { p += tile(x, 124 + k * (th + tg), e.i + 1, e.f); });
  });
  p += '</div>';
  return o + p;
}

function modulatePage(w, h) {
  let o = '';
  o += txt(40, 80, w - 80, '', { size: 12, align: 'left', ls: 0.06, id: 'mod-target' });
  o += txt(40, 114, w - 80, 'AMOUNT FROM EACH SOURCE, -100 … +100 %', { size: 9, align: 'left', color: INK2, weight: 600 });
  const rows = MTX_SRC.map((s) => s[1]);
  rows.forEach((r, i) => {
    const y = 170 + i * 94;
    o += txt(40, y + 22, 270, r, { size: 12, align: 'right' });
    o += knob({ x: 350, y: y, d: 68, v: 'cream', val: 0.5, ticks: 5, id: 'modslot' + i, domId: 'modslot' + i });
    o += txt(464, y + 22, 180, '0', { size: 11, align: 'left', color: INK2, weight: 600, id: 'modslot' + i + '__val' });
  });
  return o;
}

// ------------------------------------------------------------------ assemble
let body = '';
body += '<div class="fp-wrap">' + topBar() + '</div>';
body += '<div class="fp-wrap">' + layerRow(ROW1, true) + hrule(PL, ROW2 - 6, PR - PL, 'var(--rule)') + '</div>';
body += '<div class="fp-wrap">' + layerRow(ROW2, false) + hrule(PL, STRIP - 6, PR - PL, 'var(--rule)') + '</div>';
body += '<div class="fp-wrap">' + globalStrip(STRIP) + '</div>';
body += '<div class="fp-wrap">' + hrule(PL, BOTTOM - 2, KBX - PL, 'var(--rule)') + perfBlock(PL + 6, BOTTOM + 6, KBX - PL - 40) + '</div>';
const RIBW = PR - 470 - KBX;
body += '<div class="fp-wrap">' + hrule(PR - 470, BOTTOM - 2, 470, 'var(--rule)') + vrule(KBX - 24, BOTTOM + 4, H - BOTTOM - 8) + ribbonRow(RIBY, KBX, RIBW) + wordmark(PR - 470, RIBY) + '</div>';

// desktop-only cards. MASTER VOLUME and DETUNE sit in the full panel's per-layer left
// column; desktop mode shows one layer (chosen by the LAYER keys), so both get a shared card. Same parameters,
// separate DOM ids - every control bound to a parameter is repainted on change.
{
  const y = DK_SRC_Y, B = y + 60;
  let o = dk('master', 0, y, 210, STRIPH, 'dk-only');
  o += sect(0, y + 4, 210, 'MASTER');
  o += knob({ x: 22, y: B - 6, d: 56, v: 'cream', val: 0.8, ticks: 11, label: 'VOLUME', lsize: 7.6, ly: B + 66, id: 'global.masterVolume', domId: 'dk.masterVolume', minmax: ['0', '+4dB'] });
  o += knob({ x: 124, y: B - 6, d: 56, v: 'orange', val: 0.5, ticks: 11, label: 'LOWER DETUNE', lsize: 7.6, ly: B + 66, sub: 'PERF DETUNE', sy: B + 66 + lineH(7.6) + 2, id: 'perf.lowerDetune', domId: 'dk.lowerDetune', shiftId: 'perf.perfDetune', minmax: ['-7', '+7'] });
  o += dkEnd;
  body += o;
}
body += keyboard(KBX, KBY, PR - KBX, KBH);

// pop-overs (top-most): software pages in the dark flat look, so their knobs and keys are drawn flat
STYLE.flat = true;
const POPS = [];
const pop = (id, title, x, y, w, h, content, o) => { POPS.push({ id, w, h }); return popover(id, title, x, y, w, h, content, o); };
const popW = 2200, popH = 1060, popX = Math.round((W - popW) / 2), popY = Math.round((H - popH) / 2) - 40;
body += pop('pop-matrix', 'MODULATION MATRIX', popX, popY, popW, popH, matrixPage(popW, popH), { subtitle: '8 SOURCES × 8 DESTINATIONS PER LAYER · DRAG A KNOB · DOUBLE-CLICK CLEARS' });
body += pop('pop-seq', 'SEQUENCER', popX, popY + 150, popW, 560, seqPage(popW, 560), { subtitle: '64 STEPS · 16 MEMORIES' });
body += pop('pop-altA', 'ALTERNATIVE WAVE A', popX, popY, popW, 1060, altPage(popW, 1060, 'A'), { subtitle: 'DDS 1 ALT A: CHOOSE ONE OF 32 WAVES' });
body += pop('pop-custom', 'CUSTOM WAVE', Math.round((W - CU_W) / 2), Math.round((H - CU_H) / 2) - 40, CU_W, CU_H, customPage(CU_W, CU_H), { subtitle: 'DDS 1 · ONE SHOT · LOOP · OR SLICE IT ACROSS THE KEYS' });
body += patchBrowser();   // full-screen, not one of the floating POPS
body += pop('pop-settings', 'SETTINGS', Math.round((W - SET_W) / 2), Math.round((H - SET_H) / 2) - 40, SET_W, SET_H, settingsPage(SET_W), { subtitle: 'GLOBAL · SAVED WITH THE HOST PROJECT' });
body += pop('pop-fx', 'FX', Math.round((W - FXW) / 2), Math.round((H - FXH) / 2), FXW, FXH, fxPage(FXW, FXH), { subtitle: 'THREE SLOTS · SERIAL OR PARALLEL · LAYER MIX UNDER EACH SLOT' });
body += pop('pop-modulate', 'MODULATE', Math.round((W - 740) / 2), Math.round((H - 960) / 2), 740, 960, modulatePage(740, 960));
STYLE.flat = false;

// ---- desktop layout: lines of cards, each line justified to the widest one -----------
const DK_M = 24, DK_GAP = 16;
const DK_LINES = [
  { h: TOPBAR, cards: ['bar-l', 'bar-r'], rules: false },
  { h: ROWH, cards: ['lfo', 'mod', 'osc', 'mix'], engine: true },
  { h: 352, cards: ['perf', 'vcf', 'vca', 'env', 'dly'], engine: true },
  { h: STRIPH, cards: ['master', 'strip-manual', 'strip-tempo', 'strip-hold', 'strip-keyboard', 'strip-voice', 'strip-arp', 'strip-layer'] },
  { h: STRIPH, cards: ['strip-mod', 'strip-edit', 'strip-chorus', 'strip-delay', 'bender'] }
];
const cardSize = (line, n) => DKCARD.get(line.engine && !DKCARD.has(n) ? 'upper-' + n : n);
for (const line of DK_LINES) for (const n of line.cards) if (!cardSize(line, n)) throw new Error('desktop card missing: ' + n);
const lineW = (line) => line.cards.reduce((a, n) => a + cardSize(line, n).w, 0) + DK_GAP * (line.cards.length - 1);
const DK_CW = Math.max(...DK_LINES.map(lineW));
const DESKTOP_W = DK_CW + 2 * DK_M;
let dkCss = '', dkDecor = '', ly0 = 0;
DK_LINES.forEach((line, li) => {
  line.h = Math.max(line.h, ...line.cards.map((n) => cardSize(line, n).h));
  const top = li === 0 ? 0 : ly0;
  const gap = line.cards.length > 1 ? DK_GAP + (DK_CW - lineW(line)) / (line.cards.length - 1) : 0;
  let x = DK_M;
  line.cards.forEach((n, i) => {
    const sel = line.engine && !DKCARD.has(n) ? ['upper-' + n, 'lower-' + n] : [n];
    dkCss += sel.map((k) => 'body.desktop-layout [data-dk="' + k + '"]').join(',') + '{left:' + r1(x) + 'px;top:' + top + 'px}\n';
    if (i > 0 && line.rules !== false) dkDecor += vrule(x - gap / 2, top + 6, line.h - 12);
    x += cardSize(line, n).w + gap;
  });
  if (li > 1) dkDecor += hrule(DK_M, top - 6, DK_CW, 'var(--rule)');
  ly0 = top + line.h + (li === 0 ? 8 : 12);
});
const DESKTOP_H = ly0 - 2;
for (const [n, c] of DKCARD) dkCss += 'body.desktop-layout [data-dk="' + n + '"]{width:' + c.w + 'px;height:' + c.h + 'px}\n';
// pop-overs keep their own geometry, centred and scaled to fit the smaller canvas
for (const p of POPS) {
  const k = Math.min(1, (DESKTOP_W - 2 * DK_M) / p.w, (DESKTOP_H - 40) / p.h);
  dkCss += 'body.desktop-layout #' + p.id + ' .pop-panel{left:' + r1((DESKTOP_W - p.w * k) / 2) + 'px!important;top:'
    + r1((DESKTOP_H - p.h * k) / 2) + 'px!important;transform:scale(' + k.toFixed(3) + ');transform-origin:0 0}\n';
}
dkCss += '#pop-patches .pb-page{width:' + r1(W / PB_Z) + 'px;height:' + r1(H / PB_Z) + 'px;transform:scale(' + PB_Z + ')}\n'
  + 'body.desktop-layout #pop-patches .pb-page{width:' + DESKTOP_W + 'px;height:' + DESKTOP_H + 'px;transform:none}\n';
body = '<div class="dk-only">' + dkDecor + '</div>' + body;
console.log('desktop ' + DESKTOP_W + 'x' + DESKTOP_H);

const themeVars = (t) => CSS_KEYS.filter((k) => THEMES[t][k] != null).map((k) => '--' + k + ':' + THEMES[t][k]).join(';');
const rgb = (hex) => [1, 3, 5].map((i) => parseInt(hex.slice(i, i + 2), 16) / 255);
// piecewise: black -> colour at the asset's mid-tone L -> white (keeps highlights)
const tintFilter = (id, L, hex) => {
  const tv = (c) => Array.from({ length: 33 }, (_, i) => { const l = i / 32; return (l <= L ? c * l / L : c + (1 - c) * (l - L) / (1 - L)).toFixed(4); }).join(' ');
  const [r, gg, b] = rgb(hex);
  return '<filter id="' + id + '" color-interpolation-filters="sRGB"><feColorMatrix type="matrix" values="'
    + '.2126 .7152 .0722 0 0 .2126 .7152 .0722 0 0 .2126 .7152 .0722 0 0 0 0 0 1 0"/>'
    + '<feComponentTransfer><feFuncR type="table" tableValues="' + tv(r) + '"/><feFuncG type="table" tableValues="' + tv(gg) + '"/><feFuncB type="table" tableValues="' + tv(b) + '"/></feComponentTransfer></filter>';
};
// linear: the texture's mean lands on the colour, its grain scales with it
// Each output channel is the texture's luminance x (colour / L), so the photo's own cream tint does not
// leak into the result (it used to scale R, G and B separately, which warmed every theme's panel).
const flatFilter = (id, L, hex) => {
  const row = (c) => [.2126, .7152, .0722].map((w) => (w * c / L).toFixed(4)).join(' ') + ' 0 0';
  const [r, gg, b] = rgb(hex);
  return '<filter id="' + id + '" color-interpolation-filters="sRGB"><feColorMatrix type="matrix" values="'
    + row(r) + ' ' + row(gg) + ' ' + row(b) + ' 0 0 0 1 0"/></filter>';
};
// mid-tones measured from the assets (median luminance of opaque pixels)
const TINTED = THEME_KEYS.filter((k) => THEMES[k].cap1);
const TINT_SVG = '<svg width="0" height="0" style="position:absolute" aria-hidden="true"><defs>'
  + TINTED.map((k) => { const P = THEMES[k]; return ''
    + tintFilter('t-light-' + k, 0.77, P.cap1)       // cream knob faces, grey fader caps
    + tintFilter('t-key-' + k, 0.70, P.cap1)         // light push keys
    + tintFilter('t-lower-' + k, 0.42, P.cap2)       // orange caps -> LOWER colour
    + tintFilter('t-dark-' + k, 0.24, P.capDark)     // black caps and keys
    + flatFilter('t-base-' + k, 0.888, P.base)       // panel texture
    + flatFilter('t-inset-' + k, 0.259, P.inset)
    + (P.keys ? tintFilter('t-keys-' + k, 0.95, P.keys) : '');   // keybed (octave.png / whitekey.png), measured 0.95    // dark inset texture
  }).join('') + '</defs></svg>';
const TINT_CSS = TINTED.map((k) => [
  ['[src*="cream"],[src*="fader-grey"]', 't-light-'],
  ['[src*="button-light"]', 't-key-'],
  ['[src*="orange"]', 't-lower-'],
  ['[src*="-dark"]', 't-dark-'],
  ...(THEMES[k].keys ? [['[src*="octave.png"],[src*="whitekey"],[src*="key-off"],[src*="key-on"]', 't-keys-']] : [])
].map(([sel, f]) => sel.split(',').map((s) => 'body.theme-' + k + ' img' + s).join(',') + '{filter:url(#' + f + k + ')}').join('\n')
  + '\nbody.theme-' + k + ' .tex{filter:url(#t-base-' + k + ')}'
  // a light inset is a solid fill: the dark texture cannot be lifted to near-white without clipping
  + '\nbody.theme-' + k + ' .tex-dark{' + (THEMES[k].insetTex === 'hide' ? 'display:none' : 'filter:url(#t-inset-' + k + ')') + '}'
).join('\n');

const html = `<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <title>Somii</title>
  <style>
    /* Bahnschrift is a Windows face; macOS has no condensed face with its metrics, so the
       panel carries one (Roboto Condensed, SIL OFL - ui/FONT-LICENSE.txt). Windows still
       picks Bahnschrift first, so the panel there is unchanged. */
    @font-face { font-family: 'SPKR Condensed'; src: url(spkr-condensed-600.woff2) format('woff2'); font-weight: 600; font-display: block; }
    @font-face { font-family: 'SPKR Condensed'; src: url(spkr-condensed-700.woff2) format('woff2'); font-weight: 700; font-display: block; }
    html, body { margin: 0; background: #1c1d1f; }
    body { ${themeVars('gemini')}; }
${THEME_KEYS.filter((t) => t !== 'gemini').map((t) => '    body.theme-' + t + ' { ' + themeVars(t) + '; }').join('\n')}
    /* text-only commands: the clickable word is the control */
    body { --barlink: var(--oink); }
    body.desktop-layout [data-dk^="bar-"] { --barlink: var(--link); }
    .link { color: var(--link); border-radius: 3px; transition: color .1s, background .1s; }
    .link:hover { background: rgba(0,0,0,.08); }
    .link.pill { border: 1.5px solid currentColor; border-radius: 20px; box-sizing: border-box; }
    .link.pill.primary { background: var(--accent); border-color: var(--accent); color: var(--accent-ink) !important; }
    .link.pill.primary:hover { filter: brightness(1.12); }
    .folder-row:hover { background: rgba(0,0,0,.07); }
    body:not(.arp-free) .arp-free-only, body.arp-free .arp-sync-only { display: none; }
    .link.pill[aria-pressed="true"] { background: var(--accent); border-color: var(--accent); color: var(--accent-ink) !important; text-decoration: none; }
    /* pop-over pages: the theme, darker and flat (SPKR_UI_STYLE.md). Solid blocks on hairlines, the
       accent kept for one job - what is on / active - and no gradients, glows or texture. */
    .pop .pop-panel { --base:var(--pop-bg2); --ink:var(--pop-ink); --ink2:var(--pop-ink2); --opt:var(--pop-ink2); --accent:var(--pop-accent); --inset:var(--pop-bg2);
      --oink:var(--pop-ink); --oink2:var(--pop-ink2); --rule:var(--pop-rule); --otick:var(--pop-ink2); --badge:var(--pop-key); --badge-ink:var(--pop-ink);
      --hi:var(--pop-ink); --field:color-mix(in srgb,var(--pop-bg2) 70%,#000); --shade:.2; --sel:var(--pop-accent); --osel:var(--pop-accent); --link:var(--pop-link); --pressed:var(--pop-ink);
      --cell:color-mix(in srgb,var(--pop-bg2),var(--pop-ink) 5%); --cell-hi:color-mix(in srgb,var(--pop-bg2),var(--pop-ink) 10%);
      --wash:color-mix(in srgb,var(--pop-accent) 16%,transparent);
      background:var(--pop-bg2) !important; border-radius:8px !important;
      box-shadow:0 28px 56px -16px rgba(0,0,0,.7),0 0 0 1px var(--pop-rule) !important; }
    .pop .pop-head { background:var(--cell); border-bottom:1px solid var(--pop-rule); }
    .pop-close { color:var(--ink2); border-radius:6px; transition:color .12s,background-color .12s; }
    .pop-close:hover { color:var(--ink); background:var(--cell-hi); }
    .pop ::selection { background:var(--pop-accent); color:var(--pop-bg2); }
    .pop input { caret-color:var(--pop-accent); }
    .pop input::placeholder { color:var(--pop-ink2); opacity:.7; }
    .pop input:focus { border-color:var(--pop-accent) !important; }
    .pop :focus-visible { outline:2px solid var(--pop-accent); outline-offset:2px; }
    .pop ::-webkit-scrollbar { width:10px; height:10px; } .pop ::-webkit-scrollbar-track { background:transparent; }
    .pop ::-webkit-scrollbar-thumb { background:var(--cell-hi); border-radius:5px; border:2px solid var(--pop-bg2); }
    .pop ::-webkit-scrollbar-thumb:hover { background:var(--pop-ink2); }
    /* opening: the backdrop fades and the page rises into place (translate composes with the desktop-mode scale) */
    .pop:not([hidden]) { animation:pop-fade .16s ease-out; }
    .pop:not([hidden]) .pop-panel { animation:pop-rise .32s cubic-bezier(.16,1,.3,1); }
    @keyframes pop-fade { from { background-color:rgba(12,13,15,0); } }
    @keyframes pop-rise { from { translate:0 16px; opacity:0; } }
    /* flat keys (tools/make-fx-controls.mjs): the fill comes from the theme */
    .pop img[src$="flat-btn-off.svg"] { background:var(--pop-key); border-radius:7px; }
    .pop img[src$="flat-btn-on.svg"] { background:var(--pop-accent); border-radius:7px; }
    .pop .pop-panel .tex, .pop .pop-panel .tex-dark { display:none; }
    /* FX page backdrop (user, 2026-09-18): one solid colour - the theme gradient's dark end, no grain;
       the effect windows on it keep their own gradients */
    #pop-fx .pop-panel { background:var(--pop-bg2) !important; }
    .pop .pop-panel .section-heading { text-transform:lowercase; letter-spacing:.04em; }
    .fx-tile { position:absolute; box-sizing:border-box; border-radius:6px; cursor:pointer; padding:16px 14px 0 92px; background:var(--cell);
      transition:background-color .12s,box-shadow .12s,translate .18s cubic-bezier(.16,1,.3,1); font-family:Bahnschrift,'SPKR Condensed',sans-serif; font-stretch:75%; }
    .fx-tile:hover { background:var(--cell-hi); translate:0 -2px; }
    .fx-tile.sel { box-shadow:inset 0 0 0 2px var(--pop-accent); }
    .fx-sw { position:absolute; left:10px; top:10px; width:64px; height:64px; border-radius:7px; box-shadow:0 0 0 1px rgba(255,255,255,.1); transition:scale .2s cubic-bezier(.16,1,.3,1); }
    .fx-sw svg { display:block; }
    .fx-tile:hover .fx-sw { scale:1.06; }
    .fx-none { position:absolute; display:flex; align-items:center; gap:12px; padding:8px 10px; border-radius:6px; cursor:pointer; color:var(--ink2);
      font:700 17px Bahnschrift,'SPKR Condensed',sans-serif; font-stretch:75%; letter-spacing:.14em; transition:color .12s,background-color .12s; }
    .fx-none:hover { color:var(--ink); background:var(--cell); }
    .fx-none.sel { color:var(--pop-accent); }
    .fx-tn { display:block; font-weight:700; font-size:19px; letter-spacing:.06em; color:var(--ink); white-space:nowrap; }
    .fx-tb { display:block; margin-top:6px; font-weight:600; font-size:14px; letter-spacing:.02em; color:var(--ink2); white-space:nowrap; overflow:hidden; text-overflow:ellipsis; }
    #fx-picker:not([hidden]) { animation:pop-rise .26s cubic-bezier(.16,1,.3,1); }
    .fx-route[data-fx-grip]:hover { box-shadow:inset 0 0 0 1px var(--pop-ink2); }
    .pop .link.pill { border-color:var(--pop-rule); border-width:1px; transition:background-color .12s,border-color .12s,color .12s,transform .08s; }
    .pop .link.pill:hover { background:var(--cell-hi); border-color:var(--pop-ink2); }
    .pop .link.pill:active { transform:translateY(1px); }
    .pop .link.pill[aria-pressed="true"] { background:var(--wash); border-color:var(--pop-accent); color:var(--pop-ink) !important; }
    .pop .link.pill.primary { background:var(--pop-accent); border-color:var(--pop-accent); color:var(--pop-bg2) !important; }
    .pop .link:not(.pill):hover { background:none; color:var(--pop-ink) !important; text-decoration:underline; text-underline-offset:5px; text-decoration-thickness:1px; }
    /* settings: the theme cards - flat, on the pop-over's own colours; the one in use is outlined */
    .theme-card { position:absolute; box-sizing:border-box; display:grid; grid-template-columns:150px 1fr; grid-template-rows:auto 1fr auto; column-gap:20px;
      padding:20px; border-radius:8px; background:var(--pop-bg2); box-shadow:inset 0 0 0 1px var(--pop-rule); cursor:pointer;
      font-family:Bahnschrift,'SPKR Condensed','Arial Narrow',sans-serif; font-stretch:75%; transition:background-color .12s, box-shadow .12s, translate .12s; }
    .theme-card:hover { background:color-mix(in srgb, var(--pop-bg2), var(--pop-ink) 6%); translate:0 -2px; }
    .theme-card:focus-visible { outline:2px solid var(--pop-accent); outline-offset:2px; }
    .theme-card .tc-sw { grid-row:1 / span 3; display:block; }
    .theme-card .tc-name { font-size:24px; font-weight:700; letter-spacing:.1em; color:var(--pop-ink); line-height:1.1; }
    .theme-card .tc-blurb { margin-top:8px; font-size:16px; font-weight:600; letter-spacing:.03em; line-height:1.3; color:var(--pop-ink2); }
    .theme-card .tc-on { font-size:14px; font-weight:700; letter-spacing:.16em; color:var(--pop-accent); visibility:hidden; }
    .theme-card[aria-pressed="true"] { box-shadow:inset 0 0 0 2px var(--pop-accent); }
    .theme-card[aria-pressed="true"] .tc-on { visibility:visible; }
    @media (prefers-reduced-motion: reduce) { .theme-card { transition:none; } .theme-card:hover { translate:none; } }
    /* matrix cells: idle sits back, an amount lights the cell and draws a bar from its centre */
    .mtx-cell { position:absolute; border-radius:4px; background:var(--cell); pointer-events:none; transition:background-color .15s; }
    .mtx-cell::after { content:''; position:absolute; bottom:8px; height:3px; left:50%; width:46%; background:var(--pop-accent); border-radius:2px;
      transform:scaleX(var(--mag,0)); transform-origin:0 50%; transition:transform .12s ease-out; }
    .mtx-cell.neg::after { left:auto; right:50%; transform-origin:100% 50%; }
    .mtx-cell.on { background:var(--wash); }
    .mtx-cell.on + svg + img { filter:brightness(1.15); }
    .mtx-cell.cross { background:var(--cell-hi); } .mtx-cell.on.cross { background:color-mix(in srgb,var(--pop-accent) 24%,transparent); }
    .mtx-head { transition:color .12s; } .mtx-head.hot { color:var(--pop-ink) !important; }
    .mtx-val.on { color:var(--pop-ink) !important; }
    #pop-fx.fx-mode-serial .fx-par, #pop-fx.fx-mode-parallel .fx-ser { display: none; }
    .fx-slot:hover, .fx-sel:hover { text-decoration: underline; text-underline-offset: 5px; }
    .fx-knob.off, .fx-sel:empty { display: none; }
    .fx-drop { position: absolute; inset: 0; display: none; align-items: center; justify-content: center; font: 700 22px Bahnschrift,sans-serif; font-stretch: 75%; letter-spacing: .12em; color: var(--accent); background: rgba(0,0,0,.72); border: 2px dashed var(--accent); border-radius: 4px; pointer-events: none; }
    .fx-screen.drag .fx-drop { display: flex; }
    .link[aria-pressed="true"] { color: var(--pressed) !important; text-decoration: underline; text-underline-offset: 4px; }
    [data-dk^="bar-"] .link[aria-pressed="true"] { color: var(--osel) !important; }
    body.desktop-layout [data-dk^="bar-"] .link[aria-pressed="true"] { color: var(--pressed) !important; }
${TINT_CSS}
    body { font-family: ${FONT}; font-stretch: 75%; -webkit-user-select: none; user-select: none; -webkit-tap-highlight-color: transparent; overflow: hidden; }
    img { display: block; -webkit-user-drag: none; }
    .shift-only { display: none; }
    body.shift .shift-only { display: block; }
    body.shift .shift-hide { display: none; }
    [id^="pop-"] .pop-panel { }
    /* patch browser: a full-screen page (after Arturia's), laid out as a grid so it fills either layout */
    .pb-pop-page { position:absolute; left:0; top:0; width:100%; height:100%; z-index:40; background:var(--pop-bg2); }
    #pop-patches .pb-page { position:absolute; left:0; top:0; transform-origin:0 0; border-radius:0 !important; box-shadow:none !important; overflow:hidden;
      display:grid; grid-template-columns:320px minmax(0,1fr) 440px; grid-template-rows:76px minmax(0,1fr);
      font-family:Bahnschrift,'SPKR Condensed',sans-serif; font-stretch:75%; color:var(--pop-ink); }
    #pop-patches .pb-page > * { min-width:0; min-height:0; }
    .pb-top { grid-column:1 / -1; display:flex; align-items:center; gap:20px; padding:0 14px 0 0; background:var(--cell); border-bottom:1px solid var(--pop-rule); }
    .pb-back { width:306px; flex:none; height:76px; display:flex; align-items:center; gap:10px; padding-left:22px; box-sizing:border-box; cursor:pointer;
      font-weight:700; font-size:22px; letter-spacing:.12em; color:var(--ink2); border-right:1px solid var(--pop-rule); transition:color .12s,background-color .12s; }
    .pb-back:hover { color:var(--ink); background:var(--cell-hi); }
    .pb-title { font-weight:700; font-size:26px; letter-spacing:.16em; }
    .pb-search { flex:1; max-width:640px; height:46px; display:flex; align-items:center; gap:10px; padding:0 14px; box-sizing:border-box; border-radius:23px;
      background:var(--field); box-shadow:inset 0 0 0 1px var(--pop-rule); color:var(--ink2); transition:box-shadow .12s; }
    .pb-search:focus-within { box-shadow:inset 0 0 0 1.5px var(--pop-accent); }
    .pb-search input { flex:1; min-width:0; border:0; outline:none; background:none; color:var(--ink); font:700 19px Bahnschrift,'SPKR Condensed',sans-serif; font-stretch:75%; letter-spacing:.04em; }
    .pb-chip { height:46px; flex:none; display:flex; align-items:center; gap:10px; padding:0 18px; box-sizing:border-box; border-radius:23px; cursor:pointer;
      box-shadow:inset 0 0 0 1px var(--pop-rule); font-weight:700; font-size:16px; letter-spacing:.1em; color:var(--ink2); transition:background-color .12s,color .12s; }
    .pb-chip:hover { background:var(--cell-hi); color:var(--ink); }
    .pb-chip.on { color:var(--pop-ink); background:var(--wash); box-shadow:inset 0 0 0 1px var(--pop-accent); } .pb-chip.on svg { color:var(--pop-accent); }
    .pb-textbtn { flex:none; cursor:pointer; font-weight:700; font-size:15px; letter-spacing:.1em; color:var(--ink2); }
    .pb-textbtn:hover { color:var(--ink); text-decoration:underline; text-underline-offset:5px; }
    .pb-count { margin-left:auto; flex:none; font-weight:600; font-size:16px; letter-spacing:.12em; color:var(--ink2); }
    #pop-patches .pop-close { flex:none; }
    /* sidebar */
    .pb-side { display:flex; flex-direction:column; padding:18px 14px; gap:2px; border-right:1px solid var(--pop-rule); overflow:hidden; }
    .pb-nav { display:flex; align-items:center; justify-content:space-between; height:48px; padding:0 14px; border-radius:6px; cursor:pointer;
      font-weight:700; font-size:20px; letter-spacing:.1em; color:var(--ink2); transition:background-color .12s,color .12s; }
    .pb-nav span { font-weight:600; font-size:15px; }
    .pb-nav:hover { background:var(--cell); color:var(--ink); }
    .pb-nav.on { background:var(--wash); color:var(--pop-ink); }
    .pb-side-h { margin:22px 14px 10px; font-weight:700; font-size:14px; letter-spacing:.18em; color:var(--ink2); }
    .pb-packlist { flex:1; min-height:0; overflow-y:auto; display:flex; flex-direction:column; gap:2px; }
    .pb-pl { display:flex; align-items:center; gap:12px; padding:6px 10px; border-radius:6px; cursor:pointer; transition:background-color .12s; }
    .pb-pl:hover { background:var(--cell); } .pb-pl.on { background:var(--wash); }
    .pb-pl-cover { width:44px; height:44px; flex:none; border-radius:4px; overflow:hidden; box-shadow:0 0 0 1px var(--pop-rule); }
    .pb-pl-name { flex:1; min-width:0; font-weight:700; font-size:17px; letter-spacing:.05em; white-space:nowrap; overflow:hidden; text-overflow:ellipsis; }
    .pb-pl-n { font-weight:600; font-size:14px; color:var(--ink2); }
    .pb-side-foot { border-top:1px solid var(--pop-rule); padding:14px 10px 0; margin-top:10px; }
    .pb-links { display:grid; grid-template-columns:1fr 1fr; gap:6px 10px; margin-top:10px; }
    .pb-l { font-weight:700; font-size:14px; letter-spacing:.1em; color:var(--ink2) !important; cursor:pointer; padding:4px 0; }
    .pb-l:hover { color:var(--ink) !important; text-decoration:underline; text-underline-offset:4px; }
    .pb-note { font-weight:600; font-size:14px; line-height:1.4; letter-spacing:.03em; color:var(--ink2); white-space:nowrap; overflow:hidden; text-overflow:ellipsis; }
    /* middle */
    .pb-main { position:relative; overflow:hidden; }
    .pb-view { position:absolute; inset:0; display:flex; flex-direction:column; padding:18px 26px 0; }
    .pb-view[hidden] { display:none; }
    .pb-packhead { flex:none; display:flex; align-items:center; gap:22px; margin-bottom:16px; padding:14px; border-radius:8px; background:var(--cell); animation:pb-art-in .35s ease-out; }
    .pb-packhead[hidden] { display:none; }
    .pb-packhead .pb-ph-cover { width:110px; height:110px; flex:none; border-radius:6px; overflow:hidden; cursor:pointer; position:relative; }
    .pb-packhead h3 { margin:0; font-weight:700; font-size:34px; letter-spacing:.04em; }
    .pb-packhead p { margin:6px 0 0; font-weight:600; font-size:15px; letter-spacing:.12em; color:var(--ink2); }
    .pb-packhead .pb-ph-acts { margin-left:auto; display:flex; gap:10px; }
    .pb-types { flex:none; display:flex; gap:10px; overflow-x:auto; padding-bottom:12px; scrollbar-width:thin; }
    .pb-type { flex:none; height:38px; line-height:38px; padding:0 18px; border-radius:19px; cursor:pointer; box-shadow:inset 0 0 0 1px var(--pop-rule);
      font-weight:700; font-size:15px; letter-spacing:.1em; color:var(--ink2); white-space:nowrap; transition:background-color .12s,color .12s,box-shadow .12s; }
    .pb-type:hover { color:var(--ink); background:var(--cell-hi); }
    .pb-type.on { color:var(--pop-ink); background:var(--wash); box-shadow:inset 0 0 0 1px var(--pop-accent); }
    /* the table: one column template for the head and every row, so the words line up */
    .pb-grid { display:grid; grid-template-columns:56px minmax(0,2.3fr) minmax(0,1.4fr) minmax(0,1.2fr); column-gap:16px; align-items:center; padding:0 12px 0 0; }
    .pb-thead { flex:none; height:40px; border-bottom:1px solid var(--pop-rule); font-weight:700; font-size:14px; letter-spacing:.16em; color:var(--ink2); }
    .pb-head { cursor:pointer; white-space:nowrap; display:flex; align-items:center; gap:8px; transition:color .12s; }
    .pb-head:first-child { justify-content:center; }
    .pb-head:hover { color:var(--ink) !important; }
    .pb-arrow { display:inline-block; width:0; height:0; border:5px solid transparent; }
    .pb-arrow.up { border-bottom-color:currentColor; border-top-width:0; } .pb-arrow.down { border-top-color:currentColor; border-bottom-width:0; }
    .pb-list { flex:1; min-height:0; overflow-y:auto; outline:none; padding-bottom:20px; }
    .patch-row { height:50px; font-size:19px; font-weight:700; letter-spacing:.04em; color:var(--ink); cursor:pointer; border-bottom:1px solid var(--pop-rule); transition:background-color .1s; }
    .patch-row > span { white-space:nowrap; overflow:hidden; text-overflow:ellipsis; }
    .patch-row:hover { background:var(--cell); }
    .patch-row.sel { background:var(--wash); }
    .patch-row.sel .pb-name { color:var(--pop-accent); }
    .pb-type-c, .pb-bank { color:var(--ink2); font-weight:600; }
    .pb-fav { display:flex; justify-content:center; color:var(--pop-rule); cursor:pointer; transition:color .12s; }
    .pb-fav:hover { color:var(--ink2); }
    .pb-fav.on { color:var(--pop-accent); }
    .pb-fav.pop-in { animation:pb-like .32s cubic-bezier(.16,1,.3,1); }
    @keyframes pb-like { from { scale:1.4; } }
    .patch-empty { padding:40px 16px; font-size:18px; font-weight:600; letter-spacing:.04em; color:var(--ink2); }
    /* pack grid */
    .pb-packgrid { flex:1; min-height:0; overflow-y:auto; display:grid; grid-template-columns:repeat(auto-fill,minmax(200px,1fr)); gap:26px 22px; align-content:start; padding:6px 4px 24px; }
    .pb-pack { cursor:pointer; position:relative; transition:translate .2s cubic-bezier(.16,1,.3,1); }
    .pb-pack:hover { translate:0 -4px; }
    .pb-cover { position:relative; aspect-ratio:1; border-radius:8px; overflow:hidden; background:var(--cell); box-shadow:0 0 0 1px var(--pop-rule); }
    .pb-cover img, .pb-cover svg, .pb-art img, .pb-art svg, .pb-pl-cover img, .pb-pl-cover svg, .pb-ph-cover img, .pb-ph-cover svg { display:block; width:100%; height:100%; object-fit:cover; }
    .pb-pack-name { margin-top:12px; font-weight:700; font-size:20px; letter-spacing:.06em; white-space:nowrap; overflow:hidden; text-overflow:ellipsis; }
    .pb-pack-count { margin-top:3px; font-weight:600; font-size:14px; letter-spacing:.12em; color:var(--ink2); }
    /* change image: appears over a cover on hover */
    .pb-change { position:absolute; left:10px; right:10px; bottom:10px; height:36px; line-height:36px; text-align:center; border-radius:18px; cursor:pointer;
      background:rgba(14,14,15,.82); color:#F2EEE8; font-weight:700; font-size:14px; letter-spacing:.12em; opacity:0; translate:0 6px; transition:opacity .15s,translate .2s cubic-bezier(.16,1,.3,1); }
    .pb-cover:hover .pb-change, .pb-art:hover .pb-change, .pb-ph-cover:hover .pb-change { opacity:1; translate:0 0; }
    .pb-change:hover { background:var(--pop-accent); color:var(--pop-bg2); }
    /* right */
    .pb-detail { display:flex; flex-direction:column; gap:12px; padding:22px 24px 18px; border-left:1px solid var(--pop-rule); overflow:hidden; }
    .pb-art { position:relative; flex:none; aspect-ratio:1; border-radius:8px; overflow:hidden; background:var(--cell); box-shadow:0 0 0 1px var(--pop-rule); cursor:pointer; }
    .pb-art > :not(.pb-change) { animation:pb-art-in .4s ease-out; }
    @keyframes pb-art-in { from { opacity:0; scale:1.03; } }
    .pb-cur { display:flex; align-items:center; gap:10px; margin-top:6px; }
    .pb-cur-name { flex:1; min-width:0; font-weight:700; font-size:32px; letter-spacing:.02em; white-space:nowrap; overflow:hidden; text-overflow:ellipsis; }
    .pb-cur-fav svg { width:28px; height:28px; }
    .pb-cur-meta { font-weight:600; font-size:15px; letter-spacing:.12em; color:var(--ink2); }
    .pb-row { display:flex; gap:10px; }
    .pb-row > .pill { flex:1; height:44px; line-height:44px; text-align:center; font-weight:700; font-size:16px; letter-spacing:.1em; cursor:pointer; color:var(--ink); }
    .pb-save { margin-top:auto; display:flex; flex-direction:column; gap:8px; }
    .pb-save .pb-side-h { margin:4px 0 4px; }
    .pb-field { display:grid; grid-template-columns:62px minmax(0,1fr) 44px; gap:8px; align-items:center; font-weight:700; font-size:13px; letter-spacing:.14em; color:var(--ink2); }
    .pb-input { height:44px; box-sizing:border-box; min-width:0; border:1px solid var(--pop-rule); border-radius:6px; background:var(--field); color:var(--ink); outline:none; padding:0 12px;
      font:700 18px Bahnschrift,'SPKR Condensed',sans-serif; font-stretch:75%; letter-spacing:.04em; }
    .pb-menu-btn { width:44px; height:44px; display:flex; align-items:center; justify-content:center; border-radius:6px; cursor:pointer; box-shadow:inset 0 0 0 1px var(--pop-rule); color:var(--ink2); transition:background-color .12s,color .12s; }
    .pb-menu-btn:hover { background:var(--cell-hi); color:var(--ink); }
    .pb-status { min-height:20px; color:var(--ink); white-space:normal; }
    .pb-pop { position:absolute; z-index:30; min-width:220px; max-height:420px; overflow-y:auto; padding:6px 0; border-radius:8px;
      background:var(--cell-hi); box-shadow:0 18px 36px -10px rgba(0,0,0,.7),0 0 0 1px var(--pop-rule); animation:pb-menu .16s ease-out;
      font:700 16px Bahnschrift, sans-serif; font-stretch:75%; letter-spacing:.04em; color:var(--ink); }
    @keyframes pb-menu { from { opacity:0; translate:0 -6px; } }
    .pb-pop div { padding:8px 16px; cursor:pointer; white-space:nowrap; }
    .pb-pop div:hover { background:var(--wash); }
    .pb-pop div.on { color:var(--accent); }
    /* VCF STYLE: the Super Gemini layout or the 3rd Wave one, per layer */
    .vcf-3w-upper, .vcf-3w-lower { display: none; }
    body.vcf3w-upper .vcf-3w-upper { display: block; }
    body.vcf3w-upper .vcf-sg-upper { display: none; }
    body.vcf3w-lower .vcf-3w-lower { display: block; }
    body.vcf3w-lower .vcf-sg-lower { display: none; }
    /* the sample page: text segments bound to a parameter; groups that do not apply to the mode dim */
    .cu-seg { display:flex; align-items:center; justify-content:center; box-sizing:border-box; border-radius:6px; box-shadow:inset 0 0 0 1px var(--pop-rule);
      font:700 17px Bahnschrift,'SPKR Condensed',sans-serif; font-stretch:75%; letter-spacing:.1em; color:var(--ink2); transition:background-color .12s,color .12s,box-shadow .12s; }
    .cu-seg:hover { background:var(--cell-hi); color:var(--ink); }
    .cu-seg[aria-pressed="true"] { background:var(--pop-accent); color:var(--pop-bg2); box-shadow:none; }
    .cu-group { position:absolute; left:0; top:0; width:100%; height:100%; pointer-events:none; transition:opacity .2s; }
    .cu-group > [data-param], .cu-group > [data-action] { pointer-events:auto; }
    #pop-custom:not(.cu-loop) .cu-loop-only, #pop-custom:not(.cu-slice) .cu-slice-only { opacity:.28; }
    #pop-custom:not(.cu-loop) .cu-loop-only > *, #pop-custom:not(.cu-slice) .cu-slice-only > * { pointer-events:none !important; }
    .seq-sel { box-shadow: inset 0 0 0 3px ${ORANGE} !important; }
    .seq-off { opacity: .35; }
    /* sequencer: flat step blocks; the selection is drawn in ink, the accent is kept for what is on */
    .seq-cell { position:absolute; border-radius:6px; cursor:pointer; z-index:5; background:var(--cell); transition:background-color .12s,opacity .2s; }
    .seq-cell:hover { background:var(--cell-hi); }
    .pop .seq-cell.seq-sel { box-shadow:inset 0 0 0 2px var(--pop-ink) !important; }
    .seq-cell.seq-now { background:var(--wash); }
    .seq-cell.seq-off { opacity:.35; }
    .seq-play { position:absolute; height:4px; border-radius:6px 6px 0 0; background:var(--pop-accent); opacity:0; pointer-events:none; z-index:6; transition:opacity .06s; }
    .seq-flag { position:absolute; height:24px; border-radius:4px; cursor:pointer; z-index:7; transition:background-color .1s; }
    .seq-flag::before { content:''; position:absolute; left:8px; top:7px; width:9px; height:9px; border-radius:2px; box-shadow:inset 0 0 0 1.5px var(--pop-ink2); }
    .seq-flag:hover { background:var(--cell-hi); }
    .seq-flagtxt { opacity:.6; }
    .flag-on { background:var(--pop-accent) !important; }
    .flag-on::before { background:var(--pop-bg2); box-shadow:none; }
    .flag-on + div { color:var(--pop-bg2) !important; opacity:1; }
    .seq-note.empty { opacity:.25; }
    @media (prefers-reduced-motion: reduce) { .pop *, .pop { animation:none !important; transition:none !important; } }
    .key-down { filter: brightness(.86); }
    /* desktop mode - layout generated from DK_LINES in gen.mjs */
    .dk-only, .dk-only.dk { display: none; }
    body.desktop-layout .dk-only { display: block; }
    body.desktop-layout .fp-only, body.desktop-layout #keybed { display: none; }
    body.desktop-layout .fp-wrap > :not(.dk) { display: none !important; }
    body.desktop-layout .dk { position: absolute; }
    body.desktop-layout .dk-in { position: absolute; left: 0; top: 0; transform-origin: 0 0; transform: scale(var(--k)) translate(var(--sx), var(--sy)); }
    body.desktop-layout.layer-upper [data-dk^="lower-"], body.desktop-layout.layer-lower [data-dk^="upper-"] { display: none; }
    body.desktop-layout #panel { width: ${DESKTOP_W}px !important; height: ${DESKTOP_H}px !important; }
    /* the top-bar keys sit on the panel itself in desktop mode, so print their legends in panel ink */
    body.desktop-layout [data-dk^="bar-"] { --oink: var(--ink); --oink2: var(--ink2); --hi: var(--ink); }
    body.desktop-layout .patch-field { background: var(--field) !important; box-shadow: inset 0 1px 2px rgba(0,0,0,.25), inset 0 0 0 1px var(--rule) !important; }
${dkCss}  </style>
</head>
<body class="layer-upper">
${TINT_SVG}
<div id="panel" style="position:relative;width:${W}px;height:${H}px;overflow:hidden;background:var(--base);">
<div class="tex" style="position:absolute;inset:0;background:url(${A.panel}) repeat;pointer-events:none;"></div>
${body}
  <div style="position:absolute;inset:0;background:url(${A.shading}) no-repeat center/100% 100%;mix-blend-mode:multiply;opacity:var(--shade);pointer-events:none;"></div>
  <div style="position:absolute;inset:0;box-shadow:inset 0 1px 0 rgba(255,255,255,.65),inset 0 -2px 6px rgba(0,0,0,.14);pointer-events:none;"></div>
</div>
<script id="fx-spec" type="application/json">${JSON.stringify({ FX, DIVS, NP, CARVE_BEATS, CARVE_SHAPERS, CARVE_HINTS, FX_MOD_SOURCES, FX_EXT }).replace(/</g, '\\u003c')}</script>
<script id="parameter-spec" type="application/json">${JSON.stringify(Object.fromEntries(PARAMS)).replace(/</g, '\\u003c')}</script>
<script type="module" src="geminus.js"></script>
</body>
</html>
`;

// ---- validation: every bound id must be a parameter, every parameter must be bound ----------
const bound = new Set();
for (const m of html.matchAll(/data-param="([^"]+)"/g)) bound.add(m[1]);
for (const m of html.matchAll(/data-layered="([^"]+)"/g)) { bound.add('upper.' + m[1]); bound.add('lower.' + m[1]); }
for (const m of html.matchAll(/data-shift="([^"]+)"/g)) {
  if (PARAMS.has(m[1])) bound.add(m[1]);
  else { bound.add('upper.' + m[1]); bound.add('lower.' + m[1]); }
}
for (const m of html.matchAll(/data-shiftparam="([^"]+)"/g)) bound.add(m[1]);
const ignore = (id) => /^modslot\d$|^modamt$|^global\.transposeLed$/.test(id);
const bad = [...bound].filter((b) => !PARAMS.has(b) && !ignore(b));
// the direct-mapping matrix (mtxd) is reached through the right-click MODULATE page
// every FX-rack parameter is drawn per effect by ui/fx.js, not by this page
const missing = [...PARAMS.keys()].filter((p) => !bound.has(p) && !/\.mtxd\./.test(p) && !/^fx\d?\./.test(p) && p !== 'perf.ribbon'
  && !/\.dds1\.altB$/.test(p));   // ALT B: still the ALT morph target of older patches; its key became CUSTOM   // perf.ribbon: host/MIDI side of the ribbon
if (bad.length) console.error('BOUND BUT NOT A PARAMETER:\n  ' + bad.join('\n  '));
if (missing.length) console.error('PARAMETER WITHOUT A CONTROL:\n  ' + missing.join('\n  '));
if (bad.length || missing.length) process.exit(1);

const htmlOut = html.replace(/ (stroke|fill)="(var(--[a-z0-9-]+))"/g, ' style="$1:$2"');
if (/<[^>]*style="[^>]*style="/.test(htmlOut)) throw new Error('duplicate style attribute after var() rewrite');
fs.writeFileSync(path.join(OUT, 'index.html'), htmlOut);
console.log('wrote index.html  ' + W + 'x' + H + '  bound ' + bound.size + ' ids, ' + PARAMS.size + ' parameters (' + [...PARAMS.keys()].filter((p) => /\.mtxd\./.test(p)).length + ' direct mappings via MODULATE)');
