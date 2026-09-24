# Geminus editor-band layout prompt

Re-layout the Geminus Upper/Lower editor band so it uses the available vertical space above the global performance strip and reads as the main instrument panel.

Requirements:

- Keep the real Super Gemini hardware hierarchy and left-to-right section order: LFO 1, DDS Modulator, Oscillators, Mixer, VCF, VCA, Envelopes, DLY.
- Use only the supplied photographic assets for physical controls. Do not invent replacement hardware, redraw knobs, or distort photographs.
- Preserve every asset's natural aspect ratio. Any enlargement must be uniform for that asset; never use a non-uniform vertical stretch.
- Increase readable scale through layout: give the controls, black oscillator insets, headings, and labels more vertical room and stronger spacing.
- Keep each label visually attached to its control and prevent label/control collisions.
- Keep the DLY section fully visible at the right edge.
- Keep Upper and Lower as alternate views of the same editor slot; do not stack both visible at once.
- Reduce the dead zone between the editor band and the global strip without pushing the ribbon or keyboard out of the intended composition.
- Preserve the existing parameter IDs, hit targets, layer switching, and WebView bindings.
- The result should look like a naturally enlarged hardware panel, not a stretched screenshot.

Before shipping, inspect the generated page at its intended aspect ratio and reject any version with cropped sections, distorted assets, overlapping text, or a newly introduced empty band.
