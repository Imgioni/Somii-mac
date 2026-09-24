import fs from 'node:fs';
import path from 'node:path';
import {
  A, INK, INK2, ORANGE, txt, txtInv, vrule, hrule, blackPanel,
  knob, rotary, fader, sw3, button, led, sect, presetBar
} from './lib.mjs';

const OUT = path.dirname(new URL(import.meta.url).pathname.replace(/^\/([A-Za-z]:)/, '$1'));

const W = 2240, H = 945;
const TOPBAR = 62;                      // host-style preset strip above the panel
const PL = 34, PR = 2206;              // panel inner left/right (inside the cheeks)

// one control centre-line and one label baseline per layer row, so knobs,
// faders and switches all sit on the same axis instead of floating
const CY = 98;                          // control centre, relative to row top
const LBL = 172;                        // label baseline
const SUB = 184;                        // inverse secondary-label baseline
const FH = 128;                         // fader travel
const SWD = 10;                         // fader slot width
const KD = 46;                          // standard knob
const SWH = 46;                         // three-position switch height

const kpad = (d) => Math.max(6, d * 0.18);
// visual footprints, used to centre each section's cluster
const wFader = SWD + 12;
const wKnob = (d) => d + 2 * kpad(d);
const wRotary = (d, lw) => d + 2 * (kpad(d) + 7 + (lw == null ? 18 : lw));
const wSw3 = (h) => h * 0.458 + 3 + 34;

// lay a row of items out centred inside a section
function pack(sec, items, gap) {
  const total = items.reduce((a, i) => a + i.w, 0) + gap * (items.length - 1);
  let cx = sec.x + (sec.w - total) / 2;
  let out = '';
  for (const it of items) { out += it.r(cx); cx += it.w + gap; }
  return out;
}

// ---------------------------------------------------------------- layer row
// x-map shared by both rows so UPPER and LOWER line up exactly, as on the hardware
const S = {
  lfo:  { x: 146,  w: 300 },
  mod:  { x: 456,  w: 320 },
  osc:  { x: 786,  w: 400 },
  mix:  { x: 1196, w: 76 },
  vcf:  { x: 1282, w: 270 },
  vca:  { x: 1562, w: 175 },
  env:  { x: 1747, w: 366 },
  dly:  { x: 2123, w: 84 }
};
// labels inside the black inset sub-panels
const ONBLACK = { lc: '#A8A6A1', tick: '#8A8884', labelColor: '#D8D6D0' };

