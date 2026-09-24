# Design Decisions

Rule set (from the build prompt): **function follows the manual, look follows the mockup.** Where the manual
is silent or ambiguous, the option closest to the manual is chosen and logged here. Section A lists the
deviations from the manual or mockup that the user explicitly approved.

Page cites are manual pages. Transcription: [manual_notes.md](manual_notes.md).

---

## A. User-approved deviations (all four approved by the user on 2026-09-10)

### DD-14 — Which layer is on top · APPROVED
Manual: the Super Gemini's **top row is the UPPER layer**, the second row the LOWER [p.23]. The manual also
colour-codes **upper = red, lower = blue** throughout [pp.82–83]. The mockup labels the top strip "LAYER 1 /
LOWER" (red LED) and the bottom "LAYER 2 / UPPER" (blue LED).
**Proposal:** keep the mockup exactly as drawn (red LED top, blue LED bottom) but label the top strip **UPPER**
and the bottom **LOWER**, with tabs "UPPER" then "LOWER". Only the labels change. This matches the hardware
row order, the manual's colours and the mockup's LED colours.

### DD-4 — Hardware-only features left out · APPROVED
These manual features have no meaning inside a plugin, so the plan omits them rather than ship dead
controls:
- **TUNE** filter auto-calibration [p.100] (a software filter never drifts out of calibration).
- **DUMP** and **MPE** — both "reserved for future use" on the hardware [p.100].
- USB-drive unlock, firmware update, footswitch polarity auto-calibration, headphone output [pp.27–29, 102].
  File management is replaced by an in-plugin browser using the same folder/file layout [pp.102–103].
- Global reset by holding MANUAL for 5 s at power-on [p.101] becomes a **Settings → Global reset** menu item.

### DD-10 — What MANUAL mode means in software · APPROVED
Hardware: MANUAL makes the layer follow the physical knob positions instead of the stored patch [p.26].
In a plugin the on-screen knobs *always* show the live values, so "follow the panel" never changes the
sound. **Proposal:** MANUAL detaches the layer from its memory slot: the sound stays as it is, the patch
display reads MANUAL, the edited indicator and COMPARE are disabled, and alt waveforms are kept from the
previous patch [p.34]. Storing then asks for a slot. CC 59 toggles it [p.119].

### DD-15 — The mockup's header LAYER "BOTH" button · APPROVED
The hardware LAYER section has only **LOWER / UPPER** [p.83]. BOTH exists only on the DEST and PORTAMENTO
layer selectors [pp.73–74], which are placed in the LFO 2 tab and the MASTER section. A header BOTH button
would be an invented feature. **Proposal:** keep the third button's slot and style, and relabel it
**MANUAL** (DD-10) for the selected layer.

---

## B. Where the build prompt disagreed with the manual (manual wins, no action needed)

| # | Prompt said | Manual says | Page |
|---|---|---|---|
| B1 | Middle octave LED flashes when fine tune ≠ 440 Hz | It flashes when **global transpose** ≠ 0 ("TRANSPOSED") | 75 |
| B2 | Direct mappings: "any continuous control" | Exactly **24 listed destinations** | 88 |
| B3 | DDS 2 LFO rate "0.1–100 Hz" | Same (≈) — confirmed | 38 |
| B4 | Effects chain: chorus → delay → pan | Chorus → delay in series; **delay is fed by a SEND** (send/return, dry always passes) | xiii, 64, 67 |
| B5 | Layer tabs "LAYER 1 / LOWER" on top | Top row = UPPER — see DD-14 | 23 |
| B6 | CLK DIV includes "1/16, 1/32, ¼T, ⅛T, etc." | Exactly 8 values: 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/4T, 1/8T | 92, 121 |
| B7 | Unison options "read p.90" | U. SIZE: 1 half · 2 all · 3 octave stack · 4 fifth + octave stack | 91 |
| B8 | Freeze "in SINGLE footswitch logic targets upper" | Single footswitch → upper; dual → left upper, right lower; CC 69 | 29, 67 |
| B9 | LFO 1 HF "sine by default" | Sine is the only HF shape: no CC value or switch selects another | 59, 116 |
| B10 | Mockup PORTAMENTO on/off LED button | No portamento on/off exists: fader at 0 = off. LED becomes a status light (lit when time > 0) — DD-16 | 74 |
| B11 | Mockup strip LEVEL = layer volume | No layer volume exists; LEVEL is a linked view of VCA ENV LEVEL (CC 7) — DD-11 | 44, 116 |

