// FX tab skins. Every module is ours: its colours come from the 002 itself - the cream panel, the
// charcoal insets, the SUPER SIX slate, a walnut, and the SPKR orange - and its knob caps are the
// 002's own cream / orange / black caps drawn flat. The first versions of some faces were modelled on
// commercial plug-ins; on 2026-10-04 the user asked for those to be toned down into something
// personal, so no face here borrows another product's palette, layout, typeface or wording.
// Used by tools/make-fx-controls.mjs (to draw the knob caps) and by ui/fx.js.
//   bg         face colour (solid)       ink / ink2  text, secondary text
//   accent     value arcs, lit keys      screen      display background
//   knob       cap style (fx-knob-<knob>.svg)        layout  arrangement in ui/fx.js
//   hero       the cap for a module's one big knob
const M = {
  cream:    { bg: '#E7E2DA', ink: '#23252A', ink2: '#625E57', screen: '#DAD4CA', knob: 'coal', hero: 'coal' },
  graphite: { bg: '#2A2A2C', ink: '#E7E2DA', ink2: '#A8A49C', screen: '#1E1E20', knob: 'bone', hero: 'bone' },
  slate:    { bg: '#1E3944', ink: '#FFE8D1', ink2: '#A9C4CC', screen: '#162C35', knob: 'slate', hero: 'bone' },
  walnut:   { bg: '#382A22', ink: '#F1E6D8', ink2: '#BFAB97', screen: '#2B2019', knob: 'bone', hero: 'ember' }
};
const ORANGE = '#F65A27', TEAL = '#68C3D4', AMBER = '#F2A33A';
const theme = { bg: 'var(--pop-bg)', ink: 'var(--pop-ink)', ink2: 'var(--pop-ink2)', accent: 'var(--pop-accent)', screen: 'var(--pop-bg2)', knob: 'house', hero: 'house' };
export const SKINS = {
  // the unbranded effects and CONTOUR follow the active theme (GEMINI / SUPER SIX / DARK), darker
  house:    { ...theme, layout: 'grid' },
  eq:       { ...theme, layout: 'eq' },
  undertow: { ...M.slate, accent: TEAL, layout: 'undertow' },
  halo:     { ...M.cream, accent: ORANGE, layout: 'halo' },
  spaces:   { ...M.walnut, accent: AMBER, layout: 'spaces' },
  marble:   { ...M.cream, accent: ORANGE, layout: 'marble' },
  valve:    { ...M.graphite, accent: AMBER, layout: 'valve' },
  heat:     { ...M.graphite, accent: ORANGE, layout: 'heat' },
  pump:     { ...M.cream, accent: ORANGE, knob: 'ember', layout: 'pump' },
  ceiling:  { ...M.slate, accent: '#FFE8D1', knob: 'bone', layout: 'ceiling' },
  prism:    { ...M.graphite, accent: TEAL, layout: 'prism' },
  // PARLOUR wears the 002's own LOWER orange, printed in its ink
  parlour:  { bg: ORANGE, ink: '#23252A', ink2: '#4A1D10', screen: '#E04E1F', accent: '#23252A', knob: 'coal', hero: 'bone', lower: '#FFE8D1', layout: 'parlour' }
};
// which skin each effect wears (ui/fxdefs.mjs ids); anything unlisted is 'house'
export const SKIN_OF = { revocean: 'undertow', ambient: 'halo', valleyverb: 'spaces', nudestort: 'marble', tuba: 'valve', saturn: 'heat',
  vulf: 'pump', faraday: 'ceiling', autochroma: 'prism', proq: 'eq', parlour: 'parlour' };
// layer colours of the LAYER MIX strip: the theme's own (GEMINI: cream UPPER, orange LOWER)
export const UPPER_COLOUR = 'var(--pop-upper)', LOWER_COLOUR = 'var(--pop-lower)';
// the cap styles tools/make-fx-controls.mjs draws
export const KNOB_STYLES = ['house', 'bone', 'coal', 'ember', 'slate'];