function layerRow(y, upper) {
  let o = '';
  const ly = y + LBL, sy = y + SUB, cy = y + CY;
  const fy = cy - FH / 2;                      // fader top
  const ky = (d) => cy - d / 2;                // knob/switch top for a given size

  const F = (val, label, cap, sub, ex) => ({
    w: wFader + (ex || 0),
    r: (x) => fader({ x: x + (wFader + (ex || 0) - SWD) / 2, y: fy, h: FH, sw: SWD, cap: cap || 'grey', val: val, label: label, sub: sub, ly: ly, sy: sy })
  });
  const K = (d, v, val, label, sub, ticks) => ({
    w: wKnob(d),
    r: (x) => knob({ x: x + kpad(d), y: ky(d), d: d, v: v, val: val, ticks: ticks == null ? 11 : ticks, label: label, sub: sub, ly: ly, sy: sy })
  });
  const R = (d, sel, steps, label, lw, ss) => ({
    w: wRotary(d, lw),
    r: (x) => rotary({ x: x + (wRotary(d, lw) - d) / 2, y: ky(d), d: d, sel: sel, steps: steps, label: label, ly: ly, ssize: ss || 6.0 })
  });
  const SW = (pos, opts, label) => ({
    w: wSw3(SWH),
    r: (x) => sw3({ x: x, y: ky(SWH), h: SWH, pos: pos, opts: opts, label: label, ly: ly })
  });
  // two switches stacked in one column
  const SWCOL = (a, b) => ({
    w: wSw3(38),
    r: (x) => sw3({ x: x, y: cy - 48, h: 38, pos: a.pos, opts: a.opts, label: a.label, ly: cy - 6 })
           + sw3({ x: x, y: cy + 10, h: 38, pos: b.pos, opts: b.opts, label: b.label, ly: ly })
  });
  const GAP = (w) => ({ w: w, r: () => '' });

  // -- left column: master volume (upper) / detune (lower)
  if (upper) {
    o += knob({ x: 62, y: ky(60), d: 60, v: 'cream', val: 0.72, ticks: 11, label: 'MASTER VOLUME', lsize: 8.5, ly: ly });
  } else {
    o += knob({ x: 66, y: ky(52), d: 52, v: 'orange', val: 0.5, ticks: 11, label: 'DETUNE', lsize: 8.5, ly: ly, sub: 'PERF DETUNE', sy: sy });
  }
  o += txt(38, y + 4, 96, upper ? 'UPPER' : 'LOWER', { size: 12, align: 'left', ls: 0.14 });
  o += led(100, y + 5, upper, 8);

  // -- LFO 1
  let g = S.lfo;
  o += sect(g.x, y + 2, g.w, 'LFO 1');
  o += pack(g, [
    F(0.55, 'RATE'), F(0.12, 'DELAY', 'grey', null, 6), F(0.40, 'LR PHASE', 'orange', 'SPREAD', 16),
    GAP(4),
    R(38, 0, ["FREE", "ONCE", "RESET"], "MODE", 14),
    R(38, 1, ['TRI', 'SAW', 'S&H', 'SQR', 'HF', 'TRK'], 'WAVE', 12)
  ], 7);

  // -- DDS MODULATOR
  g = S.mod;
  o += sect(g.x, y + 2, g.w, 'DDS MODULATOR');
  o += pack(g, [
    SW(2, ['ON', '1/2', 'OFF'], 'SUPER'),
    GAP(2),
    F(0.30, 'DETUNE', 'grey', 'DRIFT', 10), F(0.00, 'PW', 'dark'), F(0.45, 'PWM / WAVE', 'orange', null, 20),
    GAP(4),
    F(0.18, 'LFO 1'), F(0.00, 'ENV 1'),
    GAP(4),
    F(0.10, 'CROSS MOD', 'orange', 'RING', 18),
    GAP(2),
    SWCOL({ pos: 1, opts: ['DDS 2', '1+2', 'DDS 1'], label: 'DEST' },
          { pos: 2, opts: ['MAN', 'LFO 1', 'ENV 1'], label: 'SRC' })
  ], 6);

  // -- OSCILLATORS (two black inset sub-panels)
  g = S.osc;
  o += sect(g.x, y + 2, g.w, 'OSCILLATORS');
  const IW = (g.w - 10) / 2, bt = y + 24, bh = 168, b2 = g.x + IW + 10;
  o += blackPanel(g.x, bt, IW, bh);
  o += blackPanel(b2, bt, IW, bh);
  o += txt(g.x + 8, bt + 6, 60, 'DDS 1', { size: 9, align: 'left', color: '#D8D6D0' });
  o += txt(b2 + 8, bt + 6, 60, 'DDS 2', { size: 9, align: 'left', color: '#D8D6D0' });
  const rd = 36, rcy = bt + 66, rly = bt + 108;
  const RB = (sel, steps, label) => ({
    w: wRotary(rd, 15),
    r: (x) => rotary(Object.assign({
      x: x + (wRotary(rd, 15) - rd) / 2, y: rcy - rd / 2, d: rd, sel: sel, steps: steps,
      label: label, ly: rly, lsize: 7.4, ssize: 5.6
    }, ONBLACK))
  });
  o += pack({ x: g.x, w: IW }, [RB(1, ['SIN', 'SAW', 'SQR', 'TRI', 'NSE', 'ALT'], 'WAVEFORM'),
                                RB(3, ["64'", "32'", "16'", "8'", "4'", "2'"], 'RANGE')], 4);
  o += pack({ x: b2, w: IW }, [RB(2, ['SIN', 'SAW', 'SQR', 'TRI', 'NSE', 'PLS'], 'WAVEFORM'),
                               RB(3, ['LFO', "32'", "16'", "8'", "4'", "2'"], 'RANGE')], 4);
  // lower shelf inside each inset
  const shy = bt + 126, shly = bt + 152;
  o += pack({ x: g.x, w: IW }, [
    { w: 34, r: (x) => button({ x: x + 4, y: shy, w: 26, on: true, led: false, label: 'ALT A', dark: true, lsize: 6.8, ly: shly }) },
    { w: 34, r: (x) => button({ x: x + 4, y: shy, w: 26, on: false, led: false, label: 'ALT B', dark: true, lsize: 6.8, ly: shly }) }
  ], 16);
  o += pack({ x: b2, w: IW }, [
    { w: wKnob(24), r: (x) => knob({ x: x + kpad(24), y: shy - 1, d: 24, v: 'cream', val: 0.5, ticks: 9, tick: '#8A8884', label: 'TUNE', lsize: 7, ly: shly, labelColor: '#B9B7B1' }) },
    { w: wSw3(26), r: (x) => sw3(Object.assign({ x: x, y: shy - 1, h: 26, pos: 0, opts: ['SYNC', 'RING', 'NORM'], label: 'MODE', lsize: 7, ly: shly, ssize: 5.6 }, ONBLACK)) },
    { w: wSw3(26), r: (x) => sw3(Object.assign({ x: x, y: shy - 1, h: 26, pos: 1, opts: ['SIN', 'SQR', 'OFF'], label: 'SUB', lsize: 7, ly: shly, ssize: 5.6 }, ONBLACK)) }
  ], 4);

  // -- MIXER
  g = S.mix;
  o += sect(g.x, y + 2, g.w, 'MIXER');
  o += pack(g, [K(50, 'orange', 0.5, 'MIX', 'PAN')], 0);
  o += txt(g.x - 4, cy + 30, 34, 'DDS 1', { size: 6.6, color: INK2, weight: 600 });
  o += txt(g.x + 46, cy + 30, 34, 'DDS 2', { size: 6.6, color: INK2, weight: 600 });

  // -- VCF
  g = S.vcf;
  o += sect(g.x, y + 2, g.w, 'VCF');
  o += pack(g, [
    SWCOL({ pos: 1, opts: ['2', '1', 'OFF'], label: 'DRIVE' },
          { pos: 2, opts: ['ON', '1/2', 'OFF'], label: 'KEYTRK' }),
    GAP(2),
    F(0.00, 'HPF'), F(0.62, 'LPF', 'orange'), F(0.34, 'RES', 'orange'),
    GAP(4),
    F(0.48, 'ENV'), F(0.14, 'LFO 1'), F(0.00, 'DDS 2', 'dark')
  ], 6);

  // -- VCA
  g = S.vca;
  o += sect(g.x, y + 2, g.w, 'VCA');
  o += pack(g, [
    SWCOL({ pos: 2, opts: ['ON', '1/2', 'OFF'], label: 'DYNAM' },
          { pos: 0, opts: ['ENV 2', '1+2', 'ENV 1'], label: 'ENV' }),
    GAP(2),
    F(0.78, 'ENV LVL', 'orange', null, 8), F(0.08, 'LFO 1'), F(0.00, 'AT')
  ], 6);

  // -- ENVELOPES (two black inset sub-panels)
  g = S.env;
  o += sect(g.x, y + 2, g.w, 'ENVELOPES');
  const EW = (g.w - 10) / 2, ebt = y + 24, ebh = 168, e2 = g.x + EW + 10;
  o += blackPanel(g.x, ebt, EW, ebh);
  o += blackPanel(e2, ebt, EW, ebh);
  o += txt(g.x + 8, ebt + 6, 40, 'ENV 1', { size: 9, align: 'left', color: '#D8D6D0' });
  o += txt(e2 + 8, ebt + 6, 40, 'ENV 2', { size: 9, align: 'left', color: '#D8D6D0' });
  const efy = ebt + 32, efh = 98, ely = ebt + 138;
  const EF = (v, lbl, cap, sub) => ({
    w: 22,
    r: (x) => fader({ x: x + 4, y: efy, h: efh, sw: 9, cap: cap || 'grey', val: v, scale: false, label: lbl, lsize: 8.4, ly: ely, labelColor: '#D8D6D0' })
           + (sub ? txtInv(x - 2, ebt + 148, 26, sub, { size: 6.2 }) : '')
  });
  const EKT = (pos, label) => ({
    w: wSw3(28),
    r: (x) => sw3(Object.assign({ x: x, y: efy + 6, h: 28, pos: pos, opts: ['ON', '1/2', 'OFF'], label: label, lsize: 6.8, ly: ely, ssize: 5.6 }, ONBLACK))
  });
  o += pack({ x: g.x, w: EW }, [
    EKT(2, 'KEYTRK'), GAP(2),
    EF(0.12, 'A', 'grey', 'AH'), EF(0.40, 'D', 'grey', 'DH'), EF(0.66, 'S'), EF(0.30, 'R', 'orange')
  ], 5);
  o += pack({ x: e2, w: EW }, [
    EKT(2, 'KEYTRK'), GAP(2),
    EF(0.05, 'A'), EF(0.52, 'D', 'grey', 'DH'), EF(0.74, 'S'), EF(0.36, 'R', 'orange')
  ], 5);

  // -- DLY send
  g = S.dly;
  o += sect(g.x, y + 2, g.w, 'DLY');
  o += pack(g, [F(0.22, 'SEND', 'orange')], 0);

  // section separators
  [S.mod.x, S.osc.x, S.mix.x, S.vcf.x, S.vca.x, S.env.x, S.dly.x]
    .forEach((vx) => { o += vrule(vx - 10, y + 4, 192); });
  o += vrule(S.lfo.x - 10, y + 4, 192);
  return o;
}

