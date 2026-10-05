import fs from 'node:fs';
let g = fs.readFileSync('gen.mjs', 'utf8');
let n = 0;
const sub = (a, b) => { if (!g.includes(a)) { console.log('MISS:', a.slice(0, 64)); return; } g = g.replace(a, b); n++; };

// 1. ribbon + keybed back to full size, spanning from clear of the PERFORMANCE block
sub(
  '// The keybed and ribbon are 20% smaller than the furniture and anchored bottom-right,\n' +
  '// which buys the layer rows their extra height.\n' +
  'const KBR = sc(PR), KBW = Math.round(sc(PR - 424) * 0.8), KBX = KBR - KBW;\n' +
  'const KBH = sc(296);                        // ~80% of the previous keybed height\n' +
  'body += ribbonRow(H - KBH - sc(78), KBX, KBW - sc(300));',
  'const KBX = sc(424), KBR = sc(PR), KBW = KBR - KBX;\n' +
  'const KBH = H - sc(700);\n' +
  'body += ribbonRow(sc(628), KBX, sc(PR - 330) - KBX - sc(30));'
);
sub('body += keyboard(KBX, H - KBH, KBW, KBH);',
    'body += keyboard(KBX, sc(700), KBW, H - sc(700));');

// 2. the matrix keeps its full size and opens over the ribbon and keybed
sub("body += popover('pop-matrix', 'MODULATION MATRIX', Math.round(BASEW - 1040), 300, 1010, 660,",
    "body += popover('pop-matrix', 'MODULATION MATRIX', Math.round((BASEW - 1010) / 2), 250, 1010, 700,");
sub("body += popover('pop-modulate', 'MODULATE', Math.round(BASEW - 470), 340, 420, 540, modulatePage());",
    "body += popover('pop-modulate', 'MODULATE', Math.round((BASEW - 420) / 2), 280, 420, 540, modulatePage());");

fs.writeFileSync('gen.mjs', g);
console.log(n + '/4 layout reverts applied');