---

## C. Ambiguities resolved (closest to the manual)

**DD-1 — Master volume.** Default 0 dB; applies to the Main bus only (the hardware knob sets main +
headphone level [p.79]). Never saved in a performance or patch [pp.17, 79].

**DD-2 — MIDI defaults** (manual gives none): base channel 1 (lower = 2) [p.99]; CC receive on, CC transmit
off, NRPN off; program change receive on, transmit off. CC/PC/clock *transmit* work only in hosts that
accept MIDI output from plugins.

**DD-3 — Clock.** "External clock" = the host. `global.clockRx` on (default) → arp/seq, LFO 1 sync and delay
sync follow host tempo and transport. Off → internal TEMPO (30–300 BPM). MIDI Stop receive maps to the host
stopping [p.93].

**DD-5 — Tempo.** Default 120 BPM (not given). Stored in the performance: p.79 lists it as an "overarching"
control, even though the NRPN table files it under patch-related [p.124].

**DD-6 — DEST / PORTAMENTO layer selectors** default BOTH. Switching to BOTH copies upper → lower, exactly as on
pp.73–75.

**DD-7 — Init patch.** The manual gives only "single oscillator, sawtooth, basic settings" [p.22]. Chosen:
DDS 1 SAW 8' · DDS 2 SQUARE 8', NORM, tune 0 · MIX fully DDS 1 · PAN centre · VCF drive off, HPF 0, LPF 10,
RES 0, env source ENV 1, keytrack OFF, all mod amounts 0 · VCA ENV LEVEL 0 dB, ENV 2, dynamics OFF ·
ENV 1 A 1 ms, D 500 ms, S 0, R 500 ms, NORMAL, keytrack OFF · ENV 2 A 1 ms, D 500 ms, S 10, R 100 ms ·
LFO 1 TRIANGLE ≈ 5 Hz, FREE · DDS Mod dest 1+2, SUPER OFF, PWM source MANUAL, all amounts 0, DRIFT 0 ·
chorus off, delay 350 ms / feedback 3 / send 0 · bender DDS 2 st, VCF 0 · LFO 2 SINE ≈ 5 Hz, TRIG,
DDS 2 (so the mod wheel gives light vibrato), others 0 · portamento 0 · octave 0 · POLY 1, unison off,
U. SIZE 2, BINAURAL on [p.91] · arp off, 1/16 [p.92], UP, range 1, swing 0. Loading init switches the
performance to SINGLE [p.22]. "Set current patch as init" lives in Settings [p.108].

**DD-8 — LFO 1 HF top frequency.** The text says 20 Hz – **20 kHz** [p.59]; the panel legend says 10 kHz [p.54].
Chosen: the text, 20 kHz.