// ------------------------------------------------------------- global strip
function globalStrip(y) {
  let o = '';
  const T = y + 4, B = y + 22;
  const mk = (x, w, title) => { o += txt(x, T, w, title, { size: 9.5, align: 'left', ls: 0.1 }); };

  mk(44, 110, 'MANUAL');
  o += button({ x: 46, y: B + 12, w: 26, on: false, label: 'LOWER' });
  o += button({ x: 82, y: B + 12, w: 26, on: false, label: 'UPPER' });
  o += txtInv(44, B + 42, 66, 'INIT PATCH', { size: 6.6 });
  o += vrule(132, y + 4, 86);

  mk(146, 60, 'TEMPO');
  o += knob({ x: 152, y: B + 8, d: 32, v: 'cream', val: 0.42, ticks: 11, label: '', lsize: 7.4 });
  o += txt(140, B + 44, 56, '30 . . 300', { size: 6.4, color: INK2, weight: 600 });
  o += vrule(206, y + 4, 86);

  mk(220, 80, 'HOLD');
  o += button({ x: 222, y: B + 12, w: 26, on: false, label: 'LOWER' });
  o += button({ x: 258, y: B + 12, w: 26, on: false, label: 'UPPER' });
  o += vrule(300, y + 4, 86);

  mk(314, 120, 'KEYBOARD');
  o += button({ x: 316, y: B + 12, w: 26, on: true, label: 'SINGLE' });
  o += button({ x: 352, y: B + 12, w: 26, on: false, label: 'DUAL' });
  o += button({ x: 388, y: B + 12, w: 26, on: false, label: 'SPLIT' });
  o += vrule(430, y + 4, 86);

  // LAYER sits on a black inset on the hardware
  o += blackPanel(442, y + 6, 104, 82);
  o += txt(450, T + 4, 60, 'LAYER', { size: 9.5, align: 'left', color: '#D8D6D0', ls: 0.1 });
  o += button({ x: 458, y: B + 14, w: 26, on: false, label: 'LOWER', dark: true });
  o += button({ x: 500, y: B + 14, w: 26, on: true, label: 'UPPER', dark: true });

  mk(562, 180, 'VOICE ASSIGN');
  o += button({ x: 564, y: B + 12, w: 26, on: true, label: 'MODE' });
  ['SOLO', 'LEGATO', 'POLY 1', 'POLY 2'].forEach((s, i) => {
    o += led(600, B + 8 + i * 11, i === 2, 7);
    o += txt(611, B + 8 + i * 11, 44, s, { size: 6.4, align: 'left', color: INK2, weight: 600 });
  });
  o += button({ x: 664, y: B + 12, w: 26, on: false, label: 'UNISON' });
  o += button({ x: 700, y: B + 12, w: 26, on: true, label: 'BINAURAL' });
  o += vrule(744, y + 4, 86);

  mk(758, 320, 'ARPEGGIATOR / SEQUENCER');
  o += button({ x: 760, y: B + 14, w: 26, on: false, label: 'ON' });
  o += knob({ x: 798, y: B + 10, d: 30, v: 'cream', val: 0.5, ticks: 9, label: 'CLK DIV', lsize: 7.4 });
  o += button({ x: 846, y: B + 14, w: 26, on: false, label: 'SYNC' });
  o += button({ x: 882, y: B + 14, w: 26, on: false, label: 'RANGE' });
  o += button({ x: 918, y: B + 14, w: 26, on: false, label: 'MODE' });
  o += button({ x: 960, y: B + 14, w: 26, on: false, label: 'SEQ REC' });
  o += button({ x: 996, y: B + 14, w: 26, on: false, label: 'LOAD' });
  o += button({ x: 1032, y: B + 14, w: 26, on: false, label: 'STORE' });
  o += vrule(1074, y + 4, 86);

  // MOD ASSIGN matrix
  mk(1088, 120, 'MOD AMOUNT');
  o += knob({ x: 1092, y: B + 10, d: 32, v: 'orange', val: 0.5, ticks: 11, label: '', lsize: 7.4 });
  o += txt(1080, B + 46, 56, '-100 . . +100', { size: 6.2, color: INK2, weight: 600 });
  o += txt(1150, T, 120, 'MOD ASSIGN', { size: 9.5, align: 'left', ls: 0.1 });
  const srcs = ['DDS 2', 'LFO 2', 'ENV 1', 'VEL', 'AT', 'EXPR', 'RIBBON', 'NOTE'];
  const dsts = ['LFO 1 R', 'X MOD', 'WAVE', 'MIX', 'HPF', 'RES', 'ENV 1 D', 'DLY T'];
  const mx = 1150, step = 54;
  srcs.forEach((s, i) => {
    o += button({ x: mx + i * step, y: B + 14, w: 24, on: i === 1, label: s, lsize: 6.4 });
  });
  dsts.forEach((s, i) => {
    o += button({ x: mx + i * step, y: B + 48, w: 24, on: i === 4, label: s, lsize: 6.4, led: false });
  });
  o += txt(mx - 34, B + 18, 30, 'SRC', { size: 7, align: 'right', color: INK2 });
  o += txt(mx - 34, B + 52, 30, 'DEST', { size: 7, align: 'right', color: INK2 });
  o += vrule(1592, y + 4, 86);

  mk(1606, 90, 'CHORUS');
  o += button({ x: 1608, y: B + 14, w: 26, on: true, label: 'I' });
  o += button({ x: 1644, y: B + 14, w: 26, on: false, label: 'II' });
  o += vrule(1688, y + 4, 86);

  mk(1702, 130, 'DELAY');
  o += knob({ x: 1704, y: B + 10, d: 30, v: 'cream', val: 0.36, ticks: 11, label: 'TIME', lsize: 7.4 });
  o += knob({ x: 1756, y: B + 10, d: 30, v: 'cream', val: 0.44, ticks: 11, label: 'FEEDBACK', lsize: 7.4 });
  o += button({ x: 1806, y: B + 14, w: 26, on: false, label: 'FREEZE' });
  o += vrule(1848, y + 4, 86);

  // patch display + write controls, right end of the strip
  mk(1862, 150, 'PATCH');
  o += `<img src="${A.glass}" alt="" style="position:absolute;left:1862px;top:${B + 10}px;width:150px;height:40px;opacity:.92;">`;
  o += txt(1872, B + 18, 130, 'A1-3   BINAURAL BRASS', { size: 9, align: 'left', color: '#20242B', ls: 0.04 });
  o += txt(1872, B + 30, 130, 'PERF  A-1   SINGLE', { size: 7.2, align: 'left', color: '#4A5058', ls: 0.04 });
  o += button({ x: 2026, y: B + 14, w: 26, on: false, label: 'EDIT' });
  o += button({ x: 2062, y: B + 14, w: 26, on: false, label: 'SAVE' });
  o += button({ x: 2098, y: B + 14, w: 26, on: false, label: 'A / B' });
  o += button({ x: 2140, y: B + 14, w: 26, on: false, label: 'POWER' });
  return o;
}

