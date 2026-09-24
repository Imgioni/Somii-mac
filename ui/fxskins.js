// FX tab skins: each effect named after a real plug-in gets a look inspired by it (a lookalike,
// never a copy, no names or logos); the unbranded ones share the house style.
// Used by tools/make-fx-controls.mjs (to draw the flat control assets) and by ui/fx.js.
//   bg / bg2   panel gradient          ink / ink2  text, secondary text
//   accent     value arcs, lit keys    screen      display background
//   knob       cap style (fx-knob-<knob>.svg)       font  heading face
//   layout     control arrangement in ui/fx.js
export const SKINS = {
  // the pop-over pages' dark flat look, in the active theme's darker colours (ui/gen.mjs THEMES 'pop-*')
  house:   { bg: 'var(--pop-bg)', bg2: 'var(--pop-bg2)', ink: 'var(--pop-ink)', ink2: 'var(--pop-ink2)', accent: 'var(--pop-accent)', screen: 'rgba(0,0,0,.26)', knob: 'house', font: 'sans', layout: 'grid' },
  // PRO-Q: the FF-style layout in the theme's darker colours
  eq:      { bg: 'var(--pop-bg)', bg2: 'var(--pop-bg2)', ink: 'var(--pop-ink)', ink2: 'var(--pop-ink2)', accent: 'var(--pop-accent)', screen: 'rgba(0,0,0,.3)', knob: 'house', font: 'sans', layout: 'fab' },
  ocean:   { bg: '#142C5C', bg2: '#0C1D42', ink: '#E8EEFF', ink2: '#8FA3CF', accent: '#6E98FF', screen: '#0A1A3C', knob: 'ocean', font: 'mono', layout: 'ocean' },
  ambient: { bg: '#F2F1EC', bg2: '#E4E3DC', ink: '#141414', ink2: '#7A7A74', accent: '#1C9E9A', screen: '#ECEBE5', knob: 'rings', font: 'wide', layout: 'ambient' },
  // VintageVerb-style: deep navy, violet-to-cyan knobs in outlined boxes
  valley:  { bg: '#10143A', bg2: '#070920', ink: '#F2F3FF', ink2: '#9CA2DA', accent: '#8E6BFF', screen: '#0B0E2A', knob: 'violet', font: 'sans', layout: 'valley' },
  // Nudistort-style: a white card, serif headings, outlined circle knobs round a marbled painting
  nude:    { bg: '#F7F5F1', bg2: '#EFECE6', ink: '#141414', ink2: '#77726B', accent: '#1FB6C9', screen: '#F7F5F1', knob: 'house', font: 'serif', layout: 'nude', nameFont: 'serif', nameColor: '#141414' },
  tuba:    { bg: '#1A1A1B', bg2: '#0E0E0F', ink: '#F1F1EE', ink2: '#8E8E8A', accent: '#F1F1EE', screen: '#0E0E0F', knob: 'tuba', font: 'small', layout: 'tuba' },
  fab:     { bg: '#252B34', bg2: '#161A20', ink: '#E6EBF2', ink2: '#8B96A6', accent: '#FFC94A', screen: '#12161C', knob: 'fab', font: 'sans', layout: 'fab' },
  saturn:  { bg: '#3B1719', bg2: '#110909', ink: '#F4E9E7', ink2: '#B8938F', accent: '#E0453A', screen: '#2A1011', knob: 'white', font: 'sans', layout: 'saturn', nameFont: 'serif', nameColor: '#D9B26A' },
  vulf:    { bg: '#F1EEE7', bg2: '#E8E4DB', ink: '#2A2A2A', ink2: '#8A857B', accent: '#D42F3C', screen: '#F1EEE7', knob: 'house', font: 'typewriter', layout: 'ghz' },
  faraday: { bg: '#EFEFEE', bg2: '#E3E3E2', ink: '#34363E', ink2: '#8C8E96', accent: '#4B4E5C', screen: '#EFEFEE', knob: 'house', font: 'sans', layout: 'ghz' },
  chroma:  { bg: '#E7E2D4', bg2: '#DDD7C6', ink: '#3E4A3F', ink2: '#7F8A7E', accent: '#8FBF97', screen: '#E7E2D4', knob: 'house', font: 'sans', layout: 'chroma' }
};
// which skin each effect wears (ui/fxdefs.mjs ids); anything unlisted is 'house'
export const SKIN_OF = { revocean: 'ocean', ambient: 'ambient', tuba: 'tuba', saturn: 'saturn', proq: 'eq', vulf: 'vulf', faraday: 'faraday', autochroma: 'chroma', valleyverb: 'valley', nudestort: 'nude' };
// the cap styles tools/make-fx-controls.mjs draws
// layer colours of the LAYER MIX strip: the theme's own (GEMINI: cream UPPER, orange LOWER)
export const UPPER_COLOUR = 'var(--pop-upper)', LOWER_COLOUR = 'var(--pop-lower)';
export const KNOB_STYLES = ['house', 'ocean', 'rings', 'tuba', 'fab', 'white', 'violet', 'azure', 'cyan', 'vdecay'];
