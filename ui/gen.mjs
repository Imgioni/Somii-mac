// Generates ui/index.html - the Geminus panel. Run: node gen.mjs
// Layout follows the real Super Gemini (docs/reference/ui_prompt_v2.md).
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import {
  A, INK, INK2, ORANGE, ONBLACK_INK, ONBLACK_INK2, txt, txtInv, vrule, hrule, blackPanel, well,
  knob, rotary, wRotary, fader, hfader, sw3, wSw3, button, led, ledBound, ledLadder, sect, popover,
  keyButton, hit, TEXT, lineH, wText, r1, esc, FONT, BTN_ASPECT, STYLE
} from './lib.mjs';
import { FX, DIVS } from './fxdefs.mjs';

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
        steps: ['TRI', 'SAW', 'S&H', 'SQR', 'HF', 'TRK'], label: 'WAVE', ly: y + 230, id: P('lfo1.wave') });
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
      F(0.0, 'PW/DET', null, 'ddsMod.pwDetune'),
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

  // -- VCF
  o += group('vcf', S.vcf.x, S.vcf.w);
  g = S.vcf;
  o += sect(g.x, y + 2, g.w, 'VCF');
  o += pack(g, [
    SW(0, ['2', '1', 'OFF'], 'DRIVE', 'vcf.drive'),
    F(0.0, 'HPF', null, 'vcf.hpf'),
    cluster([F(1.0, 'LPF', null, 'vcf.lpf'), F(0.0, 'RES', null, 'vcf.res')], 'FILTER'),
    SWCOL({ pos: 0, opts: ['ENV 2', '1+2', 'ENV 1'], label: 'ENV SRC', id: 'vcf.envSource' },
          { pos: 0, opts: ['ON', '1/2', 'OFF'], label: 'KEYTRACK', id: 'vcf.keytrack' }),
    cluster([F(0.0, 'ENV', null, 'vcf.envAmt'), F(0.0, 'LFO 1', null, 'vcf.lfo1Amt'),
      F(0.0, 'DDS 2', null, 'vcf.dds2Amt', 'dark')], 'CUTOFF MODULATION', 3)
  ], 3, 'vcf');
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
  o += knob({ x: x + 14, y: B - 6, d: 56, v: 'cream', val: 0.5, ticks: 11, label: '', id: 'modamt', domId: 'modamt', title: 'Modulation amount: -100 to +100 percent' });
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
  o += sect(rx, y - 33, rw, 'RIBBON', { size: 9, ruleColor: 'var(--rule)' });
  o += '<img src="' + A.ribbon + '" alt="" style="position:absolute;left:' + rx + 'px;top:' + y + 'px;width:' + rw + 'px;height:' + rh + 'px;">';
  o += '<img src="' + A.ribbonLeft + '" alt="" style="position:absolute;left:' + rx + 'px;top:' + y + 'px;width:' + endW + 'px;height:' + rh + 'px;">';
  o += '<img src="' + A.ribbonRight + '" alt="" style="position:absolute;left:' + (rx + rw - endW) + 'px;top:' + y + 'px;width:' + endW + 'px;height:' + rh + 'px;">';
  // touch marker: a lit bar that follows the finger, wider with pressure
  o += '<div id="ribbon-marker" style="position:absolute;left:' + (rx + rw / 2) + 'px;top:' + (y + 8) + 'px;width:6px;height:' + (rh - 16) + 'px;margin-left:-3px;'
    + 'border-radius:3px;background:' + ORANGE + ';box-shadow:0 0 14px 4px rgba(246,90,39,.75),0 0 3px 1px rgba(255,200,170,.9);opacity:0;pointer-events:none;transition:opacity .12s;"></div>';
  o += '<div id="ribbon-input" title="Ribbon: slide left/right or up/down = pitch bend (relative)" style="position:absolute;left:' + playableX + 'px;top:' + (y + 8) + 'px;width:' + playableW + 'px;height:' + (rh - 16) + 'px;cursor:pointer;touch-action:none;z-index:5;"></div>';
  for (let i = 1; i < 4; i++) {
    const mxp = playableX + (playableW * i / 4);
    o += '<div style="position:absolute;left:' + r1(mxp) + 'px;top:' + (y - 9) + 'px;width:1.5px;height:6px;background:' + INK2 + ';opacity:.8;"></div>';
  }
  return o;
}

