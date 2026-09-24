# Super Gemini Manual — Working Notes

Source: `UDO_SuperGemini-Super8_Manual.pdf`, Owner's Manual v1.0, March 2024 (136 PDF pages, A4).
PDF page N == manual page N (roman front matter included). Rendered pages: scratchpad `pages/pNN.png`.

These notes are transcription + interpretation, page-cited. `[p.X]` = manual page. Anything marked
**UNCLEAR** must be checked again before it becomes code.

## Page map (from TOC, pp. iii–vi)
- Overview xii · DDS Oscillators xii · Binaural xiii
- Quick Start 14 · Setup 16 · Performances 17 · Load perf 17 · Edit perf 19 · Store perf 19
- Patches 20 · Load patch 20 · Init patch 22 · Edit patch 23 · Store patch 24 · Compare 25 · Shift mode 25 · Manual mode 26 · Level up 26
- Firmware 27 · Connections 28
- Sound design 30 · Oscillators 30 · DDS 1 31 · Alt waveforms 33 · DDS 2 35 · DDS 2 as LFO 38 · Sub-osc 39
- Mixer 40 · VCF 41 · VCA 44 · Envelopes 46 · ENV 1 47 · Loop mode 50 · ENV 2 52 · LFO 1 54 · DDS Modulator 60
- Effects 64 · Chorus 64 · Delay 65 · Freeze 67
- Performance section 68 · Bender 69 · LFO 2 70 · Osc/Layer select 72 · Portamento 73 · Octave/Transpose 75 · Fine tune 76
- Ribbon 77
- Additional 79 · Master Vol 79 · Tempo 79 · Lower Layer Detune 80 · Hold 81 · Keyboard 82 · Layer 83
- Mod Matrix 84 · Matrix dest 86 · Direct mappings 87 · Clearing 89
- Voice Assign 90
- Arp/Seq 92 · Clock 92 · Arp 94 · Seq 95 · Clear seq 98 · Load/store seq 98
- Global Settings 99
- File Mgmt 102 · Naming 103 · Imports 104–105 · Backups 106–107
- How-to 108 · Init patch 108 · Wave packs 109 · Own waveforms 109 · DAW setup 110
- Cheat sheet 111 · Shortcuts 111 · Secondary functions 112 · Multi select buttons 113 · Global (shift) 114
- MIDI 115 · Realtime 115 · Channel msgs 115 · CC 116 · RPN 123 · NRPN 123 · Global NRPN 123 · Patch NRPN 124
- Glossary 129 · Support 134

## Overview [p.xii–xiii]
- Super Gemini = 20 voices; Super 8 = 16 (we build Super Gemini). Bi-timbral, hybrid analog/digital.
- DDS: clock ~3 orders of magnitude above audio rate, sample every 20 ns, interpolates between table
  indices; each NCO has own DAC at the same high rate, then a preliminary analog LPF stage. No severe
  band-limiting; content extends far above hearing, "as is the norm with analog oscillator synthesis" [p.xii].
- Binaural [p.xiii]: true stereo path; 20 voices twinned → 10 stereo super voices (single) or 5 per layer
  (dual/split). L and R each a complete voice per layer. Params of both channels of each super voice can be
  independently controlled ("starting with the stereo oscillators").
- **Signal path per layer (diagram p.xiii), per side L/R:** DDS 1 + DDS 2 → MIXER → VC HPF → VC LPF (CV ENV MOD)
  → VCA (CV ENV) → node splits to **FX (send)** and to output MIXER; FX return also into output MIXER → AUDIO L/R.
  i.e. **effects are a send/return in parallel with the dry signal**, not an insert. "PHASE CONTROL" block
  (driven by MOD) sits between L and R paths feeding DDS1/2, LPF, VCA.

## Quick start [p.14–16]
- Upper panel = per-layer sound controls; Super Gemini has TWO rows (one per layer, simultaneous editing).
- Bottom row = shared controls: load patch into layer, arp/seq/effect settings per layer, etc. [p.15]
- 61-note velocity-sensitive Fatar keybed with **polyphonic aftertouch** [p.15].
- Performance control section left of keys: bender, transpose, portamento, LFO 2; ribbon controller (SG only).
- Outputs: MIX OUTPUT (L/R), headphones. MASTER VOLUME "about 0 dB" is a normal setting [p.16] → dB-scaled.
- Pedals: sustain, expression, volume [p.16 diagram].

## Performances [p.17–19]
- Performance = two layers (upper, lower) each with own patch + overarching settings (pp.79–83).
  Always loaded. SINGLE = one patch; DUAL = two stacked; SPLIT = left/right halves (pp.82–83).
- **MASTER VOLUME is not part of a performance** [p.17].
- 128 perfs = groups A/B × banks A–H × 8. PERF button = group A; SHIFT+PERF = group B [p.17].
- Select-button row (hardware): 1–8, A–H, PERF (A/B LEDs), PATCH (A/B LEDs), SHIFT. Shift legends under
  A–H: MIDI CH, TX/RX E, TX/RX P, DUMP, LOCAL, TUNE, MPE [p.18 figure] → global settings (see p.99/114).
- Unlit select LED = empty memory location [p.18]. Changing bank doesn't load; pressing 1–8 does [p.18].
- **Edited state:** any parameter change except MASTER VOLUME → lit numbered LED flashes [p.19].
- Store: select group/bank, hold 1–8 for 3 s; LEDs flash once. Overwrites [p.19].
- Stored performance = snapshot with **copies** of both layers' patches (original patches unchanged) [p.19].

## Patches [p.20–22]
- 256 patches = groups A/B × 16 banks (A1–H2) × 8 [p.20]. Loaded into a layer of a performance.
- LAYER section on hardware: **LOWER / UPPER** buttons (only two — no BOTH button here) [p.20 figure].
- PATCH button = group A; SHIFT+PATCH = group B. A–H button toggles bank X1 (lit) / X2 (flashing) [p.21].
- Staying in X2 select mode until a X1 patch is loaded [p.22].
- **Init patch:** "single oscillator set to a sawtooth wave, among other basic settings" [p.22].
  Load: SHIFT + MANUAL UPPER / MANUAL LOWER. **Loaded by default on power-on.** Loading init into a layer
  switches the performance to SINGLE mode [p.22].

## Editing / storing / compare / shift / manual [p.23–26]
- **Super Gemini: TOP row = UPPER layer, SECOND row = LOWER layer** [p.23]. (Mockup has "Layer 1/Lower" on
  top → conflict, see DESIGN_DECISIONS.)
- Hardware per-layer row (figure p.23), left→right: **LFO 1 · DDS MODULATOR · DDS 1 · DDS 2 · MIXER · VCF ·
  VCA · ENVELOPES (ENV 1, ENV 2) · DELAY (send fader)**.
- Shared bottom row (figure p.23): LAYER (LOWER/UPPER) · VOICE ASSIGN · ARPEGGIATOR/SEQUENCER · multifunction
  select buttons · CHORUS · DELAY (TIME, FEEDBACK knobs). Toggle which layer these edit with UPPER/LOWER [p.23].
- Performance control section (left of keys): LFO 2, portamento, and which patch params of each layer are
  affected by pitch bend and poly AT (pp.68–74) [p.23].
- Control types: rotary switch, rotary control, fader, toggle switch [p.24].
- Any patch param change → numbered LED flashes (edit mode) [p.24]. Store patch = hold 1–8 3 s [p.24].
- **Compare** [p.25]: in edit mode press the target numbered button → hear stored patch at that slot;
  press it again → return to edited patch.
- **Shift** [p.25]: press+release SHIFT = latched shift mode (LED flashes) for shift-mode params (global
  settings pp.99–101) or secondary functions (inverse-colour labels under primary names). Hold SHIFT while
  moving a control = temporary secondary function (e.g. DRIFT, PAN, DECAY HOLD); exits on release.
- **Manual mode** [p.26]: MANUAL UPPER / MANUAL LOWER buttons → layer follows actual panel positions,
  ignoring patch. Exit by pressing a numbered patch/perf select button. Shift legend: INIT PATCH.

