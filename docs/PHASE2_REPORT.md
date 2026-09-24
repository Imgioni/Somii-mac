# Phase 2 Report — DSP Core

Status: **complete**. 250 automated checks pass; the built VST3 loads and plays in a headless VST3 host.

## What was built

| Module | Files | Manual pages |
|---|---|---|
| DDS 1: centroid + 6 sisters, 6 waveforms, ALT A/B wavetables, PWM/WAVE morph, SUPER OFF/½/ON | `src/core/Dds1.*`, `WaveTable.*`, `PhaseOsc.h` | xii, 30–34, 61–62 |
| DDS 2: 6 waveforms, phase reset per note, TUNE ±7, RANGE incl. LFO (0.1–100 Hz), NORM/RING/SYNC, sub-osc square/sine | `src/core/Dds2.h`, `Voice.cpp` | 35–39 |
| Mixer: MIX crossfade, PAN | `Voice.cpp`, `PluginProcessor.cpp` | 40 |
| VCF: 1-pole HPF, 4-pole SSI-style ZDF ladder, RES to self-oscillation, DRIVE OFF/1/2, ENV 1/1+2/2, KEYTRACK OFF/½/ON, ENV/LFO 1/DDS 2 mod | `src/core/Filters.h`, `Voice.cpp` | 41–43 |
| VCA: ENV LEVEL (−∞…+4 dB), LFO 1 tremolo, DDS 2 AM, ENV 2 / GATE / GATE+REL, DYNAMICS | `Voice.cpp` | 44–45 |
| ENV 1: AH, A, DH, D, S, R, NORMAL/INVERTED/LOOP, KEYTRACK · ENV 2: A, DH, D, S, R | `src/core/Envelope.h` | 46–53 |
| LFO 1: triangle/rev saw/S&H/square/HF/HF TRK, FREE/ONCE/RESET, HF routing NORM/DDS 1/DDS 2, DELAY, LR PHASE, SPREAD | `src/core/Lfo1.h`, `SuperVoice.cpp`, `Voice.cpp` | 54–59 |
| DDS Modulator: LFO 1/ENV 1 pitch → DDS 1/1+2/2, PW/DETUNE, DRIFT, PWM/WAVE + source, CROSS MOD (reversed in SYNC) | `Voice.cpp`, `LayerControl.h` | 60–63 |
| Binaural super voices (L/R complete voices), non-binaural pairs, 20 voices | `src/core/SuperVoice.*`, `LayerEngine.*` | xiii, 54, 91 |
| Oversampling 1–16× + half-band decimator | `src/core/Decimator.*` | xii |
| Plugin shell: VST3 / CLAP / Standalone, 3 stereo outs, upper-layer parameters, generic editor | `src/plugin/*` | 29 |

Phase 3 fills in: the lower layer, voice-assign modes, SINGLE/DUAL/SPLIT, LFO 2, bender, portamento, ribbon
and the matrix. The Phase 2 plugin plays the upper layer in POLY 1 with a ±2-semitone pitch bend.

## Measurements (i7-10700K, 48 kHz, Release)

| What | Result | Spec |
|---|---|---|
| Pitch, A4 at 8' | 440.001 Hz | 440 Hz |
| Worst alias, sawtooth at A7 | −67 dB (2×) · −80 dB (4×) · −90 dB (8×) | "no audible aliasing" |
| Top end, 3rd harmonic of A7 saw | −10.4 dB (ideal −9.54) | "don't dull the top end" |
| Hard sync, non-harmonic content | −88 dB | clean sync |
| Filter self-oscillation pitch at 440 / 1000 / 4000 Hz | 440.0 / 1000.0 / 3999.9 Hz, 3rd harmonic −62 dB | clean sine [p.42] |
| Keytrack ON, C4 → C5 | ×2.00 | semitone tracking [p.43] |
| LPF slope, 3 octaves above cutoff | −72.8 dB | 24 dB/oct [p.41] |
| DRIVE 1 bass loss at RES 9 | 0.9 dB (DRIVE OFF: 13.2 dB) | resonance compensation [p.42] |
| ENV 1 LOOP, A 10 ms / D 20 ms / S 0.2 | 35 cycles/s between 0.21 and 1.00 | [p.48] |
| 150 random patches × chords | all finite, peak 1.21 | stable |
| **CPU, 20 voices, 2× (default)** | **13.2 % SUPER OFF · 15.0 % SUPER ON** | **< 15 % incl. effects** |
| CPU, 20 voices, 4× | 23.4 % · 26.6 % | — |

The CPU figure is at the target with no effects yet. Phase 4 adds chorus and delay (roughly +1–2 %), so
Phase 3–4 includes an optimisation pass (processing voice pairs in parallel) to win back headroom.

## Couldn't verify

- **Against real hardware.** No Super Gemini is available to A/B against, so sound character (filter drive
  curve, chorus, sister detune amount, envelope curves) is modelled from the manual's text and figures only.
- **CLAP and Standalone** were built but not loaded in a host. The VST3 was loaded headlessly
  (`SGPluginSmoke`), but not in FL Studio.
- **AU** needs a Mac to build.
- The design decisions in DESIGN_DECISIONS.md still stand: HF max 20 kHz (DD-8), fixed-envelope release
  (DD-24) and PW/PWM combination (DD-25).

## How to rebuild and test

```
scripts\build.cmd --target SGTests Geminus_VST3 Geminus_CLAP Geminus_Standalone SGPluginSmoke
build\SGTests_artefacts\Release\SGTests.exe            (all tests; add "osc", "filter", "mod", "voice", "core" or "bench" to run one group)
build\SGPluginSmoke_artefacts\Release\SGPluginSmoke.exe "build\Geminus_artefacts\Release\VST3\Geminus.vst3"
```
