import fs from 'node:fs';
let s = fs.readFileSync('geminus.js', 'utf8');

const q = String.fromCharCode(39);
const old = [
  'function reportPageSize() {',
  '  const root = document.querySelector(' + q + 'div[style*="position:relative"]' + q + ');',
  '  if (!root || !Juce || !Juce.getNativeFunction) return;',
  '  const w = Math.round(root.getBoundingClientRect().width);',
  '  const h = Math.round(root.getBoundingClientRect().height);',
  '  try { Juce.getNativeFunction(' + q + 'pageSize' + q + ')(w, h); } catch {}',
  '}'
].join('\n');

if (!s.includes(old)) { console.log('ANCHOR MISS'); process.exit(1); }

const neu = [
  'let natW = 0, natH = 0;',
  '',
  '// offsetWidth, not getBoundingClientRect: the rect shrinks with the zoom set below, so',
  '// measuring with it would report a page that gets smaller every time it is measured.',
  'function measurePage() {',
  '  const root = document.querySelector(' + q + 'div[style*="position:relative"]' + q + ');',
  '  if (!root) return false;',
  '  natW = root.offsetWidth;',
  '  natH = root.offsetHeight;',
  '  return natW > 0 && natH > 0;',
  '}',
  '',
  '// The panel is a fixed-size drawing that scales as a unit, and the fit is computed here',
  '// rather than in C++ on purpose: CSS zoom works in CSS pixels, while the host measures',
  '// the view in device pixels. On a display at anything but 100% those differ by the scale',
  '// factor, and a zoom derived from device pixels overflows the window by exactly that much.',
  'function fitToWindow() {',
  '  if (natW === 0 && !measurePage()) return;',
  '  const z = Math.min(window.innerWidth / natW, window.innerHeight / natH);',
  '  const d = document.documentElement;',
  '  d.style.zoom = z > 0.01 ? String(z) : ' + q + '1' + q + ';',
  '  d.style.overflow = ' + q + 'hidden' + q + ';',
  '  if (document.body) {',
  '    document.body.style.overflow = ' + q + 'hidden' + q + ';',
  '    document.body.style.margin = ' + q + '0' + q + ';',
  '  }',
  '}',
  'window.geminusFit = fitToWindow;',
  'window.addEventListener(' + q + 'resize' + q + ', fitToWindow);',
  '',
  '// Tell the plugin how big the design actually is, so the window aspect and the resize',
  '// limits follow it instead of a constant that goes stale when the panel is re-laid.',
  'function reportPageSize() {',
  '  if (natW === 0 && !measurePage()) return;',
  '  if (!Juce || !Juce.getNativeFunction) return;',
  '  try { Juce.getNativeFunction(' + q + 'pageSize' + q + ')(natW, natH); } catch {}',
  '}'
].join('\n');

s = s.replace(old, neu);

// fit on start, alongside the other boot steps
let n = 0;
s = s.split('reportPageSize(); startStateFeed();').join('reportPageSize(); fitToWindow(); startStateFeed();');
n = (s.match(/fitToWindow\(\); startStateFeed\(\)/g) || []).length;

fs.writeFileSync('geminus.js', s);
console.log('fit installed, boot hooks: ' + n);