## Connections [p.28–29]
- Headphones (MASTER VOLUME controls level) [p.28]. USB-MIDI class compliant; DIN and USB not simultaneous.
- Rear [p.29]: MIDI IN/OUT/THRU · VOL pedal (TRS 0–5 V) · EXPR pedal (TRS 0–5 V, matrix source) ·
  SUS (single = upper layer; dual: left = upper, right = lower; polarity auto-detect at power-up) ·
  **DLY FREEZE** footswitch (single → upper layer freeze; dual: left = upper, right = lower) ·
  **MIX OUTPUT L/R** (both layers) · **LOWER L/R** · **UPPER L/R** layer outputs.
  → plugin buses: Main (stereo), Upper (stereo), Lower (stereo). **UNCLEAR:** whether layer outs are
  pre- or post-effects; assume post (effects are per layer).

## Oscillators overview [p.30–31]
- Patch params identical for both layers [p.30]. SG oscillator panel (figure p.30):
  - DDS 1: WAVEFORM rotary switch (icons: sine, saw, square, triangle, noise, ALT-grid) with dashed morph
    arrows between adjacent positions; RANGE rotary switch 64/32/16/8/4/2.
  - DDS 2: WAVEFORM rotary (sine, saw, square, triangle, noise, pulse-icon); TUNE rotary control scale
    −7…0…+7; RANGE rotary LFO/32/16/8/4/2; 3-pos toggle **SYNC (top) / RING (mid) / NORM (bottom)**.
    When RANGE = LFO the toggle legends become SUB OSC: **sine (top) / square (mid) / OFF (bottom)**.
- Two FPGA oscillators, classic analog waveforms. DDS 1 also offers up to 32 alt waveforms in two groups,
  user definable (p.109) [p.31].

## DDS 1 [p.31–34]
- Super waveform core: centroid + 6 "sister oscillators", dynamically de-phased in the stereo field via one
  of the two super modes (DDS Modulator, pp.61–62). 7 free-running oscillators [p.31].
- **WAVEFORM** rotary switch, 6 positions: SINE, SAW, SQUARE, TRIANGLE, NOISE (white), ALT [p.32].
- **Wave morph (PWM/WAVE fader, p.62)** morphs to the next waveform to the right [p.32 table]:
  SINE→SAW · SAW→SQUARE · SQUARE→TRIANGLE · TRIANGLE→NOISE · NOISE→ALT(A) · ALT A→ALT B.
- At ALT position you hear channel A; B audible with PWM/WAVE at max [p.33].
- **RANGE** rotary switch 64' 32' 16' 8' 4' 2' (coarse frequency) [p.33].
- Alt waveforms [p.33–34]: WAVE button toggles channel A/B selection (LEDs A, B). Select buttons W1–W16;
  press once → W1–16 (LED lit), twice → W17–32 (LED flashing). 32 waveforms in 2 groups of 16.
  Moving DDS 1 WAVEFORM to ALT also enters wave mode. PATCH exits wave mode.
- **Each patch remembers its alt waveforms**; replacing the library doesn't change factory patches [p.34]
  → patch must embed or reference-by-content its A/B waveform data.
- Manual mode retains the alt waveforms from the previously loaded patch [p.34].

## DDS 2 [p.35–]
- Algorithmic core (not sampled), very high sample rate, six classic waveforms; "behaves in a subtly
  different way" [p.35].
- **Phase reset to zero on every note** (enables binaural pitch & PW modulation by LFO 1) [p.35].
- **WAVEFORM** rotary switch: SINE, SAW, SQUARE, TRIANGLE, NOISE (white), PULSE [p.36].
- **RANGE** rotary switch: LFO, 32', 16', 8', 4', 2' (text says "DDS 1" — manual typo, means DDS 2) [p.36].
- **TUNE** rotary control: ±7 semitones, fine-tune relative to DDS 1 (detune or intervals) [p.36].
  (LAYER = Super 8-only shift function of TUNE — not applicable to SG.)
- **Mode toggle (3-pos)** [p.36–37]: NORM (default) · RING (carrier DDS 1 × modulator DDS 2; output passed
  through the **channel of DDS 2**, faded in with MIX) · SYNC (hard sync: DDS 2 restarts each DDS 1 cycle).
- **LFO mode** (RANGE = LFO) [p.38]: DDS 2 **no longer fed into audio path**; TUNE sets frequency
  **≈ 0.1–100 Hz**; WAVEFORM still selects shape. Offers sine + pulse on top of LFO 1's shapes. Can be
  PW-modulated (DDS Modulator) or FM'd by LFO 1. Routable to matrix in both LFO and audio mode.
- **Sub-oscillator** [p.39]: only with RANGE = LFO; mode toggle **middle = square sub, upper = sine sub**
  (bottom = OFF). Sub audio **replaces DDS 2 audio**; pitch locked **1 octave below DDS 1**; WAVEFORM & TUNE
  don't affect sub. MIX blends DDS 1 ↔ sub. DDS 2 still usable as LFO simultaneously.

## Mixer [p.40]
- Single **MIX** rotary control (legend "1+2" at top, DDS 1 left, DDS 2 right). 12 o'clock = equal;
  full left = DDS 1 only; full right = DDS 2 (or sub) only.
- **PAN** = shift function of MIX: layer pan, 12 o'clock centre, hard L / hard R.

## VCF [p.41–]
- SG panel left→right [p.41 fig]: **DRIVE** toggle (OFF/1/2) · **HPF** fader · **LPF** fader · **RES** fader ·
  **ENV source** toggle (ENV 2 top / 1+2 mid / ENV 1 bottom) · **KEYTRACK** toggle (ON top / ½ mid / OFF
  bottom) · **ENV** fader · **LFO 1** fader · **DDS 2** fader. All faders 0–10. (Super 8 labels these ENV MOD,
  LFO MOD, OSC MOD.)
- LPF: 4-pole 24 dB/oct analog resonant LPF, classic SSI polysynth design. Preceded by 1-pole 6 dB/oct
  analog HPF [p.41].
- Left half = basic settings; right half = LPF modulation & key tracking [p.42].
- **DRIVE**: OFF = clean · 1 = subtle saturation **with resonance compensation** · 2 = healthy overdrive [p.42].
- **HPF** fader = HPF cutoff (remove muddy lows). **LPF** fader = LPF cutoff. **RES** fader = resonance [p.42].
- **Self-oscillation** at RES max: pitch set by cutoff, sine-like timbre; playable with key tracking [p.42].
- **ENV** (ENV MOD) fader: amount either/both envelopes modulate LPF cutoff. Source toggle: ENV 1 / 1+2 /
  ENV 2 [p.42]. **UNCLEAR:** unipolar (0–10) — is it positive-only? Fader scale 0..10, no centre detent →
  treat as unipolar positive; ENV 1 INVERTED mode supplies negative sweeps.
- **LFO 1** (LFO MOD) fader: LFO 1 → LPF cutoff amount [p.42].
- **DDS 2** (OSC MOD) fader: DDS 2 → LPF cutoff amount (audio-rate FM of filter) [p.42].
- **KEYTRACK** [p.43]: OFF = none · ½ = follows keyboard in **quarter-tone steps** per semitone (half
  tracking) · ON = follows in semitones (full tracking, self-osc plays in tune).
- Keytrack design note [p.43]: tracking "in a musical way" determines how far the LPF fader can open the
  filter; remaining headroom reachable by ENV or expression pedal via matrix.

## VCA [p.44–45]
- Default VCA level controlled by ENV 2; alternatively one of two fixed envelopes (frees ENV 2) [p.44].
- SG panel [p.44 fig]: **DYNAMICS** toggle (ON top / ½ / OFF bottom) · **envelope selector** toggle (upper =
  fixed env w/ release icon, middle = gate icon, lower = ENV 2) · **ENV LEVEL** fader with scale
  **−∞ … 0 dB … +4 dB** (0 dB mark ≈ 3/4 travel) · **LFO 1** fader 0–10 · **DDS 2** fader 0–10.
- ENV LEVEL: amount ENV 2 / fixed env modulates VCA level [p.45].
- LFO 1 (LFO MOD): tremolo amount (triangle = smooth, square = abrupt) [p.45].
- DDS 2 (OSC MOD): DDS 2 → VCA level (amplitude modulation) [p.45].
- Env selector [p.45]: **lower = ENV 2 (default)** · **middle = fixed env 1: A/D/R zero duration (gate)** ·
  **upper = fixed env 2: A/D zero duration, has a release stage**. **UNCLEAR:** release time of the upper
  fixed envelope (fixed value? follows ENV 2 R?). Check cheat sheet/NRPN.
