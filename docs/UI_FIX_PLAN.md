# Geminus UI fix plan (2026-09-14)

Status legend: `[ ]` open · `[x]` done · `[~]` partly done, see note.

This is the working list for the pass requested on 2026-09-14. The rewritten brief that this list
executes is `docs/reference/ui_prompt_v2.md`. The reference for the look is the real Super Gemini:
`docs/reference/assets/Screenshot 2026-09-11 171626.png` (hero photo) and
`docs/reference/assets/68d148e7ff02075891c68d26_Gemini_Outlines_lr.png` (official outline drawing).
Function follows the owner's manual (`docs/manual_notes.md`, page-cited).

## Root causes found

| Symptom the user reported | Cause | Fix |
|---|---|---|
| Typing-keyboard MIDI in FL Studio stops after touching the plugin until the title bar is clicked | WebView2 takes Win32 focus on every click; its HWND belongs to the msedgewebview2 process, so FL's keyboard hook never sees the keys | After every pointer gesture the page calls a native `hostFocus`; C++ moves focus back to the JUCE editor window (`SetFocus`). Text fields keep focus while being typed in. |
| Right-click shows Back / Refresh / Save as / Print | WebView2's default context menu | `contextmenu` is cancelled page-wide; right-click on a control still opens the MODULATE routing page |
| Ribbon: cursor is a plain arrow and vertical movement does nothing | Only X was read; no touch marker was drawn | Ribbon is now 2-D: X = relative pitch bend (as hardware), Y = pressure → channel aftertouch (drives LFO 2 in AT modes and the AT matrix source). A lit touch marker follows the finger, the cursor hides while touching, a bend read-out sits beside the strip |
| Voice-assign LEDs never change | The four LEDs were static images, only POLY 1 was drawn lit | Every LED ladder (voice mode, arp range/mode/clock div, octave, chorus, track) is bound to its parameter and clickable |
| Labels unreadable, mis-placed text | The page links Google Fonts (Barlow Condensed). Inside the plugin there is no network, so text falls back to Arial and every width estimate is wrong. Font size was also simply too small for a 2-row panel | Bahnschrift (ships with Windows 10/11, DIN-like, condensed axis) in both the browser preview and the plugin; larger type; default window scale computed from the monitor |
| Matrix page shows both layers stacked | one pop-over with two 8×8 grids | one grid, UPPER / LOWER tabs that follow the LAYER keys, amount read-outs under each knob |
| "Some buttons should be black/grey" | only the light button sprite was cut | dark sprite cut from `05_buttos_gray.png`; used where the hardware is dark: MANUAL/HOLD/LAYER LOWER, the three KEYBOARD keys, TRACK, matrix A–H row, SHIFT |
| Patch browser "looks terrible" | placeholder rows, no real file listing | folder-backed browser: lists `.gpatch` files in the user folder, load on click, save with name, INIT, prev/next in the top bar, current patch name shown |
| Dead buttons (SEQ REC, LOAD, STORE, MOD ASSIGN 1–8 / A–H, SAVE, A/B, POWER, GLIDE) | drawn but never bound | every button now does its hardware job or was removed if the hardware has no such control (GLIDE, POWER) |
| Matrix knobs for LFO 1 RATE / ENV 1 DECAY / DELAY TIME did nothing | pop-over used ids `lfo1`, `env1`, `dly`; the parameters are `lfo1Rate`, `env1Decay`, `dlyTime` | ids corrected; a coverage check in `gen.mjs` fails the build if a bound id is not a parameter |
| 13 parameters unreachable (ALT A/B waves, ENV 1 mode, U.SIZE, SWING, SEQ slot, single-layer select) | no control | ALT wave picker page, ENV 1 MODE switch, SHIFT+MODE = U.SIZE, SHIFT+RANGE = SWING, sequencer page slots, LAYER LOWER/UPPER keys |

## Work items