// the product mark (user, 2026-09-19): 002, by S·P·K·R - centred in the corner beside the ribbon
function wordmark(x, y) {
  return '<div style="position:absolute;left:' + x + 'px;top:' + (y - 4) + 'px;width:470px;height:84px;display:flex;align-items:center;justify-content:center;gap:22px;'
    + 'font-family:' + FONT + ';font-stretch:75%;color:' + INK + ';white-space:nowrap;pointer-events:none">'
    + '<span style="font-size:64px;font-weight:700;letter-spacing:.06em;line-height:1">002</span>'
    + '<span style="width:1.5px;height:46px;background:currentColor;opacity:.8"></span>'
    + '<span style="display:flex;flex-direction:column;align-items:flex-start;line-height:1.05">'
    + '<span style="font-size:15px;font-weight:600;letter-spacing:.3em;color:' + INK2 + '">BY</span>'
    + '<span style="font-size:30px;font-weight:700;letter-spacing:.04em">S·P·K·R</span></span></div>';
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
  // patch field: click the name to browse; SAVE and prev / next live inside its right end
  const fx = PL + 16, fy = 15, fw = 600, fh = 34;
  o += dk('bar-l', PL, 0, BAR_L, TOPBAR);
  o += '<div class="patch-field" style="position:absolute;left:' + fx + 'px;top:' + fy + 'px;width:' + fw + 'px;height:' + fh + 'px;background:rgba(0,0,0,.3);border-radius:3px;box-shadow:inset 0 1px 3px rgba(0,0,0,.6),inset 0 0 0 1px rgba(255,255,255,.1);"></div>';
  o += txt(fx + 12, fy + 9, 70, 'PATCH', { size: 8, align: 'left', color: 'var(--hi)', ls: 0.14 });
  const inW = [74, 34, 34], inX = fx + fw - 6 - inW.reduce((s, v) => s + v, 0);
  o += txt(fx + 82, fy + 6, inX - fx - 92, 'INIT', { size: 11, align: 'left', color: 'var(--hi)', ls: 0.06, id: 'patch-name', style: 'overflow:hidden;text-overflow:ellipsis;' });
  o += '<div data-open="pop-patches" title="Open the patch browser" style="position:absolute;left:' + fx + 'px;top:' + fy + 'px;width:' + (inX - fx) + 'px;height:' + fh + 'px;cursor:pointer;z-index:6;"></div>';
  const inLink = (x, wd, label, action, opt) => keyButton(x, fy, wd, label, action, Object.assign({ link: true, size: label.length > 1 ? 8.4 : 9, h: fh, color: 'var(--barlink)' }, opt));
  o += inLink(inX, inW[0], 'SAVE', 'patchSave', { title: 'Save the patch under its name (Shift: save as)' });
  o += inLink(inX + inW[0], inW[1], '◀', 'patchPrev', { title: 'Previous patch in the folder' });
  o += inLink(inX + inW[0] + inW[1], inW[2], '▶', 'patchNext', { title: 'Next patch in the folder' });
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

function matrixPage(w, h) {
  let o = '';
  o += layerLink(w, 'mtx-layer-link');
  const x0 = 250, y0 = 138, cw = (w - x0 - 30) / 8, ch = (h - y0 - 30) / 8;
  MTX_DST.forEach((d, c) => o += txt(x0 + c * cw, y0 - 44, cw, d[1], { size: 8.6, color: INK, ls: 0.08 }));
  MTX_DST.forEach((d, c) => o += txt(x0 + c * cw, y0 - 26, cw, 'ABCDEFGH'[c], { size: 7, color: INK2, weight: 600 }));
  MTX_SRC.forEach((s, r) => {
    o += txt(30, y0 + r * ch + ch / 2 - 18, 200, s[1], { size: 9.4, align: 'right' });
    o += txt(30, y0 + r * ch + ch / 2 + 2, 200, String(r + 1), { size: 7, align: 'right', color: INK2, weight: 600 });
    o += hrule(30, y0 + r * ch, w - 60, 'var(--rule)', 1);
  });
  const kd = 44;
  MTX_SRC.forEach((s, r) => MTX_DST.forEach((d, c) => {
    const dom = 'mtx.' + s[0] + '.' + d[0];
    const cx = x0 + c * cw + cw / 2, cy = y0 + r * ch + ch / 2 - 8;
    o += knob({ x: cx - kd / 2, y: cy - kd / 2, d: kd, v: 'cream', val: 0.5, ticks: 5, id: 'mtx.' + s[0] + '.' + d[0], layered: true, domId: dom });
    o += txt(cx - 40, cy + kd / 2 + 10, 80, '0', { size: 6.8, color: INK2, weight: 600, id: dom + '__val', cls: 'mtx-val' });
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
    o += '<div id="seq-cell' + i + '" data-seqstep="' + i + '" style="position:absolute;left:' + r1(cx) + 'px;top:' + gy + 'px;width:' + r1(cwi) + 'px;height:' + chh + 'px;border-radius:5px;cursor:pointer;z-index:5;background:var(--inset);box-shadow:0 2px 4px rgba(0,0,0,.25);"></div>';
    o += '<div id="seq-play' + i + '" style="position:absolute;left:' + r1(cx) + 'px;top:' + gy + 'px;width:' + r1(cwi) + 'px;height:6px;border-radius:5px 5px 0 0;background:' + ORANGE + ';opacity:0;pointer-events:none;z-index:6;"></div>';
    o += txt(cx, gy + 14, cwi, String(i + 1), { size: 7, color: inkC, weight: 600, id: 'seq-num' + i, style: 'z-index:6;opacity:.75;' });
    o += txt(cx, gy + 48, cwi, '—', { size: 15, color: inkC, id: 'seq-note' + i, style: 'z-index:6;' });
    o += txt(cx, gy + 88, cwi, '', { size: 6.4, color: inkC, weight: 600, id: 'seq-more' + i, style: 'z-index:6;' });
    ['SLIDE', 'ACCENT', 'REST'].forEach((f, k) => {
      const fy = gy + 118 + k * 30;
      o += '<div id="seq-flag' + i + '_' + k + '" data-seqflag="' + k + '" data-seqstep="' + i + '" style="position:absolute;left:' + r1(cx + 8) + 'px;top:' + fy + 'px;width:' + r1(cwi - 16) + 'px;height:24px;border-radius:12px;box-shadow:inset 0 0 0 1.5px ' + inkC + ';cursor:pointer;z-index:7;"></div>';
      o += txt(cx + 8, fy + 5, cwi - 16, f, { size: 6, color: inkC, weight: 700, style: 'z-index:8;', id: 'seq-flagtxt' + i + '_' + k });
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
  o += txt(30, h - 36, w - 60, 'Click a step to select it · SLIDE ties into the next step · ACCENT adds level (DYNAMICS) · REST skips · arp MODE = SEQ and ON to play · HOLD transposes from C4', { size: 6.6, align: 'left', color: INK2, weight: 600 });
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
  o += txt(30, h - 34, w - 60, 'FACTORY CYCLES  ·  CLICK A WAVE TO SELECT  ·  SET DDS 1 WAVEFORM TO ALT TO HEAR IT', { size: 6.8, align: 'left', color: INK2, weight: 600 });
  return o;
}

// DDS 1 CUSTOM: a sample page. The waveform display takes the drop and shows START / END as
// handles you drag; the controls under it are this layer's dds1.smp* parameters.
const CU_W = 1640, CU_H = 760;
function customPage(w, h) {
  let o = '';
  o += layerLink(w, 'custom-layer-link');
  const x0 = 30, dw = w - 60, dy = 120, dh = 330;
  o += txt(x0, 74, dw * 0.7, 'NO SAMPLE', { size: 12, align: 'left', id: 'custom-name', style: 'overflow:hidden;text-overflow:ellipsis;' });
  o += txt(x0 + dw * 0.5, 80, dw * 0.5, '', { size: 8, align: 'right', color: INK2, weight: 600, id: 'custom-meta' });
  o += '<div id="custom-drop" title="Drop an audio file, or click to browse" style="position:absolute;left:' + x0 + 'px;top:' + dy + 'px;width:' + dw + 'px;height:' + dh + 'px;'
    + 'border-radius:10px;background:rgba(0,0,0,.28);box-shadow:inset 0 0 0 1.5px var(--rule);cursor:pointer;touch-action:none;z-index:5;">'
    + '<canvas id="custom-canvas" width="' + dw * 2 + '" height="' + dh * 2 + '" style="position:absolute;inset:0;width:100%;height:100%;"></canvas>'
    + '<div id="custom-hint" style="position:absolute;inset:0;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:14px;pointer-events:none;'
    + 'font-family:' + FONT + ';font-stretch:75%;font-weight:700;text-transform:uppercase;letter-spacing:.08em;color:' + INK2 + ';">'
    + '<div style="font-size:30px;color:' + INK + '">DROP AN AUDIO FILE HERE</div><div style="font-size:16px">WAV · AIFF · FLAC · OGG · MP3 · UP TO 30 S · OR CLICK TO BROWSE</div></div>'
    + '<input id="custom-file" type="file" accept=".wav,.aif,.aiff,.flac,.ogg,.mp3,audio/*" hidden></div>';
  // controls: one line in four groups
  const cy = dy + dh + 44, kd = 76, ly = cy + kd + 14;
  const vals = (x, id) => txt(x - 40, ly + 26, kd + 80, '', { size: 9, color: INK, id });
  o += sect(x0, cy - 30, 190, 'PLAY', { align: 'left' });
  o += button({ x: x0 + 8, y: cy + 14, w: 44, label: 'ON', id: 'dds1.smpOn', layered: true, domId: 'custom-on', ly, title: 'DDS 1 plays the sample instead of its waveform' });
  o += button({ x: x0 + 76, y: cy + 14, w: 44, label: 'ONE SHOT', id: 'dds1.smpLoop', layered: true, domId: 'custom-mode__0', value: 0, steps: 2, ly, title: 'Play START to END once per note' });
  o += button({ x: x0 + 144, y: cy + 14, w: 44, label: 'LOOP', id: 'dds1.smpLoop', layered: true, domId: 'custom-mode__1', value: 1, steps: 2, ly, title: 'Repeat START to END while the note sounds' });
  const rx = x0 + 250;
  o += sect(rx, cy - 30, 370, 'REGION', { align: 'left' });
  o += knob({ x: rx + 10, y: cy, d: kd, v: 'cream', val: 0, ticks: 11, label: 'START', ly, id: 'dds1.smpStart', layered: true, domId: 'custom-start', title: 'Where each note starts playing' }) + vals(rx + 10, 'custom-start-val');
  o += knob({ x: rx + 140, y: cy, d: kd, v: 'cream', val: 0, ticks: 11, label: 'LOOP START', ly, id: 'dds1.smpLoopStart', layered: true, domId: 'custom-loop', title: 'LOOP jumps back here from END (START plays once)' }) + vals(rx + 140, 'custom-loop-val');
  o += knob({ x: rx + 270, y: cy, d: kd, v: 'cream', val: 1, ticks: 11, label: 'END', ly, id: 'dds1.smpEnd', layered: true, domId: 'custom-end' }) + vals(rx + 270, 'custom-end-val');
  const px = rx + 430;
  o += sect(px, cy - 30, 360, 'PITCH', { align: 'left' });
  o += knob({ x: px + 10, y: cy, d: kd, v: 'cream', val: 60 / 127, ticks: 11, label: 'ROOT KEY', ly, id: 'dds1.smpRoot', layered: true, domId: 'custom-root', hitType: 'combo', hitData: { steps: 128 },
    title: 'The note the sample was recorded at: that key plays it at its own pitch' }) + vals(px + 10, 'custom-root-val');
  o += keyButton(px + 116, cy + 2, 44, '◀', 'customRootDown', { title: 'Root key down a semitone' });
  o += keyButton(px + 166, cy + 2, 44, '▶', 'customRootUp', { title: 'Root key up a semitone' });
  o += keyButton(px + 116, cy + 54, 94, 'LEARN', 'customLearn', { domId: 'act-customLearn', title: 'The next key you play becomes the root key' });
  o += knob({ x: px + 250, y: cy, d: kd, v: 'cream', val: 0.5, ticks: 11, label: 'FINE', ly, id: 'dds1.smpFine', layered: true, domId: 'custom-fine', title: 'Fine tune, -100 to +100 cents' }) + vals(px + 250, 'custom-fine-val');
  const lx = px + 400;
  o += sect(lx, cy - 30, 110, 'OUTPUT', { align: 'left' });
  o += knob({ x: lx + 16, y: cy, d: kd, v: 'cream', val: 0.8, ticks: 11, label: 'LEVEL', ly, id: 'dds1.smpLevel', layered: true, domId: 'custom-level', title: 'Sample volume (0 dB at 8, up to +4 dB)' }) + vals(lx + 16, 'custom-level-val');
  const fx2 = lx + 150;
  o += sect(fx2, cy - 30, w - 30 - fx2, 'FILE', { align: 'left' });
  o += keyButton(fx2, cy + 2, 170, 'LOAD…', 'customBrowse', { title: 'Choose an audio file' });
  o += keyButton(fx2, cy + 54, 170, 'CLEAR', 'customClear', { title: 'Remove the sample from this layer' });
  o += txt(fx2 + 190, cy + 6, w - 30 - fx2 - 190, '', { size: 7.4, align: 'left', color: INK2, weight: 600, id: 'custom-status', style: 'white-space:normal;line-height:1.3;' });
  o += hrule(30, h - 60, w - 60, 'var(--rule)', 1);
  o += txt(30, h - 44, w - 60, 'ROOT KEY = THE NOTE THE SAMPLE WAS RECORDED AT  ·  DRAG THE HANDLES: START PLAYS ONCE, LOOP REPEATS LOOP START → END  ·  BINAURAL PLAYS A STEREO FILE LEFT / RIGHT  ·  SUPER, FILTER, ENVELOPES AND FX STILL APPLY', { size: 6.6, align: 'left', color: INK2, weight: 600 });
  return o;
}

// Patch browser, laid out as three numbered steps for new users:
// 1 choose a folder  ·  2 click a sound to load it  ·  3 save your own sound.
// Patch browser, after the user's reference (Arturia's Explore): search, TYPES / BANKS filters and
// a sortable table of ♥ / NAME / TYPE / BANK. A bank IS a folder inside the patch folder, so a new
// bank name makes a folder, and a folder dropped in by hand shows up as a bank.
const PB_W = 2120, PB_H = 940;
function patchPage(w, h) {
  const inputCss = 'box-sizing:border-box;border:1.5px solid var(--rule);border-radius:8px;background:var(--field);color:' + INK
    + ';font:700 19px ' + FONT + ';font-stretch:75%;letter-spacing:.04em;outline:none;z-index:5;padding:6px 14px;';
  const pill = (x, y, wd, label, action, opt) => keyButton(x, y, wd, label, action, Object.assign({ link: true, cls: 'pill', size: 8.6, h: 40 }, opt || {}));
  const chip = (x, y, wd, id, label, title) => '<div class="pb-chip" id="' + id + '" title="' + esc(title || '') + '" style="position:absolute;left:' + x + 'px;top:' + y + 'px;width:' + wd + 'px;height:40px;'
    + 'line-height:40px;text-align:center;border:1.5px solid var(--rule);border-radius:20px;cursor:pointer;z-index:6;'
    + 'font:700 17px ' + FONT + ';font-stretch:75%;letter-spacing:.06em;color:var(--ink)">' + label + '</div>';
  const listW = w - 430, rx = w - 380, rw = 350;
  let o = '';

  // ---- title, search
  o += txt(30, 58, 300, 'EXPLORE', { size: 17, align: 'left', ls: 0.02 });
  o += '<input id="patch-search" placeholder="Search patches" spellcheck="false" style="position:absolute;left:250px;top:56px;width:' + (listW - 250) + 'px;height:46px;' + inputCss + '">';
  o += '<div id="patch-clear" class="link" style="position:absolute;left:' + (listW - 150) + 'px;top:56px;width:130px;height:46px;line-height:46px;text-align:right;padding-right:14px;cursor:pointer;z-index:7;font:700 15px ' + FONT + ';font-stretch:75%;letter-spacing:.06em">CLEAR ALL</div>';

  // ---- filter chips and the count
  o += chip(30, 122, 150, 'patch-f-type', 'TYPES', 'Filter by type');
  o += chip(196, 122, 150, 'patch-f-bank', 'BANKS', 'Filter by bank');
  o += chip(362, 122, 190, 'patch-f-fav', '♥  FAVOURITES', 'Show only favourites');
  o += txt(listW - 320, 132, 320, '', { size: 8.4, align: 'right', color: INK2, weight: 600, id: 'patch-count' });

  // ---- table
  const hy = 186, ly = 224;
  const head = (x, wd, label, key, align) => '<div class="pb-head" data-sort="' + key + '" style="position:absolute;left:' + x + 'px;top:' + hy + 'px;width:' + wd + 'px;height:34px;line-height:34px;cursor:pointer;z-index:6;'
    + 'text-align:' + (align || 'left') + ';font:700 17px ' + FONT + ';font-stretch:75%;letter-spacing:.08em;color:var(--ink)">' + label + '<span class="pb-arrow"></span></div>';
  o += head(30, 46, '♥', 'fav', 'center') + head(92, 520, 'NAME', 'name') + head(628, 360, 'TYPE', 'type') + head (1004, 360, 'BANK', 'bank');
  o += hrule(30, hy + 40, listW - 30, 'var(--rule)', 1);
  o += '<div id="patch-list" style="position:absolute;left:24px;top:' + ly + 'px;width:' + (listW - 18) + 'px;height:' + (h - ly - 40) + 'px;overflow-y:auto;overflow-x:hidden;z-index:5;"></div>';
  o += vrule(w - 410, 56, h - 96);

  // ---- save panel
  o += txt(rx, 58, rw, 'SAVE THIS SOUND', { size: 9.4, align: 'left' });
  const field = (y, id, label, placeholder, menuId, menuTitle) =>
    txt(rx, y, rw, label, { size: 7, align: 'left', color: INK2, weight: 600 })
    + '<input id="' + id + '" placeholder="' + placeholder + '" spellcheck="false" maxlength="40" style="position:absolute;left:' + rx + 'px;top:' + (y + 22) + 'px;width:' + (rw - 52) + 'px;height:44px;' + inputCss + '">'
    + '<div class="pb-menu-btn" id="' + menuId + '" title="' + menuTitle + '" style="position:absolute;left:' + (rx + rw - 44) + 'px;top:' + (y + 22) + 'px;width:44px;height:44px;line-height:44px;text-align:center;'
    + 'border:1.5px solid var(--rule);border-radius:8px;cursor:pointer;z-index:6;font:700 20px ' + FONT + ';color:var(--ink)">▾</div>';
  o += field(104, 'patch-name-input', 'NAME', 'Patch name', 'patch-name-menu', 'Patches already here');
  o += field(190, 'patch-type-input', 'TYPE', 'e.g. LEAD, PAD, or your own', 'patch-type-menu', 'Pick a type');
  o += field(276, 'patch-bank-input', 'BANK', 'Folder in Patches, or a new name', 'patch-bank-menu', 'Pick a bank');
  o += pill(rx, 366, rw, 'SAVE', 'patchSave', { cls: 'pill primary', size: 9.4, domId: 'act-patchSave2', title: 'Save into this bank under this name' });
  o += pill(rx, 418, rw, 'SAVE A COPY AS…', 'patchSaveAs', { title: 'Choose where to save and under what name' });
  o += hrule(rx, 480, rw, 'var(--rule)', 1);
  o += txt(rx, 494, rw, 'MORE', { size: 7, align: 'left', color: INK2, weight: 600 });
  o += pill(rx, 518, (rw - 12) / 2, 'OPEN A FILE…', 'patchOpen', { title: 'Load any .gpatch file' });
  o += pill(rx + (rw + 12) / 2, 518, (rw - 12) / 2, 'REFRESH', 'patchRefresh', { title: 'Re-read the folder' });
  o += pill(rx, 570, (rw - 12) / 2, 'INIT UPPER', 'initUpper', { title: 'Reset the upper layer to the init sound' });
  o += pill(rx + (rw + 12) / 2, 570, (rw - 12) / 2, 'INIT LOWER', 'initLower', { title: 'Reset the lower layer to the init sound' });
  o += pill(rx, 622, (rw - 12) / 2, 'MY PATCHES', 'patchFolderUser', { title: 'Documents / 002 / Patches' });
  o += pill(rx + (rw + 12) / 2, 622, (rw - 12) / 2, 'OTHER FOLDER…', 'patchFolderChoose', { title: 'Pick any folder that has .gpatch files' });
  o += txt(rx, 690, rw, 'FOLDER', { size: 7, align: 'left', color: INK2, weight: 600 });
  o += '<div id="patch-folder-path" style="position:absolute;left:' + rx + 'px;top:712px;width:' + rw + 'px;height:56px;color:var(--ink);font:600 13px/1.35 ' + FONT + ';font-stretch:75%;letter-spacing:.02em;overflow:hidden;word-break:break-all;"></div>';
  o += '<div id="patch-status" style="position:absolute;left:' + rx + 'px;top:' + (h - 120) + 'px;width:' + rw + 'px;height:80px;color:var(--ink);font:600 13px/1.35 ' + FONT + ';font-stretch:75%;letter-spacing:.02em;overflow:hidden;word-break:break-all;"></div>';
  return o;
}

function settingsPage(w, h) {
  let o = '';
  const col = (x, title) => { o += sect(x, 70, 300, title, { align: 'left' }); };
  col(30, 'TUNING');
  o += knob({ x: 50, y: 120, d: 60, v: 'cream', val: 0.5, ticks: 11, label: 'FINE TUNE', ly: 200, id: 'global.fineTune', minmax: ['-100', '+100'] });
  o += knob({ x: 190, y: 120, d: 60, v: 'cream', val: 0.5, ticks: 25, label: 'TRANSPOSE', ly: 200, id: 'global.transpose', minmax: ['-12', '+12'] });
  col(370, 'KEYBOARD');
  o += knob({ x: 390, y: 120, d: 60, v: 'cream', val: 0.47, ticks: 11, label: 'SPLIT POINT', ly: 200, id: 'perf.splitPoint', minmax: ['C-2', 'G8'] });
  o += txt(360, 226, 130, '', { size: 7.4, color: INK2, weight: 600, id: 'split-readout' });
  o += keyButton(500, 130, 110, 'LEARN', 'splitLearn', { title: 'Next key played sets the split point' });
  col(660, 'MIDI');
  o += knob({ x: 680, y: 120, d: 60, v: 'cream', val: 0, ticks: 16, label: 'CHANNEL', ly: 200, id: 'global.midiChannel', minmax: ['1', '16'] });
  o += txt(650, 226, 130, '', { size: 7.4, color: INK2, weight: 600, id: 'midich-readout' });
  o += button({ x: 800, y: 132, w: 44, label: 'CC RX', id: 'global.ccRx', ly: 176 });
  o += button({ x: 870, y: 132, w: 44, label: 'HOST CLOCK', id: 'global.clockRx', ly: 176, title: 'Arp / seq follow the host tempo and transport' });
  o += txt(650, 250, 320, 'LOWER LAYER = CHANNEL + 1', { size: 6.6, align: 'left', color: INK2, weight: 600 });
  col(1000, 'ENGINE');
  o += txt(1000, 110, 300, 'OVERSAMPLING  2×', { size: 8, align: 'left' });
  o += txt(1000, 134, 300, 'BINAURAL VOICES  10 / 5 PER LAYER', { size: 7, align: 'left', color: INK2, weight: 600 });
  o += keyButton(1000, 160, 150, 'PANIC', 'panic', { title: 'All notes off' });
  o += keyButton(1160, 160, 150, 'RESET ALL', 'resetAll', { title: 'Both layers to init, performance to single upper' });
  col(1340, 'DISPLAY');
  o += keyButton(1340, 130, 210, 'DESKTOP LAYOUT', 'toggleDesktopLayout', { domId: 'act-desktop-layout', title: 'Larger controls for laptop screens: one engine at a time, no keyboard' });
  o += txt(1340, 200, 210, 'OFF', { size: 7.4, color: INK2, weight: 600, id: 'desktop-layout-state' });
  o += txt(1340, 226, 210, 'FULL PANEL WITH KEYBOARD', { size: 6.2, color: INK2, weight: 600, id: 'desktop-layout-detail' });
  col(1600, 'THEME');
  o += keyButton(1600, 120, 170, 'GEMINI', 'themeGemini', { domId: 'act-theme-gemini', title: 'The original hardware colours (default)' });
  o += keyButton(1600, 172, 170, 'SUPER SIX', 'themeSuper6', { domId: 'act-theme-super6', title: 'Blue and brown' });
  o += hrule(30, 300, w - 60, 'var(--rule)', 1);
  o += txt(30, 316, w - 60, 'SHIFT (KEY OR PANEL) REVEALS SECONDARY FUNCTIONS  ·  CTRL-DRAG = FINE  ·  DOUBLE-CLICK = DEFAULT  ·  RIGHT-CLICK A CONTROL = ROUTE MODULATION TO IT  ·  WHEEL STEPS', { size: 6.8, align: 'left', color: INK2, weight: 600 });
  return o;
}

// FX rack page (docs/fx/FX_PROMPTS.md): routing column, three slot panels after the user's
// reference, a layer fader under each slot, and the grouped type picker.
// Knobs, faders and power keys are the photographed assets; the display is a screen (canvas).
const FXW = 3000, FXH = 1040;
function fxPage(w, h) {
  let o = '';
  const S = 'stroke:var(--ink);fill:none;stroke-width:2';
  // ---- routing column
  const cx = 40, cw = 250;
  o += sect(cx, 70, cw, 'ROUTING', { align: 'left', size: 9.4 });
  o += keyButton(cx, 112, 120, 'SERIAL', 'fxSerial', { link: true, cls: 'pill', size: 8.6, h: 40, title: 'FX 1 feeds FX 2 feeds FX 3' });
  o += keyButton(cx + 130, 112, 120, 'PARALLEL', 'fxParallel', { link: true, cls: 'pill', size: 8.6, h: 40, title: 'Each slot hears the dry sound; the results are mixed' });
  const bx = cx + 25, bw = 200, bh = 70, ys = [190, 300, 410, 520, 640];
  const box = (y, hh, top, nameId, id) => '<div' + (id ? ' id="' + id + '"' : '') + ' class="fx-route" style="position:absolute;left:' + bx + 'px;top:' + y + 'px;width:' + bw + 'px;height:' + hh + 'px;box-sizing:border-box;'
    + 'border:1.5px solid var(--ink);border-radius:8px;display:flex;flex-direction:column;align-items:center;justify-content:center;pointer-events:none;'
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

  // ---- type picker, opened from a slot's name
  const rows = [['TIME'], ['DISTORTION'], ['DYNAMICS', 'MODULATION'], ['FILTER / EQ', 'EXPERIMENTAL']];
  const tw = 196, th = 58, tg = 16;
  const inGroup = (g) => FX.map((f, i) => ({ f, i })).filter((e) => e.f.group === g);
  const rowW = (r) => r.reduce((a, g) => a + inGroup(g).length * (tw + tg) - tg, 0) + (r.length - 1) * tg * 3;
  const pw = Math.max(...rows.map(rowW)) + 48, ph2 = 66 + th + 20 + rows.length * 112 + 10;
  let p = '<div id="fx-picker" hidden style="position:absolute;left:' + r1((w - pw) / 2) + 'px;top:' + r1((h - ph2) / 2) + 'px;width:' + pw + 'px;height:' + ph2 + 'px;z-index:30;border-radius:8px;'
    + 'background:var(--base);box-shadow:0 18px 50px rgba(0,0,0,.55),inset 0 0 0 1.5px var(--rule)">';
  p += txt(0, 20, pw, 'FX 1 TYPE', { size: 11, id: 'fx-picker-title', ls: 0.1 });
  p += '<div data-fx-close title="Close" style="position:absolute;right:14px;top:14px;width:34px;height:30px;cursor:pointer;z-index:6;font:700 26px/30px ' + FONT + ';color:var(--ink);text-align:center">×</div>';
  const tile = (x, y, val, label) => '<div class="fx-tile" data-fx-tile="' + val + '" style="position:absolute;left:' + r1(x) + 'px;top:' + r1(y) + 'px;width:' + tw + 'px;height:' + th + 'px;border-radius:6px;cursor:pointer;'
    + 'display:flex;align-items:center;justify-content:center;font:700 17px ' + FONT + ';font-stretch:75%;letter-spacing:.05em;color:var(--ink);white-space:nowrap">' + label + '</div>';
  p += tile(24, 66, 0, 'NONE');
  let y = 66 + th + 20;
  rows.forEach((row) => {
    let x = 24;
    row.forEach((g) => {
      const list = inGroup(g), gw = list.length * (tw + tg) - tg;
      p += '<div style="position:absolute;left:' + r1(x + 24) + 'px;top:' + (y + 10) + 'px;width:' + r1(gw - 48) + 'px;height:8px;border:1.5px solid var(--rule);border-bottom:none;pointer-events:none"></div>';
      p += '<div style="position:absolute;left:' + r1(x) + 'px;top:' + y + 'px;width:' + r1(gw) + 'px;text-align:center;pointer-events:none"><span style="background:var(--base);padding:0 12px;font:700 15px ' + FONT + ';font-stretch:75%;letter-spacing:.1em;color:var(--ink2)">' + g + '</span></div>';
      list.forEach((e, k) => { p += tile(x + k * (tw + tg), y + 30, e.i + 1, e.f.name); });
      x += gw + tg * 3;
    });
    y += 112;
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
body += '<div class="fp-wrap">' + hrule(PR - 470, BOTTOM - 2, 470, 'var(--rule)') + vrule(KBX - 24, BOTTOM + 4, H - BOTTOM - 8) + ribbonRow(RIBY, KBX, RIBW) + wordmark(PR - 470, RIBY - 18) + '</div>';

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
body += pop('pop-custom', 'CUSTOM WAVE', Math.round((W - CU_W) / 2), Math.round((H - CU_H) / 2) - 40, CU_W, CU_H, customPage(CU_W, CU_H), { subtitle: 'DDS 1 · DROP A SAMPLE · PLAY IT ACROSS THE KEYBOARD' });
body += pop('pop-patches', 'PATCH BROWSER', Math.round((W - PB_W) / 2), Math.round((H - PB_H) / 2) - 20, PB_W, PB_H, patchPage(PB_W, PB_H), { subtitle: 'LOAD A SOUND · SAVE INTO A BANK' });
body += pop('pop-settings', 'SETTINGS', popX + 200, popY + 200, popW - 400, 380, settingsPage(popW - 400, 380));
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
body = '<div class="dk-only">' + dkDecor + '</div>' + body;
console.log('desktop ' + DESKTOP_W + 'x' + DESKTOP_H);

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
          'pop-upper': '#E7E2DA', 'pop-lower': '#F65A27' },   // the layers' cap colours: cream UPPER, orange LOWER
  // palette: base #568EA3, accent #826251, light #FFE8D1, text #FFFFFF, lines #68C3D4
  super6: { base: '#568EA3', ink: '#FFFFFF', ink2: '#FFE8D1', opt: '#68C3D4', accent: '#826251', inset: '#FFE8D1', oink: '#568EA3', oink2: '#68C3D4',
          rule: '#68C3D4', otick: '#568EA3', badge: '#568EA3', 'badge-ink': '#FFFFFF', hi: '#FFFFFF', field: 'rgba(0,0,0,.14)', shade: '.12',
          sel: '#FFFFFF', osel: '#826251', link: '#274957', pressed: '#FFFFFF',
          // pop-overs: the theme, darker - deep blue, white ink, cream secondary, the line colour as accent
          'pop-bg': '#2F5A6B', 'pop-bg2': '#18323D', 'pop-ink': '#FFFFFF', 'pop-ink2': '#D9C9B6', 'pop-accent': '#68C3D4',
          'pop-rule': 'rgba(104,195,212,.3)', 'pop-link': '#FFE8D1', 'pop-key': '#244654',
          'pop-upper': '#FFE8D1', 'pop-lower': '#C08A6C',
          cap1: '#FFE8D1', cap2: '#826251', capDark: '#826251', insetTex: 'hide' }
};
const THEME_KEYS = ['gemini', 'super6'];
const CSS_KEYS = ['base', 'ink', 'ink2', 'opt', 'accent', 'inset', 'oink', 'oink2', 'rule', 'otick', 'badge', 'badge-ink', 'hi', 'field', 'shade', 'sel', 'osel', 'link', 'pressed', 'pop-bg', 'pop-bg2', 'pop-ink', 'pop-ink2', 'pop-accent', 'pop-rule', 'pop-link', 'pop-key', 'pop-upper', 'pop-lower'];
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
const flatFilter = (id, L, hex) => {
  const [r, gg, b] = rgb(hex);
  return '<filter id="' + id + '" color-interpolation-filters="sRGB"><feColorMatrix type="matrix" values="'
    + (r / L).toFixed(4) + ' 0 0 0 0 0 ' + (gg / L).toFixed(4) + ' 0 0 0 0 0 ' + (b / L).toFixed(4) + ' 0 0 0 0 0 1 0"/></filter>';
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
    + flatFilter('t-inset-' + k, 0.259, P.inset);    // dark inset texture
  }).join('') + '</defs></svg>';
const TINT_CSS = TINTED.map((k) => [
  ['[src*="cream"],[src*="fader-grey"]', 't-light-'],
  ['[src*="button-light"]', 't-key-'],
  ['[src*="orange"]', 't-lower-'],
  ['[src*="-dark"]', 't-dark-']
].map(([sel, f]) => sel.split(',').map((s) => 'body.theme-' + k + ' img' + s).join(',') + '{filter:url(#' + f + k + ')}').join('\n')
  + '\nbody.theme-' + k + ' .tex{filter:url(#t-base-' + k + ')}'
  // a light inset is a solid fill: the dark texture cannot be lifted to near-white without clipping
  + '\nbody.theme-' + k + ' .tex-dark{' + (THEMES[k].insetTex === 'hide' ? 'display:none' : 'filter:url(#t-inset-' + k + ')') + '}'
).join('\n');

const html = `<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <title>002 by SPKR</title>
  <style>
    /* Bahnschrift is a Windows face; macOS has no condensed face with its metrics, so the
       panel carries one (Roboto Condensed, SIL OFL - ui/FONT-LICENSE.txt). Windows still
       picks Bahnschrift first, so the panel there is unchanged. */
    @font-face { font-family: 'SPKR Condensed'; src: url(spkr-condensed-600.woff2) format('woff2'); font-weight: 600; font-display: block; }
    @font-face { font-family: 'SPKR Condensed'; src: url(spkr-condensed-700.woff2) format('woff2'); font-weight: 700; font-display: block; }
    html, body { margin: 0; background: #1c1d1f; }
    body { ${themeVars('gemini')}; }
    body.theme-super6 { ${themeVars('super6')}; }
    /* text-only commands: the clickable word is the control */
    body { --barlink: var(--oink); }
    body.desktop-layout [data-dk^="bar-"] { --barlink: var(--link); }
    .link { color: var(--link); border-radius: 3px; transition: color .1s, background .1s; }
    .link:hover { background: rgba(0,0,0,.08); }
    .link.pill { border: 1.5px solid currentColor; border-radius: 20px; box-sizing: border-box; }
    .link.pill.primary { background: var(--accent); border-color: var(--accent); color: #fff !important; }
    .link.pill.primary:hover { filter: brightness(1.12); }
    .folder-row:hover { background: rgba(0,0,0,.07); }
    body:not(.arp-free) .arp-free-only, body.arp-free .arp-sync-only { display: none; }
    .link.pill[aria-pressed="true"] { background: var(--accent); border-color: var(--accent); color: #fff !important; text-decoration: none; }
    /* pop-over pages: dark flat look (charcoal, lowercase-feel, periwinkle; salmon = LOWER) */
    .pop .pop-panel { --base:var(--pop-bg2); --ink:var(--pop-ink); --ink2:var(--pop-ink2); --opt:var(--pop-ink2); --accent:var(--pop-accent); --inset:var(--pop-bg2);
      --oink:var(--pop-ink); --oink2:var(--pop-ink2); --rule:var(--pop-rule); --otick:var(--pop-ink2); --badge:var(--pop-key); --badge-ink:var(--pop-ink);
      --hi:var(--pop-ink); --field:rgba(0,0,0,.25); --shade:.2; --sel:var(--pop-accent); --osel:var(--pop-accent); --link:var(--pop-link); --pressed:var(--pop-ink);
      background:linear-gradient(180deg,var(--pop-bg),var(--pop-bg2)) !important; box-shadow:0 20px 60px rgba(0,0,0,.65),inset 0 0 0 1px rgba(255,255,255,.07) !important; }
    /* flat keys (tools/make-fx-controls.mjs): the fill comes from the theme */
    .pop img[src$="flat-btn-off.svg"] { background:var(--pop-key); border-radius:7px; }
    .pop img[src$="flat-btn-on.svg"] { background:var(--pop-accent); border-radius:7px; }
    .pop .pop-panel .tex, .pop .pop-panel .tex-dark { display:none; }
    /* FX page backdrop (user, 2026-09-18): one solid colour - the theme gradient's dark end, no grain;
       the effect windows on it keep their own gradients */
    #pop-fx .pop-panel { background:var(--pop-bg2) !important; }
    .pop .pop-panel .section-heading { text-transform:lowercase; letter-spacing:.04em; }
    .pop .fx-tile { background:rgba(255,255,255,.06); }
    .pop .fx-tile:hover { background:rgba(255,255,255,.12); }
    .pop .link.pill { border-color:rgba(255,255,255,.22); }
    #pop-fx.fx-mode-serial .fx-par, #pop-fx.fx-mode-parallel .fx-ser { display: none; }
    .fx-tile:hover { background: rgba(0,0,0,.08); }
    .fx-slot:hover, .fx-sel:hover { text-decoration: underline; text-underline-offset: 5px; }
    .fx-knob.off, .fx-sel:empty { display: none; }
    .fx-drop { position: absolute; inset: 0; display: none; align-items: center; justify-content: center; font: 700 22px Bahnschrift,sans-serif; font-stretch: 75%; letter-spacing: .12em; color: var(--accent); background: rgba(0,0,0,.72); border: 2px dashed var(--accent); border-radius: 4px; pointer-events: none; }
    .fx-screen.drag .fx-drop { display: flex; }
    .fx-tile { background: rgba(0,0,0,.12); }
    .fx-tile.sel { background: var(--accent); color: #fff !important; }
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
    /* patch browser: one row per patch, columns ♥ / NAME / TYPE / BANK */
    .patch-row { position: relative; display: flex; align-items: center; height: 44px; font-size: 17px; font-weight: 700; letter-spacing: .04em;
      color: var(--ink); cursor: pointer; border-bottom: 1px solid rgba(127,127,127,.18); }
    .patch-row:hover { background: rgba(127,127,127,.12); }
    .patch-row.sel { background: rgba(127,127,127,.14); }
    .patch-row.sel .pb-name, .patch-row.sel .pb-type, .patch-row.sel .pb-bank { color: var(--accent); }
    .patch-row > span { padding: 0 6px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    .pb-fav { width: 52px; text-align: center; color: rgba(127,127,127,.5); font-size: 19px; }
    .pb-fav.on { color: var(--accent); }
    .pb-name { width: 536px; }
    .pb-type { width: 376px; color: var(--ink2); font-weight: 600; }
    .pb-bank { flex: 1; color: var(--ink2); font-weight: 600; }
    .pb-head:hover, .pb-chip:hover, .pb-menu-btn:hover { background: rgba(127,127,127,.14); }
    .pb-chip.on { background: var(--accent); border-color: var(--accent); color: #fff; }
    .pb-arrow { font-size: 13px; opacity: .8; padding-left: 6px; }
    .pb-pop { position: absolute; z-index: 30; min-width: 220px; max-height: 420px; overflow-y: auto; padding: 6px 0; border-radius: 10px;
      background: var(--pop-bg); box-shadow: 0 16px 40px rgba(0,0,0,.6), inset 0 0 0 1px rgba(255,255,255,.12);
      font: 700 16px Bahnschrift, sans-serif; font-stretch: 75%; letter-spacing: .04em; color: var(--ink); }
    .pb-pop div { padding: 8px 16px; cursor: pointer; white-space: nowrap; }
    .pb-pop div:hover { background: rgba(127,127,127,.2); }
    .pb-pop div.on { color: var(--accent); }
    .patch-empty { padding: 20px 16px; font-size: 15px; font-weight: 600; color: ${INK2}; }
    #patch-list::-webkit-scrollbar { width: 10px; } #patch-list::-webkit-scrollbar-thumb { background: rgba(0,0,0,.25); border-radius: 5px; }
    .seq-sel { box-shadow: inset 0 0 0 3px ${ORANGE} !important; }
    .seq-off { opacity: .35; }
    .flag-on { background: ${ORANGE} !important; }
    .flag-on + div { color: #fff !important; }
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
<script id="fx-spec" type="application/json">${JSON.stringify({ FX, DIVS }).replace(/</g, '\\u003c')}</script>
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
