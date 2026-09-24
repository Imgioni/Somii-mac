# Prompt: SPKR-branded Geminus color scheme

Reskin the Geminus plugin UI to match the spkr.shop brand palette, replacing the current
warm-grey/orange hardware look with these five colors:

- **Base / panel** `#46494C` — charcoal, replaces the current cream/grey panel surface
- **Grey accent** `#818D92`
- **Taupe accent** `#B9A394`
- **Pink-grey accent** `#D4C5C7`
- **Text / knob / highlight** `#C5C3C6` — replaces the current orange knob/LED color as the
  primary accent (labels, pointers, active states, LEDs)

## Rules
- Keep the panel a single flat charcoal (`#46494C`) — no gradient or fake lighting added
  beyond what's already baked into the existing panel/knob assets.
- `#C5C3C6` becomes the new "signal" color: knob indicator lines, active LEDs, selected
  buttons/switches — everywhere the current build uses orange.
- `#818D92`, `#B9A394`, `#D4C5C7` are for secondary accents only — e.g. alternate-layer
  knobs (Lower layer currently uses orange to distinguish it from Upper), section dividers,
  or inactive-state fills. Assign them by function, don't scatter randomly.
- Per the assets-only rule: only recolor the existing PNG assets (knobs, buttons, LEDs,
  panel surface) — don't fake shading, gradients, or textures in CSS that aren't in the
  source art. If recoloring an asset isn't possible without redrawing it, flag it instead
  of approximating.
- Keep contrast readable: text/labels must stay legible on the charcoal base, same bar as
  the website's C5C3C6-on-46494C pairing.
- Do not touch layout, knob positions, or panel geometry — color only.