- **DYNAMICS** [p.45]: OFF · ½ (half intensity) · ON (full) — velocity → VCA level AND LPF brightness.

## Envelopes [p.46–53]
- SG ENV panel [p.46 fig]: ENV 1: **KEYTRACK** toggle (ON/½/OFF) · **MODE** toggle (LOOP top / inverted
  middle / normal bottom) with LOOP LED · faders **A** (1 ms–10 s), **D** (1 ms–10 s), **S** (0–10),
  **R** (1 ms–10 s); shift legends **AH** under A, **DH** under D. ENV 2: **A, D, S, R** faders, **DH** under D.
- Envs can modulate LPF cutoff (p.42), VCA level (p.45), DDS 1 waveform & DDS 2 PW (p.62) [p.46].
- Both envelopes: attack, decay hold, decay, sustain, release (5 stages). ENV 1 additionally: **attack
  hold** (delay before attack starts) [p.46].

### ENV 1 [p.47–51]
- **AH** (shift of ATTACK): delay before attack begins, **up to 10 s**; minimum = no effect [p.47].
- **A**: 1 ms … 10 s [p.47].
- **DH** (shift of DECAY): hold at peak before decay, **up to 10 s**; minimum = no effect [p.47].
- **D**: 1 ms … 10 s (max level → sustain level) [p.48]. **S**: level (only level-type stage) [p.48].
  **R**: 1 ms … 10 s [p.48].
- **MODE toggle** (bottom toggle) [p.48]: **lower = NORMAL** · **middle = INVERTED** (shape inverted; effect on
  destination is the opposite of normal) · **upper = LOOP**: attack → decay hold → decay repeat
  indefinitely while key held; release on key-up. **LOOP LED** indicates loop repeat speed.
- In LOOP, **sustain = level the env rises from at attack start and falls to at decay end** [p.48, fig p.49].
  Attack hold happens once at the start (not looped) [fig p.51 ex.3]. Loop = periodic mod source, can be
  key-tracked; short A/DH/D gives FM-like results [p.49].
- **KEYTRACK** (ENV 1 only) [p.49]: OFF = none · ½ = decay & release times **decrease in quarter-tone steps**
  up the keyboard · ON = decrease **in semitone steps**. → time scale = 2^(−k·(note−ref)/12), k = 0/0.5/1.
  **UNCLEAR:** reference note (assume C4 = MIDI 60, same as seq transpose reference). Applies in loop mode too.
- Inverted-mode keytrack effect is the opposite of normal mode [p.50].
- Loop-mode examples [p.50–51]: (1) AH=min, A/DH/D mid, S=0 → repeating AR-ish hump; (2) A long, D mid,
  S high → rising/falling between sustain and peak; (3) A=min, DH=min, D long, AH mid → decaying sawtooth
  after an initial hold.

### ENV 2 [p.52–53]
- A: 1 ms–10 s · **DH** (shift of DECAY) up to 10 s (min = no effect) · D: 1 ms–10 s · S: level ·
  R: 1 ms–10 s. No attack hold, no mode switch, no keytrack [p.52–53]. Default VCA envelope.

## LFO 1 [p.54–59]
- Default: vibrato (osc pitch) / tremolo (VCA). HF modes: 3rd oscillator, drone, audio-rate mod [p.54].
- SG panel [p.54 fig]: **rate LED** (top-left) · **RATE** fader (LF legend 0.05 Hz … 50 Hz; HF boxed legend
  20 Hz … 10 kHz) · **DELAY** fader (0 … 10 s) · **LR PHASE** fader (0 … 2π) with boxed legend **SPREAD** ·
  **WAVEFORM** rotary switch (triangle, reverse saw, S&H, square, **HF**, **HF TRK**) · **MODE** toggle
  (top RESET / mid ONCE / bottom FREE; in HF: top **DDS 2** / mid **DDS 1** / bottom **NORM**).
- **10 individual LFOs (one per super voice)**; non-binaural: each shared by two voices [p.54].
- **RATE** [p.55]: rate LED blinks. With **SYNC** on (arp/seq section) RATE selects clock divisions of
  internal TEMPO or external MIDI clock. Sync table, fader bottom→top [p.55–56]:
  | # | division | cycle length (beats) |
  |---|---|---|
  | 1 | 8 whole notes | 32 |
  | 2 | 4 whole notes | 16 |
  | 3 | 2 whole notes | 8 |
  | 4 | whole note | 4 |
  | 5 | 1/2 note | 2 |
  | 6 | dotted 1/4 | 1.5 |
  | 7 | 1/2 triplet | 4/3 ("1/3 of 4 beats") |
  | 8 | 1/4 | 1 |
  | 9 | dotted 1/8 | 3/4 |
  | 10 | 1/4 triplet | 2/3 ("1/3 of 2 beats") |
  | 11 | 1/8 | 1/2 |
  | 12 | dotted 1/16 | 3/8 |
  | 13 | 1/8 triplet | 1/3 |
  | 14 | 1/16 | 1/4 |
  | 15 | dotted 1/32 | 3/16 |
  | 16 | 1/16 triplet | 1/6 |
- **DELAY** [p.56]: time until LFO modulation affects the sound after note-on (fade-in). Range 0–10 s (legend).
- **LR PHASE** [p.56–58] (binaural): L/R phase offset of LFO 1; 0 % = 0, 25 % = π/2, 50 % = π, 75 % = 3π/2,
  100 % = 2π (in phase again). Affects **LPF cutoff, VCA level, DDS 2 pitch and DDS 2 pulse width**.
- **SPREAD** [p.58]: in **non-binaural + POLY 1/POLY 2**, LR PHASE becomes pan spread: min = all voices
  centred, max = voices alternately hard-panned L/R.
- **WAVEFORM** (LF) [p.58–59]: triangle (**bipolar**), reverse sawtooth (**unipolar +**), sample & hold
  (**bipolar** random per cycle; white noise at max RATE), square (**unipolar +**; trills at high rate).
- **HF** [p.59]: 20 Hz – **20 kHz** (text) — panel legend says 10 kHz → **UNCLEAR, text vs legend**. Drone or
  constant audio-rate mod source. "By default, LFO 1 is set to a sine wave in this mode."
- **HF TRK** [p.59]: HF with key tracking → 3rd oscillator / audio-rate source; RATE tunes it to match
  DDS 1/2. Sine by default. **UNCLEAR:** whether other HF waveforms are selectable (check NRPN tables).
- **MODE** LF [p.59]: FREE (free-running) · ONCE (one duty cycle per note → simple envelope) · RESET
  (phase reset per note).
- **MODE** HF [p.59]: NORM (mod source only) · **DDS 1** (LFO audio summed into DDS 1 channel, then MIX
  crossfades vs DDS 2) · **DDS 2** (summed into DDS 2 channel, MIX crossfades vs DDS 1).

## DDS Modulator [p.60–63]
- Three subsections: pitch mod · DDS 1 / DDS 2-pulse modulation · cross mod [p.60].
- SG panel [p.60 fig], left→right: **LFO 1** fader (0–10) · **ENV 1** fader (0–10) · **osc selector** toggle
  (top DDS 2 / mid 1+2 / bottom DDS 1) · **SUPER (DDS 1)** toggle (top ON / mid ½ / bottom OFF) · **PW** fader
  (header DETUNE; inverse shift legend **DRIFT**) 0–10 · **PWM** fader (header WAVE) scale **0 [A] … 10 [B]** ·
  **PWM/WAVE source** toggle (top ENV 1 / mid LFO 1 / bottom MANUAL) · **CROSS MOD** fader 0–10 with boxed
  context legend **RING MOD**.