// -------------------------------------------------------------- lower block
function lowerLeft(x, y) {
  const w = 346;
  const sec = { x: x, w: w };
  let o = '';

  // ---- PERFORMANCE band -----------------------------------------------
  o += sect(x, y, w, 'PERFORMANCE');
  const c1 = y + 64, l1 = y + 116;
  const octStrip = {
    w: 134,
    r: (bx) => {
      let s = txt(bx, c1 - 34, 134, 'OCTAVE', { size: 8.5 });
      [-2, -1, 0, 1, 2].forEach((n, i) => {
        s += led(bx + 6 + i * 26, c1 - 16, n === 0, 11);
        s += txt(bx - 6 + i * 26, c1 + 1, 34, (n > 0 ? '+' : '') + n, { size: 7.4, color: INK2, weight: 600 });
      });
      return s;
    }
  };
  o += pack(sec, [
    octStrip,
    { w: 8, r: () => '' },
    { w: wSw3(40), r: (bx) => sw3({ x: bx, y: c1 - 20, h: 40, pos: 1, opts: ['OCT +', '0', 'OCT -'], label: 'TRANSPOSE', ly: l1 }) },
    { w: 8, r: () => '' },
    { w: wFader, r: (bx) => fader({ x: bx + 4, y: c1 - 44, h: 88, sw: SWD, cap: 'grey', val: 0.2, label: 'PORTAMENTO', ly: l1 }) },
    { w: 10, r: () => '' },
    { w: wSw3(40), r: (bx) => sw3({ x: bx, y: c1 - 20, h: 40, pos: 2, opts: ['ON', 'LEG', 'OFF'], label: 'GLIDE', ly: l1 }) }
  ], 6);

  // ---- LFO 2 band -------------------------------------------------------
  const l2y = y + 140;
  o += sect(x, l2y, w, 'LFO 2');
  const c2 = l2y + 66, lb2 = l2y + 118;
  o += pack(sec, [
    { w: wRotary(34, 15), r: (bx) => rotary({ x: bx + (wRotary(34, 15) - 34) / 2, y: c2 - 17, d: 34, sel: 0, steps: ['SIN', 'SAW', 'S&H', 'SQR', 'TRI', 'NSE'], label: 'WAVE', ly: lb2, ssize: 5.8 }) },
    { w: wKnob(40), r: (bx) => knob({ x: bx + kpad(40), y: c2 - 20, d: 40, v: 'cream', val: 0.5, ticks: 11, label: 'RATE', ly: lb2 }) },
    { w: wKnob(40), r: (bx) => knob({ x: bx + kpad(40), y: c2 - 20, d: 40, v: 'cream', val: 0.12, ticks: 11, label: 'DELAY', ly: lb2 }) },
    { w: 6, r: () => '' },
    {
      w: wSw3(34),
      r: (bx) => sw3({ x: bx, y: c2 - 42, h: 34, pos: 1, opts: ['DDS 2', '1+2', 'DDS 1'], label: '', ly: 0 })
             + sw3({ x: bx, y: c2 + 4, h: 34, pos: 2, opts: ['BOTH', 'UPPER', 'LOWER'], label: 'DESTINATION', ly: lb2 })
    }
  ], 6);

  // ---- performance depth faders ----------------------------------------
  const l3y = y + 268;
  o += sect(x, l3y, w, 'DEPTH');
  const c3 = l3y + 70, lb3 = l3y + 126;
  const DF = (v, lbl, cap) => ({
    w: wFader,
    r: (bx) => fader({ x: bx + 4, y: c3 - 46, h: 92, sw: SWD, cap: cap || 'grey', val: v, label: lbl, ly: lb3 })
  });
  o += pack(sec, [
    DF(0.30, 'LFO 2 RATE'), DF(0.20, 'DDS'), DF(0.40, 'VCF'), DF(0.10, 'VCA'),
    { w: 14, r: (bx) => vrule(bx + 7, c3 - 52, 108) },
    DF(0.50, 'DDS', 'orange'), DF(0.60, 'VCF', 'orange'),
    { w: 8, r: () => '' },
    { w: wSw3(34), r: (bx) => sw3({ x: bx, y: c3 - 17, h: 34, pos: 2, opts: ['ON', 'AT+T', 'TRIG'], label: 'TRIG', ly: lb3 }) }
  ], 6);
  o += txt(x + 176, lb3 + 12, 120, 'BENDER AMOUNT', { size: 7.6 });
  return o;
}

