# Geminus FX — build prompts

One prompt per effect. Each was written before its effect was built. The effect is then built
against that prompt and checked against it. They are **light versions** that sound in the same
spirit as the plug-in they are named after. They are not clones, and they never use that
plug-in's name, logo or artwork in the UI.

## Shared rules (apply to every effect)

**Rack**
- Three slots: FX 1, FX 2 and FX 3.
- Routing is SERIAL (1 → 2 → 3) or PARALLEL. In PARALLEL, each slot hears the dry signal and the
  wet differences are summed.
- Every slot is processed **per layer**: UPPER and LOWER each run their own copy of the effect.

**Layer fader** (one under each slot, parked in the middle)
- It works like the DDS 1 / DDS 2 mix knob, between UPPER and LOWER.
- In the middle, both layers get 100 % of the effect.
- Moving toward UPPER keeps UPPER at 100 % and fades LOWER to dry.
- Moving toward LOWER does the reverse.
- The maths is `amtUpper = min(1, 2·(1−f))` and `amtLower = min(1, 2·f)`.
- Each layer's output is `dry + amt · (effect(dry) − dry)`.

**Slot panel look** (after the user's reference screenshot, using our assets)
- The slot is a black inset panel.
- The header has:
  - a power button (the photographed button with its LED);
  - the effect name as a clickable word with ▾, which opens the grouped type picker.
- A live display (a canvas "screen") sits beside a vertical DRY / WET fader (fader asset).
- Two rows of up to four knobs use the knob assets.
- Up to three stepped selectors are printed as clickable words that cycle through their values.

**Parameters**
- Each slot has host parameters `fxN.type`, `fxN.on`, `fxN.mix` (dry/wet), `fxN.layer` (the layer
  fader) and `fxN.p1…p28`, all normalised 0…1.
- The meaning of each `p` comes from `ui/fxdefs.mjs`. It is the single source of truth: the
  generator writes `src/plugin/FxDefs.h` from it, so the UI read-outs and the DSP ranges can never
  drift apart.
- Choosing a type in the picker resets that slot's `p`s and mix to the type's defaults.

**Safety**
- No allocation on the audio thread. Effects are built on the message thread and swapped in
  lock-free, with a 10 ms fade-in.
- Outputs are denormal-safe.
- Feedback paths are soft-clipped, so no setting can run away to infinity.

---

## Time based

### 1. PS DELAY
*Inspiration: the pitch-shifting delay in the reference screenshot.*

This is a stereo delay with a **pitch shifter inside the feedback loop**, so every repeat climbs
(or falls) by the chosen interval.

**Controls**
- **MODE** selects the interval:
  - OCT UP (+12);
  - OCT DOWN (−12);
  - FIFTH (+7);
  - OCT UP+DOWN (alternating repeats);
  - DETUNE (±15 cents).
- **TIME**: 10 ms – 2 s, log. **SYNC** (FREE / SYNC) switches TIME to 1/32 … 1/1 with dotted and
  triplet values.
- **STEREO OFFSET**: the right channel's time is ±50 % of the left's.
- **FEEDBACK**: 0 – 95 %.
- **STEREO DETUNE**: 0 – 25 cents, opposite in L and R.
- **PITCH SHIFT**: how much of the fed-back signal is shifted, 0 – 100 %. The rest recirculates
  unshifted.
- **SPRAY**: random jitter of each repeat's delay time and pan.
- **HP FREQ**: 20 Hz – 2 kHz. **LP FREQ**: 1 – 20 kHz. Both are in the loop, so repeats darken or
  thin as they go.

**DSP.** A stereo delay line feeds a granular pitch shifter (two taps crossfaded over 60 ms
windows), then the HP/LP one-poles, then tanh-soft feedback.

**Display.** A row of dots, one per audible repeat. Each dot's size and brightness is the repeat's
level, and its vertical offset is its pitch. It matches the screenshot.

### 2. OCEANA
> **Removed (user, 2026-10-05):** a reverb the rack did not need. Saved slots that held it load empty;
> later types moved up one place (the processor converts states saved before, see `fxTypes`).

*Inspiration: Arturia Rev OCEAN, a "tidal" FDN reverb with three motion modes.*

This is an 8-line feedback-delay-network reverb (Hadamard mixing) with modulated delay lengths.

**Controls**
- **MODE** (the macro is its intensity):
  - ABYSS mixes a reverse, octave-up pitch-shifted copy of the tail back in, for an evolving,
    shimmering depth.
  - TIDE sweeps a slow band-pass through the tail, for a phase-like spectral motion.
  - FOAM puts a diffusion stage before the tank, so the attack blurs into a swell.
- **FREEZE**: feedback goes to 1, the input is muted and damping is off.
- **SIZE**: scales the line lengths, 0.3× – 2×.
- **DECAY**: 0.3 – 20 s RT60.
- **BRIGHTNESS**: in-loop damping, 1.5 – 18 kHz.
- **MACRO**: the intensity of the current mode.
- **PRE-DELAY**: 0 – 250 ms.
- **WIDTH**: 0 – 150 %, M/S.
- **DUCKING**: an envelope follower on the dry signal turns the wet down.
- **LOW CUT**: an input high-pass, 20 – 1000 Hz.

**Display.** The decay envelope, drawn as a wave-like tail. Its colour ripple shows the mode.

### 3. TAPE ECHO
*Inspiration: the classic three-head tape echo (RE-201 family).*

A single tape loop has three playback heads at 1×, 2× and 3× the base spacing.

**Controls**
- **HEADS**: 1, 2, 3, 1+2, 2+3, 1+3 or 1+2+3.
- **RATE**: the head-1 spacing, 60 – 600 ms. It is smoothed, so turning it pitches the echoes like
  real tape.
- **INTENSITY**: the feedback, 0 – 110 %. Above 100 % it self-oscillates, but it is
  saturation-limited.
- **BASS / TREBLE**: ±12 dB shelves on the echoes.
- **WOW** (slow) and **FLUTTER** (fast): pitch modulation depth.
- **SATURATION**: tape drive, tanh.
- **AGE**: high-frequency loss plus hiss.

**Display.** A tape line with three head blocks. The active heads are lit, and a moving tick shows
the tape speed.

### 4. CONVOLVER
Real convolution reverb using the user's own impulse responses.

- **Drop a WAV onto the slot display** to load it. Mono or stereo is accepted, trimmed to 8 s and
  resampled to the session rate.
- The IR travels with the patch or project. Its name shows in the display.
- The default IR is a synthetic 2.2 s stereo hall (decaying filtered noise), so the slot works
  before anything is dropped.

**Controls**
- **PRE-DELAY**: 0 – 200 ms.
- **LENGTH**: trims the IR, 5 – 100 %, with a fade-out.
- **LOW CUT**, **HIGH CUT**: on the wet signal.
- **WIDTH**: M/S.
- **OUTPUT**: ±12 dB.
- **REVERSE** (OFF / ON): plays the IR backwards.

**DSP.** `juce::dsp::Convolution`: uniform partitioned, with IR loading on a background thread.

**Display.** The IR waveform with its file name, and "DROP A WAV HERE" when hovered.

---

## Distortion

### 5. TUBO
*Inspiration: Analog Obsession TUBA, a tube mic/line amp with a 2-band EQ.*

**Controls**
- **MODE**: MIC (more gain, asymmetric, 2nd-harmonic rich) or LINE (cleaner, symmetric).
- **PAD**: −20 dB before the tube stage, for headroom.
- **GAIN**: the tube drive range, 0 – 30 dB.
- **LEVEL**: the input drive, compensated so loudness stays about the same.
- **LF**: a broad ±10 dB shelf at 100 Hz.
- **HF**: a broad ±10 dB shelf at 8 kHz.
- **OUTPUT**: ±18 dB.

**DSP.** Asymmetric tube curve `(tanh(k·(x+b)) − tanh(k·b))`, then a DC blocker, then the
shelves, with 2× oversampling.

**Display.** The transfer curve, with a live input dot.

### 6. SATURNE
*Inspiration: FabFilter Saturn 2, multiband saturation with styles.*

The signal is split into three bands by 4th-order Linkwitz-Riley crossovers, then each band is
saturated.

**Controls**
- **STYLE**: WARM TUBE, CLEAN TUBE, TAPE, TRANSFORMER, AMP, RECTIFY or BREAK (hard fold).
- **LOW XOVER**: 50 – 1000 Hz.
- **HIGH XOVER**: 1 – 12 kHz.
- **DRIVE LOW / DRIVE MID / DRIVE HIGH**: 0 – 36 dB each.
- **DYNAMICS**: −100 … +100 %. Positive restores transients (expands) and negative compresses.
- **TONE**: a post tilt, ±6 dB.
- **OUTPUT**: ±18 dB.

**Display.** The spectrum split into three boxes, each as tall as its drive. The crossover
positions are marked on a log axis.

### 7. DISTORTION
A straightforward drive box.

**Controls**
- **TYPE**: SOFT (tanh), HARD (clip), FOLD (wavefolder), FUZZ (asymmetric with gating), DIODE.
- **DRIVE**: 0 – 48 dB.
- **BIAS**: asymmetry.
- **TONE**: a post low-pass, 800 Hz – 20 kHz.
- **LOW CUT**: a pre high-pass, 20 – 800 Hz.
- **OUTPUT**: ±18 dB.

It uses 2× oversampling.

**Display.** The transfer curve, with a live input dot.

### 8. BITCRUSHER
**Controls**
- **BITS**: 1 – 16, continuous (fractional bits are allowed).
- **RATE**: sample-and-hold, 500 Hz – 48 kHz, log.
- **JITTER**: randomises the hold period.
- **DRIVE**: 0 – 24 dB, before the quantiser.
- **TONE**: a post low-pass.
- **OUTPUT**: ±18 dB.

**Display.** A staircase sine, showing the current bits and rate.

---

## Dynamics

### 9. WULF COMP
*Inspiration: Goodhertz Vulf Compressor, the aggressive lo-fi pumping compressor.*

**Controls**
- **INPUT**: ±18 dB.
- **COMPRESS**: 0 – 100 %. The ratio rises from 2:1 to 20:1 while the threshold falls. Even at 0 %
  a little colour remains.
- **ATTACK**: 0.1 – 30 ms.
- **RELEASE**: 20 – 800 ms. Fast settings give the signature pump.
- **WOW/FLUTTER**: a vinyl-style pitch wobble.
- **LO-FI**: harmonic distortion, band-limiting (SR reduction plus low-pass) and noise.
- **OUTPUT**: ±18 dB.

**Display.** A gain-reduction history trace (the "pump"), scrolling.

### 10. FARADAE LIMITER
*Inspiration: Goodhertz Faraday Limiter, a vari-mu style colourful limiter.*

**Controls**
- **THRESHOLD**: −40 … 0 dB.
- **RATIO**: 2 … 40:1.
- **ATTACK**: 0.05 – 50 ms.
- **RELEASE**: 20 ms – 2 s, program-dependent (slower after long reduction).
- **COLOR**: a saturation amount that grows with gain reduction.
- **WARMTH**: a low-shelf / high-cut tilt.
- **VIBE**: a gentle 2nd-harmonic tube asymmetry.
- **OUTPUT**: ±18 dB.
- **AUTO GAIN** (OFF / ON).

**Display.** The GR history plus a static knee curve.

### 11. MULTIBAND COMP
*Inspiration: the MULTIBAND slot in the reference screenshot (3-band, OTT-like).*

The signal is split into three LR4 bands. Each band has an upward and a downward compressor.

**Controls**
- **MODE**: ABOVE & BELOW (both), ABOVE (downward only) or BELOW (upward only).
- **AMOUNT**: depth, 0 – 100 %.
- **INPUT**, **OUTPUT**: ±18 dB.
- **ATTACK**: 0.1 – 100 ms.
- **RELEASE**: 10 – 1000 ms.
- **OUT LOW / OUT MID / OUT HIGH**: ±12 dB per band.
- **LOW XOVER** (40 – 1000 Hz) and **HIGH XOVER** (1 – 16 kHz) are dragged on the display, like
  the screenshot's 150 Hz and 2500 Hz boxes.

**Display.** Three bands, each with a bar that shows its live gain change.

---

## Modulation

### 12. PHASER STEREO PAN
A stereo phaser feeding an auto-panner. The panner can be locked to the phaser LFO.

**Controls**
- **STAGES**: 4, 6, 8 or 12 all-pass stages.
- **RATE**: 0.02 – 10 Hz.
- **DEPTH**.
- **FEEDBACK**: −95 … +95 %.
- **CENTER**: 100 Hz – 4 kHz.
- **SPREAD**: the L/R LFO phase, 0 – 180°.
- **PAN RATE**: 0.05 – 10 Hz.
- **PAN DEPTH**.
- **PAN LINK** (FREE / LINKED): LINKED makes the panner follow the phaser LFO.

**Display.** Both LFOs as traces, plus a moving pan dot.

### 13. FLANGER
*After the reference screenshot's FLANGER slot.*

**Controls**
- **RATE**: 0.02 – 10 Hz.
- **DELAY**: base delay, 0.1 – 10 ms.
- **DEPTH**.
- **FEEDBACK**: −95 … +95 %.
- **HP FREQ**, **LP FREQ**: in the feedback path.
- **MONO / STEREO**: STEREO puts the right LFO 90° apart.
- **LFO SHAPE**: SINE or TRIANGLE.

**Display.** Two phase-offset LFO traces, with a shaded area between them, as in the screenshot.

### 14. STEREO PAN
An auto-panner and stereo widener.

**Controls**
- **RATE**: 0.05 – 20 Hz, or synced 1/32 … 4 bars with **SYNC** (FREE / SYNC).
- **DEPTH**.
- **SHAPE**: a morph from sine through triangle to square.
- **PHASE**: an offset in degrees, for manual placement at depth 0.
- **WIDTH**: 0 – 200 %, M/S.
- **HAAS**: 0 – 20 ms delay on one side.

**Display.** The pan position as a dot moving left and right over the LFO shape.

---

## Filter / EQ

### 15. PRO-EQ (light)
*Inspiration: FabFilter Pro-Q 4, a graph-first parametric EQ.*

There are six band slots, and they all start off: nothing is preloaded.

**On the graph**
- Double-click the graph to add a band there. A band dropped at the very low end starts as a
  low cut, and one at the very high end starts as a high cut.
- Drag a band to set its frequency, and its gain if its shape has one.
- The mouse wheel sets Q.
- Right-click a band to choose its shape from a menu: bell, low shelf, high shelf, low cut
  (12 / 24 / 48 dB per octave), high cut (12 / 24 / 48 dB per octave), notch or band pass. The
  same menu can delete the band.
- Double-click a band to remove it.

**Controls**
- The floating bar shows the selected band's shape, FREQ, GAIN and Q, plus OUTPUT (±12 dB).

**DSP.** RBJ biquads. The cuts are cascaded sections, the first at the band's Q.

**Display.** The summed response, plus each band's own curve faintly, with coloured, numbered
nodes.

### 16. FILTER
A multimode filter with its own modulation.

**Controls**
- **TYPE**: LP, HP, BP or NOTCH.
- **SLOPE**: 12 or 24 dB.
- **CUTOFF**: 20 Hz – 20 kHz.
- **RESONANCE**.
- **DRIVE**.
- **LFO RATE**: 0.02 – 20 Hz.
- **LFO DEPTH**: ±4 oct.
- **ENV**: an envelope follower to cutoff, ±4 oct.

**DSP.** A TPT state-variable filter (Simper). It is stable under fast modulation.

**Display.** The live magnitude response, which moves with the modulation.

---

## Experimental

### 17. COLOR (super light)
*Inspiration: imagiro autochroma, which shreds the input into grains that come back randomly
delayed and pitched.*

There is one grain stream (user decision, 2026-09-18). It reads a 3-second circular buffer.

**Controls**
- **PITCH**: −12 to +24 semitones.
- **FINE**: ±100 cents.
- **SIZE**: 20 – 500 ms.
- **DENSITY**: 1 – 40 grains per second.
- **SPRAY**: a random start offset, 0 – 1.5 s back.
- **FEEDBACK**.
- **LEVEL**.
- **REVERSE**: the chance that a grain plays backwards.
- **SPREAD**: random stereo pan width.
- **SHAPE**: the grain envelope; SMOOTH, TRIANGLE, PERC or GATE.
- **TONE**: a low-pass.

**Display.** Grains as vertical strokes: x is age and y is pitch. Reversed grains are drawn in
salmon.

### 18. AMBIENT
> **Removed (user, 2026-10-05):** a reverb the rack did not need. Saved slots that held it load empty;
> later types moved up one place (the processor converts states saved before, see `fxTypes`).

*Inspiration: Arturia Efx AMBIENT, a two-macro (TONE × SPACE) atmosphere processor with six
modes.*

The signal runs through a mode "tone" processor and then a modulated FDN space.

**MODE** chooses the tone processor, and **TONE** sets its intensity:
- REFLECT: reverse swells.
- WOVEN: multi-tap woven chorus.
- SIREN: +12 shimmer.
- ORGANIST: octave harmonics, −12 and +12.
- CODEC: bit/rate artefacts.
- SUNKEN: a resonant, underwater low-pass.

**Other controls**
- **TONE × SPACE**: an XY pad on the display, dragged directly. SPACE scales the reverb size and
  its send.
- **SIZE**.
- **DECAY**: 0.5 – 30 s.
- **WIDTH**.
- **MOD**: the depth of the delay-line modulation.
- **PRE-DELAY**.
- **DUCKING**.
- **LOW CUT**, **HIGH CUT**: the input filter.

**Display.** The XY pad, with a glowing puck and a mode-specific background pattern.

---

## Experimental (added 2026-10-05)

The user asked for three newcomers in EXPERIMENTAL: ShaperBox 3, oeksound bloom ("near identical") and a
better, more appealing Output Portal. Each is rebuilt from how the original works, under our own name and on
our own chassis. None borrows the original's name, wording, palette or layout.

Sources read: the ShaperBox 3 review in Sound On Sound and Cableguys' feature list; the bloom review in
Sound On Sound and oeksound's KVR listing; Output's own Portal grain-controls article and gearnews' overview.

### 23. CARVE
*Inspiration: Cableguys ShaperBox 3, a chain of "shapers", each driven by its own drawn wave.*

Rebuilt the same day (user: "everything that the shaper box has ... all those modes"). The first
version had one wave driving nine depth knobs. CARVE is now eleven shapers in ShaperBox's chain order:
PITCH, REVERB, TIME, DRIVE, NOISE, LIQUID, FILTER, CRUSH, VOLUME, PAN, WIDTH. They show as tabs; the lit
dot marks a shaper that is on.

**Every shaper has**
- **ON**: each is switched on by itself.
- **Its own wave**: 16 points with STEPS, LINES or SMOOTH between them. Drag on the display to draw;
  right-click for shapes (pump, ramps, sine, triangle, square, gate 1/16, stairs, chop, random, flat).
  The waves are saved with the state (the slot's EXT values), not as host parameters.
- **RATE**: 1/32 to 8 bars per cycle.
- **TRIGGER**:
  - SYNC: in time with the host (with EXT CLK on; free at the tempo while stopped).
  - NOTE: restarts on each note played in the layer (ShaperBox's MIDI trigger).
  - TRANSIENT: restarts on each hit, plays once and holds.
  - FOLLOW: read by the input level instead of time.
- **BAND**: FULL, or only the LOW, MID or HIGH band of a three-way split.
- **MIX**: its own dry/wet.

**The shapers and what their wave does**
- **PITCH**: ±RANGE semitones around the middle line. STEPS rounds to semitones.
- **REVERB**: how much of a space (SIZE, DECAY, TONE) you hear: chopped, pumping or swelling tails.
- **TIME**, four modes:
  - SHIFT: the wave is how far back to read, up to RANGE of a cycle. Flat lines are stutters; slopes
    are speed and pitch changes.
  - HALF-TIME: each cycle is played at 1/RATIO speed (1.5x, 2x, 3x, 4x); the wave says how much.
  - REVERSE: each cycle is played backwards; the wave says how much.
  - TAPE STOP: the wave is the tape speed (top = normal, bottom = stopped).
  - Jumps back to the live signal cross-fade over 10 ms.
- **DRIVE**: up to AMOUNT dB. SMOOTH, HARD, FOLD or TUBE, a TONE tilt, the level held.
- **NOISE**: WHITE, PINK, HISS, VINYL, CRACKLE or RUMBLE at LEVEL, through a COLOR low-pass.
  STATIC, FOLLOW (louder with the input) or DUCK (fills the gaps).
- **LIQUID**: FLANGER, PHASER (8 stages) or CHORUS. The wave moves the centre within DEPTH, with
  FEEDBACK (±) and a STEREO offset.
- **FILTER**: LP 12/24, HP 12/24, BAND PASS, NOTCH or PEAK. The wave sweeps the cutoff between LOW
  and HIGH, with RESONANCE and a pre-DRIVE.
- **CRUSH**: down to BITS and RESAMPLE at the top of the wave, plus JITTER.
- **VOLUME**: ducks by DEPTH. **PAN**: left to right within DEPTH. **WIDTH**: narrower to wider within RANGE.

**Global**: LOW / HIGH XOVER (the band split), SENSITIVITY (TRANSIENT's threshold, FOLLOW's floor), OUTPUT.

Only VOLUME starts on, at DEPTH 0, so CARVE starts level (SGFxCheck: null −240 dB). SGFxCheck also runs
each shaper on its own and each TIME mode.

## FX modulation (added 2026-10-05)

Right-click any continuous control of any effect (knob, fader, number, DRY / WET) to modulate it. It does
not appear in the matrix page. The panel lists the layer's sources:
- LFO 1: a layer-wide twin of the panel's LFO 1, same rate and wave.
- LFO 2.
- ENV 1 and ENV 2, from the newest voice.
- VELOCITY and NOTE, from the last note.
- MOD WHEEL, AFTERTOUCH, EXPRESSION, RIBBON, BENDER.
- INPUT: the slot's own input level.
- RANDOM: a new value each beat, glided.

Drag a row sideways for −100…+100 %; the wheel gives fine steps, and a double-click clears. A route adds
amount × source to the control's normalised value, as the matrix does, and is applied per layer in 64-sample
pieces (src/plugin/FxRack.cpp). Knobs show the reachable range as an inner ring and a dot where the plugin
has them now.

Routes are saved with the state, travel with a swapped slot, and are cleared when the slot's effect changes.
Stepped keys and LAYER MIX are not modulation targets. SGPluginSmoke checks a route through the hosted
VST3: VELOCITY → ECHO DELAY's DRY / WET at −100 % takes the echo away.

### 24. POISE
*Inspiration: oeksound bloom, an adaptive tone shaper.*

Twelve bands, about 0.8 octave apart, listen to the sound and lean it towards a balanced target: equal
energy per band (pink) with a little more at both ends than pink. Bell filters move the tone; the loudness
is held (an RMS match, as VALVE does), so turning it up changes the balance, not the level.

**Controls**
- **AMOUNT** 0–10: how hard it leans. Past 7 it also squashes: each band's fast level is pulled towards its
  slow level, up and down, above **SQUASH CAL**.
- **ATTACK**, **RELEASE** 0–10: how fast the bands follow the sound (2 ms–0.5 s, 20 ms–2 s).
- **TONE** handles 1–4 on the display (LOW, LOW MID, HIGH MID, HIGH): drag sideways for the frequency, up and
  down for ±6 dB. They move the target, not the sound, so they act through AMOUNT.
- **WET TRIM** ±12 dB. **STEREO** (both sides moved together) or **MID / SIDE** (each shaped on its own).
  **DELTA**: hear only what POISE changes.
- AMOUNT 0 leaves the sound untouched (SGFxCheck: level start; and a check that the level stays within 2 dB
  while a bass-heavy signal gets more even).

**Display**: the target (dashed), the correction it is making now (filled, sent by the plugin), the output
spectrum behind, the four numbered handles.

### 25. RIFT
*Inspiration: Output Portal, a granular effect with an XY pad.*

The input goes through a **DELAY** (free or SYNC) into a 9 s buffer that **FREEZE** stops writing. Grains
are read from it at **DENSITY** (1–100 /s), **SIZE** (10 ms–1 s) long, up to **SPRAY** into the past, pitched
by **PITCH** (±24 st) and kept to a **SCALE** (chromatic, major, minor, pentatonic, octaves, fifths), some
**REVERSED**, panned within **SPREAD**, shaped by **SHAPE** (smooth, triangle, perc, gate). They are filtered
(**LOW CUT**, **HIGH CUT**), fed back into the delay (**FEEDBACK**) and sent into a **SPACE**.

**What makes it more than the original**: the pad needs no mapping. Its two axes are fixed macros that always
do something musical.
- **SCATTER** (across) randomises pitch within the scale, pan, size and position, all at once.
- **BLOOM** (up) feeds the grains back on themselves and opens and lengthens the space.

**Display**: the pad as a tunnel of rings that drift towards you, faster with density; grains pulled through
it (reversed ones fly out); the SCATTER × BLOOM point.