- **LFO 1** fader: pitch-mod amount by LFO 1. **ENV 1** fader: pitch-mod amount by ENV 1 [p.60].
- **Osc selector**: DDS 1 / 1+2 / DDS 2 — destination of LFO 1 & ENV 1 pitch mod [p.60].
- **SUPER (DDS 1)** [p.61]: OFF (DETUNE no effect; only centroid sounds) · **½** (super at half depth AND DDS 1
  phase reset per note; flanging with high detune, punchy with detune 0) · **ON** (full depth). Six sisters
  de-phased/spread in the stereo field.
  Spectrum figures [p.61–62]: ½ + detune 50 % → centroid full, 3 sister pairs spread symmetrically with
  **descending amplitude outward** (≈50/37/25 % of centroid); ON + detune 100 % → **all 7 equal amplitude**,
  evenly spaced. → "depth" scales sister level as well as spread.
- **PW/DETUNE** [p.62]: DDS 2 pulse width; in super mode also DDS 1 detune spread.
- **DRIFT** (shift of PW/DETUNE) [p.62]: randomisation of osc pitch, filters, envelopes (vintage instability).
- **PWM/WAVE** [p.62]: PWM amount on DDS 2 pulse + wave-mod amount on DDS 1 (morph adjacent waveform or ALT
  A↔B). Lowest (A) = selected waveform / ALT A; highest (B) = next waveform to the right / ALT B.
