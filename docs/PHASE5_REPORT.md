# Phase 5 Report — User Interface (SPKR Geminus)

Status: **complete**. The plugin opens its own panel in place of the generic parameter list.

- **Coverage:** the UI check proves all 682 host parameters can be reached from a control.
- **Interaction:** the UI check drives 21 interactions with synthesised mouse events. Each one checks that
  the right parameter moved.
- **Host:** the VST3 editor opens through the real VST3 view interface in the headless host.
- **Unit tests:** all 421 pass.

![Panel](screenshots/panel.png)

Direction from the user (2026-09-11): the mockup is an "ideas picture"; the UI must have everything it needs
(DD-59). Later the same day came three more directions:
- The panel wears the original synth's colours, with restrained real-world depth and texture (DD-71).
- The brand is **SPKR Geminus** (DD-72).
- A full hardware-replica layout was tried and set aside at the user's request; this layout is the one kept.
- The instrument is now drawn as a physical object — cheeks, a panel that curves into the front lip, and a
  playable 61-note keyboard — with live displays, visible modulation and lamps that decay (DD-73).

## What was built

| Part | Files | Manual pages |
|---|---|---|
| Vector panel, 1862 × 1182 design canvas, resizable 50–200 % at a fixed aspect ratio, HiDPI (drawn, no bitmaps) | `src/plugin/PluginEditor.*`, `src/ui/Panel.*` | — |
| Theme (DD-71): grey painted metal with grain; raised plates with soft shadows; white (UPPER) / orange (LOWER) / black (global) knobs and caps; charcoal oscillator / envelope panels; amber screens; bevelled keycaps; recessed wells; orange title rules; themed menus and tooltips | `src/ui/Theme.*`, `BrandConfig.h` | — |
| Controls bound to host parameters (automation, MIDI CC and the panel stay in step); right-click = reset / "Modulate by…" / MIDI CC number; double-click = default; Ctrl = fine | `src/ui/Controls.*` | 84–88, 116–122 |
| Header: PATCH, LAYER (LOWER / UPPER / MANUAL), KEYBOARD MODE, SPLIT POINT with LEARN | `Panel.cpp` | 22, 26, 82–83 |
| Top editor row (follows the selected layer): MASTER / PERFORMANCE, DDS MODULATOR, OSCILLATORS with live waveform displays, MIXER, VCF, VCA, ENVELOPES, CHORUS, DELAY | `Panel.cpp` | 30–67, 73–81 |
| Two strips, UPPER (white caps) over LOWER (orange caps): LAYER, OSCILLATORS, MIXER, VCF, VCA, ENVELOPES, LFO 1 / LFO 2 (performance section + ribbon), MODULATION, EFFECTS, VOICES, ARP / SEQ | `Panel.cpp` | 40–98 |
| SHIFT / inverse legends: latched button, Shift key, or Alt-drag | `Controls.cpp`, `Panel.cpp` | 112 |
| Pop-overs: full 8 × 32 modulation matrix with clearing; 64-step sequencer editor; 32-slot waveform browser; Settings | `src/ui/Popovers.*` | 33–34, 84–89, 95–101 |
| Editor ↔ audio bridge: lock-free command FIFO, atomics for LEDs and meters, try-locked sequence snapshot | `src/plugin/UiBridge.h` | — |
| LEDs: output meter, TEMPO, LFO rate, DELAY TIME, ENV 1 LOOP, PORTAMENTO, TRANSPOSED, CHORUS ON, layer activity, SEQ REC — re-triggered then decaying, the layer lamp driven by that layer's own level | `Panel.cpp` | 48, 54, 65, 74–75, 79 |
| Instrument body (DD-73): fluted dark end cheeks, brushed panel face with a vignette, the fold into the shelf and the rolled front lip, engraved nameplate, dark header band and status bar | `src/ui/Theme.*`, `Panel.cpp` | — |
| Shelf controls: BENDER (sideways = pitch + cutoff, push = LFO 2 / mod wheel) and the ribbon controller | `src/ui/Controls.*`, `Panel.cpp` | 69, 77–78 |
| Playable 61-note keyboard, C1–C6, velocity from where the key is clicked | `src/ui/Controls.*` | 16 |
| Live displays: output oscilloscope, envelope curve with the playing level, VCF response with the modulated cutoff and a Hz read-out, running LFO shape per strip | `Panel.cpp`, `UiBridge.h` | 41–53, 54, 70 |
| Visible modulation: a second indicator on the knob or fader at the value modulation has reached | `Controls.*`, `Theme.*`, `Panel.cpp` | 84–88 |
| Status bar: the hint line, voice count, tempo, edited layer, settings menu | `Panel.cpp` | — |