// ------------------------------------------------------------------- panel
function panelChrome() {
  let o = '';
  o += `<img src="${A.cheek}" alt="" style="position:absolute;left:0;top:0;width:28px;height:${H}px;">`;
  o += `<img src="${A.cheek}" alt="" style="position:absolute;left:${W - 28}px;top:0;width:28px;height:${H}px;transform:scaleX(-1);">`;
  return o;
}

function ribbonRow(y) {
  let o = '';
  const rx = 410, rw = 1462;
  o += `<img src="${A.ribbon}" alt="" style="position:absolute;left:${rx}px;top:${y}px;width:${rw}px;height:52px;">`;
  // travel marks printed on the panel above the strip, as on the hardware
  for (let i = 1; i < 4; i++) {
    const mxp = rx + (rw * i / 4);
    o += `<div style="position:absolute;left:${mxp}px;top:${y - 7}px;width:1px;height:5px;background:${INK2};opacity:.7;"></div>`;
  }
  o += txt(rx, y + 56, 90, 'RIBBON', { size: 8, align: 'left' });

  // wordmark
  const wx = 1902;
  o += txt(wx, y + 6, 80, 'S P K R', { size: 20, align: 'left', ls: 0.02 });
  o += vrule(wx + 86, y + 4, 30, INK);
  o += txt(wx + 96, y + 4, 210, 'GEMINUS', { size: 26, align: 'left', ls: 0.01 });
  o += txt(wx + 98, y + 34, 300, '20 VOICE DUAL LAYER POLYPHONIC BINAURAL', { size: 7.2, align: 'left', color: INK2 });
  o += txt(wx + 98, y + 44, 300, 'ANALOG-HYBRID SYNTHESIZER', { size: 7.2, align: 'left', color: INK2 });
  return o;
}