**DD-9 — Delay freeze.** Not stored in patches (it's a performance gesture). While engaged, the delay input
is muted and the loop recirculates at unity; on release, normal SEND/FEEDBACK resume [p.67]. The UI button
is momentary on click and latches on Alt-click. CC 69 on the base channel → upper layer, base+1 → lower
[pp.29, 67].

**DD-11 — Strip LEVEL knob** = linked view of `{L}.vca.envLevel`.

**DD-12 — Matrix duplicate exclusions** [p.87]. The manual states the rule ("existing mappings are not
duplicated") with a single example: LFO 2 → LPF cutoff. Applied to every source → destination pair that already
has its own dedicated amount fader: LFO 2 → LPF cutoff (LFO 2 VCF fader), DDS 2 → LPF cutoff (VCF DDS 2
fader), ENV 1 → LPF cutoff (VCF ENV fader), AT → LFO 2 rate (LFO2 RATE fader). Pairs that are only
hard-wired through a *switch* (velocity via DYNAMICS, note via KEYTRACK) stay allowed, because the matrix
gives continuous control the switch can't.

**DD-13 — Tapers.** End points come from the manual; curves are chosen (PARAMETERS.md §6).

**DD-16 — PORTAMENTO LED** in the MASTER section is read-only, lit when the edited layer's portamento > 0.

**DD-17 — Signal topology per layer** (p.xiii diagram + p.67): voices → layer sum → PAN → CHORUS (insert;
bypassed when off) → dry out + SEND → DELAY → return. Pan sits before the effects so delay repeats come
from the panned position.

**DD-18 — Output buses.** Main (stereo) = both layers × master volume. Upper / Lower (stereo) = each layer
after its effects, before master volume, mirroring the hardware's layer jacks [p.29]. In SINGLE mode the
active layer feeds its own bus and Main (the hardware splits one layer across both engines [p.83]; copying
that would be pointless in software).

**DD-19 — MIDI note routing.** Notes on the base channel behave like the built-in keyboard: SINGLE → active
layer, DUAL → both, SPLIT → by split point [p.82]. Notes on base+1 → lower layer only, as in the manual's
two-track DAW setup [p.110]. Sustain (CC 64): base+1 → lower; base channel → the layers base-channel notes
reach in SINGLE, and **upper only** in DUAL/SPLIT (single-footswitch rule, p.29).

**DD-20 — Pedals.** CC 11 "Expression" = expression pedal = matrix source 6. CC 4 "Foot Controller" = volume
pedal, scaling the Main output [pp.29, 116]. The manual doesn't say which CC is which pedal.

**DD-21 — Program change.** Bank Select (CC 0) 0/1 = group A/B. On the base channel PC 0–63 loads a
performance (banks A–H × 1–8). In patch mode, PC 0–127 on a layer's channel loads a patch into that layer
(banks A1–H2 × 1–8). The manual doesn't spell out PC semantics [pp.17–21, 100].

**DD-22 — RPNs** [p.123]. RPN 0 (bend sensitivity, ±12 st) writes that channel's layer `bender.ddsAmt`. RPN 1/2
(channel fine/coarse) write `global.fineTune` / `global.transpose`: the Super Gemini has no per-layer tuning
besides lower detune.

**DD-23 — Ribbon range** (not stated). A full-length slide = one octave, in the direction of travel from the
touch point [p.77]. Affects held notes only, not notes in release [p.77]. Assigning RIBN in the matrix
disables pitch control; clearing all RIBN mappings restores it [p.77].

**DD-24 — VCA fixed envelope 2 release** (upper selector position) uses the ENV 2 R fader value. The manual
says it "has a release stage" but not its length [p.45].

**DD-25 — DDS 2 pulse width in MANUAL source mode.** Width = PW/DETUNE + PWM/WAVE offset, clamped to
5 %…95 %. With LFO 1 or ENV 1 as the source, PWM/WAVE sets the modulation depth instead [p.62].

**DD-26 — ENV 1 keytrack** reference note C4 (MIDI 60, the manual's transpose reference [p.97]). Decay/release
time × 2^(−k·(note−60)/12), with k = 0 / 0.5 / 1 for OFF / ½ / ON [p.49].

**DD-27 — VCF ENV amount** is unipolar (0–10 legend, no centre detent [p.41]). ENV 1 INVERTED provides
downward sweeps [p.48].

**DD-28 — Matrix source polarity.** ENV 1, velocity, aftertouch, expression: unipolar 0…1. DDS 2 / LFO 2:
native polarity of the selected waveform [pp.58, 70]. Ribbon: bipolar, relative to the touch point [p.77].
Note number: bipolar around C4. 100 % = the destination's full range.

**DD-29 — Alt waveform files.** `.ws6` = 4096 × int16, headerless [p.109]; byte order assumed little-endian.
WAV import: resample one cycle to 4096 points and band-limit to 512 harmonics (fs/8) [p.109].

**DD-30 — Receiving stepped CCs.** Each printed value is the lower bound of its band (0–20 → 1st, 21–42 → 2nd…).
Values are sent exactly as printed.

**DD-31 — CC 9 "Mod Amount/Fine Adjust"** [p.116]. With Shift latched it sets global fine tune. In mod-assign
mode it sets the selected mapping's amount. Otherwise it's ignored.

**DD-32 — CC 107 misprint.** The table prints "70 = Thirty-second notes" between 64 and 96 [p.121], breaking the
16-step pattern (80 expected). TX sends 70 as printed; RX maps 70–95 → 1/32.

**DD-33 — Voice pools.** SINGLE: 20 mono voices / 10 binaural super voices for the active layer. DUAL/SPLIT:
10 / 5 per layer [pp.xiii, 82, 91]. UNISON in poly modes: stack = floor(pool ÷ held notes), recomputed on
every note-on [p.90].

**DD-34 — HOLD** is stored in the performance (it's in "Additional controls", p.81) and addressable per layer
by CC 87 [p.120].

**DD-35 — Analog tolerances.** Every voice gets fixed tiny random offsets (tuning ±1.5 ct, cutoff ±2 %,
envelope times ±3 %) on top of DRIFT, as the build prompt asks (§8). Not in the manual; audibly "analog" only.

---

## D. Phase 2 implementation decisions

**DD-36 — Waveform phase alignment.** Every classic shape has its fundamental in phase with the sine: SAW is a
falling ramp, TRIANGLE starts at 0 going up. With a rising saw, the SINE→SAW morph cancelled the fundamental
halfway (a hollow notch mid-morph); aligned shapes morph smoothly [p.32].

**DD-37 — Anti-aliasing.** 4-point polyBLEP/polyBLAMP (cubic B-spline kernel, 2-sample latency) on every
discontinuity, including hard sync, with the oscillator + filter stage oversampled and a linear-phase half-band
decimator. Measured on a sawtooth at A7 (3520 Hz): worst alias −67 dB at 2× (default), −80 dB at 4×, −90 dB
at 8×; the 3rd harmonic sits within 1 dB of an ideal sawtooth, so the top end isn't dulled.

**DD-38 — Control rate.** Pitch, cutoff, PWM/WAVE and HF-LFO settings are recomputed every 4 host samples
(12 kHz at 48 kHz); the filter coefficient is interpolated per sample in between. Envelopes and VCA gain run
every sample. This meets the spec's "≤ 64-sample control rate with smoothing" and cut CPU by ~25 %.
Audio-rate paths (cross mod, DDS 2 → cutoff / VCA, HF LFO) stay at the oversampled rate.

**DD-39 — LPF fully open.** The fader top is 20 Hz · 2^11.5 ≈ 58 kHz, clamped to 0.45 × the oversampled rate
(43 kHz at 2×). A 4-pole ladder "fully open" at 20 kHz would audibly roll off the top octave.

**DD-40 — Cross-mod pitch shift is intentional.** Exponential FM raises the average pitch of DDS 1. The manual
expects this: LOWER DETUNE exists partly "to compensate for pitch offsets caused by cross modulation" [p.80].

**DD-41 — Voice headroom.** Each voice outputs at −12 dB (0.25) before MASTER VOLUME, so a full 20-voice chord
peaks in a sensible range.

**DD-42 — VST3 MIDI CC.** The VST3 build exposes JUCE's hidden MIDI-CC parameters (the host sees ~2,100
parameters, ~67 of them ours in Phase 2). VST3 can't deliver CCs any other way, and the Phase 4 MIDI map
(pp.116–122) depends on receiving them.

## E. Phase 3 implementation decisions

**DD-43 — Unison stack layout.** U. SIZE 3 alternates stacked voices between root and +12, and U. SIZE 4
cycles root / +7 / +12 [p.91]. Every stack also gets a ±5 cent spread so the stacked voices don't phase-lock.
The manual gives no detune amount for unison.

**DD-44 — POLY 2.** "The release stage of overlapping notes is curtailed" [p.90] is implemented Roland-style:
a re-pressed note reuses its own voice, and voices in release are taken for new notes before idle ones
(with the 3 ms steal fade). POLY 1 takes idle voices first, so releases overlap.

**DD-45 — Mono note priority.** SOLO and LEGATO use last-note priority. Releasing the sounding key returns to the
most recent key still held (SOLO retriggers the envelopes, LEGATO doesn't). The manual doesn't state a
priority.

**DD-46 — Unison in POLY.** When a new note needs voices and all are in use, it takes them from the note with
the largest stack, so stacks stay balanced (2 notes → 10/10 voices, 3 notes → 6/8/6) [p.90].

**DD-47 — LFO 2.** One layer-wide phase. Each voice drifts from it only when its own rate is modulated (matrix
direct 18, poly aftertouch through LFO2 RATE), and flipping the trigger switch resyncs all voices [p.72]. As a
matrix source, LFO 2 carries its trigger depth (push / aftertouch / always-on) and delay fade.

**DD-48 — Matrix sources.** DDS 2 as a *matrix* source is sampled at the 12 kHz control rate. Audio-rate DDS 2
modulation keeps its dedicated paths (cross mod, VCF DDS 2, VCA DDS 2). Ribbon as a source is its absolute
position 0…1 (CC 2/34 or the on-screen strip); this replaces DD-28's "relative" wording. Note number =
(note − C4) / 60, clamped to ±1.

**DD-49 — BOTH on the DEST / PORTAMENTO selectors.** Both layers keep their own stored values. With the
selector on BOTH, switching to it copies upper → lower [pp.73–75], and while it stays on BOTH an edit to either
layer's value is copied to the other (the processor checks 30× per second on the message thread).

**DD-50 — Volume pedal (CC 4)** scales both layers' outputs (square law) before MASTER VOLUME.

**DD-51 — Ribbon release.** Lifting the finger returns the relative bend to zero. Notes already in their release
keep the bend they had when their key was let go, because the ribbon no longer affects them [p.77].

## F. Phase 4 implementation decisions

**DD-52 — Arp timing.** Gate length is 50 % of the step. The manual gives none. SWING 0…4 [p.93] delays the
second step of each pair to 50 / 54 / 58 / 62.5 / 67 % of the pair (67 % ≈ triplet shuffle); the manual names the
settings but gives no amounts. While the host transport runs (DD-3), the arp starts on the next CLK DIV grid line
of the host timeline, so it lands in time with the song. Free-running (host stopped, or sync off), it starts on
the key press at TEMPO.

**DD-53 — Chorus model.** p.64 lists modes I, II and I+II without depths or rates. It is modelled on the
classic BBD dual-mode chorus:
- I: 0.513 Hz triangle, 3.5 ± 1.6 ms, L/R in antiphase.
- II: 0.863 Hz, two taps per side (3.2 ± 2.1 ms and 5.4 ∓ 1.4 ms).
- I+II: a three-phase string-ensemble chorus, 5 ms ± 1.1 ms at 0.62 Hz plus ± 0.18 ms at 5.9 Hz, with the right
  channel offset by 60°.

The wet path gets gentle input saturation and a 2-pole 9 kHz low-pass (BBD character). Mix is dry 0.8 / wet 0.7.
Mode changes crossfade through zero wet level over 20 ms, so switching doesn't click.

**DD-54 — Delay loop.** FEEDBACK at maximum recirculates at exactly unity, with no filtering and no decay:
"infinite repeats" [p.66]. FREEZE fades new input out over 4 ms and holds the loop at unity [p.67]. The loop
is linear up to ±1.5. Above that a soft knee stops a full-feedback loop with constant input from running away,
so the output stays bounded without colouring normal use. TIME changes glide over ~60 ms, which gives a
tape-like pitch bend instead of zipper clicks.

**DD-55 — Sequencer accent.** Accented steps play at full velocity (1.0). Other steps play at 80 % of their
recorded velocity. So ACCENT is heard wherever velocity is heard (DYNAMICS ½ / ON, or a velocity matrix
route) [p.97].

**DD-56 — Delay as a matrix destination.** DELAY TIME / SEND / FEEDBACK are matrix destinations (H, 23, 24)
[pp.84–89]. The delay is one per layer but matrix sources are per voice, so the delay follows the most recently
played active voice.

**DD-57 — No MIDI output.** The plugin receives MIDI but transmits none. CC TX, program-change TX and clock TX
[pp.99, 115] are left out under the approved "hardware-only features" deviation; VST3 has no dependable
MIDI-out path. The receive side is complete.

**DD-58 — How the CPU budget is measured.** The build prompt asks for "20 voices plus both layers' effects
under 15 % of one core at default oversampling". The benchmark (`tests/VoiceTests.cpp`) uses:
- 10 binaural notes in one layer, which is all 20 voices.
- A patch with DDS 2 pulse, filter envelope, LOOP ENV 1 and LFO 1 → VCF.
- Chorus I+II and the delay (send 0.5, feedback 0.6) running in *both* layers.
- 2× oversampling, 48 kHz, 256-sample blocks, one thread.

To meet the budget, the ladder filter is written in an algebraically equivalent form with a shorter
per-sample dependency chain.

## G. Phase 5 (UI) decisions

**DD-59 — The mockup is a style reference · USER DIRECTION (2026-09-11).** The user said the panel picture is
"just an ideas picture, make sure the UI has everything it needs". So the panel keeps the mockup's look:
- the palette, the knobs, faders and buttons, the header and footer;
- the section order, with the top editor row, the layer tabs and two strips.

Slots are rearranged wherever a manual control needs room. The UI check (`SGUiSnapshot`) fails if any host
parameter has no control.

**DD-60 — Envelope faders.** ENV 1 shows six faders, AH A DH D S R, so both hold times are visible; on the
ENV 2 tab the AH slot is greyed out. The mockup's single "H" fader plus hidden secondary functions would have
put two of ENV 1's six times behind SHIFT [pp.47–53].

**DD-61 — Arp ON + MODE.** The strip has an ON button and a MODE dropdown (UP / DOWN / U&D / RANDOM / SEQ). The
mockup's single "OFF / UP / …" dropdown is not used: on the hardware these are separate controls [p.92], and
they are separate parameters (CC 86 and CC 85).

**DD-62 — Strip EFFECTS.** CHORUS I / II buttons, the delay SEND and FREEZE. Buttons that "open/toggle" the
effects would have been duplicates or dead.

**DD-63 — Which layer the top row edits.** It is chosen with the header LOWER / UPPER buttons, the UPPER /
LOWER tabs, or a strip's EDIT button; all three are views of one setting. In SINGLE mode that choice also
picks the playing layer, as the hardware's LAYER buttons do [p.82]. The choice is saved with the project.

**DD-64 — OCTAVE.** A five-button −2 … +2 selector instead of the spring-loaded rocker. The middle button
flashes while global transpose ≠ 0 ("TRANSPOSED", p.75). GLIDE and OCTAVE act on the layer set by the
portamento layer toggle [p.74]. With BOTH they edit upper and the processor mirrors to lower (DD-49).

**DD-65 — LFO 2 / bender in the strips.** Each strip's LFO 2 tab shows that layer's own LFO 2, bender and DEST
values. The DEST layer selector (UPPER / LOWER / BOTH) is one global setting shown in both strips [p.73].

**DD-66 — SHIFT.** SHIFT is active in any of three ways:
- the latched SHIFT button;
- holding the Shift key while the pointer is over the panel;
- Alt-dragging a single control.

Inverse legends mark the secondary functions: PW/DETUNE → DRIFT, MIX → PAN, DETUNE → PERF DETUNE [p.112]. The
other secondary functions have their own visible controls, so nothing needs SHIFT: U. SIZE, SWING, sequence
LOAD / STORE, SUB OSC, HF routing, AH / DH.

**DD-67 — Editor ↔ audio thread.** Nothing on the audio thread ever waits on the editor:
- **Editor → audio:** sequencer edits and the on-screen ribbon go through a lock-free FIFO that processBlock
  drains.
- **Audio → editor:** meters, the LOOP LED, voice counts and the arp step go through atomics. The working
  sequence is published with a try-lock; if the editor is reading at that moment, the audio thread retries on
  the next block.

**DD-68 — Window size.** The window is resizable from 50 to 200 % at the 1774 × 887 aspect ratio. The first
size is the largest that fits 92 % × 88 % of the screen, capped at 100 %. Resized sizes are saved with the
project.

**DD-69 — Rate LEDs.** The TEMPO, LFO 1 / LFO 2 RATE and DELAY TIME LEDs are computed in the editor from the
parameters and the tempo in force: synced note values, or HF = steady. The LOOP LED counts the envelope
cycles of the newest voice in the engine.

**DD-71 — Look · USER DIRECTION (2026-09-11).** The user said: keep the Phase 5 layout, but use the original
synth's colours and give it real-world depth and texture, "by no means over do it"; it must not look cheap
("made in Patcher"). A full hardware-replica layout was tried and rejected. What was kept:
- **Surfaces:** light grey painted metal with fine grain; raised section plates with soft shadows and
  bevels; recessed wells for dropdowns and the strip area.
- **Colour follows the layer, as on the original's two rows:** knobs and fader caps are white on UPPER and
  orange on LOWER. The top editor row turns white or orange with the layer it edits. Global and performance
  knobs are black.
- **Sub-panels:** OSCILLATORS and ENVELOPES sit on charcoal, like the hardware.
- **Screens:** the waveform scopes, delay time and arp step are amber screens.
- **Section titles:** each title is followed by a thin orange rule, like the lines printed on the hardware.
- **One light source, top left.** It sets every shadow, highlight and bevel.

**DD-72 — Brand · USER DIRECTION (2026-09-11).** Product **Geminus**, maker **SPKR**. The panel wordmark is
written S·P·K·R, in the style of the original maker's dotted wordmark. The host vendor name is SPKR. IDs:
bundle and CLAP `com.spkr.geminus`, manufacturer code `Spkr`, plugin code `Gmns`. Hosts treat it as a new
plugin, so the earlier "Super Gemini" VST3 installed on this machine is a separate, stale entry. All brand
text lives in `src/ui/BrandConfig.h` and CMakeLists.txt. The documentation keeps citing the UDO Super
Gemini manual, because that is the hardware being emulated.

**DD-70 — Patch menus before Phase 6.** The PATCH dropdowns offer "Load Init Patch" (resets the layer and
switches to SINGLE [p.22]) and show MANUAL when the layer is detached (DD-10). The patch library comes with
Phase 6. SUPER mode's seven oscillators get an SSE2 path for samples with no waveform
edge. A super voice's two voices are rendered interleaved, so their filter recursions overlap.

**DD-73 — Look · the instrument as an object · USER DIRECTION (2026-09-11).** The user asked for the depth
and detail of a classic-synth recreation (Arturia's JUP-8 V was the named reference) *without* losing the
UDO Super Gemini identity, and asked for the keyboard back: "the slight curvature from keyboard to panel,
the actual depth and texture". The Phase 5 layout is unchanged; what changed is that the whole instrument is
now drawn as a physical body on a 1862 × 1182 canvas:

- **Chassis.** Dark anodised end cheeks with shallow milled fluting run the full depth of the body and cast
  a shadow onto the panel; the panel face is brushed metal with a vertical shade (it leans back) and a
  radial vignette from the one top-left light source. Fixing screws sit along the seam with the cheeks.
- **Curvature to the keybed.** Below the face the panel folds into a **shelf** (a bright catch-light at the
  crease, darkening as it turns toward the player), then the **rolled front lip** (crest highlight, shade
  underneath), then the keybed. The shelf carries the BENDER, the ribbon controller [pp.69, 77] and an
  engraved S·P·K·R / GEMINUS nameplate.
- **Keyboard [p.16].** 61 mouse-playable keys, C1–C6 (MIDI 36–96); the lower part of a key plays louder.
  Black keys are offset toward the outside of their group, have a moulded crown highlight and a lit front
  face, and cast a soft shadow onto the white keys; white keys have a stepped front face and an ivory
  gradient. Notes go to the audio thread as UiCommand NoteOn/NoteOff on the base channel.
- **Header chrome.** The top band is dark anodised metal with the wordmark in white and an orange rule
  underneath; the light section plates (PATCH, LAYER, KEYBOARD MODE, SPLIT POINT) and the output
  oscilloscope sit on it. The status bar at the bottom mirrors it and carries the hint line, the voice
  count, the tempo, the edited layer and the settings menu.
- **Live displays.** Output oscilloscope (zero-crossing triggered), the edited layer's envelope curve drawn
  from its own times with the playing level [pp.46–53], the VCF response with a ghost curve at the
  *modulated* cutoff and a Hz read-out [pp.41–43], and a running LFO shape per strip [pp.54, 70].
- **Visible modulation.** Every knob and fader can show a second, dimmer indicator at the value modulation
  has actually reached; the panel reads the newest voice's matrix offsets and effective cutoff from the
  bridge each frame (`MainPanel::updateModulationRings`).
- **Livelier lamps.** TEMPO, DELAY and the LOOP LED are re-triggered and then decay like real lamps; each
  layer's LAYER lamp glows with that layer's own output level; the LFO lamps follow the LFO shape.

The DD-71 palette is unchanged apart from a slightly deeper panel grey, so the light section plates and
charcoal sub-panels separate from the metal behind them.

**DD-74 — HOLD survives the arpeggiator · USER DIRECTION (2026-09-17).** With HOLD on, switching the
arpeggiator on or off must not stop what is sounding. Switching it on hands the held notes to the
arpeggiator, which starts arpeggiating them immediately; switching it off plays the same notes
straight again. Only turning HOLD off (or releasing the keys with HOLD off) stops them. Changing
CLK DIV, the free RATE or the tempo while the arpeggio runs keeps the next step where it is and
spaces the following ones by the new length, instead of re-deriving the grid from its origin, which
could leave the arpeggio silent for many beats.

**DD-75 — SYNC off frees the arpeggiator clock · USER DIRECTION (2026-09-17).** On the hardware SYNC
only makes LFO 1 and the delay follow the clock, and the arpeggiator always runs on the clock grid
[p.92]. Here SYNC off also frees the arpeggiator: the same knob becomes ARP RATE, a free step length
of 20 ms … 2000 ms (new parameter `arp.rate`, logarithmic, 200 ms centred), and the panel prints
those three values instead of the CLK DIV divisions. SYNC on restores CLK DIV and the clock grid.
