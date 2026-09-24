# Super Gemini Plugin — Parameter Table (Phase 1)

Source of truth for function: *UDO Super Gemini / Super 8 Owner's Manual v1.0 (March 2024)*.
Working transcription with page cites: [manual_notes.md](manual_notes.md). Decisions: [DESIGN_DECISIONS.md](DESIGN_DECISIONS.md).

## Legend

- **ID**: stable host parameter ID (`juce::ParameterID`, version hint 1). `{L}` = the parameter exists twice,
  as `upper.…` and `lower.…` (one complete patch per layer).
- **Scope**: **G** global setting (never stored in a performance or patch) · **P** stored in the performance ·
  **L** stored in the layer's patch.
- **Default**: **M** = stated in the manual · **DD-n** = design decision n in DESIGN_DECISIONS.md.
  The manual only says the init patch is "a single oscillator set to a sawtooth wave, among other basic
  settings" [p.22], so most init values are design decisions.
- **CC / NRPN**: MIDI address from pp.116–128. Layer parameters are addressed on the layer's channel
  (upper = base channel, lower = base + 1 [p.99]). Stepped CC values are what's transmitted; on receive each
  value is the lower bound of its band (DD-30).
- **0–10** means a hardware fader/knob with a 0–10 legend. It's stored as a normalised float; the physical
  meaning comes from the taper listed in the last section.

---

## 1. Global settings (scope G)

| ID | Name | Range / values | Default | CC | NRPN | Page |
|---|---|---|---|---|---|---|
| `global.masterVolume` | MASTER VOLUME | −∞ … 0 dB … +4 dB (taper T-dB) | 0 dB (DD-1) | – | – | 79, 17 |
| `global.transpose` | Global transpose (Shift + OCT) | −12 … +12 st, integer | 0 | – | – | 75, 114 |
| `global.fineTune` | FINE ADJ (Shift + MOD AMOUNT) | −100 … +100 ct (A = 440 Hz at 0) | 0 (M: 440 Hz) | 9* | 1033* | 76, 114 |
| `global.midiChannel` | MIDI CH (upper; lower = +1) | 1 … 16 | 1 (DD-2) | – | 2051 | 99 |
| `global.ccTx` | TX/RX E · 1 CC transmit | off / on | off (DD-2) | – | – | 99 |
| `global.ccRx` | TX/RX E · 2 CC receive | off / on | on (DD-2) | – | – | 99 |
| `global.nrpnMode` | TX/RX E · 3 NRPN mode (14-bit TX) | off / on | off (DD-2) | – | – | 99 |
| `global.pcTx` | TX/RX P · 1 program change TX | off / on | off (DD-2) | – | 2055 | 100 |
| `global.pcRx` | TX/RX P · 2 program change RX | off / on | on (DD-2) | – | 2056 | 100 |
| `global.localControl` | LOCAL | off / on | on | 122 | – | 100 |
| `global.clockRx` | EXT CLK · 2 clock receive → **host tempo/transport** | off / on | on (DD-3) | 84 | 2053 | 92–93 |
| `global.clockTx` | EXT CLK · 1 clock transmit | off / on | off | – | 2052 | 92 |
| `global.stopRx` | EXT CLK · 4 MIDI Stop receive | off / on | on (DD-3) | – | – | 93 |

\* CC 9 / NRPN 1033 is "Mod Amount/Fine Adjust", the encoder itself. It edits fine tune only in Shift mode;
otherwise it edits the selected matrix amount (DD-31).