// 5 full octaves (60 keys). A 61st top C isn't cuttable from the octave
// render: every white key in it has a black key overlapping one side.
function keyboard(x, y, w, h) {
  const oct = w / 5;
  let keys = '';
  for (let i = 0; i < 5; i++) {
    keys += `<img src="${A.octave}" alt="" style="position:absolute;left:${Math.round(oct * i * 10) / 10}px;top:0;`
      + `width:${Math.round(oct * 10) / 10}px;height:${h}px;">`;
  }
  return `<div style="position:absolute;left:${x}px;top:${y}px;width:${w}px;height:${h}px;overflow:hidden;">${keys}</div>`;
}

// ------------------------------------------------------------------ assemble

function page(title, w, h, body, previewW, previewH) {
  return `<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <script src="./support.js"></script>
</head>
<body>
<x-dc>
<helmet>
  <link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Barlow+Condensed:wght@500;600;700&display=swap">
  <style>
    body { margin: 0; font-family: 'Barlow Condensed','Arial Narrow',Arial,sans-serif; }
    a { color: #B4441C; } a:hover { color: #8A3314; }
    img { display: block; -webkit-user-drag: none; }
  </style>
</helmet>
<div style="position:relative;width:${w}px;height:${h}px;overflow:hidden;background:#E7E2DA url(${A.panel}) repeat;">
${body}
  <div style="position:absolute;inset:0;background:url(${A.shading}) no-repeat center/100% 100%;mix-blend-mode:multiply;opacity:.5;pointer-events:none;"></div>
  <div style="position:absolute;inset:0;box-shadow:inset 0 1px 0 rgba(255,255,255,.65),inset 0 -2px 6px rgba(0,0,0,.14);pointer-events:none;"></div>
</div>
</x-dc>
<script data-dc-script data-props='{"$preview":{"width":${previewW},"height":${previewH}}}'>
class Component extends DCLogic {}
</script>
</body>
</html>
`;
}

