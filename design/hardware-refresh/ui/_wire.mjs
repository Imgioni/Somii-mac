import fs from 'node:fs';
let g = fs.readFileSync('gen.mjs', 'utf8');
const was = g;
let n = 0;
const sub = (a, b) => { if (!g.includes(a)) { console.log('MISS:', a.slice(0, 70)); return; } g = g.replace(a, b); n++; };

// ---- helpers gain an id ----------------------------------------------------
sub(`  const K = (d, v, val, label, sub, ticks) => ({
    w: wKnob(d),
    r: (x) => knob({ x: x + kpad(d), y: ky(d), d: d, v: v, val: val, ticks: ticks == null ? 11 : ticks, label: label, sub: sub, ly: ly, sy: sy })
  });`,
`  const K = (d, v, val, label, sub, ticks, id) => ({
    w: wKnob(d),
    r: (x) => knob({ x: x + kpad(d), y: ky(d), d: d, v: v, val: val, ticks: ticks == null ? 11 : ticks, label: label, sub: sub, ly: ly, sy: sy, id: id ? P(id) : null })
  });`);

sub(`  const R = (d, sel, steps, label, lw, ss) => ({
    w: wRotary(d, lw),
    r: (x) => rotary({ x: x + (wRotary(d, lw) - d) / 2, y: ky(d), d: d, sel: sel, steps: steps, label: label, ly: ly, ssize: ss || 6.0 })
  });`,
`  const R = (d, sel, steps, label, lw, ss, id) => ({
    w: wRotary(d, lw),
    r: (x) => rotary({ x: x + (wRotary(d, lw) - d) / 2, y: ky(d), d: d, sel: sel, steps: steps, label: label, ly: ly, ssize: ss || 6.0, id: id ? P(id) : null })
  });`);

sub(`  const SW = (pos, opts, label) => ({
    w: wSw3(SWH),
    r: (x) => sw3({ x: x, y: ky(SWH), h: SWH, pos: pos, opts: opts, label: label, ly: ly })
  });`,
`  const SW = (pos, opts, label, id) => ({
    w: wSw3(SWH),
    r: (x) => sw3({ x: x, y: ky(SWH), h: SWH, pos: pos, opts: opts, label: label, ly: ly, id: id ? P(id) : null })
  });`);

// ---- LFO 1 -----------------------------------------------------------------
sub(`    F(0.55, 'RATE'), F(0.12, 'DELAY', 'grey', null, 6), F(0.40, 'LR PHASE', 'orange', 'SPREAD', 16),
    GAP(4),
    R(38, 0, ["FREE", "ONCE", "RESET"], "MODE", 14),
    R(38, 1, ['TRI', 'SAW', 'S&H', 'SQR', 'HF', 'TRK'], 'WAVE', 12)`,
`    F(0.55, 'RATE', null, null, 0, 'lfo1.rate'),
    F(0.12, 'DELAY', 'grey', null, 6, 'lfo1.delay'),
    F(0.40, 'LR PHASE', 'orange', 'SPREAD', 16, 'lfo1.lrPhase'),
    GAP(4),
    R(38, 0, ["FREE", "ONCE", "RESET"], "MODE", 14, null, 'lfo1.mode'),
    R(38, 1, ['TRI', 'SAW', 'S&H', 'SQR', 'HF', 'TRK'], 'WAVE', 12, null, 'lfo1.wave')`);

// ---- DDS MODULATOR ---------------------------------------------------------
// PW had no parameter of its own; DRIFT is a real parameter that had no control,
// so that fader becomes DRIFT (manual p.62).
sub(`    SW(2, ['ON', '1/2', 'OFF'], 'SUPER'),
    GAP(2),
    F(0.30, 'DETUNE', 'grey', 'DRIFT', 10), F(0.00, 'PW', 'dark'), F(0.45, 'PWM / WAVE', 'orange', null, 20),
    GAP(4),
    F(0.18, 'LFO 1'), F(0.00, 'ENV 1'),
    GAP(4),
    F(0.10, 'CROSS MOD', 'orange', 'RING', 18),
    GAP(2),
    SWCOL({ pos: 1, opts: ['DDS 2', '1+2', 'DDS 1'], label: 'DEST' },
          { pos: 2, opts: ['MAN', 'LFO 1', 'ENV 1'], label: 'SRC' })`,
`    SW(2, ['ON', '1/2', 'OFF'], 'SUPER', 'ddsMod.super'),
    GAP(2),
    F(0.30, 'PW / DETUNE', 'grey', null, 18, 'ddsMod.pwDetune'),
    F(0.00, 'DRIFT', 'dark', null, 4, 'ddsMod.drift'),
    F(0.45, 'PWM / WAVE', 'orange', null, 20, 'ddsMod.pwmWave'),
    GAP(4),
    F(0.18, 'LFO 1', null, null, 0, 'ddsMod.lfo1Amt'),
    F(0.00, 'ENV 1', null, null, 0, 'ddsMod.env1Amt'),
    GAP(4),
    F(0.10, 'CROSS MOD', 'orange', 'RING', 18, 'ddsMod.crossMod'),
    GAP(2),
    SWCOL({ pos: 1, opts: ['DDS 2', '1+2', 'DDS 1'], label: 'DEST', id: 'ddsMod.dest' },
          { pos: 2, opts: ['MAN', 'LFO 1', 'ENV 1'], label: 'SRC', id: 'ddsMod.pwmSource' })`);

