# Hardware asset refresh

Reference: [official UDO Super Gemini](https://www.udo-audio.com/super-gemini), top-down product photograph saved in references/super-gemini-official.webp.

Direction: preserve the existing 002 full and desktop section layouts. Refresh every main-face hardware asset: panel material, keys, ribbon, knobs, faders, tracks, switches, buttons, LEDs and bender. Matte silicone finish, ivory/orange/charcoal hardware colors. Refine direction arrows and track visibility. Original tick marks retained at the user's request. No FX or patch-panel redesign.

## Generation prompt

Use the supplied official UDO Super Gemini photograph as the hardware shape reference. Generate a new reusable transparent control asset sheet, not a panel mockup. Preserve the real synth's compact cylindrical knob silhouettes, short straight rectangular fader caps with horizontal index stripe, and upright rectangular pushbuttons. Clean matte silicone/rubber, smooth, never chrome, shiny, glassy, knurled or overly pillowy. Avoid generic oversized flat pucks.

Exactly nine isolated objects on a square transparent canvas in a precise 3 by 3 grid. Centers at one sixth, one half, five sixths. Row one: three identical rotary knobs in ivory, orange, charcoal; overhead orthographic circular top, subtle bevel, short base lip, narrow contrasting indicator at twelve o'clock. Row two: identical horizontal slider caps in ivory, orange, charcoal, approximately 2.1:1 aspect, straight small radii, shallow grip channel, thin contrasting horizontal stripe. Row three: upright rectangular pushbuttons with a close dark frame in ivory, orange, charcoal; 0.7:1 aspect, flat matte bevel, no LED. Ample transparent space, no labels or ticks. Consistent diffuse top-left light, minimal shadows, clean alpha, high resolution. Replacement assets for the existing layout, not an entire synth.

## Preview

Run `node build-refresh.mjs` to rebuild the copied UI, then `node server.mjs` for http://127.0.0.1:5180/. SVG viewports reference the unmodified generated PNG. Production source and installed plugin are untouched by this asset study.

## Remaining hardware generation prompt

Generate a professional transparent PNG sprite atlas for the main face of a UDO Super Gemini inspired virtual synth. Use the official hardware photograph for shapes and proportions. Match matte ivory, charcoal silicone, orange-red indicators, soft studio light top-left. Four columns by four rows on a square canvas. Each cell contains one isolated asset with ample transparent padding. No text, labels or tick marks.

Row one: single full length ivory piano white key overhead, plain rectangular flat satin top, no black keys; single charcoal piano black key overhead, long narrow rectangle with beveled tip; long horizontal black matte silicone ribbon touch strip, blank subtle inset edge, 5:1 aspect; horizontal pitch bend lever assembly in black socket, neutral centered.

Row two: identical narrow vertical black three-position slider switches, first down, second centered, third up; fourth cell an empty narrow vertical fader rail with recessed black slot and soft silver-grey rim, no cap or ticks.

Row three: horizontal octave rocker with black socket and short silicone lever tilted left; same centered; same tilted right; thin upright charcoal end-cap for ribbon.

Row four: small round dark red LED unlit in tiny black bezel; identical red LED lit subtly; blank wide smoked-black display lens with thin inset frame, 4:1; matte charcoal rectangular chassis edge strip. Orthographic overhead, smooth refined physical material. Black keys should resemble a real hardware keyboard, not rounded buttons. Do not render an entire synthesizer.

## Panel material prompt

Create a high resolution square seamless material texture, filling the canvas edge to edge. Premium light warm-neutral silver-grey powder-coated synthesizer front panel as seen on UDO Super Gemini. Very fine, almost imperceptible matte micrograin, beautifully smooth clean industrial surface, restrained and easy to read small dark labels on. No parts, typography, seams, scratches, vignette, gradient lighting, large mottling or shine. Flat uniform soft lighting. Average color approximately #c6c6c1. A repeatable UI panel material asset, not a synthesizer image.

## Implementation and checks

Keys are assembled from generated white and black key artwork at the existing note boundaries. Switch and bender states use the new atlas. Main-panel guide lines use the generated recessed rail. Pointer glyphs use smooth SVG chevrons. Section positions, parameter bindings, labels and keyboard hit regions remain unchanged. The preview generator validates 369 bound IDs and 836 parameters. Browser checks cover the full layout, desktop layout, filter-style switching, and missing image detection. This is an HTML visual study; browser audio/DSP equivalence is not claimed.