// ---- Main ------------------------------------------------------------------
let body = '';
body += panelChrome();
body += hrule(PL, 8, PR - PL, '#8E8F8D');
body += layerRow(12, true);
body += hrule(PL, 210, PR - PL, '#8E8F8D');
body += layerRow(216, false);
body += hrule(PL, 414, PR - PL, '#6F706E');
body += hrule(PL, 417, PR - PL, '#6F706E');
body += globalStrip(422);
body += hrule(PL, 530, PR - PL, '#8E8F8D');
body += lowerLeft(44, 538);
body += vrule(408, 536, 400);
body += ribbonRow(546);
body += keyboard(424, 616, 1778, 320);
const shifted = '<div style="position:absolute;left:0;top:' + TOPBAR + 'px;width:' + W + 'px;height:' + H + 'px;">' + body + '</div>';
const full = presetBar(PL, 10, PR - PL) + hrule(PL, TOPBAR - 4, PR - PL, '#8E8F8D') + shifted;
fs.writeFileSync(path.join(OUT, 'Main.dc.html'), page('Geminus', W, H + TOPBAR, full, W, H + TOPBAR));

// ---- LayerRow detail (1.5x) ------------------------------------------------
const DW = Math.round(2172 * 1.5) + 40, DH = 330;
let d = `<div style="position:absolute;left:20px;top:8px;width:2172px;height:200px;transform:scale(1.5);transform-origin:0 0;">`
  + layerRow(0, true).replace(/left:(-?[\d.]+)px/g, (m, n) => `left:${(+n) - 34}px`)
  + `</div>`;
fs.writeFileSync(path.join(OUT, 'LayerRow.dc.html'), page('Layer row', DW, DH, d, DW, DH));

// ---- Components sheet ------------------------------------------------------
let c = '';
c += txt(40, 26, 500, 'GEMINUS - PANEL COMPONENTS', { size: 18, align: 'left', ls: 0.08 });
c += txt(40, 48, 700, 'every part is a crop of the supplied renders. only ticks, text and rules are drawn.', { size: 9.5, align: 'left', color: INK2, ls: 0.03 });
c += hrule(40, 66, 1120);

c += txt(40, 84, 300, 'KNOB - ROTATION + PRINTED SCALE', { size: 10, align: 'left' });
[0, 0.25, 0.5, 0.75, 1].forEach((v, i) => {
  c += knob({ x: 48 + i * 92, y: 108, d: 46, v: 'cream', val: v, ticks: 11, label: Math.round(v * 10) + '' });
});
[0, 0.5, 1].forEach((v, i) => {
  c += knob({ x: 520 + i * 92, y: 108, d: 46, v: 'orange', val: v, ticks: 11, label: Math.round(v * 10) + '' });
});
[0, 0.5, 1].forEach((v, i) => {
  c += knob({ x: 820 + i * 92, y: 108, d: 46, v: 'dark', val: v, ticks: 11, label: Math.round(v * 10) + '' });
});

c += txt(40, 200, 300, 'STEPPED ROTARY', { size: 10, align: 'left' });
c += rotary({ x: 60, y: 234, d: 40, sel: 1, steps: ['SIN', 'SAW', 'SQR', 'TRI', 'NSE', 'ALT'], label: 'WAVEFORM' });
c += rotary({ x: 240, y: 234, d: 40, sel: 3, steps: ["64'", "32'", "16'", "8'", "4'", "2'"], label: 'RANGE' });

c += txt(440, 200, 300, 'FADER', { size: 10, align: 'left' });
[0, 0.35, 0.7, 1].forEach((v, i) => {
  c += fader({ x: 460 + i * 46, y: 228, h: 96, sw: 15, cap: i === 3 ? 'orange' : 'grey', val: v, label: (v * 10).toFixed(0) });
});
c += fader({ x: 660, y: 228, h: 96, sw: 15, cap: 'dark', val: 0.5, label: 'DARK' });

c += txt(740, 200, 300, '3-POSITION SWITCH', { size: 10, align: 'left' });
[0, 1, 2].forEach((p, i) => {
  c += sw3({ x: 760 + i * 76, y: 232, h: 34, pos: p, opts: ['ON', '1/2', 'OFF'], label: 'KEYTRACK' });
});

