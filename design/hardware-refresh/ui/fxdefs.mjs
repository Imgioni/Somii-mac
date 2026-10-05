// The single source of truth for the FX rack (docs/fx/FX_PROMPTS.md).
// gen.mjs embeds it in the page; tools/gen-fx-header.mjs writes src/plugin/FxDefs.h from it,
// so the read-outs and the DSP ranges cannot drift. Each slot parameter is normalised 0..1:
//   p1..p8  the knobs (two rows of four)   p9..p11 stepped selectors   p12..p28 extra (display-driven)
// A continuous param: { p, label, lo, hi, def, curve: 'lin'|'log', unit }   (def in real units)
// A stepped param:    { p, label, steps: [...], def: index }
// sync: { p, divs } - while stepped param `p` is on (index 1) the knob picks a note division.

export const NP = 28;
export const DIVS = ['1/32', '1/16T', '1/32D', '1/16', '1/8T', '1/16D', '1/8', '1/4T', '1/8D', '1/4', '1/2T', '1/4D', '1/2', '1/1T', '1/2D', '1/1', '2/1', '4/1'];
// length of each division in quarter notes
export const DIV_BEATS = [0.125, 1 / 6, 0.1875, 0.25, 1 / 3, 0.375, 0.5, 2 / 3, 0.75, 1, 4 / 3, 1.5, 2, 8 / 3, 3, 4, 8, 16];
const PITCHES = ['-12', '-7', '-5', '0', '+5', '+7', '+12', '+19', '+24'];
// PRO-Q: each band's FREQ / GAIN / Q slots, its SHAPE slot, and the shapes (src/plugin/FxUnits.cpp builds them in this order)
export const PQ_BANDS = [['p12', 'p13', 'p14'], ['p15', 'p16', 'p17'], ['p18', 'p19', 'p20'], ['p21', 'p22', 'p23'], ['p24', 'p25', 'p26'], ['p1', 'p2', 'p3']];
export const PQ_SHAPE = ['p5', 'p6', 'p7', 'p8', 'p10', 'p11'];
export const PQ_SHAPES = ['BELL', 'LOW SHELF', 'HIGH SHELF', 'LOW CUT 12', 'LOW CUT 24', 'LOW CUT 48', 'HIGH CUT 12', 'HIGH CUT 24', 'HIGH CUT 48', 'NOTCH', 'BAND PASS'];

const k = (p, label, lo, hi, def, curve, unit) => ({ p, label, lo, hi, def, curve: curve || 'lin', unit: unit || '' });
const s = (p, label, steps, def) => ({ p, label, steps, def: def || 0 });
const db = (p, label, r, def) => k(p, label, -r, r, def || 0, 'lin', 'dB');
const pct = (p, label, def, lo) => k(p, label, lo == null ? 0 : lo, 100, def, 'lin', '%');
const hz = (p, label, lo, hi, def) => k(p, label, lo, hi, def, 'log', 'Hz');
const ms = (p, label, lo, hi, def, curve) => k(p, label, lo, hi, def, curve || 'log', 'ms');