Plugin-only settings (not synth parameters, stored in the plugin's settings file, not host-automatable):
oversampling quality, UI scale, MIDI-learn table, bender-push CC (default CC 1), ribbon CCs (CC 2/34
[p.78]), expression CC (11), volume-pedal CC (4), delay-freeze footswitch target (upper [p.67]).
Not implemented (user-approved, DD-4): filter **TUNE**, **DUMP**, **MPE**. Global reset by button hold
becomes a Settings menu item.

## 2. Performance (scope P)

| ID | Name | Range / values | Default | CC | NRPN | Page |
|---|---|---|---|---|---|---|
| `perf.keyboardMode` | KEYBOARD | SINGLE / DUAL / SPLIT | SINGLE (M: init → single) | – | – | 82, 22 |
| `perf.singleLayer` | Layer played in SINGLE | UPPER / LOWER | UPPER (M) | – | – | 82 |
| `perf.splitPoint` | SPLIT (NOTE) — first note of upper | MIDI 0 … 127 | 60 = C4 (M) | – | – | 82 |
| `perf.lowerDetune` | LOWER · DETUNE | −7 … +7 st, continuous | 0 | 108 | 1132 | 80 |
| `perf.perfDetune` | PERF DETUNE (Shift + DETUNE) | −7 … +7 st, continuous | 0 | 111 | 1135 | 80 |
| `perf.tempo` | TEMPO | 30 … 300 BPM | 120 (DD-5) | 3 | 1027 | 79 |
| `perf.modLayer` | DEST layer selector | BOTH / LOWER / UPPER | BOTH (DD-6) | 104 | – | 73 |
| `perf.portaLayer` | PORTAMENTO layer selector (also octave) | BOTH / LOWER / UPPER | BOTH (DD-6) | 65 | – | 74–75 |

`perf.modLayer` and `perf.portaLayer` choose which layer's (patch-stored) bender/LFO 2 and portamento/octave
values the performance-section controls edit. Switching to BOTH copies upper → lower [p.73–75].

## 3. Layer / patch parameters (scope L — each exists as `upper.*` and `lower.*`)

### 3.1 DDS 1 [pp.31–34]
| ID | Name | Range / values | Default | CC | NRPN | Page |
|---|---|---|---|---|---|---|
| `{L}.dds1.wave` | WAVEFORM | SINE / SAW / SQUARE / TRIANGLE / NOISE / ALT | SAW (M) | 29 | – | 32, 22 |
| `{L}.dds1.range` | RANGE | 64' / 32' / 16' / 8' / 4' / 2' | 8' (DD-7) | 30 | – | 33 |
| `{L}.dds1.altA` | ALT channel A slot | W1 … W32 | W1 (DD-7) | – | – | 33–34 |
| `{L}.dds1.altB` | ALT channel B slot | W1 … W32 | W2 (DD-7) | – | – | 33–34 |

Patch state (not a parameter): the 4096-point int16 data for the A and B waveforms is embedded in the patch,
so replacing the library doesn't change patches [p.34, p.109].

### 3.2 DDS 2 [pp.35–39]
| ID | Name | Range / values | Default | CC | NRPN | Page |
|---|---|---|---|---|---|---|
| `{L}.dds2.wave` | WAVEFORM | SINE / SAW / SQUARE / TRIANGLE / NOISE / PULSE | SQUARE (DD-7) | 31 | – | 36 |
| `{L}.dds2.range` | RANGE | LFO / 32' / 16' / 8' / 4' / 2' | 8' (DD-7) | 106 | – | 36 |
| `{L}.dds2.tune` | TUNE | −7 … +7 st, continuous (LFO range: ≈0.1 … 100 Hz) | 0 | 35 | 1059 | 36, 38 |
| `{L}.dds2.mode` | Mode toggle | NORM / RING / SYNC — with RANGE = LFO: SUB OFF / SUB SQUARE / SUB SINE | NORM | 36 | – | 36–39, 112 |

### 3.3 Mixer [p.40]
| ID | Name | Range / values | Default | CC | NRPN | Page |
|---|---|---|---|---|---|---|
| `{L}.mixer.mix` | MIX | DDS 1 … 1+2 … DDS 2/SUB (0–10, 5 = equal) | 0 = DDS 1 only (DD-7, "single oscillator") | 37 | 1061 | 40 |
| `{L}.mixer.pan` | PAN (Shift + MIX) | L … C … R | C | 10 | 1034 | 40 |

### 3.4 VCF [pp.41–43]
| ID | Name | Range / values | Default | CC | NRPN | Page |
|---|---|---|---|---|---|---|
| `{L}.vcf.drive` | DRIVE | OFF / 1 / 2 | OFF | 41 | – | 42 |
| `{L}.vcf.hpf` | HPF | 0–10 (T-HPF) | 0 | 95 | 1119 | 42 |
| `{L}.vcf.lpf` | LPF | 0–10 (T-LPF) | 10 (DD-7) | 74 | 1098 | 42 |
| `{L}.vcf.res` | RES | 0–10 (self-oscillates at max) | 0 | 71 | 1095 | 42 |
| `{L}.vcf.envSource` | ENV source toggle | ENV 1 / 1+2 / ENV 2 | ENV 1 (DD-7) | 44 | – | 42 |
| `{L}.vcf.keytrack` | KEYTRACK | OFF / ½ / ON | OFF (DD-7) | 43 | – | 43 |
| `{L}.vcf.envAmt` | ENV (ENV MOD) | 0–10 | 0 | 45 | 1069 | 42 |
| `{L}.vcf.lfo1Amt` | LFO 1 (LFO MOD) | 0–10 | 0 | 46 | 1070 | 42 |
| `{L}.vcf.dds2Amt` | DDS 2 (OSC MOD) | 0–10 | 0 | 47 | 1071 | 42 |

### 3.5 VCA [pp.44–45]
| ID | Name | Range / values | Default | CC | NRPN | Page |
|---|---|---|---|---|---|---|
| `{L}.vca.envLevel` | ENV LEVEL | −∞ … 0 dB … +4 dB (T-dB) | 0 dB (DD-7) | 7 | 1031 | 44–45 |
| `{L}.vca.lfo1Amt` | LFO 1 (LFO MOD) | 0–10 | 0 | 92 | 1116 | 45 |
| `{L}.vca.dds2Amt` | DDS 2 (OSC MOD) | 0–10 | 0 | 109 | 1133 | 45 |
| `{L}.vca.envMode` | Envelope selector | ENV 2 / FIXED 1 (gate) / FIXED 2 (gate + release) | ENV 2 (M) | 49 | – | 45 |
| `{L}.vca.dynamics` | DYNAMICS | OFF / ½ / ON (velocity → VCA and LPF) | OFF (DD-7) | 48 | – | 45 |

### 3.6 Envelope 1 [pp.46–51]
| ID | Name | Range / values | Default | CC | NRPN | Page |
|---|---|---|---|---|---|---|
| `{L}.env1.attackHold` | AH (Shift + A) | 0 (off) … 10 s (T-HOLD) | 0 | 52 | 1076 | 47 |
| `{L}.env1.attack` | A | 1 ms … 10 s (T-TIME) | 1 ms (DD-7) | 53 | 1077 | 47 |
| `{L}.env1.decayHold` | DH (Shift + D) | 0 (off) … 10 s (T-HOLD) | 0 | 32 | 1056 | 47 |
| `{L}.env1.decay` | D | 1 ms … 10 s (T-TIME) | 500 ms (DD-7) | 54 | 1078 | 48 |
| `{L}.env1.sustain` | S | 0–10 (level) | 0 (DD-7) | 55 | 1079 | 48 |
| `{L}.env1.release` | R | 1 ms … 10 s (T-TIME) | 500 ms (DD-7) | 56 | 1080 | 48 |
| `{L}.env1.mode` | Mode toggle (+ LOOP LED) | NORMAL / INVERTED / LOOP | NORMAL | 50 | – | 48 |
| `{L}.env1.keytrack` | KEYTRACK (decay + release) | OFF / ½ / ON | OFF | 51 | – | 49 |

### 3.7 Envelope 2 [pp.52–53]
| ID | Name | Range / values | Default | CC | NRPN | Page |
|---|---|---|---|---|---|---|
| `{L}.env2.attack` | A | 1 ms … 10 s | 1 ms (DD-7) | 73 | 1097 | 52 |
| `{L}.env2.decayHold` | DH (Shift + D) | 0 (off) … 10 s | 0 | 33 | 1057 | 52 |
| `{L}.env2.decay` | D | 1 ms … 10 s | 500 ms (DD-7) | 57 | 1081 | 52 |
| `{L}.env2.sustain` | S | 0–10 | 10 (DD-7) | 58 | 1082 | 53 |
| `{L}.env2.release` | R | 1 ms … 10 s | 100 ms (DD-7) | 72 | 1096 | 53 |

### 3.8 LFO 1 [pp.54–59]
| ID | Name | Range / values | Default | CC | NRPN | Page |
|---|---|---|---|---|---|---|
| `{L}.lfo1.wave` | WAVEFORM | TRIANGLE / REV SAW / S&H / SQUARE / HF / HF TRK | TRIANGLE (DD-7) | 16 | – | 58–59 |
| `{L}.lfo1.rate` | RATE (+ LED) | LF 0.05 … 50 Hz · HF 20 Hz … 20 kHz (DD-8) · SYNC: 16 divisions (Table S1) | ≈ 5 Hz (DD-7) | 17 | 1041 | 54–56 |
| `{L}.lfo1.delay` | DELAY (fade-in) | 0 … 10 s | 0 | 18 | 1042 | 54, 56 |
| `{L}.lfo1.lrPhase` | LR PHASE / SPREAD | 0 … 100 % (0 … 2π) / spread 0 … 100 % | 0 | 19 | 1043 | 56–58, 91 |
| `{L}.lfo1.mode` | MODE | FREE / ONCE / RESET — HF: NORM / DDS 1 / DDS 2 | FREE | 20 | – | 59, 112 |

### 3.9 DDS Modulator [pp.60–63]
| ID | Name | Range / values | Default | CC | NRPN | Page |
|---|---|---|---|---|---|---|
| `{L}.ddsMod.lfo1Amt` | PITCH · LFO 1 | 0–10 | 0 | 21 | 1045 | 60 |
| `{L}.ddsMod.env1Amt` | PITCH · ENV 1 | 0–10 | 0 | 22 | 1046 | 60 |
| `{L}.ddsMod.dest` | Osc selector | DDS 1 / 1+2 / DDS 2 | 1+2 (DD-7) | 23 | – | 60 |
| `{L}.ddsMod.super` | SUPER (DDS 1) | OFF / ½ / ON | OFF | 24 | – | 61 |
| `{L}.ddsMod.pwDetune` | PW / DETUNE | 0–10 | 0 | 25 | 1049 | 62 |
| `{L}.ddsMod.drift` | DRIFT (Shift + PW) | 0–10 | 0 (DD-7) | 94 | 1118 | 62 |
| `{L}.ddsMod.pwmWave` | PWM / WAVE | 0 (A) … 10 (B) | 0 | 26 | 1050 | 62 |
| `{L}.ddsMod.pwmSource` | PWM/WAVE source | MANUAL / LFO 1 / ENV 1 | MANUAL | 27 | – | 62 |
| `{L}.ddsMod.crossMod` | CROSS MOD (RING MOD context) | 0–10 | 0 | 28 | 1052 | 63 |

### 3.10 Effects [pp.64–67]
| ID | Name | Range / values | Default | CC | NRPN | Page |
|---|---|---|---|---|---|---|
| `{L}.fx.chorus` | CHORUS I / II buttons | OFF / I / II / I+II | OFF | 93 (0/32/64/96) | – | 64 |
| `{L}.fx.delayTime` | DELAY TIME (+ LED) | 1 ms … 1 s (T-DLY) · SYNC: 16 divisions (Table S2) | 350 ms (DD-7) | 12 | 1036 | 65–66 |
| `{L}.fx.delayFeedback` | FEEDBACK | 0–10 (10 = infinite, no decay) | 3 (DD-7) | 13 | 1037 | 66 |
| `{L}.fx.delaySend` | DLY SEND | 0–10 | 0 | 91 | 1115 | 67 |
| `{L}.fx.freeze` | DELAY FREEZE | off / on (momentary or latched) | off, **not stored** (DD-9) | 69 | – | 67 |

### 3.11 Performance-section values stored per patch [pp.68–76]
Edited via `perf.modLayer` / `perf.portaLayer` (p.73–75: "save either the performance or both patches").

| ID | Name | Range / values | Default | CC | NRPN | Page |
|---|---|---|---|---|---|---|
| `{L}.bender.ddsAmt` | Bender DDS | 0–10 = 0 … 12 st (max one octave) | 2 st (DD-7) | 76 · RPN 0 | 1100 | 69, 123 |
| `{L}.bender.vcfAmt` | Bender VCF | 0–10 (max = fully open/close) | 0 | 77 | 1101 | 69 |
| `{L}.dest.osc` | DEST osc selector (bender + LFO 2 pitch) | DDS 1 / 1+2 / DDS 2 | 1+2 (DD-7) | 61 | – | 72 |
| `{L}.lfo2.wave` | LFO 2 WAVEFORM | SINE / REV SAW / S&H / SQUARE / SAW / NOISE | SINE (DD-7) | 105 | – | 70 |
| `{L}.lfo2.rate` | LFO 2 RATE (+ LED) | 0.05 … 50 Hz | ≈ 5 Hz (DD-7) | 62 | 1086 | 70–71 |
| `{L}.lfo2.delay` | LFO 2 DELAY | 0 … 5 s | 0 | 63 | 1087 | 70–71 |
| `{L}.lfo2.trigger` | Trigger toggle | TRIG / AT+TRIG / ON (AT→BEND) | TRIG (DD-7) | 60 | – | 71 |
| `{L}.lfo2.rateMod` | LFO2 RATE (push/AT → rate) | 0–10 | 0 | 110 | 1134 | 71 |
| `{L}.lfo2.ddsAmt` | LFO 2 DDS | 0–10 | 2 (DD-7) | 70 | 1094 | 71 |
| `{L}.lfo2.vcfAmt` | LFO 2 VCF | 0–10 | 0 | 75 | 1099 | 71 |
| `{L}.lfo2.vcaAmt` | LFO 2 VCA | 0–10 | 0 | 42 | 1066 | 72 |
| `{L}.porta.time` | PORTAMENTO | 0 = off … 10 = 10 s per octave (T-PORTA) | 0 | 5 | 1029 | 73–74 |
| `{L}.octave` | OCT − / OCT + | −2 … +2 | 0 | 67 (0/26/51/77/102) | – | 75 |

### 3.12 Voice assign [pp.90–91]
| ID | Name | Range / values | Default | CC | NRPN | Page |
|---|---|---|---|---|---|---|
| `{L}.voice.mode` | MODE | SOLO / LEGATO / POLY 1 / POLY 2 | POLY 1 (M) | 78 (0/32/64/96) | – | 90 |
| `{L}.voice.unison` | UNISON | off / on | off | – (no CC in table) | – | 90 |
| `{L}.voice.unisonSize` | U. SIZE (Shift + MODE) | 1 half · 2 all · 3 octave · 4 fifth + octave | 2 (DD-7) | 79 (1/2/3/4) | – | 91 |
| `{L}.voice.binaural` | BINAURAL | off / on | on (M) | 80 | – | 91 |

### 3.13 Arpeggiator / sequencer [pp.92–98]
| ID | Name | Range / values | Default | CC | NRPN | Page |
|---|---|---|---|---|---|---|
| `{L}.arp.on` | ON | off / on | off | 86 | – | 94, 96 |
| `{L}.arp.clockDiv` | CLK DIV | 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/4T, 1/8T | 1/16 (M: reset value) | 107 | – | 92 |
| `{L}.arp.sync` | SYNC (LFO 1 rate + delay time to clock) | off / on | off | 81 | – | 92 |
| `{L}.arp.range` | RANGE (arp mode) | 1 … 4 octaves | 1 | 82 (0/32/64/96) | – | 94 |
| `{L}.arp.swing` | SWING (Shift + RANGE) | 0 (off), 1 … 4 | 0 | 83 (0/26/51/77/102) | – | 93 |
| `{L}.arp.mode` | MODE | UP / DOWN / U&D / RANDOM / SEQ | UP | 85 (0/26/51/77/102) | – | 94 |
| `{L}.seq.slot` | Linked sequence (LOAD) | 1 … 16 | 1 | 14 (0–15) | – | 95, 98 |

Sequencer data (per slot, not parameters): 64 steps × {notes/chord, tie (SLIDE), ACCENT, REST, bender
position}, LENGTH 1–64 [pp.95–97]. Transpose reference C4 [p.97].

### 3.14 Layer utility
| ID | Name | Range / values | Default | CC | NRPN | Page |
|---|---|---|---|---|---|---|
| `{L}.hold` | HOLD (LOWER / UPPER buttons) | off / on — stored in the performance | off | 87 | – | 81 |
| `{L}.manual` | MANUAL (UPPER / LOWER) | off / on | off | 59 (64 = on) | – | 26 (DD-10) |

The mockup's strip **LEVEL** knob is a linked view of `{L}.vca.envLevel`, not a new parameter (DD-11). The
manual has no separate layer volume, and CC 7 is VCA Envelope Level.

## 4. Modulation matrix (scope L) [pp.84–89]

Sources `s`: `dds2`, `lfo2`, `env1`, `vel`, `at`, `expr`, `ribbon`, `note` (buttons 1–8, p.84).
Every amount: −100 … +100 %, default 0, host-automatable, no MIDI address (the manual has none).

**4a. Fixed matrix (8 × 8 = 64 per layer)**: `{L}.mtx.<s>.<d>` with `d` ∈ `lfo1Rate` (A), `xmod` (B),
`wave` (C), `mix` (D), `hpf` (E), `res` (F), `env1Decay` (G), `dlyTime` (H) [p.84].

**4b. Direct parameter mappings (8 × 24 per layer, minus excluded duplicates)**: `{L}.mtxd.<s>.<n>` where
`n` is the destination number from p.88:
1 DDS 2 Tune · 2 LPF Cutoff · 3 VCF Env Amt · 4 VCF LFO 1 Amt · 5 VCF DDS 2 Amt · 6 VCA Env Level ·
7 VCA LFO 1 Amt · 8 VCA DDS 2 Amt · 9 Env 1 A · 10 Env 1 S · 11 Env 1 R · 12 Env 2 A · 13 Env 2 D ·
14 Env 2 S · 15 Env 2 R · 16 LFO 1 Delay · 17 LFO 1 LR Phase · 18 LFO 2 Rate · 19 LFO 2 Delay ·
20 DDS Mod LFO 1 Amt · 21 DDS 2 PW / DDS 1 Detune · 22 Portamento Time · 23 Delay Send · 24 Delay Feedback.
Excluded because a hard-wired control already makes that routing [p.87]: `lfo2→2` (manual's example),
`dds2→2`, `env1→2`, `at→18` (DD-12).

Clear operations (UI actions, not parameters): all · by source · by destination [p.89].

## 5. Parameter count

| Group | Count |
|---|---|
| Global (automatable: master volume, transpose, fine tune) | 3 (+10 non-automatable settings) |
| Performance | 8 |
| Layer sound + control | 82 × 2 = 164 |
| Matrix fixed | 64 × 2 = 128 |
| Matrix direct | (192 − 4) × 2 = 376 |
| **Host parameters total** | **679** |

## 6. Tapers (normalised fader position x ∈ [0, 1] → physical value)

These are design decisions (DD-13). The manual gives end points but no curves.

| Taper | Formula | Range |
|---|---|---|
| T-TIME | 1 ms · 10^(4x) | 1 ms … 10 s [pp.47–53] |
| T-HOLD | 0 if x = 0, else 1 ms · 10^(4x) | 0 … 10 s [p.47] |
| T-LFO-LF | 0.05 Hz · 1000^x | 0.05 … 50 Hz [p.54 legend, p.70] |
| T-LFO-HF | 20 Hz · 1000^x | 20 Hz … 20 kHz [p.59] |
| T-LPF | 20 Hz · 2^(x · (11.5 − 2k)), k = keytrack 0/½/1 [p.43]; clamped to 0.45 × oversampled rate | ≈ 20 Hz … 58 kHz (DD-39) |
| T-HPF | 10 Hz · 500^x | 10 Hz … 5 kHz |
| T-DLY | 1 ms · 1000^x | 1 ms … 1 s [p.65] |
| T-PORTA | 0 if x = 0, else 10 s/oct · 10^(−4(1−x)) | off, 1 ms … 10 s per octave [p.74] |
| T-dB | x ≤ 0.8: gain = (x/0.8)²; x > 0.8: +20·(x−0.8) dB | −∞ … 0 dB (at 80 %) … +4 dB [pp.44, 79] |
| T-SEMI | linear | DDS 2 tune, detunes, bender range |
| T-LFO2-DELAY | 5 s · x² | 0 … 5 s [p.70] |
| T-LFO1-DELAY | 10 s · x² | 0 … 10 s [p.54] |

## 7. Sync tables

**S1 — LFO 1 RATE when SYNC is on** [pp.55–56], fader bottom → top: 8 whole (32 beats) · 4 whole (16) ·
2 whole (8) · whole (4) · ½ (2) · dotted ¼ (1.5) · ½T (4/3) · ¼ (1) · dotted ⅛ (¾) · ¼T (⅔) · ⅛ (½) ·
dotted 1/16 (⅜) · ⅛T (⅓) · 1/16 (¼) · dotted 1/32 (3/16) · 1/16T (1/6).

**S2 — DELAY TIME when SYNC is on** [pp.65–66], knob min → max: 1/32T (1/12 beat) · dotted 1/64 (3/32) ·
1/32 (⅛) · 1/16T (1/6) · dotted 1/32 (3/16) · 1/16 (¼) · ⅛T (⅓) · dotted 1/16 (⅜) · ⅛ (½) · ¼T (⅔) ·
dotted ⅛ (¾) · ¼ (1) · ½T (4/3) · dotted ¼ (1.5) · ½ (2) · whole T (8/3).

## 8. UI / session state (not parameters, not stored in patches)

Edit layer (LAYER UPPER/LOWER, p.83) · SHIFT latch (p.25) · compare toggle (p.25) · edited flag (p.19, p.24) ·
mod-assign selection (p.86) · wave-select mode A/B (p.33) · SEQ REC, TRACK, page (pp.96–97) · current
perf/patch location · performance controllers (bender X/Y, ribbon, aftertouch, expression, volume pedal,
sustain), which come in only as live MIDI or UI gestures.
