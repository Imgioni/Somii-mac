import fs from 'node:fs';
let g = fs.readFileSync('gen.mjs', 'utf8');

const oldShift = "const shifted = '<div style=\"position:absolute;left:0;top:' + TOPBAR + 'px;width:' + W + 'px;height:' + H + 'px;\">' + body + '</div>';";
const oldFull  = "const full = presetBar(PL, 10, PR - PL) + hrule(PL, TOPBAR - 4, PR - PL, '#8E8F8D') + shifted;";

if (!g.includes(oldShift) || !g.includes(oldFull)) { console.log('anchors not found'); process.exit(1); }

// The preset bar is furniture: it belongs in a scaled layer like everything else,
// or it draws unscaled coordinates into a scaled page and stops short of the edge.
const q = String.fromCharCode(39);
const bar = [
  'const barH = Math.round(62 * FURN);          // scaled strip height',
  'const topBar = ' + q + '<div style="position:absolute;left:0;top:0;width:' + q + ' + BASEW + ' + q + 'px;height:62px;' + q + '',
  '     + ' + q + 'transform:scale(' + q + ' + FURN + ' + q + ');transform-origin:0 0;">' + q + '',
  '     + presetBar(MARGIN, 9, BASEW - MARGIN * 2)',
  '     + hrule(MARGIN, 58, BASEW - MARGIN * 2, ' + q + '#8E8F8D' + q + ')',
  '     + ' + q + '</div>' + q + ';',
  '',
  'const shifted = ' + q + '<div style="position:absolute;left:0;top:' + q + ' + barH + ' + q + 'px;width:' + q + ' + W + ' + q + 'px;height:' + q + ' + H + ' + q + 'px;">' + q + ' + body + ' + q + '</div>' + q + ';',
  'const full = topBar + shifted;'
].join('\n');

g = g.replace(oldShift + '\n' + oldFull, bar);

// the page is that much taller now that the bar is scaled
g = g.replace("page('Geminus', W, H + TOPBAR, full, W, H + TOPBAR)",
              "page('Geminus', W, H + barH, full, W, H + barH)");
g = g.replace("{ file: 'Main.dc.html', x: 0, y: 0, w: W, h: H + TOPBAR,",
              "{ file: 'Main.dc.html', x: 0, y: 0, w: W, h: H + Math.round(62 * FURN),");

fs.writeFileSync('gen.mjs', g);
console.log('preset bar moved into a scaled layer and spans the full width');