c += txt(40, 360, 300, 'BUTTON + LED', { size: 10, align: 'left' });
c += button({ x: 60, y: 400, w: 34, on: false, label: 'OFF' });
c += button({ x: 120, y: 400, w: 34, on: true, label: 'ON' });
c += txt(200, 360, 300, 'LED', { size: 10, align: 'left' });
c += led(214, 400, false, 14); c += led(244, 400, true, 14);

c += txt(320, 360, 300, 'BLACK INSET SUB-PANEL', { size: 10, align: 'left' });
c += blackPanel(340, 392, 200, 76);
c += txt(348, 398, 60, 'DDS 1', { size: 9, align: 'left', color: '#D8D6D0' });
c += rotary({ x: 366, y: 416, d: 30, sel: 2, steps: ['SIN', 'SAW', 'SQR', 'TRI', 'NSE', 'ALT'] });
c += rotary({ x: 466, y: 416, d: 30, sel: 3, steps: ["64'", "32'", "16'", "8'", "4'", "2'"] });

c += txt(580, 360, 400, 'SECONDARY (SHIFT) LABEL - PRINTED INVERSE', { size: 10, align: 'left' });
c += fader({ x: 606, y: 394, h: 70, sw: 15, cap: 'orange', val: 0.3, label: 'LR PHASE', sub: 'SPREAD' });
c += fader({ x: 676, y: 394, h: 70, sw: 15, cap: 'grey', val: 0.3, label: 'DETUNE', sub: 'DRIFT' });
c += knob({ x: 740, y: 398, d: 40, v: 'orange', val: 0.5, ticks: 11, label: 'MIX', sub: 'PAN' });

c += txt(880, 360, 300, 'RIBBON / CHEEK / KEYS', { size: 10, align: 'left' });
c += `<img src="${A.ribbon}" alt="" style="position:absolute;left:880px;top:396px;width:240px;height:26px;">`;
c += `<img src="${A.cheek}" alt="" style="position:absolute;left:880px;top:434px;width:18px;height:90px;">`;
c += `<img src="${A.octave}" alt="" style="position:absolute;left:912px;top:434px;width:208px;height:90px;">`;

c += hrule(40, 560, 1120);
c += txt(40, 574, 700, 'SOURCE SHEETS', { size: 10, align: 'left' });
c += txt(40, 592, 1100, 'knobs.png  -  magenta key pulled, three caps cut on even thirds, squared about the cap centre so rotation stays concentric', { size: 8.6, align: 'left', color: INK2, ls: 0.02 });
c += txt(40, 606, 1100, 'faders.png  -  grey / orange / dark caps at x274, x720, x1172 and the empty slot at x1630', { size: 8.6, align: 'left', color: INK2, ls: 0.02 });
c += txt(40, 620, 1100, 'buttons.png  -  body rows 288-645, LED rows 68-244, cut separately so the LED can sit above its button', { size: 8.6, align: 'left', color: INK2, ls: 0.02 });
c += txt(40, 634, 1100, 'octave.png  -  white-key pitch measured at 182.67px, cut to exactly one 7-key period so the 61-key bed tiles seamlessly', { size: 8.6, align: 'left', color: INK2, ls: 0.02 });
c += txt(40, 648, 1100, 'panel_surface.png  -  even-lit centre patch, mirrored 2x2 into a seamless tile', { size: 8.6, align: 'left', color: INK2, ls: 0.02 });
c += txt(40, 662, 1100, 'cheek.png  -  taken from docs/reference/assets/06_cheek.png (light); resources/cheek.png is the dark _gray variant', { size: 8.6, align: 'left', color: INK2, ls: 0.02 });

fs.writeFileSync(path.join(OUT, 'Components.dc.html'), page('Components', 1180, 700, c, 1180, 700));

// ---- canvas ----------------------------------------------------------------
const canvas = {
  artboards: [
    { file: 'Main.dc.html', x: 0, y: 0, w: W, h: H + TOPBAR, title: 'Geminus - front panel' },
    { file: 'LayerRow.dc.html', x: 0, y: H + 160, w: DW, h: DH, title: 'Layer row at 1.5x' },
    { file: 'Components.dc.html', x: W + 160, y: 0, w: 1180, h: 700, title: 'Component sheet' }
  ],
  launch: { view: 'canvas' }
};
fs.writeFileSync(path.join(OUT, 'canvas.json'), JSON.stringify(canvas, null, 2));

console.log('wrote Main.dc.html, LayerRow.dc.html, Components.dc.html, canvas.json');
console.log('Main', W + 'x' + H, ' LayerRow', DW + 'x' + DH, ' Components 1180x700');
