import fs from 'node:fs';
const p = 'C:/Users/w0nde/Desktop/Vst/src/plugin/WebEditor.cpp';
let s = fs.readFileSync(p, 'utf8');

const old = s.slice(s.indexOf('// The page is a fixed-size panel'), s.indexOf('void GeminusWebEditor::resized()'));
if (!old.includes('applyZoom')) { console.log('ANCHOR MISS'); process.exit(1); }

const neu = [
  '// The page owns the fit - see fitToWindow() in ui/geminus.js. It is computed there and not',
  '// here because CSS zoom works in CSS pixels while this component is measured in device',
  '// pixels; on a display at anything but 100% the two differ by the display scale factor, and',
  '// a zoom derived from device pixels overflows the window by exactly that much. The page also',
  '// refits on its own resize event, so this is only a nudge for the cases that do not raise one.',
  'void GeminusWebEditor::applyZoom()',
  '{',
  '    if (web == nullptr || getWidth() < 10)',
  '        return;',
  '',
  '    web->evaluateJavascript ("window.geminusFit && window.geminusFit();", nullptr);',
  '}',
  '',
  ''
].join('\n');

fs.writeFileSync(p, s.replace(old, neu));
console.log('applyZoom now defers to the page');