// ---- OSCILLATORS -----------------------------------------------------------
sub(`  const RB = (sel, steps, label) => ({
    w: wRotary(rd, 15),
    r: (x) => rotary(Object.assign({
      x: x + (wRotary(rd, 15) - rd) / 2, y: rcy - rd / 2, d: rd, sel: sel, steps: steps,
      label: label, ly: rly, lsize: 7.4, ssize: 5.6
    }, ONBLACK))
  });`,
`  const RB = (sel, steps, label, id) => ({
    w: wRotary(rd, 15),
    r: (x) => rotary(Object.assign({
      x: x + (wRotary(rd, 15) - rd) / 2, y: rcy - rd / 2, d: rd, sel: sel, steps: steps,
      label: label, ly: rly, lsize: 7.4, ssize: 5.6, id: id ? P(id) : null
    }, ONBLACK))
  });`);

sub(`[RB(1, ['SIN', 'SAW', 'SQR', 'TRI', 'NSE', 'ALT'], 'WAVEFORM'),
                                RB(3, ["64'", "32'", "16'", "8'", "4'", "2'"], 'RANGE')]`,
`[RB(1, ['SIN', 'SAW', 'SQR', 'TRI', 'NSE', 'ALT'], 'WAVEFORM', 'dds1.wave'),
                                RB(3, ["64'", "32'", "16'", "8'", "4'", "2'"], 'RANGE', 'dds1.range')]`);

sub(`[RB(2, ['SIN', 'SAW', 'SQR', 'TRI', 'NSE', 'PLS'], 'WAVEFORM'),
                               RB(3, ['LFO', "32'", "16'", "8'", "4'", "2'"], 'RANGE')]`,
`[RB(2, ['SIN', 'SAW', 'SQR', 'TRI', 'NSE', 'PLS'], 'WAVEFORM', 'dds2.wave'),
                               RB(3, ['LFO', "32'", "16'", "8'", "4'", "2'"], 'RANGE', 'dds2.range')]`);

sub(`label: 'TUNE', lsize: 7, ly: shly, labelColor: '#B9B7B1' }) },`,
    `label: 'TUNE', lsize: 7, ly: shly, labelColor: '#B9B7B1', id: P('dds2.tune') }) },`);

sub(`pos: 0, opts: ['SYNC', 'RING', 'NORM'], label: 'MODE', lsize: 7, ly: shly, ssize: 5.6 }, ONBLACK)) },`,
    `pos: 0, opts: ['SYNC', 'RING', 'NORM'], label: 'MODE', lsize: 7, ly: shly, ssize: 5.6, id: P('dds2.mode') }, ONBLACK)) },`);

// SUB is the same parameter as MODE on the hardware - a linked duplicate view.
sub(`pos: 1, opts: ['SIN', 'SQR', 'OFF'], label: 'SUB', lsize: 7, ly: shly, ssize: 5.6 }, ONBLACK)) }`,
    `pos: 1, opts: ['SIN', 'SQR', 'OFF'], label: 'SUB', lsize: 7, ly: shly, ssize: 5.6, id: P('dds2.mode'), domId: P('dds2.mode') + '__sub' }, ONBLACK)) }`);

// ---- MIXER -----------------------------------------------------------------
sub(`o += pack(g, [K(50, 'orange', 0.5, 'MIX', 'PAN')], 0);`,
    `o += pack(g, [K(50, 'orange', 0.5, 'MIX', 'PAN', 11, 'mixer.mix')], 0);`);

