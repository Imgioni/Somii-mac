# Phase 3 & 4 Report — Performance, Modulation, Effects, Arp/Seq, MIDI

Status: **complete**. 421 automated checks pass. The built VST3 passes the headless host smoke test, which
includes a MIDI CC 74 → Upper LPF check.

## Phase 3 — what was built

| Module | Files | Manual pages |
|---|---|---|
| Lower layer; SINGLE / DUAL / SPLIT; split point (default C4); upper = base channel, lower = base + 1 | `src/core/Performance.*` | 82–83, 99, 110 |
| Voice assign: SOLO / LEGATO (last-note priority), POLY 1 / POLY 2, UNISON, U. SIZE 1–4, BINAURAL on/off | `src/core/LayerEngine.*`, `SuperVoice.*` | 90–91 |
| Voice stealing with a 3 ms declick fade | `src/core/Voice.*` | xiii |
| LFO 2: 6 waveforms, RATE 0.05–50 Hz, DELAY 0–5 s, TRIG / AT+TRIG / ON (AT→BEND), LFO2 RATE / DDS / VCF / VCA depths, per-voice when modulated | `Voice.cpp`, `LayerControl.h` | 70–72 |
| Bender: DDS (≤ 1 octave) and VCF amounts, vertical push → LFO 2; DEST osc selector (DDS 1 / 1+2 / DDS 2) | `Voice.cpp`, `Performance.cpp` | 69, 72–73 |
| Portamento (glide time grows with the interval, up to 10 s/octave); octave −2…+2; global transpose ±12; fine tune ±100 ct | `Voice.cpp`, `Performance.cpp` | 73–76 |
| DEST / PORTAMENTO layer selectors with BOTH mirroring | `PluginProcessor.cpp` | 73–75 |
| Ribbon: relative pitch, held notes only; as a matrix source it releases pitch control | `Performance.cpp`, `Voice.cpp` | 77–78 |
| TEMPO, LOWER DETUNE ±7 st, PERF DETUNE ±7 st, HOLD per layer, sustain pedal, volume pedal | `Performance.*` | 79–81 |
| Modulation matrix: 8 sources × (8 matrix + 24 direct) destinations, −100…+100 % per route | `src/core/Matrix.h`, `Voice.cpp`, `Parameters.cpp` | 84–89 |

## Phase 4 — what was built

| Module | Files | Manual pages |
|---|---|---|
| Chorus I / II / I+II, click-free mode changes | `src/core/Effects.*` | 64 |
| Stereo delay: TIME, SEND, FEEDBACK (max = infinite repeats), FREEZE, SYNC note values | `src/core/Effects.*`, `LayerEngine.cpp` | 65–67 |
| Clock: host tempo and transport, or internal TEMPO; LFO 1 SYNC and delay SYNC tables | `src/core/Clock.h`, `PluginProcessor.cpp` | 55–56, 65–66, 79, 92–93 |
| Arpeggiator: UP / DOWN / U&D / RANDOM, RANGE 1–4, CLK DIV (8 values), SWING 0–4, HOLD / sustain latch, new chord replaces the latched one | `src/core/ArpSeq.*` | 81, 92–94 |
| Sequencer: 64 steps; STEP / SLIDE / ACCENT / REST / LENGTH; chords; bender position per step; transpose from C4; 16 memories with LOAD / STORE / clear; linked sequence slot; stored in the plugin state | `src/core/ArpSeq.*`, `PluginProcessor.cpp` | 95–98 |
| MIDI receive: 86 CCs → parameters (both layers), stepped CC bands, NRPN 14-bit patch and global, RPN 0 / 1 / 2, CC receive on/off, performance CCs (mod wheel, ribbon, pedals, all-notes-off) | `src/core/MidiMap.h`, `MidiDecoder.h`, `src/plugin/MidiRouter.*` | 99–100, 115–128 |

New design decisions: DD-52 … DD-58 in `DESIGN_DECISIONS.md`.