- **Source toggle** [p.62]: **MANUAL** (fader morphs directly; "if DDS 2 is set to pulse wave, this fader
  also controls the pulse width of DDS 2") · **LFO 1** · **ENV 1**.
  **UNCLEAR:** in MANUAL, how PW/DETUNE and PWM/WAVE combine for DDS 2 pulse width (sum? PWM overrides?).
- **CROSS MOD** [p.63]: default **DDS 2 → DDS 1 exponential FM**. With **SYNC**: routing reverses (DDS 1 FMs
  DDS 2; wavefolding/PM-like since DDS 2 restarts with DDS 1). With **RING**: additional cross mod applied
  to the carrier (DDS 1).

## Effects [p.64–67]
- Two 24-bit effects per layer: classic dual-mode **stereo chorus** and **stereo delay** (modulatable,
  syncable to arp/seq clock or external clock). **Series: chorus → delay** [p.64].
- **Chorus** [p.64]: two buttons **I** and **II** with LEDs. I = subtle; II = denser, higher mod rate;
  I+II = even more intense, string-machine ensemble. (Neither = off.)
- **Delay** [p.65]: **TIME** knob 1 ms … 1 s with LED (top-right) · **FEEDBACK** knob 0–10. With SYNC on,
  TIME selects clock divisions of TEMPO / external clock. Sync table (knob min→max) [p.65–66]:
  | # | division | beats |
  |---|---|---|
  | 1 | 1/32 triplet | 1/12 |
  | 2 | dotted 1/64 | 3/32 |
  | 3 | 1/32 | 1/8 |
  | 4 | 1/16 triplet | 1/6 |
  | 5 | dotted 1/32 | 3/16 |
  | 6 | 1/16 | 1/4 |
  | 7 | 1/8 triplet | 1/3 |
  | 8 | dotted 1/16 | 3/8 |
  | 9 | 1/8 | 1/2 |
  | 10 | 1/4 triplet | 2/3 |
  | 11 | dotted 1/8 | 3/4 |
  | 12 | 1/4 | 1 |
  | 13 | 1/2 triplet | 4/3 |
  | 14 | dotted 1/4 | 3/2 |
  | 15 | 1/2 | 2 |
  | 16 | whole-note triplet | 8/3 |
- **FEEDBACK** [p.66]: repeats; fully clockwise = **repeats indefinitely with no decay or degradation**.
- **SEND** fader (per-layer row, "DLY", 0–10) [p.67]: how much of each layer's output feeds the delay;
  higher = wetter. (Confirms send/return topology from p.xiii.)
- **Delay Freeze** [p.67]: footswitch (single → upper; dual: L = upper, R = lower; polarity auto-cal at
  power-on). Footswitch released → notes enter delay loop; **held → new notes no longer added, current
  content loops endlessly**. SEND = loop level, TIME = loop length, FEEDBACK "to ensure looped notes are
  repeated at a constant level"; best with long times + moderate feedback. **MIDI CC 69** sends/receives.
  Interpretation: freeze engaged → delay input muted + feedback forced to unity; released → normal.

## Performance control section [p.68–76]
- Settings can affect both layers individually or together [p.68].
- Panel [p.68 fig]: **TRANSPOSED** LED · octave LEDs **−2 −1 0 +1 +2** · **OCT− / OCT+** rocker (shift legends
  **ST− / ST+**) · **PORTAMENTO** fader 0–10 → portamento **layer toggle UPPER / LOWER / BOTH** ·
  **LFO 2**: WAVEFORM rotary, RATE knob (0.05–50 Hz) + LED, DELAY knob (0–5 s) · **DEST**: osc selector
  toggle (DDS 2 / 1+2 / DDS 1) and layer toggle (UPPER / LOWER / BOTH) · LFO 2 trigger toggle (top **LFO2 ON**
  [boxed AT→BEND] / mid **AT+TRIG** / bottom **TRIG**) · faders **LFO2 RATE, DDS, VCF, VCA** (LFO 2 depths) ·
  faders **DDS, VCF** (bender horizontal amounts, also AT) · bender: vertical push Δ → LFO 2; horizontal ◁▷.

### Bender [p.69]
- Modulates osc pitch and LPF cutoff; horizontal L/R and vertical (pressure pad, not a lever throw).
- **DDS** fader: bender pitch amount, **max range one octave**; osc selector (DEST) chooses oscillator(s).
- **VCF** fader: bender → LPF cutoff; at max the filter can be fully opened/closed by horizontal bend.

### LFO 2 [p.70–71]
- Six waveforms: **sine** (bipolar), **reverse saw** (+), **S&H** (±), **square** (+), **sawtooth** (+),
  **noise** (white) [p.70].
- **RATE** knob 0.05–50 Hz with LED; **DELAY** knob 0–5 s (time before LFO 2 starts affecting sound) [p.71].
- **Trigger toggle** [p.71]: **TRIG** = bender push triggers LFO 2 · **AT+TRIG** = aftertouch or bender push;
  whichever is greater controls depth · **ON (AT→BEND)** = LFO 2 permanently on; aftertouch now triggers the
  same modulations as horizontal bender movement.
- **LFO2 RATE** fader: how much vertical push / AT increases LFO 2 rate [p.71].
- **DDS** fader: LFO 2 → pitch (osc selector in DEST) · **VCF** fader: LFO 2 → LPF cutoff [p.71].
- **VCA** fader: LFO 2 → VCA level [p.72].
- **LFO 2 is polyphonic when used as a mod destination** (e.g. ENV 1 → LFO 2 rate per voice). Phase of the
  per-voice LFO 2s resyncs when the trigger toggle is flipped [p.72].

### DEST — oscillator & layer selection [p.72–73]
- **Osc selector** (DDS 1 / 1+2 / DDS 2): which osc(s) receive **horizontal-bender and LFO 2 pitch mod**.
- **Layer selector** (UPPER / LOWER / BOTH): which layer's bender/LFO 2 settings the controls edit.
  **Switching to BOTH from UPPER or LOWER copies the upper layer's settings to both.** [p.73]
- Example [p.73]: per-layer bend ranges (upper = octave, lower = fifth) via DDS fader → these settings are
  **stored per patch** ("save either the performance or both patches").

### Portamento [p.73–74]
- Glide time scales with interval: **smaller interval = faster, larger = slower**; in chords each note glides
  at its own rate [p.73].
- **PORTAMENTO** fader: leftmost = **off**; rightmost = **10 s per octave** [p.74].
- Its **layer toggle UPPER / LOWER / BOTH** (BOTH copies upper → both) [p.74]; also used for octave (p.75).
  Stored per patch (save perf or both patches) [p.74].

### Octave selector & transpose [p.75]
- Spring-loaded toggle, range **5 octaves: −2 … +2**, LEDs show current octave.
- Layer selection via the **portamento layer toggle** (UPPER / LOWER / BOTH; BOTH copies upper).
- **Shift + octave toggle = global transpose ±12 semitones**; right LEDs flash when up, left when down.
- **Middle octave LED keeps flashing while global transpose ≠ 0** (legend "TRANSPOSED") [p.75].
  (The build prompt said this flashes for fine-tune ≠ 440 — manual says transpose. Manual wins.)

### Global fine tuning [p.76]
- SHIFT + **MOD AMOUNT** encoder (= **FINE ADJ** in shift mode). **±100 cents**; pressing encoder resets to
  440 Hz / 0 ct. LEDs of 1–8, A–H show the value against the printed −100 % … 0 % … +100 % legend.
- **Select-row legends revealed [p.76 fig]:** MOD AMOUNT encoder (shift: FINE ADJ; push: CLEAR) ·
  **MOD ASSIGN** button (shift: **CLEAR**) · buttons 1–8 = matrix sources **DDS 2, LFO 2, ENV 1, VEL, AT, EXPR,
  RIBN, NOTE** · buttons A–H = matrix destinations **LFO 1, X MOD, WAVE, MIX, HPF, RES, ENV1-D, DLY TIME** ·
  EDIT LEDs over PERF/PATCH · SHIFT. Shift legends under A–H: MIDI CH, TX/RX E, TX/RX P, DUMP, LOCAL, TUNE, MPE.

## Ribbon controller (SG) [p.77–78]
- Default: **osc pitch**. **Relative**: range depends on finger start — start at bottom & slide to top =
  max bend up; start at top & slide down = max bend down [p.77].
- **Only affects held notes; no pitch effect during envelope release** [p.77].
- Matrix source **7 (RIBN)**. **Assigning ribbon to any matrix destination disables its pitch control;
  clearing all ribbon mappings re-enables it** [p.77].
- Ribbon always **sends CC 2 (MSB) + CC 34 (LSB)**. In default pitch mode it does **not receive** CCs; as a
  matrix source it sends and receives them [p.78].
- **UNCLEAR:** maximum ribbon pitch range (not stated on p.77–78). Check MIDI/NRPN tables.

## Additional controls [p.79–83]
- **MASTER VOLUME** [p.79]: knob legend **0 … 0 dB … +4 dB**; controls main + headphone volume; max +4 dB.
  **Only control not stored with a performance or patch.**
- **TEMPO** [p.79]: **30–300 BPM**, LED flashes at tempo (click). Arp/seq speed. With external MIDI clock,
  TEMPO has no effect → use **CLK DIV** [p.80].
- **Lower Layer Detune (SG only)** [p.80]: **DETUNE** knob (centre detent, −/+): fine tuning of the lower
  layer, **±7 semitones**. Shift function **PERF DETUNE**: detunes the whole performance **±7 semitones**.
  **Stored with the performance.**
- **HOLD** [p.81]: buttons **LOWER** / **UPPER** (LEDs). Notes of that layer sustain after all keys released.
  With arp on: arpeggio keeps playing; releasing all keys then playing a new chord → new arpeggio. With
  seq on: sequence transposes by the keys you play.
- **KEYBOARD** [p.82]: **SINGLE / DUAL / SPLIT** buttons (legend "(NOTE)" under SPLIT).
  - SINGLE: one layer, 20 voices (10 binaural). **Single mode defaults to the upper layer**; UPPER/LOWER
    switches which layer plays.
  - DUAL: layers stacked, 20 voices split evenly (10/10); a binaural layer drops to 5.
  - SPLIT: upper = keys **≥ split point**, lower = keys below; 10/10 voices (5 if binaural). **Hold SPLIT +
    play note** sets split point = first note of upper layer. **Default C4** [p.82].
- In DUAL/SPLIT each layer goes to its own outputs; SINGLE uses both layer engines for full polyphony →
  use MIX outputs [p.83].
- **LAYER** UPPER / LOWER [p.83]: load a patch into that layer / access its patch-related settings.

## Modulation matrix (per layer) [p.84–89]
- Enter with **MOD ASSIGN** (LED flashes). Buttons 1–8 = sources, A–H = destinations [p.84].
- **Sources** [p.84]: 1 DDS 2 · 2 LFO 2 · 3 Envelope 1 · 4 Velocity · 5 Aftertouch · 6 Expression Pedal/CV ·
  7 Ribbon (SG) · 8 Note Number.
- **Matrix destinations** [p.84]: A LFO 1 Rate · B Cross Modulation · C Wave Modulation · D Oscillator Mix ·
  E HPF Cutoff · F LPF Resonance · G Envelope 1 Decay · H Delay Time.
- **MOD AMOUNT** endless encoder: **−100 % … +100 %** per mapping; LEDs show value [p.85].
- Matrix mappings [p.86]: any of 8 sources × any of 8 destinations, **individual amount per pair**. Select
  source then dest, or dest then source; LEDs flash for active routings. PERF/PATCH exits.
- **Direct parameter mappings** [p.87]: hold a source button + move a patch-related continuous control.
  Only continuous controls (not toggle/rotary switches). **Existing hard-wired routings are not duplicated**
  (example: LFO 2 → LPF cutoff is impossible because the perf section's LFO 2 VCF fader already does it).
  **UNCLEAR:** full list of excluded source/dest pairs — only the LFO 2 → cutoff example is given.
- **Direct-mapping destinations per layer (exactly 24)** [p.88]:
  | # | destination | # | destination |
  |---|---|---|---|
  | 1 | DDS 2 Tune | 13 | Envelope 2 Decay |
  | 2 | LPF Cutoff Frequency | 14 | Envelope 2 Sustain |
  | 3 | VCF Envelope Amount | 15 | Envelope 2 Release |
  | 4 | VCF LFO 1 Amount | 16 | LFO 1 Delay |
  | 5 | VCF DDS 2 Amount | 17 | LFO 1 LR Phase |
  | 6 | VCA Envelope Level | 18 | LFO 2 Rate |
  | 7 | VCA LFO 1 Amount | 19 | LFO 2 Delay |
  | 8 | VCA DDS 2 Amount | 20 | DDS Modulator LFO 1 Amount |
  | 9 | Envelope 1 Attack | 21 | DDS 2 Pulse Width / DDS 1 Detune |
  | 10 | Envelope 1 Sustain | 22 | Portamento Time |
  | 11 | Envelope 1 Release | 23 | Delay Send |
  | 12 | Envelope 2 Attack | 24 | Delay Feedback |
  → Total destinations = 8 matrix + 24 direct = **32**. (Prompt said "any continuous control"; manual limits
  it to these 24. Manual wins.)
- **Clearing** [p.89]: all mappings (MOD ASSIGN → push MOD AMOUNT encoder, or SHIFT+MOD ASSIGN); all
  mappings **from a source** (select source → clear); all mappings **to a destination** (select dest → clear).

## Voice assign (per layer) [p.90–91]
- Panel: **MODE** button + LEDs SOLO / LEGATO / POLY 1 / POLY 2 (shift **U. SIZE**) · **UNISON** button (LED) ·
  **BINAURAL** button (LED).
- **SOLO**: mono, envelopes retrigger every note · **LEGATO**: mono, no retrigger when played legato ·
  **POLY 1** (default): full poly per layer, release stages overlap · **POLY 2**: full poly, release of
  overlapping notes curtailed [p.90].
- **UNISON** [p.90]: SOLO/LEGATO → all available voices stack on the note; POLY → available stacked voices
  divided by number of notes currently held.
- **U. SIZE** (SHIFT+MODE; LED count) [p.91]: **1 = half of available voices stacked · 2 = all available
  voices · 3 = all, stacked as octave · 4 = all, stacked as octave + fifth**.
- **BINAURAL** [p.91]: **default ON**. ON → 10 super voices (single) / 5 per layer (dual/split).
  OFF → monaural, 20 (single) / 10 per layer.
- Non-binaural → LR PHASE becomes **SPREAD** (0 = centred, max = alternating hard L/R). **In SOLO/LEGATO
  non-binaural voices are not panned; SPREAD then only sets LFO 1 phase offsets** [p.91].
- True mono per layer = BINAURAL off + SPREAD 0 [p.91].

## Arpeggiator & sequencer (per layer) [p.92–98]
- Panel [p.92 fig]: **ON** button · **CLK DIV** encoder with 8 LEDs **1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/4T, 1/8T** ·
  **SYNC** button (shift **EXT CLK**) · **RANGE** button + LEDs 1–4 (shift **SWING**) · **MODE** button + LEDs
  UP / DOWN / U&D / RANDOM / SEQ (shift **LOAD**) · **SEQ REC** button (shift **STORE**) · **TRACK** button +
  LEDs STEP / SLIDE / ACCENT / REST / LENGTH.
- **CLK DIV** [p.92]: playback speed as division of TEMPO / ext clock; pressing encoder resets to **1/16**.
- **SYNC** [p.92]: LFO 1 rate & delay time follow arp/seq clock (TEMPO or ext); RATE/TIME then pick
  divisions. Off → free-running.
- **EXT CLK** (SHIFT+SYNC) [p.92–93]: button 1 = MIDI clock **TX**; 2 = MIDI clock **RX** (+ Start/Stop);
  3 = reserved; 4 = **MIDI Stop receive** (on: HOLD arms the layer's sequencer to wait for Start, Stop halts
  it; off: HOLD starts/stops asynchronously, Start re-snaps timing, Stop ignored while clock runs). TX and
  RX mutually exclusive. Auto-switches to RX when clock detected. With RX, TEMPO has no effect and arp/seq
  wait for clock. Transport also via note on/off (and transposes the sequence). SHIFT exits.
- **SWING** (SHIFT+RANGE) [p.93]: **5 settings: off, 1 (subtle) … 4 (pronounced)**.
- **Arp** [p.94]: **ON** toggles arp (unless MODE = SEQ → sequencer). **RANGE 1–4**: held notes + 0/1/2/3
  octaves above. **MODE**: UP · DOWN · U&D (low→high→low) · RANDOM · SEQ. Changing modes while arp is on
  **skips SEQ**. HOLD keeps arpeggio running; new chord after full release → new arpeggio.
- **Sequencer** [p.95]: up to **64 steps**; programmable **step, slide, accent, rest, sequence length**.
  **16 sequences** stored/recalled; a sequence can be **linked to a patch when saved**, but sequencer memory
  is independent. Buttons 1–8 + A–H = 16 steps; **4 pages** shown by RANGE LEDs; auto page advance.
- Seq **ON** [p.96]: on/off (arp if MODE is an arp mode). **RANGE** in seq mode = page 1–4 (steps 1–16,
  17–32, 33–48, 49–64); page 1 default.
- **SEQ REC** [p.96]: arms recording (LED flashes). **TRACK** cycles 5 tracks [p.96–97]:
  - **STEP** (default in rec): press start step (LED flashes); play note/chord; **recorded when all keys
    released**; advances. Press a step to re-record it. Recorded steps lit.
  - **SLIDE**: tie adjacent steps (press both); with portamento > 0 a pitch slide happens between tied notes
    of different pitch. Lit = tie.
  - **ACCENT**: accented steps get more level & brightness **if DYNAMICS is ½ or ON**. Lit = accent.
  - **REST**: step skipped; **unlit = rest**.
  - **LENGTH**: pick page then last step; sequence loops after it.
- In rec with TRACK = STEP, **bender position can be recorded** per step [p.97].
- Sequencer running + SEQ REC flashing → remaining (unused) voices playable live [p.97].
- **HOLD + seq: transpose relative to middle C (C4)** [p.97].
- **Clear sequence**: hold SEQ REC + press MOD ASSIGN [p.98].
- **LOAD** (SHIFT+MODE): pick 1 of 16 via 1–8/A–H. **STORE** (SHIFT+SEQ REC): hold slot button 3 s [p.98].

## Global settings [p.99–101]
- SHIFT enters/exits. **Auto-saved after 30 s idle** [p.99].
- **MIDI CH**: base channel 1–16 = **upper layer; base+1 = lower layer** [p.99].
- **TX/RX E** [p.99–100]: button 1 **CC transmit** · 2 **CC receive** · 3 **NRPN mode** (14-bit, requires CC TX).
  CC receive accepts both 7- and 14-bit.
- **TX/RX P** [p.100]: 1 program change TX · 2 program change RX.
- **DUMP**: reserved · **LOCAL**: local control on/off · **TUNE**: filter calibration (hardware-only) ·
  **MPE**: reserved [p.100].
- **GLOBAL RESET**: hold MANUAL UPPER or LOWER 5 s at power-on; saved data unaffected [p.101].

## File management [p.102–107]
- Drive layout [p.102–103]: `performances_a/` & `performances_b/` (64 each; bank folders a1…h1, 8 files
  each) · `patch_banks_a/` & `patch_banks_b/` (128 each; bank folders a1…h2) · `waveforms/` (alt group 1,
  16 files) · `alt_waveforms/` (alt group 2, 16 files) · `sequences/` (16) · `init_patch.usg` at root.
- Names [p.103]: `x1_name.xsg` performance · `p1_name.usg` patch · `w1_name.ws6` waveform ·
  `s1_name.qs6` sequence. Digit = slot (1–8 perf/patch; 1–16 waves/seqs). Use underscores, not spaces.
- Imports are drag-and-drop into bank folders; renaming the prefix sets the slot [p.104].

## How-to [p.108–110]
- **Custom init patch** [p.108]: file `init_patch*.usg`. If no init file: load last active patch on power-up;
  if no patch file at all: start in manual mode.
- Waveform packs replace the `waveforms` or `alt_waveforms` folder wholesale [p.109].
- **Alt waveform format (.ws6)** [p.109]: **16-bit signed integer, single cycle, 4096 points (8192 bytes),
  normalised, band-limited to fs/8 (≤ 512 harmonics), raw binary, no header**. **UNCLEAR:** byte order —
  assume little-endian (verify against a factory .ws6 if the user has one).
- DAW setup [p.110]: two MIDI tracks — upper on channel N (e.g. 1), lower on N+1 (e.g. 2); two audio
  tracks from the UPPER/LOWER outs, or one from the MIX outs.

## Cheat sheet [p.111–114]
- Shortcuts [p.111]: SHIFT+PERF = perf group B · SHIFT+PATCH = patch group B · SHIFT+MANUAL UPPER/LOWER = load
  init patch into that layer · hold 1–8 = store perf/patch · hold source 1–8 in mod-assign + move control =
  direct mapping · SHIFT+MOD ASSIGN = clear all / from selected source / to selected dest · hold SPLIT + note
  = split point · SEQ REC+MOD ASSIGN = clear sequence · hold MANUAL UPPER/LOWER 5 s = global reset.
- **Secondary functions (complete list)** [p.112]:
  | control | condition | secondary |
  |---|---|---|
  | DDS 2 Mode | DDS 2 RANGE = LFO | Sub-osc: Off / Square / Sine |
  | LR Phase | Binaural off | Pan Spread |
  | LFO 1 Mode | LFO 1 wave = HF / HF TRK | Routing: Norm / DDS 1 / DDS 2 |
  | PW/Detune | Shift | Drift |
  | Mix | Shift | Pan |
  | Env 1 Attack | Shift | Env 1 Attack Hold |
  | Env 1 Decay | Shift | Env 1 Decay Hold |
  | Env 2 Decay | Shift | Env 2 Decay Hold |
  | DDS 2 Tune | Shift | Layer Detune ±7 st (**Super 8 only** — not SG) |
  | Detune (lower) | Shift | Performance Detune ±7 st (SG only) |
  | Voice Assign Mode | Shift | Unison Size 1 half / 2 all / 3 octave / 4 fifth+octave |
  | Arp Range | Shift | Swing 0 (none) / 1 / 2 / 3 / 4 |
  | Arp Mode | Shift | Load sequence 1–16 |
  | Seq Rec | Shift | Store sequence 1–16 |
- Select buttons [p.113]: Perf/Patch/ModAssign/Wave (alt wave 1–32)/Sequencer steps/Load/Store seq.
- Global (shift) [p.114]: MIDI CH 1–16 · TX/RX E (CC TX, CC RX, NRPN) · TX/RX P (PC TX, PC RX) · DUMP (res.) ·
  LOCAL · TUNE (filter auto-tune) · MPE (res.) · EXT CLK (clock TX, clock RX, res., Stop RX) · octave toggle =
  global transpose ±12 st · FINE ADJ = global fine ±100 ct.

## MIDI implementation [p.115–128]
- Realtime [p.115]: Timing Clock, Start, Stop — TX & RX.
- Channel [p.115]: Note Off/On, **Polyphonic Key Pressure**, Control Change (per global settings), Program
  Change (per global settings), Channel Pressure, Pitch Bend — all TX & RX.

### CC table (verbatim transcription) [p.116–122]
Stepped values: the listed number is the value transmitted; on receive, treat each as the lower bound of
its band (e.g. 0–20 → first, 21–42 → second …). **UNCLEAR:** receive banding isn't stated; lower-bound
banding is the natural reading of 0/21/43/64/85/107 and 0/43/85.

| CC | values | parameter | page |
|---|---|---|---|
| 0 | 0–127 | Bank Select | 116 |
| 1 | 0–127 | Modulation Lever | 116 |
| 2 | 0–127 | Ribbon Coarse | 116 |
| 3 | 0–127 | Tempo | 116 |
| 4 | 0–127 | Foot Controller | 116 |
| 5 | 0–127 | Portamento Time | 116 |
| 6 | 0–127 | Data Entry MSB | 116 |
| 7 | 0–127 | VCA Envelope Level | 116 |
| 8 | – | – | 116 |
| 9 | 0–127 | Mod Amount/Fine Adjust | 116 |
| 10 | 0–127 | Pan | 116 |
| 11 | 0–127 | Expression | 116 |
| 12 | 0–127 | Delay Time | 116 |
| 13 | 0–127 | Delay Feedback | 116 |
| 14 | 0–15 | Sequence Load | 116 |
| 15 | – | – | 116 |
| 16 | 0 Triangle · 21 Rev Sawtooth · 43 Random · 64 Square · 85 HF · 107 HF TRK | LFO 1 Waveform/HF Mode | 116 |
| 17 | 0–127 | LFO 1 Rate | 116 |
| 18 | 0–127 | LFO 1 Delay | 116 |
| 19 | 0–127 | LFO 1 LR Phase/Pan Spread | 116 |
| 20 | 0 Free/Norm · 43 Once/DDS 1 · 85 Reset/DDS 2 | LFO 1 Mode | 117 |
| 21 | 0–127 | DDS LFO 1 Amount | 117 |
| 22 | 0–127 | DDS Envelope 1 Amount | 117 |
| 23 | 0 DDS 1 · 43 Both · 85 DDS 2 | DDS Modulator Destination | 117 |
| 24 | 0 Off · 43 1/2 · 85 On | Super Mode | 117 |
| 25 | 0–127 | PW/Detune | 117 |
| 26 | 0–127 | PWM/Wave Modulation | 117 |
| 27 | 0 Manual · 43 LFO 1 · 85 ENV 1 | PWM/Wave Modulation Source | 117 |
| 28 | 0–127 | Cross Modulation | 117 |
| 29 | 0 Sine · 21 Sawtooth · 43 Square · 64 Triangle · 85 White Noise · 107 Alternative Waveform | DDS 1 Waveform | 117 |
| 30 | 0 64' · 21 32' · 43 16' · 64 8' · 85 4' · 107 2' | DDS 1 Range | 117 |
| 31 | 0 Sine · 21 Sawtooth · 43 Square · 64 Triangle · 85 White Noise · 107 Pulse | DDS 2 Waveform | 117 |
| 32 | 0–127 | Envelope 1 Decay Hold | 117 |
| 33 | 0–127 | Envelope 2 Decay Hold | 117 |
| 34 | 0–127 | Ribbon Fine | 117 |
| 35 | 0–127 | DDS 2 Tune | 117 |
| 36 | 0 Norm/Sub Osc Off · 43 Ring/Sub Osc Square · 85 Sync/Sub Osc Sine | DDS 2 Mode | 117 |
| 37 | 0–127 | Oscillator Mix | 118 |
| 38 | 0–127 | LSB for Control 6 (Data Entry) | 118 |
| 39 | – | – | 118 |
| 40 | – | – | 118 |
| 41 | 0 Off · 43 1 · 85 2 | VCF Drive | 118 |
| 42 | 0–127 | VCA LFO 2 Amount | 118 |
| 43 | 0 Off · 43 1/2 · 85 On | VCF Keytrack | 118 |
| 44 | 0 ENV 1 · 43 1 + 2 · 85 ENV 2 | VCF Envelope Source | 118 |
| 45 | 0–127 | VCF Envelope Amount | 118 |
| 46 | 0–127 | VCF LFO 1 Amount | 118 |
| 47 | 0–127 | VCF DDS 2 Amount | 118 |
| 48 | 0 Off · 43 1/2 · 85 On | VCA Dynamics | 118 |
| 49 | 0 ENV 2 · 43 Fixed Envelope 1 · 85 Fixed Envelope 2 | VCA Envelope Mode | 118 |
| 50 | 0 Normal · 43 Inverted · 85 Loop | Envelope 1 Mode | 118 |
| 51 | 0 Off · 43 1/2 · 85 On | Envelope 1 Keytrack | 118 |
| 52 | 0–127 | Envelope 1 Attack Hold | 118 |
| 53 | 0–127 | Envelope 1 Attack | 118 |
| 54 | 0–127 | Envelope 1 Decay | 118 |
| 55 | 0–127 | Envelope 1 Sustain | 118 |
| 56 | 0–127 | Envelope 1 Release | 118 |
| 57 | 0–127 | Envelope 2 Decay | 118 |
| 58 | 0–127 | Envelope 2 Sustain | 119 |
| 59 | 64 = On | Manual Mode | 119 |
| 60 | 0 Trig · 43 AT + Trig · 85 LFO 2 On/AT->Bend | LFO 2 Trigger Source | 119 |
| 61 | 0 DDS 1 · 43 1 + 2 · 85 DDS 2 | Performance Control Destination | 119 |
| 62 | 0–127 | LFO 2 Rate | 119 |
| 63 | 0–127 | LFO 2 Delay | 119 |
| 64 | 0 Off · 64 On | Sustain Pedal | 119 |
| 65 | 0 Both · 43 Lower · 85 Upper | Portamento Layer Select | 119 |
| 66 | – | – | 119 |
| 67 | 0 −2 · 26 −1 · 51 0 · 77 +1 · 102 +2 | Octave Select | 119 |
| 68 | – | – | 119 |
| 69 | 0 Off · 64 On | Delay Freeze | 119 |
| 70 | 0–127 | DDS LFO 2 Amount | 119 |
| 71 | 0–127 | VCF Resonance | 119 |
| 72 | 0–127 | Envelope 2 Release | 119 |
| 73 | 0–127 | Envelope 2 Attack | 119 |
| 74 | 0–127 | VCF Cutoff Frequency | 119 |
| 75 | 0–127 | VCF LFO 2 Amount | 119 |
| 76 | 0–127 | DDS Pitch Bend Amount | 119 |
| 77 | 0–127 | VCF Pitch Bend Amount | 119 |
| 78 | 0 Solo · 32 Legato · 64 Poly 1 · 96 Poly 2 | Voice Assign Mode | 120 |
| 79 | 1 Half of all available voices · 2 All available voices · 3 Octave · 4 Fifth + octave | Unison Size | 120 |
| 80 | 0 Off · 64 On | Binaural mode | 120 |
| 81 | 0 Off · 64 On | Clock Sync | 120 |
| 82 | 0 1 octave · 32 2 octaves · 64 3 octaves · 96 4 octaves | Arpeggiator Range | 120 |
| 83 | 0 Swing 0 · 26 Swing 1 · 51 Swing 2 · 77 Swing 3 · 102 Swing 4 | Arpeggiator/Sequencer Swing | 120 |
| 84 | 0 Off · 64 On | Arpeggiator/Sequencer External Clock | 120 |
| 85 | 0 Up · 26 Down · 51 Up & Down · 77 Random · 102 Sequencer | Arpeggiator/Sequencer Mode | 120 |
| 86 | 0 Off · 64 On | Arpeggiator/Sequencer On/Off | 120 |
| 87 | 0 Off · 64 On | Arpeggiator/Sequencer Hold | 120 |
| 88 | – | – | 120 |
| 89 | – | – | 120 |
| 90 | – | – | 120 |
| 91 | 0–127 | Delay Send | 120 |
| 92 | 0–127 | VCA LFO 1 Amount | 120 |
| 93 | 0 Off · 32 Chorus 1 · 64 Chorus 2 · 96 Chorus 1 & 2 | Chorus | 120 |
| 94 | 0–127 | Drift | 120 |
| 95 | 0–127 | HPF Cutoff Frequency | 121 |
| 96 | – | Data Increment | 121 |
| 97 | – | Data Decrement | 121 |
| 98 | 0–127 | NRPN LSB | 121 |
| 99 | 0–127 | NRPN MSB | 121 |
| 100 | 0–127 | RPN LSB | 121 |
| 101 | 0–127 | RPN MSB | 121 |
| 102 | – | – | 121 |
| 103 | – | – | 121 |
| 104 | 0 Both · 43 Lower · 85 Upper | Modulation Layer | 121 |
| 105 | 0 Sine · 21 Rev Sawtooth · 43 Sample & Hold · 64 Square · 85 Sawtooth · 107 Noise | LFO 2 Waveform | 121 |
| 106 | 0 LFO · 21 32' · 43 16' · 64 8' · 85 4' · 107 2' | DDS 2 Range | 121 |
| 107 | 0 Whole · 16 Half · 32 Quarter · 48 Eighth · 64 Sixteenth · **70** Thirty-second · 96 Quarter triplets · 102 Eighth triplets | Clock Divider | 121 |
| 108 | 0–127 | Lower Layer Detune (SG) / Layer Detune (S8) | 121 |
| 109 | 0–127 | VCA DDS 2 Amount | 121 |
| 110 | 0–127 | LFO 2 Rate Modulation | 121 |
| 111 | 0–127 | Performance Detune (Super 8: receive only) | 121 |
| 112–119 | – | – | 121–122 |
| 120 | 0 | All Sound Off | 122 |
| 121 | 0 | Reset All Controllers | 122 |
| 122 | 0 Off · 64 On | Local Control On/Off | 122 |
| 123 | 0 | All Notes Off | 122 |
| 124 | 0 | Omni Mode Off | 122 |
| 125 | 0 | Omni Mode On | 122 |
| 126 | 0 | Mono Mode On | 122 |
| 127 | 0 | Poly Mode On | 122 |

CC notes:
- CC 107: printed "70 = Thirty-second notes" breaks the 16-step spacing (0,16,32,48,64,**70**,96,102).
  Transcribed **as printed**; receive banding must still map 70–95 → 1/32. Flag for user.
- CC 16 calls S&H "Random". CC 1 = "Modulation Lever" (= bender vertical push). CC 4 "Foot Controller"
  vs CC 11 "Expression" — **UNCLEAR** which is the VOL pedal and which the EXPR pedal (matrix source 6).
  Assumption: CC 11 = expression pedal (matrix source), CC 4 = volume pedal.
- Layer addressing: upper layer = base channel, lower = base+1 (p.99), so the same CC hits different layers.
- No CC for: master volume, keyboard mode, split point, layer hold (except 87 arp hold), manual-mode exit,
  env 1 ... all covered above; alt waveform A/B slot selection has no CC (see NRPN).

### RPN [p.123]
| RPN | MSB (CC101) | LSB (CC100) | data | parameter |
|---|---|---|---|---|
| 0 | 00H | 00H | MSB = ±12 semitones | Pitch Bend Sensitivity |
| 1 | 00H | 01H | 00H 00H = −100 ct · 40H 00H = A440 · 7FH 7FH = +100 ct | Channel Fine Tuning |
| 2 | 00H | 02H | MSB only: 00H = −12 st · 40H = A440 · 7FH = +12 st | Channel Coarse Tuning |

### NRPN — global [p.123]
| NRPN | MSB (CC99) | LSB (CC98) | values | parameter |
|---|---|---|---|---|
| 2051 | 10H | 03H | 0 = ch 1 … 15 = ch 16 | MIDI Channel |
| 2052 | 10H | 04H | 0 Off · 1 On | MIDI Clock Transmit |
| 2053 | 10H | 05H | 0 Off · 1 On | MIDI Clock Receive |
| 2055 | 10H | 07H | 0 Off · 1 On | MIDI Program Change Transmit |
| 2056 | 10H | 08H | 0 Off · 1 On | MIDI Program Change Receive |

### NRPN — patch-related [p.124–128]
All 14-bit (0–16383), MSB = 08H, **LSB = the matching CC number** → NRPN = 1024 + CC#. Unlisted numbers
in 1024…(end) are "–" (unused).
| NRPN | LSB | parameter | page |
|---|---|---|---|
| 1025 | 01H | Modulation Lever | 124 |
| 1027 | 03H | Tempo | 124 |
| 1029 | 05H | Portamento Time | 124 |
| 1031 | 07H | VCA Envelope Level | 124 |
| 1033 | 09H | Mod Amount/Fine Adjust | 124 |
| 1034 | 0AH | Pan | 124 |
| 1035 | 0BH | Expression | 124 |
| 1036 | 0CH | Delay Time | 124 |
| 1037 | 0DH | Delay Feedback | 124 |
| 1041 | 11H | LFO 1 Rate | 124 |
| 1042 | 12H | LFO 1 Delay | 124 |
| 1043 | 13H | LFO 1 LR Phase/Spread | 124 |
| 1045 | 15H | DDS LFO 1 Amount | 124 |
| 1046 | 16H | DDS Envelope 1 Amount | 124 |
| 1049 | 19H | PW/Detune | 125 |
| 1050 | 1AH | PWM/Wave Modulation | 125 |
| 1052 | 1CH | Cross Modulation | 125 |
| 1056 | 20H | Envelope 1 Decay Hold | 125 |
| 1057 | 21H | Envelope 2 Decay Hold | 125 |
| 1059 | 23H | DDS 2 Tune | 125 |
| 1061 | 25H | Oscillator Mix | 125 |
| 1066 | 2AH | VCA LFO 2 Amount | 125 |
| 1069 | 2DH | VCF Envelope Amount | 125 |
| 1070 | 2EH | VCF LFO 1 Amount | 125 |
| 1071 | 2FH | VCF DDS 2 Amount | 125 |
| 1076 | 34H | Envelope 1 Attack Hold | 126 |
| 1077 | 35H | Envelope 1 Attack | 126 |
| 1078 | 36H | Envelope 1 Decay | 126 |
| 1079 | 37H | Envelope 1 Sustain | 126 |
| 1080 | 38H | Envelope 1 Release | 126 |
| 1081 | 39H | Envelope 2 Decay | 126 |
| 1082 | 3AH | Envelope 2 Sustain | 126 |
| 1086 | 3EH | LFO 2 Rate | 126 |
| 1087 | 3FH | LFO 2 Delay | 126 |
| 1094 | 46H | DDS LFO 2 Amount | 126 |
| 1095 | 47H | VCF Resonance | 126 |
| 1096 | 48H | Envelope 2 Release | 126 |
| 1097 | 49H | Envelope 2 Attack | 126 |
| 1098 | 4AH | VCF Cutoff Frequency | 126 |
| 1099 | 4BH | VCF LFO 2 Amount | 126 |
| 1100 | 4CH | DDS Pitch Bend Amount | 126 |
| 1101 | 4DH | VCF Pitch Bend Amount | 127 |
| 1115 | 5BH | Delay Send | 127 |
| 1116 | 5CH | VCA LFO 1 Amount | 127 |
| 1118 | 5EH | Drift | 127 |
| 1119 | 5FH | HPF Cutoff Frequency | 127 |
| 1132 | 6CH | Lower Layer Detune (SG) / Layer Detune (S8) | 128 |
| 1133 | 6DH | VCA DDS 2 Amount | 128 |
| 1134 | 6EH | LFO 2 Rate Modulation | 128 |
| 1135 | 6FH | Performance Detune (Super 8: receive only) | 128 |
- Table ends at 1135; manual points to udo-audio.com downloads for the latest MIDI spec [p.128].
- Note: NRPN list only covers continuous controls (14-bit versions of the continuous CCs). Switches are
  CC-only. **No MIDI address exists for alt-waveform slot selection, matrix amounts, split point, keyboard
  mode, or sequencer steps** — these are plugin-state only (host automation still exposes the switches).

## Pages not relevant to the plugin
- vii–x safety/acknowledgements, 27 firmware update, 106–107 backups (hardware drive), 129–134 glossary &
  support — skimmed via TOC only; no parameters.
