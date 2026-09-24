# Geminus UI — the spec

**This folder is the source of truth for how Geminus looks.** The plugin's interface is this
page, rendered by `juce::WebBrowserComponent` (JUCE 8). The native JUCE editor that used to live
in `src/ui/` is superseded.

The DSP, the parameter tree and the MIDI map are unaffected — only the interface changes.

## Files

| File | Role |
|---|---|
| `index.html` | **the panel.** Standalone, opens in any browser. 2240 × 1007. |
| `layer-row.html` | one layer row at detail scale, for checking control geometry |
| `components.html` | every asset in each of its states (knob sweep, fader positions, button lit/unlit, switch detents) |
| `*.png`, `*.jpg` | the cut assets the page references, by bare filename |
| `gen.mjs`, `lib.mjs` | the generator that produces the pages — edit these, not the HTML |
| `mkpreview.mjs` | rebuilds the three standalone pages |

Regenerate with `node gen.mjs && node mkpreview.mjs` from this folder.

## Current control artwork

The September asset refresh uses the originals in `docs/reference/assets/New assets`.
From the repository root run:

```
node tools/prepare-ui-assets.mjs
node ui/gen.mjs
node tools/check-ui-assets.mjs
```

`new-*.svg` files are self-contained views of the original PNG bytes: transparent
padding and sheet neighbors are excluded without resampling or modifying the source.
They are embedded in the VST, and their contents participate in the editor cache key.
Standard knob sheet 1, small knob sheet 1 and bender sheet 1 were selected for
consistent geometry; the alternative sheets and separate bender base remain untouched.
The bender sheet already includes its base and all six left/center/right + push poses.
The one-pose vertical toggle uses three actuator offsets over a fixed socket.
Knob top faces rotate inside an elliptical projection over the stationary sidewall.
Buttons retain their existing hitboxes and labels; their square art is aspect-fitted.

The ALT picker uses `wave-previews.json`, exported from the DSP's factory bank.
After changing factory waves, build `SGWavePreview` and run
`build-vs\Release\SGWavePreview.exe ui\wave-previews.json` from the project root,
then regenerate `ui/index.html` with `node ui/gen.mjs`. The SVG previews are
embedded in the generated HTML, so no JSON request is needed in the VST.

## Custom wave (DDS 1)

ALT B's key is now **CUSTOM** (both layers). It opens `pop-custom` (`customPage` in `gen.mjs`,
runtime in `geminus.js`): drop or browse an audio file (WAV / AIFF / FLAC / OGG / MP3, 30 s max),
drag START / LOOP START / END on the waveform (START plays once; LOOP repeats LOOP START → END with a
30 ms equal-power crossfade at the seam; ONE SHOT fades out over 5 ms at END), ONE SHOT / LOOP, LEVEL, ROOT KEY (◀ ▶ or LEARN from the next key
played) and FINE. The file goes to the plugin as base64 (`customLoad` / `customClear`), is decoded
by JUCE, and is saved in the state's `CUSTOM` child, so projects, patches and A/B carry it. The
outline comes back in the state feed as `customUpper` / `customLower`. With BINAURAL on, the left
voice plays the file's left channel and the right voice its right. The `dds1.altB` parameter is
kept (older patches morph to it) but has no control any more.

## Rules this design follows

**Assets only.** Every physical control is an actual photograph: `panel.png` (surface),
`k2-*.png` (knob caps), `fader-slot.png` + `fader-cap-*.png`, `sw-top/mid/bot.png`,
`btn-on/off.png`, `led-on/off.png`, `octave.png`, `cheek.png`, `ribbon.png`, `glass.png`,
`panel-dark.png` (inset sub-panels), `shading.jpg` (key light, multiply on top). Only silkscreen
is drawn: text, tick marks, scale numbers, section rules. Nothing is faked in CSS.

**Layout follows the real UDO hardware**, not the AI layout mockups — see
`docs/reference/assets/68d148e7…Gemini_Outlines_lr.png` and the product photo. Per-layer row
order: `LFO 1 | DDS MODULATOR | OSCILLATORS | MIXER | VCF | VCA | ENVELOPES | DLY`.

**Knobs** carry a drawn indicator bar over the original `01_knobs.png.png` render, rotated with
the cap; the panel also carries printed tick scales, as UDO does.

## Geometry (from `gen.mjs` — the numbers to hold to)

```
canvas            2240 × 1007        preset bar  y 10, h 44, TOPBAR 62
layer rows        y 12 and y 216, 190 tall, 204 apart
control centre    CY  98   (every knob, fader and switch centres on this line)
label baseline    LBL 172, inverse secondary label SUB 184
fader             slot 10 wide, travel 128, cap 13.8 × 22.4 (true 456:280 aspect)
knob              46 standard, 60 master, 50 mixer; rotaries 36–38
switch            three detents, h 46
sections    lfo 146/300 · mod 456/320 · osc 786/400 · mix 1196/76
            vcf 1282/270 · vca 1562/175 · env 1747/366 · dly 2123/84
surface     #E7E2DA warm off-white · accent #F65A27 · ink #23252A · inset #424243
```

## Themes

SETTINGS > THEME. **GEMINI** (default) is the original hardware look. **SUPER SIX**: base #568EA3,
accent + LOWER + dark caps #826251, upper caps + keys + insets #FFE8D1, text #FFFFFF, lines #68C3D4. Saved globally (`uiTheme`). Colours are CSS variables (`THEMES` in
`gen.mjs`); the photographed assets are never replaced - SVG filters re-map each asset's measured
mid-tone luminance onto the palette colour. New asset? Measure its median luminance and add it to
`TINT_SVG` / `TINT_CSS`.

## Desktop mode (laptop screens)

Toggled from SETTINGS or the DESKTOP key in the top bar; saved globally. It re-flows the same
controls, at 100 % size (MOD ASSIGN enlarged to 112 %), into a 2028 × 1152 canvas: one engine at a time (the LAYER keys choose
which), then two rows of shared sections. No keyboard or
ribbon. The layout is the `DK_LINES` table in `gen.mjs`: every section is a `.dk` card, lines
are justified to the widest one, separators and card positions are generated. Never place cards
by hand in CSS and never shrink a card (the `k` argument of `dk()` only enlarges) — readability on small screens is the whole point.

## Known gaps — real work, not decoration

1. **Bottom-left PERFORMANCE block is incomplete** (octave, transpose, portamento, glide, LFO 2,
   depth faders). It needs finishing in the same style before wiring.
2. ~~Google Fonts~~ — done: no network dependency. The page uses Bahnschrift on Windows and falls
   back to the bundled `spkr-condensed-*.woff2` (Roboto Condensed, SIL OFL) elsewhere, which is what
   macOS gets. See `docs/BUILD-MACOS.md`.
3. **MIXER is already correct** — MIX crossfade plus PAN as MIX's shift function, not the five
   faders of the earlier mockup.
4. The keybed is 5 full octaves (60 keys). A 61st top C is not cuttable from `octave.png`: every
   white key in it has a black key overlapping one side.

## Not to be reintroduced

The native editor's look — flat vector knobs, cool grey `#CCCBCB` panel, the edit-row + condensed
strips architecture. If a question comes up about how something should look, the answer is in
`index.html`, not in `src/ui/`.