## Measurements (i7-10700K, 48 kHz, Release)

| What | Result | Manual / spec |
|---|---|---|
| SOLO vs LEGATO, level 20 ms after a legato note | 0.079 (retriggered) vs 0.000 (not) | [p.90] |
| UNISON in POLY, three notes held | 6 / 8 / 6 voices | divided by notes held [p.90] |
| Portamento, 1-octave and 2-octave glides | both moved 6.00 semitones in 0.25 s | time grows with the interval [p.73] |
| LFO 2 TRIG, bender push 0 → max | 440 → 880 Hz (full depth) | [p.71] |
| Matrix VEL → HPF, 110 Hz fundamental | soft 0.156, hard 0.003 | [p.84] |
| Delay FEEDBACK max, 40 repeats | first echo 0.5000, drift 0.0000000 | infinite repeats [p.66] |
| FREEZE | loop holds 0.500 / 0.500 / 0.500; a new note stays out (0.0000) | [p.67] |
| Chorus, mono in → L/R correlation | I 0.67 · II 0.89 · I+II 0.64 | stereo width [p.64] |
| Arp step timing | on the CLK DIV grid ±1 sample; gate 50 %; swing on every 2nd step | [pp.92–94] |
| MIDI CC table | 86 CCs mapped, checked entry by entry against pp.116–122 | [pp.116–122] |
| Plugin: CC 74 = 32 on channel 1 | Upper LPF = 0.252 (= 32/127) | [p.119] |
| **CPU: 20 voices + both layers' chorus I+II and delay, 2× (default)** | **13.3 % SUPER OFF · 13.7 % SUPER ON** | **< 15 %** |
| CPU: 20 voices, no effects, 2× · 4× | 11.7 / 12.2 % · 18.3 / 18.5 % | — |

How the CPU figure is measured, and what was optimised to get there, is in DD-58. The biggest wins were a
shorter dependency chain in the ladder filter (algebraically the same filter), an SSE2 path for SUPER's seven
oscillators, and rendering each super voice's two voices interleaved. None of these changes the sound; every
filter and oscillator test from Phase 2 still passes at its original tolerance.

## Couldn't verify / not done

- **Against real hardware.** Chorus rates and depths, swing amounts, gate length and the delay's character are
  modelled; the manual gives no numbers (DD-52, DD-53).
- **MIDI output** (CC TX, program-change TX, clock TX) is left out under the approved "hardware-only" deviation
  (DD-57). Say if you want CC TX back; CLAP and AU can carry MIDI out, VST3 can't reliably.
- **Program change receive** needs presets, so it comes in Phase 6 along with the MANUAL mode parameter.
- **EXT CLK "Stop receive" option** [p.93] is not exposed. With host sync on, the arp follows the host grid
  while the transport runs. With the transport stopped it runs free at the host tempo, so you can play it
  without pressing play. The hardware with clock RX would stay silent until clock arrives. **Your call:** keep
  this, or make it wait for the transport like the hardware.
- **Sequencer editing** (SEQ REC, TRACK, step buttons, LOAD / STORE / clear) works in the engine and is
  tested. On the hardware these are buttons, not parameters, so they get controls in the Phase 5 UI.
- LOCAL, TUNE (filter calibration), DUMP and MPE are hardware-only or reserved [p.100].
- **CLAP and Standalone** are built but not loaded in a host. **AU** needs a Mac.
- **The copy installed for FL Studio is still the Phase 2 build.** It needs reinstalling to get Phase 3 and 4.

## How to rebuild and test

```
scripts\build.cmd --target SGTests Geminus_VST3 Geminus_CLAP Geminus_Standalone SGPluginSmoke
build\SGTests_artefacts\Release\SGTests.exe            (all tests; add "perf", "arp", "fx", "midi", "bench" … to run one group)
build\SGPluginSmoke_artefacts\Release\SGPluginSmoke.exe "build\Geminus_artefacts\Release\VST3\Geminus.vst3"
```