### A. Plugin side (C++)
- [x] `hostFocus` native function → `SetFocus` on the editor's peer HWND (Windows), no-op elsewhere.
- [x] Ribbon pressure: new `UiCommand::Type::Pressure` → `engine.channelPressure(base, f)`; native `ribbonPressure`; release sends 0.
- [x] Bender lever: native `bend(x)` and `push(y)` on the existing `Bend` / `Push` commands.
- [x] Sequencer: native `seqSetStep`, `seqSetLength`, `seqRecord`, `seqLoad`, `seqStore`, `seqClear`; the 30 Hz feed carries step/running/recording per layer and the working sequence whenever its revision changes.
- [x] `initPatch(layer)` (SHIFT + MANUAL and the browser's INIT keys) → `loadInitPatch`.
- [x] A/B compare: `abToggle` swaps the whole state with a stored copy; `abCopy` stores the current state.
- [x] Patch files: `patchList(folder)`, `patchLoad(path)`, `patchSave(folder, name)`, `patchFolder()` (default `Documents/Geminus/Patches`, created on demand). Current patch name is kept in the state tree (`patchName`) and pushed in the feed.
- [x] Default window scale from the primary display (≈90 % of its width, ≤ 100 %), remembered per project.

### B. Page generator (`ui/gen.mjs`, `ui/lib.mjs`)
- [x] Font: Bahnschrift condensed, no external link. Width estimates re-measured.
- [x] Type scale up; keyboard height down; wordmark and rules as on the hardware.
- [x] LAYER section (black inset, LOWER dark / UPPER light) → `perf.singleLayer`, also selects which layer the strip and the pop-overs edit.
- [x] VOICE ASSIGN: MODE key + 4 bound LEDs; SHIFT+MODE = U.SIZE (LEDs show size); UNISON, BINAURAL.
- [x] ARP/SEQ: ON, CLK DIV rotary + 8 LEDs, SYNC, RANGE + 4 LEDs (SHIFT = SWING, LEDs show swing), MODE + 5 LEDs, SEQ REC (records; LED), TRACK (dark; opens the sequencer page).
- [x] MOD ASSIGN: 1–8 source keys and A–H destination keys (dark) are live: pick source + destination, MOD AMOUNT edits that pair, LEDs show which pairs are routed, CLEAR clears the pair (SHIFT = all). MOD ASSIGN key opens the full matrix page.
- [x] Dark button sprite where the hardware is dark (list above).
- [x] Performance block: TRANSPOSED + 5 octave LEDs (clickable), OCT−/OCT+ momentary rocker (SHIFT = ST−/ST+ transpose), PORTAMENTO fader + layer switch (`perf.portaLayer`), LFO 2 (wave/rate/delay/trigger), DEST osc + layer (`perf.modLayer`), depth faders, bender lever (X bend, Y push).
- [x] GLIDE switch removed (no such control on the hardware; portamento at 0 = off, p.74).
- [x] ENV 1: MODE switch (NORMAL / INVERTED / LOOP) with LOOP LED.
- [x] DDS 1: ALT A / ALT B keys open the wave picker (W1–W32).
- [x] Every LED ladder is bound and clickable.
- [x] `gen.mjs` verifies every bound id against `params.tsv` and fails on a miss; it also writes `index.html` directly (no preview copy step).

### C. Pop-over pages
- [x] MATRIX: one layer, UPPER/LOWER tabs, 8×8 knobs with read-outs, big headings.
- [x] SEQUENCER: 16 steps per page × 4 pages, note names, SLIDE / ACCENT / REST per step, LENGTH, play head, REC, CLEAR, slots 1–16 with LOAD / STORE.
- [x] ALT WAVES: W1–W32 for channel A and B.
- [x] PATCH BROWSER: folder list, file list, name field, LOAD / SAVE / SAVE AS / OPEN / INIT UPPER / INIT LOWER, status line.
- [x] SETTINGS: tuning, keyboard, MIDI, oversampling note, global reset.
- [x] MODULATE (right-click): tidy, names the control, 8 source amounts.

### D. Behaviour
- [x] Context menu suppressed; browser accelerator keys ignored.
- [x] Focus returned to the host after each gesture (see A).
- [x] Shift layer: SHIFT key on the panel (dark, latching) as well as the keyboard Shift.
- [x] Double-click resets a control to its default.

### E. Verification
- [x] `node ui/gen.mjs` — coverage: all 178 non-matrix parameters + all 128 matrix cells bound.
- [x] Browser preview at 1× and 0.7×: no clipped labels, no overlaps, every page opened.
- [x] Build `Geminus_VST3` + `Geminus_Standalone`; `SGTests` still passes.
- [ ] FL Studio smoke test (typing keyboard after knob drag; right-click; ribbon) — needs the user.

## Not done, and why
- Factory sound bank: the browser lists files; only INIT patches ship. Sound design is a separate job.
- 61st key: the octave render cannot be cut for a lone top C (documented in `ui/README.md`).
- Hardware-only items stay out per DD-4 (TUNE, DUMP, MPE, USB, firmware).
