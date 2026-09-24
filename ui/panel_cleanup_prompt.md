# Geminus panel cleanup prompt

Fix the Geminus editor against the supplied Super Gemini references.

- Remove any separator or horizontal rule that crosses through the Upper/Lower controls, fader labels, oscillator panels, or envelope panels. Separators may appear only in the empty gutter between sections.
- Keep the Upper/Lower editor as one clean, centered layer view. Do not leave remnants of the hidden second layer or old two-row separators.
- Make every black oscillator and envelope inset contain all of its own controls, labels, ALT buttons, tuning/mode controls, and secondary text. Nothing may hang below the panel or appear detached in the background.
- Preserve the natural aspect ratio of every photographic asset. Shorten fader travel through layout geometry; never stretch a fader image vertically.
- Recenter each section's control cluster inside its section bounds, with equal visual breathing room on both sides. Pay special attention to DDS 1/DDS 2, ENV 1/ENV 2, Mixer, and the far-right DLY section.
- Keep the section order and hardware hierarchy faithful to the Super Gemini reference. Do not crop the DLY section or move the ribbon/keyboard.
- Keep labels attached to their controls, readable, and inside the same visual group. No labels may collide with a separator or panel edge.
- Preserve all existing parameter IDs, hit targets, layer switching, and WebView bindings.

Reject the result if a line crosses a control, a button floats outside its panel, a fader looks vertically distorted, or any section is visibly off-center.
