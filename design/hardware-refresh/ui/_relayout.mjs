import fs from 'node:fs';
let g = fs.readFileSync('gen.mjs', 'utf8');
let n = 0;
const sub = (a, b) => { if (!g.includes(a)) { console.log('MISS:', a.slice(0, 62)); return; } g = g.replace(a, b); n++; };

// 1. taller layer rows - more air per control, easier to read
sub('const CY = s(98);', 'const CY = s(118);');
sub('const LBL = s(172);', 'const LBL = s(206);');
sub('const SUB = s(184);', 'const SUB = s(218);');
sub('const FH = s(128);', 'const FH = s(150);');

// 2. rows / strip / lower block move down to suit; the keybed gives up the room
sub('body += layerRow(12, true);', 'body += layerRow(12, true);');
sub("body += hrule(PL, 210, PR - PL, '#8E8F8D');", "body += hrule(PL, 246, PR - PL, '#8E8F8D');");
sub('body += layerRow(216, false);', 'body += layerRow(252, false);');
sub("body += hrule(PL, 414, PR - PL, '#6F706E');", "body += hrule(PL, 486, PR - PL, '#6F706E');");
sub("body += hrule(PL, 417, PR - PL, '#6F706E');", "body += hrule(PL, 489, PR - PL, '#6F706E');");
sub('body += globalStrip(422);', 'body += globalStrip(494);');
sub("body += hrule(PL, 530, PR - PL, '#8E8F8D');", "body += hrule(PL, 602, PR - PL, '#8E8F8D');");
sub('body += lowerLeft(44, 538);', 'body += lowerLeft(44, 610);');
sub('body += vrule(408, 536, 430);', 'body += vrule(408, 608, 376);');
sub('body += wordmark(546, PR);', 'body += wordmark(624, PR);');

// 3. ribbon + keybed shrink ~20% and sit bottom-right
sub(
  'const sc = (n) => Math.round(n * FURN);\nconst KBX = sc(424), KBR = sc(PR);   // 424 clears the PERFORMANCE block (44..390)\nbody += ribbonRow(sc(546), KBX, sc(PR - 330) - KBX - sc(30));',
  'const sc = (n) => Math.round(n * FURN);\n' +
  '// The keybed and ribbon are 20% smaller than the furniture and anchored bottom-right,\n' +
  '// which buys the layer rows their extra height.\n' +
  'const KBR = sc(PR), KBW = Math.round(sc(PR - 424) * 0.8), KBX = KBR - KBW;\n' +
  'body += ribbonRow(sc(628), KBX, KBW - sc(300));'
);
sub('body += keyboard(KBX, sc(616), KBR - KBX, H - sc(616));',
    'body += keyboard(KBX, sc(700), KBW, H - sc(700));');

// 4. the OUTPUT visualiser goes; MATRIX takes that slot, where there is room for it
sub("  mk(1862, 150, 'OUTPUT');\n  o += scope(1862, B + 6, 150, 46, 'scope-main');",
    "  mk(1862, 150, 'MODULATION');\n  o += opener(1866, B + 14, 34, 'MATRIX', 'pop-matrix', 7.4);\n" +
    "  o += opener(1916, B + 14, 34, 'ROUTE', 'pop-modulate', 7.4);");
sub("  o += opener(mx - 96, B + 14, 30, 'MATRIX', 'pop-matrix', 6.6);\n", '');

// 5. the matrix pops out over the ribbon and keybed, not over the panel
sub("body += popover('pop-matrix', 'MODULATION MATRIX', Math.round((BASEW - 1010) / 2), 60, 1010, 700,",
    "body += popover('pop-matrix', 'MODULATION MATRIX', Math.round(BASEW - 1040), 300, 1010, 660,");
sub("body += popover('pop-modulate', 'MODULATE', Math.round((BASEW - 420) / 2), 120, 420, 540, modulatePage());",
    "body += popover('pop-modulate', 'MODULATE', Math.round(BASEW - 470), 340, 420, 540, modulatePage());");

fs.writeFileSync('gen.mjs', g);
console.log(n + ' layout changes applied');