// ---- VCA -------------------------------------------------------------------
sub(`    SWCOL({ pos: 2, opts: ['ON', '1/2', 'OFF'], label: 'DYNAM' },
          { pos: 0, opts: ['ENV 2', '1+2', 'ENV 1'], label: 'ENV' }),
    GAP(2),
    F(0.78, 'ENV LVL', 'orange', null, 8), F(0.08, 'LFO 1'), F(0.00, 'AT')`,
`    SWCOL({ pos: 2, opts: ['ON', '1/2', 'OFF'], label: 'DYNAM', id: 'vca.dynamics' },
          { pos: 0, opts: ['ENV 2', 'GATE', 'GATE+R'], label: 'ENV', id: 'vca.envMode' }),
    GAP(2),
    F(0.78, 'ENV LVL', 'orange', null, 8, 'vca.envLevel'),
    F(0.08, 'LFO 1', null, null, 0, 'vca.lfo1Amt'),
    F(0.00, 'DDS 2', 'dark', null, 0, 'vca.dds2Amt')`);

// ---- ENVELOPES -------------------------------------------------------------
sub(`  const EF = (v, lbl, cap, sub) => ({
    w: 22,
    r: (x) => fader({ x: x + 4, y: efy, h: efh, sw: 9, cap: cap || 'grey', val: v, scale: false, label: lbl, lsize: 8.4, ly: ely, labelColor: '#D8D6D0' })`,
`  const EF = (v, lbl, cap, sub, id) => ({
    w: 22,
    r: (x) => fader({ x: x + 4, y: efy, h: efh, sw: 9, cap: cap || 'grey', val: v, scale: false, label: lbl, lsize: 8.4, ly: ely, labelColor: '#D8D6D0', id: id ? P(id) : null })`);

sub(`  const EKT = (pos, label) => ({
    w: wSw3(28),
    r: (x) => sw3(Object.assign({ x: x, y: efy + 6, h: 28, pos: pos, opts: ['ON', '1/2', 'OFF'], label: label, lsize: 6.8, ly: ely, ssize: 5.6 }, ONBLACK))
  });`,
`  const EKT = (pos, label, id) => ({
    w: wSw3(28),
    r: (x) => sw3(Object.assign({ x: x, y: efy + 6, h: 28, pos: pos, opts: ['ON', '1/2', 'OFF'], label: label, lsize: 6.8, ly: ely, ssize: 5.6, id: id ? P(id) : null }, ONBLACK))
  });`);

sub(`    EKT(2, 'KEYTRK'), GAP(2),
    EF(0.12, 'A', 'grey', 'AH'), EF(0.40, 'D', 'grey', 'DH'), EF(0.66, 'S'), EF(0.30, 'R', 'orange')`,
`    EKT(2, 'KEYTRK', 'env1.keytrack'), GAP(2),
    EF(0.12, 'A', 'grey', 'AH', 'env1.attack'), EF(0.40, 'D', 'grey', 'DH', 'env1.decay'),
    EF(0.66, 'S', null, null, 'env1.sustain'), EF(0.30, 'R', 'orange', null, 'env1.release')`);

// ENV 2 has no keytrack on the hardware; that slot becomes ENV 1's HOLD pair,
// which are real parameters that otherwise had no control.
sub(`    EKT(2, 'KEYTRK'), GAP(2),
    EF(0.05, 'A'), EF(0.52, 'D', 'grey', 'DH'), EF(0.74, 'S'), EF(0.36, 'R', 'orange')`,
`    GAP(2),
    EF(0.00, 'AH', 'dark', null, 'env1.attackHold'), EF(0.00, 'DH', 'dark', null, 'env1.decayHold'),
    EF(0.05, 'A', null, null, 'env2.attack'), EF(0.52, 'D', 'grey', 'DH', 'env2.decay'),
    EF(0.74, 'S', null, null, 'env2.sustain'), EF(0.36, 'R', 'orange', null, 'env2.release')`);

// ---- DLY -------------------------------------------------------------------
sub(`o += pack(g, [F(0.22, 'SEND', 'orange')], 0);`,
    `o += pack(g, [F(0.22, 'SEND', 'orange', null, 0, 'fx.delaySend')], 0);`);

fs.writeFileSync('gen.mjs', g);
console.log(`${n} substitutions applied` + (g === was ? ' (NOTHING CHANGED)' : ''));
