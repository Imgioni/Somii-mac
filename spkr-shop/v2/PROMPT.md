# SPKR — website prompt (spkr.shop)

Build the storefront for **SPKR**, an independent audio company that sells software instruments (VST3).
Domain: **spkr.shop**. First product: **GEMINUS** — a 20-voice, dual-layer, polyphonic, binaural
analog-hybrid synthesizer (VST3 / Windows). More instruments will follow.

## Direction
Functional clarity of an audio-tools shop (reference: dowfimusic.com — product index, texture kits,
"VIEW →" links) run through a **Yeezy / Rick Owens** lens:

- **Yeezy**: near-empty pages, concrete/dust neutrals, tiny uppercase labels, products shown as
  objects on a plain ground, no marketing noise. The product sells itself.
- **Rick Owens**: brutal scale contrast, severe condensed type, black-as-a-material, a sense of
  architecture and weight. Slightly unsettling, never messy.
- **Grain**: a live film-grain layer on the **background only** — text, images and panels sit cleanly above it.
- **Minimal but not empty**: one bold gesture per screen, everything else quiet and precise.

## Palette
Soft pastel pink + blue, not tied to the plugin colours.
- **Light**: pale lilac-white ground `#EFECF1` washed with large, blurred pastel fields —
  pink `#F4BAD0` and blue `#B8CCF4` at ~60% — frosted white surfaces, ink `#1C1B26`.
- **Dark**: the same pink and blue at ~15% opacity on near-black `#0C0C11`; ink `#EAE7EF`.
- Accent: pastel pink `#F3B4C9` with dark text, once per screen.

## Type
- **Archivo** (variable width 62–125): ultra-condensed black for the monumental SPKR / product
  names; ultra-expanded light for small labels. The width axis *is* the experiment.
- **Martian Mono**: specs, indexes, prices, serials — the "instrument" voice.
- Everything uppercase except body sentences. Wide tracking on labels.

## Structure
1. **Header** — `SPKR` wordmark · INDEX · SOUND · INFO · `BAG (0)`. Thin, fixed, blends with grain.
2. **Hero** — a cropped, oversized `SPKR` set in condensed black, with the Geminus interface sitting
   across it as an object. Caption: `001 / GEMINUS / VST3`.
3. **Index** — the catalog as a sparse grid of objects: `001 GEMINUS` (available), `002` and `003`
   redacted/forthcoming. Serial numbers, not marketing.
4. **Object 001** — Geminus detail: full interface, a mono spec sheet (voices, layers, format,
   OS, arp/seq, ribbon, mod matrix, chorus, delay), one sentence of copy, one signal-orange CTA.
5. **Sound** — demos placeholder, stated plainly: FORTHCOMING.
6. **List** — email signup for release news.
7. **Footer** — SPKR.SHOP, support, refund policy, socials as two-letter codes (IG / YT / SC).

## Motion
Animated grain (always, subtle). Hero type settles in on load. Hover on products shifts width
of the name (Archivo width axis). Nothing else. Respect `prefers-reduced-motion`.

## Rules
- Real content only; no invented prices or reviews — `PRICE TBA` until set.
- Works at 375px wide. No horizontal scroll.
- No rounded corners, no shadows, no gradients except the grain.
