# Filter corrections — 2026-10-04

The knob panel previously bound cutoff, resonance and envelope amount to the Gemini faders' IDs. Separate audio processors alone did not isolate the two modes. The new `upper/lower.tw.*` bank is saved and automated independently; existing parameter IDs and indices remain intact. Old state files seed missing 3W values from their stored shared controls once during loading.

3W uses its own cutoff, resonance, bipolar envelope amount and continuous key amount. The SVF now also has bipolar envelope amount, velocity and key amount. Key amount ranges from no tracking to two octaves per keyboard octave. ENV 1 remains the common synth envelope generator; general performance modulation and the modulation matrix remain shared instrument facilities. Hidden Gemini filter faders, its envelope selector/keytrack switch, HPF, DRIVE and VCA Dynamics brightness do not set the 3W filter response. The SVF precedes the low-pass. Resonance compensation can be switched off.

Gemini Drive 1 now shares Drive Off's gain at zero resonance and adds input compensation as resonance rises. Drive 2 no longer adds that compensation, allowing its overdrive to fall with resonance.

3W saturation no longer uses a reference-tone normalization table. Its continuous output soft clip allows level to rise into harmonic distortion, with a clean zero setting.

## Evidence and limits

- [UDO's Drive clarification](https://forum.udo-audio.com/t/vcf-drive-toggle-not-working-properly/2878): Off/1 match without resonance; position 1 adds resonance compensation; position 2 overdrives, with less overdrive as resonance rises.
- [3rd Wave Keyboard Manual v1.9](https://groovesynthesis.com/downloads/3WAVE/pdf/3rd%20Wave%20Keyboard%20User%20Manual%20v1.9.pdf): printed pp.24, 56–59 and 117 document saturation gain, serial routing, compensation, bipolar envelopes and key tracking.
- [Groove Synthesis](https://groovesynthesis.com/): identifies its analog low-pass as a Rossum SSI2140. The existing `CurtisFilter` class name is historical, not the actual hardware part.

This is a behavioural software approximation. The manuals do not specify exact cutoff calibration, envelope span, nonlinear transfer curves or circuit tolerances. The retained cutoff span and chosen saturation curve have not been calibrated against hardware recordings. Changes deliberately alter the sound of saturated and previously cross-coupled patches; state migration cannot make those old sounds identical while correcting the behaviour.

## Checks

`scripts/build.cmd --target SGTests SGPluginSmoke Geminus_VST3 Geminus_Standalone`

`build/SGTests_artefacts/Release/SGTests.exe filter` checks inactive-bank isolation, drive, frequency response, saturation, tracking and stability. `SGPluginSmoke` checks actual hosted VST3 state recall and legacy migration. `node tests/WebUiTests.mjs` checks generated parameter bindings and bank retention. Browser interaction checks the knob value survives switching back and forth without moving the Gemini fader.

Skills used: Boss for investigation/review, Ponytail for focused implementation, Claude automation recommender for read-only setup assessment, Impeccable for scoped panel layout, and Browser for interaction verification. Existing native build and UI checks were sufficient; no additional automation or skill installation was needed.

Verified result: 61 filter checks, 21 voice checks, 35 modulation checks and 3,102 UI checks passed. All 35 UI JavaScript files parsed. Hosted VST3 smoke tests, independent state recall and legacy migration passed. The panel was inspected in full and desktop layouts; the final desktop capture is `build/filter-panel-verified.png`. The Windows VST3 and standalone targets built successfully; the existing build's post-build step updated the installed VST3 in `C:/Program Files/Common Files/VST3/002 By SPKR/`.