The mockup's typos are corrected (SROER MOD → SUPER (DDS 1), HARBCE → HARD SYNC via the MODE switch,
VELOCITCYS gone). Its invented controls are mapped to real ones or removed:
- The five mixer level faders became MIX + PAN.
- The DDS 1 FINE control was removed.
- The PRIORITY dropdown became BINAURAL.
- The header BOTH button became MANUAL (DD-15).

New decisions DD-59 … DD-73 are in `DESIGN_DECISIONS.md`.

Other screens: [lower layer selected](screenshots/panel_lower.png) · [ENV 2 and LFO 2 tabs](screenshots/panel_env2_lfo2.png) ·
[matrix](screenshots/matrix.png) · [sequencer](screenshots/sequencer.png) · [waveform browser](screenshots/altwaves.png) ·
[settings](screenshots/settings.png)

## Checks (all pass)

| Check | Result |
|---|---|
| Every host parameter has a control (both layers, every tab and pop-over) | 682 / 682 |
| Drag the LPF knob 100 px down | 1.000 → 0.500 |
| Select LOWER | top row re-binds to the lower layer; in SINGLE the lower layer plays [p.82] |
| Click DUAL; CHORUS I then II | keyboard mode DUAL; chorus I → I+II [p.64] |
| SHIFT + turn PW/DETUNE | DRIFT changes, PW/DETUNE doesn't [p.112] |
| Host automation of RES | the knob follows |
| Portamento layer = LOWER | GLIDE and OCTAVE re-bind to the lower layer [p.74] |
| MODULATION ENV 1 → HPF; LFO 2 → LPF | `upper.mtx.env1.hpf`; LFO 2 → LPF not offered (hard-wired, p.87) |
| Step editor: notes + ACCENT + LENGTH 8 | reach the engine and come back in the published snapshot |
| On-screen ribbon drag | bends pitch [p.77] |
| Split LEARN, then play G4 | split point = G4 [p.82] |
| Output meter | receives the level |
| Resize | 50 % = 931 × 591, 200 % = 3724 × 2364 |
| VST3 editor in the headless host | opens, aspect ratio 1862 : 1182 |
| CPU after the UI work (20 voices + both layers' effects, 2×) | 12.8 % SUPER OFF · 13.3 % SUPER ON |

## Couldn't verify

- **Not yet opened in FL Studio.** The copy FL loads is still the Phase 2 build, and reinstalling needs you at
  the computer. The standalone app runs without installing:
  `build\Geminus_artefacts\Release\Standalone\Geminus.exe`.
- **Real mouse and keyboard.** Tested with synthesised events only: drags, clicks, SHIFT. Popup menus and
  tooltips open as separate windows, so they don't appear in the screenshots.
- **The mockup's typeface.** It suggests Barlow Condensed / DIN Condensed. The panel uses the best condensed
  face installed (Bahnschrift on Windows 10/11). Bundling Barlow Condensed (open font licence) needs a
  download, so it waits for your OK.
- **Waveform displays** show the PWM/WAVE position or modulation depth, not the live LFO- or envelope-driven
  morph.
- **Still to come in Phase 6:**
  - patch and performance library, program change, full MANUAL semantics (edited indicator, COMPARE);
  - oversampling setting;
  - import / export of waveforms and sequences.

## How to rebuild and check

```
scripts\build.cmd --target Geminus_VST3 Geminus_CLAP Geminus_Standalone SGUiSnapshot SGPluginSmoke SGTests
build\SGUiSnapshot_artefacts\Release\SGUiSnapshot.exe docs\screenshots          (UI check + screenshots)
build\SGPluginSmoke_artefacts\Release\SGPluginSmoke.exe "build\Geminus_artefacts\Release\VST3\Geminus.vst3"
build\SGTests_artefacts\Release\SGTests.exe
```
