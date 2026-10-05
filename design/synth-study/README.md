# 002 main synth interface study

Open `index.html` directly, or run `node server.mjs` and visit http://127.0.0.1:5180.

This is an independent HTML visual prototype. It does not change the plugin, connect to its DSP, or produce audio. FX, patch browsing, and preset management are outside the design scope.

## Design read

A tactile instrument for musicians: matte stone enclosure, smooth silicone controls, graphite oscillator recesses and restrained burnt-orange state accents. Preserve the 002 identity and hardware vocabulary while enlarging the most-used controls and consolidating the playing surface into one layer at a time. The material direction follows the user's explicit request for matte finishes and smooth silicone knobs.

Design Taste dials: variance 4, motion 2, density 8. This is an operating instrument rather than a marketing page; hero, CTA, SEO and landing-page composition rules do not apply. The incumbent graphite oscillator recesses remain as functional contrast within a single light enclosure.

## Interactions

- Drag knobs vertically; Shift-drag for fine movement.
- Focus knobs and use arrow keys, Shift-arrow keys, Home/End or the mouse wheel.
- Double-click a knob or fader to restore its initial value.
- Switch between upper/lower layers and Gemini/3rd Wave layouts. Settings remain independent for this browser session.
- Change oscillator waveforms and envelope controls to see their illustrative diagrams update.
- Keyboard keys show pressed states; there is no audio engine.

Native HTML/CSS/JavaScript, locally bundled label font, and newly generated raster control art. No installed dependencies. The displayed response graphs are visual illustrations, not measurements of the production DSP. This is a composition study of the core controls, not a parameter-complete replacement editor.

## Skill selection

Boss: scope and review. Design Taste: audit, visual direction and readability. Ponytail: minimal standalone implementation. Imagegen: new reusable physical control artwork. Browser: rendered checks and interactions. The prior read-only setup assessment established that the existing project tools are sufficient; no plugin or automation installation is required.

The prototype and assets are deliberately outside the production `ui` folder.

## Verification

Browser checked at the normal 1280-pixel viewport and a 390-pixel responsive viewport. No horizontal overflow in the narrow layout and no browser console errors. Verified keyboard knob adjustment, double-click reset, independent filter-mode recall and independent layer recall. Inline JavaScript parses and local raster/font files resolve. `preview.png` captures the completed desktop composition. The prototype is vertically scrollable in shorter windows.