export const FX = [
  { id: 'psdelay', blurb: 'Repeats that climb or fall in pitch', name: 'PS DELAY', group: 'TIME', mix: 35, display: 'dots',
    params: [k('p1', 'TIME', 10, 2000, 375, 'log', 'ms'), pct('p2', 'STEREO OFFSET', 0, -50), pct('p3', 'FEEDBACK', 45), k('p4', 'STEREO DETUNE', 0, 25, 6, 'lin', 'ct'),
      pct('p5', 'PITCH SHIFT', 70), pct('p6', 'SPRAY', 10), hz('p7', 'HP FREQ', 20, 2000, 120), hz('p8', 'LP FREQ', 1000, 20000, 9000),
      s('p9', 'MODE', ['OCT. UP', 'OCT. DOWN', 'FIFTH', 'OCT. UP+DOWN', 'DETUNE']), s('p10', 'TIME', ['FREE', 'SYNC'])],
    sync: { knob: 'p1', p: 'p10', def: 9 } },
  { id: 'revocean', blurb: 'A deep, moving reverb tail you can freeze', name: 'UNDERTOW', group: 'EXPERIMENTAL', mix: 30, display: 'tail',
    params: [pct('p1', 'SIZE', 60), k('p2', 'DECAY', 0.3, 20, 3.5, 'log', 's'), hz('p3', 'BRIGHTNESS', 1500, 18000, 7000), pct('p4', 'MOTION', 40),
      ms('p5', 'PRE-DELAY', 0, 250, 20, 'lin'), pct('p6', 'WIDTH', 100), pct('p7', 'DUCKING', 0), hz('p8', 'LOW CUT', 20, 1000, 80),
      s('p9', 'MODE', ['ABYSS', 'TIDE', 'FOAM']), s('p10', 'FREEZE', ['FREEZE OFF', 'FREEZE ON'])] },
  { id: 'tapeecho', blurb: 'Clean stereo or ping-pong delay', name: 'ECHO DELAY', group: 'TIME', mix: 30, display: 'echo',
    params: [k('p1', 'TIME', 10, 2000, 375, 'log', 'ms'), pct('p2', 'FEEDBACK', 40), pct('p3', 'WIDTH', 100), hz('p4', 'LOW CUT', 20, 2000, 80),
      hz('p5', 'HIGH CUT', 1000, 20000, 12000), pct('p6', 'DUCKING', 0), pct('p7', 'MOD', 0), pct('p8', 'DRIVE', 0),
      s('p9', 'MODE', ['STEREO', 'PING-PONG', 'MONO'], 1), s('p10', 'TIME', ['FREE', 'SYNC'], 1)],
    sync: { knob: 'p1', p: 'p10', def: 9 } },
  { id: 'convolver', blurb: 'Real spaces from an impulse file', name: 'CONVOLVER', group: 'TIME', mix: 30, display: 'ir',
    params: [ms('p1', 'PRE-DELAY', 0, 200, 0, 'lin'), pct('p2', 'LENGTH', 100, 5), hz('p3', 'LOW CUT', 20, 1000, 20), hz('p4', 'HIGH CUT', 1000, 20000, 20000),
      pct('p5', 'WIDTH', 100), db('p6', 'OUTPUT', 12), s('p9', 'REVERSE', ['FORWARD', 'REVERSE'])] },

  { id: 'tuba', blurb: 'A warm valve preamp with two-band tone', name: 'VALVE', group: 'DISTORTION', mix: 100, display: 'valve',
    params: [k('p1', 'GAIN', 0, 30, 10, 'lin', 'dB'), pct('p2', 'LEVEL', 50), db('p3', 'LF', 10), db('p4', 'HF', 10), db('p5', 'OUTPUT', 18),
      s('p9', 'MODE', ['MIC', 'LINE']), s('p10', 'PAD', ['PAD OFF', 'PAD -20'])] },
  { id: 'saturn', blurb: 'Saturation split into three bands', name: 'HEAT', group: 'DISTORTION', mix: 100, display: 'bands',
    params: [hz('p1', 'LOW XOVER', 50, 1000, 200), hz('p2', 'HIGH XOVER', 1000, 12000, 3000), k('p3', 'DRIVE LOW', 0, 36, 6, 'lin', 'dB'), k('p4', 'DRIVE MID', 0, 36, 9, 'lin', 'dB'),
      k('p5', 'DRIVE HIGH', 0, 36, 4, 'lin', 'dB'), pct('p6', 'DYNAMICS', 0, -100), db('p7', 'TONE', 6), db('p8', 'OUTPUT', 18),
      s('p9', 'STYLE', ['EMBER', 'GLASS', 'TAPE', 'IRON', 'STACK', 'RECTIFY', 'SHATTER'])] },
  { id: 'distortion', blurb: 'Five ways to clip, fold and fuzz', name: 'DISTORTION', group: 'DISTORTION', mix: 100, display: 'curve',
    params: [k('p1', 'DRIVE', 0, 48, 18, 'lin', 'dB'), pct('p2', 'BIAS', 0, -100), hz('p3', 'TONE', 800, 20000, 8000), hz('p4', 'LOW CUT', 20, 800, 40), db('p5', 'OUTPUT', 18, -6),
      s('p9', 'TYPE', ['SOFT', 'HARD', 'FOLD', 'FUZZ', 'DIODE'])] },
  { id: 'bitcrusher', blurb: 'Fewer bits, lower rate, more grit', name: 'BITCRUSHER', group: 'DISTORTION', mix: 100, display: 'steps',
    params: [k('p1', 'BITS', 1, 16, 8, 'lin', 'bit'), hz('p2', 'RATE', 500, 48000, 11025), pct('p3', 'JITTER', 0), k('p4', 'DRIVE', 0, 24, 0, 'lin', 'dB'),
      hz('p5', 'TONE', 500, 20000, 20000), db('p6', 'OUTPUT', 18)] },

  { id: 'vulf', blurb: 'Squashed, wobbly, lo-fi compression', name: 'PUMP', group: 'DYNAMICS', mix: 100, display: 'gr',
    params: [db('p1', 'INPUT', 18), pct('p2', 'COMPRESS', 60), ms('p3', 'ATTACK', 0.1, 30, 3), ms('p4', 'RELEASE', 20, 800, 90),
      pct('p5', 'WOBBLE', 15), pct('p6', 'GRIT', 25), db('p7', 'OUTPUT', 18)] },
  { id: 'faraday', blurb: 'A limiter with colour and drift', name: 'CEILING', group: 'DYNAMICS', mix: 100, display: 'gr',
    params: [k('p1', 'THRESHOLD', -40, 0, -12, 'lin', 'dB'), k('p2', 'RATIO', 2, 40, 10, 'log', ':1'), ms('p3', 'ATTACK', 0.05, 50, 1), ms('p4', 'RELEASE', 20, 2000, 200),
      pct('p5', 'COLOR', 30), pct('p6', 'WARMTH', 0, -100), pct('p7', 'DRIFT', 20), db('p8', 'OUTPUT', 18),
      s('p9', 'AUTO GAIN', ['AUTO GAIN OFF', 'AUTO GAIN ON'], 1)] },
  { id: 'mbcomp', blurb: 'Three-band upward and downward compression', name: 'MULTIBAND', group: 'DYNAMICS', mix: 100, display: 'mb',
    params: [db('p1', 'OUT LOW', 12), db('p2', 'OUT MID', 12), db('p3', 'OUT HIGH', 12), pct('p4', 'AMOUNT', 50),
      db('p5', 'INPUT', 18), ms('p6', 'ATTACK', 0.1, 100, 5), ms('p7', 'RELEASE', 10, 1000, 120), db('p8', 'OUTPUT', 18),
      s('p9', 'MODE', ['ABOVE & BELOW', 'ABOVE', 'BELOW']),
      hz('p12', 'LOW XOVER', 40, 1000, 150), hz('p13', 'HIGH XOVER', 1000, 16000, 2500)] },

  { id: 'phaser', blurb: 'Phaser with an auto-panner', name: 'PHASER PAN', group: 'MODULATION', mix: 100, display: 'lfo',
    params: [hz('p1', 'RATE', 0.02, 10, 0.4), pct('p2', 'DEPTH', 70), pct('p3', 'FEEDBACK', 40, -95), hz('p4', 'CENTER', 100, 4000, 800),
      k('p5', 'SPREAD', 0, 180, 90, 'lin', '°'), hz('p6', 'PAN RATE', 0.05, 10, 0.25), pct('p7', 'PAN DEPTH', 50),
      s('p9', 'STAGES', ['4 STAGES', '6 STAGES', '8 STAGES', '12 STAGES'], 2), s('p10', 'PAN', ['PAN FREE', 'PAN LINKED'])] },
  { id: 'flanger', blurb: 'Jet sweeps, mono or stereo', name: 'FLANGER', group: 'MODULATION', mix: 60, display: 'lfo',
    params: [hz('p1', 'RATE', 0.02, 10, 0.3), ms('p2', 'DELAY', 0.1, 10, 1.5), pct('p3', 'DEPTH', 60), pct('p4', 'FEEDBACK', 50, -95),
      hz('p5', 'HP FREQ', 20, 2000, 20), hz('p6', 'LP FREQ', 1000, 20000, 20000),
      s('p9', 'CHANNELS', ['MONO', 'STEREO'], 1), s('p10', 'SHAPE', ['SINE', 'TRIANGLE'])] },
  { id: 'stereopan', blurb: 'Tremolo panning, free or in sync', name: 'STEREO PAN', group: 'MODULATION', mix: 100, display: 'pan',
    params: [hz('p1', 'RATE', 0.05, 20, 1), pct('p2', 'DEPTH', 70), pct('p3', 'SHAPE', 0), k('p4', 'PHASE', -180, 180, 0, 'lin', '°'),
      k('p5', 'WIDTH', 0, 200, 100, 'lin', '%'), ms('p6', 'HAAS', 0, 20, 0, 'lin'),
      s('p9', 'RATE', ['FREE', 'SYNC'])],
    sync: { knob: 'p1', p: 'p9', def: 9 } },

  { id: 'proq', blurb: 'Six-band EQ, drawn on the graph', name: 'CONTOUR', group: 'FILTER / EQ', mix: 100, display: 'eq',
    // six band slots, all off at first: double-click the graph to add one, right-click it for its shape.
    // Band i: FREQ / GAIN / Q in PQ_BANDS[i], SHAPE in PQ_SHAPE[i]; p27 holds the on bits.
    params: [
      ...PQ_BANDS.flatMap(([fp, gp, qp], i) => [hz(fp, 'BAND ' + (i + 1) + ' FREQ', 20, 20000, [100, 250, 600, 1500, 4000, 10000][i]),
        k(gp, 'BAND ' + (i + 1) + ' GAIN', -24, 24, 0, 'lin', 'dB'), k(qp, 'BAND ' + (i + 1) + ' Q', 0.1, 18, 1, 'log', ''),
        s(PQ_SHAPE[i], 'BAND ' + (i + 1) + ' SHAPE', PQ_SHAPES, 0)]),
      db('p4', 'OUTPUT', 12),
      k('p27', 'BANDS', 0, 63, 0, 'lin', '')] },
  { id: 'filter', blurb: 'Resonant filter with its own LFO', name: 'FILTER', group: 'FILTER / EQ', mix: 100, display: 'resp',
    params: [hz('p1', 'CUTOFF', 20, 20000, 2000), pct('p2', 'RESONANCE', 30), k('p3', 'DRIVE', 0, 24, 0, 'lin', 'dB'), hz('p4', 'LFO RATE', 0.02, 20, 0.5),
      k('p5', 'LFO DEPTH', -4, 4, 0, 'lin', 'oct'), k('p6', 'ENV', -4, 4, 0, 'lin', 'oct'),
      s('p9', 'TYPE', ['LOW PASS', 'HIGH PASS', 'BAND PASS', 'NOTCH']), s('p10', 'SLOPE', ['12 dB', '24 dB'], 1)] },

  { id: 'autochroma', blurb: 'Pitched grains in a single stream', name: 'PRISM', group: 'EXPERIMENTAL', mix: 50, display: 'grains',
    // one grain stream (user, 2026-09-18): one pitch, and that stream's own settings
    params: [ms('p1', 'SIZE', 20, 500, 120), k('p2', 'DENSITY', 1, 40, 12, 'log', '/s'), k('p3', 'SPRAY', 0, 1500, 300, 'lin', 'ms'), pct('p4', 'FEEDBACK', 20),
      pct('p5', 'LEVEL', 80), pct('p6', 'REVERSE', 0), pct('p7', 'SPREAD', 60), hz('p8', 'TONE', 1000, 20000, 12000),
      s('p9', 'PITCH', PITCHES, 6), s('p10', 'SHAPE', ['SMOOTH', 'TRIANGLE', 'PERC', 'GATE']), k('p11', 'FINE', -100, 100, 0, 'lin', 'ct')] },
  { id: 'ambient', blurb: 'Six atmospheres on one TONE × SPACE pad', name: 'HALO', group: 'EXPERIMENTAL', mix: 40, display: 'xy',
    params: [pct('p1', 'SIZE', 60), k('p2', 'DECAY', 0.5, 30, 6, 'log', 's'), pct('p3', 'WIDTH', 100), pct('p4', 'MOD', 35),
      ms('p5', 'PRE-DELAY', 0, 250, 30, 'lin'), pct('p6', 'DUCKING', 0), hz('p7', 'LOW CUT', 20, 1000, 100), hz('p8', 'HIGH CUT', 1000, 20000, 12000),
      s('p9', 'MODE', ['SWELL', 'WEAVE', 'SHIMMER', 'OCTAVES', 'ARTEFACT', 'DROWNED']),
      pct('p12', 'TONE', 50), pct('p13', 'SPACE', 50)] },
  // SPACES: algorithmic reverb, a space (MODE) and a character (COLOR)
  { id: 'valleyverb', blurb: 'Rooms to cathedrals, aged to clean', name: 'SPACES', group: 'TIME', mix: 30, display: 'echogram',
    params: [ms('p1', 'PREDELAY', 0, 250, 20, 'lin'), k('p2', 'DECAY', 0.2, 20, 2.5, 'log', 's'), pct('p3', 'SIZE', 70), pct('p4', 'ATTACK', 40),
      pct('p5', 'EARLY DIFF', 70), pct('p6', 'LATE DIFF', 80), hz('p7', 'MOD RATE', 0.05, 5, 0.8), pct('p8', 'MOD DEPTH', 30),
      s('p9', 'MODE', ['ROOM', 'CHAMBER', 'PLATE', 'HALL', 'CATHEDRAL', 'AMBIENCE'], 3), s('p10', 'COLOR', ['AGED', 'DIGITAL', 'CLEAN'], 1),
      hz('p12', 'HIGH CUT', 1000, 20000, 9000), hz('p13', 'LOW CUT', 10, 1000, 40), hz('p14', 'DAMP FREQ', 1000, 20000, 6000), k('p15', 'BASS MULT', 0.5, 2.5, 1.2, 'lin', 'x')] },
  // stereo imager: width per band, a mono-bass cutoff, balance
  { id: 'imager', blurb: 'Width per band, mono below a cutoff', name: 'STEREO IMAGER', group: 'MODULATION', mix: 100, display: 'imager',
    params: [k('p1', 'LOW WIDTH', 0, 200, 100, 'lin', '%'), k('p2', 'MID WIDTH', 0, 200, 100, 'lin', '%'), k('p3', 'HIGH WIDTH', 0, 200, 120, 'lin', '%'), hz('p4', 'LOW XOVER', 50, 1000, 250),
      hz('p5', 'HIGH XOVER', 1000, 12000, 4000), hz('p6', 'MONO BELOW', 20, 500, 20), pct('p7', 'BALANCE', 0, -100), db('p8', 'OUTPUT', 12)] },
  // MARBLE: a restless distortion with its own little delay, mu-law and buzz
  { id: 'nudestort', blurb: 'Distortion that repaints itself', name: 'MARBLE', group: 'DISTORTION', mix: 100, display: 'painting',
    params: [k('p1', 'DRIVE', 0, 48, 18, 'lin', 'dB'), pct('p2', 'GRAIN', 20), pct('p3', 'DAMPING', 20), pct('p4', 'DLY MIX', 0),
      ms('p5', 'DLY TIME', 10, 1000, 180), pct('p6', 'FEEDBACK', 35), pct('p7', 'CHAOS', 25), pct('p8', 'TONE', 0, -100),
      s('p9', 'TYPE', ['FUZZ', 'FOLD', 'CRUSH', 'SCREAM']), s('p10', 'MODE', ['STEADY', 'RESTLESS']),
      pct('p12', 'MU-LAW', 0), pct('p13', 'BUZZ', 0), db('p14', 'INPUT', 18), db('p15', 'OUTPUT', 18, -4)] },
  // PARLOUR (user, 2026-10-04: a reverb after Analog Obsession's free ROOM041, made our own): a tube-style
  // preamp (DRIVE, then HPF), a dense plate-like room (PRE-DELAY, DECAY, STEREO separation) and a
  // post EQ of a low and a high shelf, each with its own frequency
  { id: 'parlour', blurb: 'A warm, driven room with plate shine', name: 'PARLOUR', group: 'TIME', mix: 25, display: 'plate',
    params: [db('p1', 'DRIVE', 24), hz('p2', 'HPF', 20, 100, 40), pct('p3', 'STEREO', 70), ms('p4', 'PRE-DELAY', 0, 250, 10, 'lin'),
      k('p5', 'DECAY', 0.1, 6, 1.6, 'log', 's'), hz('p6', 'LOW FREQ', 20, 2000, 200), db('p7', 'LOW', 20), hz('p8', 'HIGH FREQ', 200, 20000, 6000),
      db('p12', 'HIGH', 20)] }
];
export const FX_GROUPS = ['TIME', 'DISTORTION', 'DYNAMICS', 'MODULATION', 'FILTER / EQ', 'EXPERIMENTAL'];

// actual value <-> normalised
export function toNorm(q, v) {
  if (q.steps) return q.steps.length > 1 ? v / (q.steps.length - 1) : 0;
  if (q.curve === 'log') return Math.log(v / q.lo) / Math.log(q.hi / q.lo);
  return (v - q.lo) / (q.hi - q.lo);
}
export function fromNorm(q, n) {
  if (q.steps) return Math.round(n * (q.steps.length - 1));
  if (q.curve === 'log') return q.lo * Math.pow(q.hi / q.lo, n);
  return q.lo + (q.hi - q.lo) * n;
}
