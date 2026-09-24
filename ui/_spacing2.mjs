import fs from 'node:fs';
let g = fs.readFileSync('gen.mjs','utf8'); let n=0;
const sub=(a,b,tag)=>{if(!g.includes(a)){console.log('MISS '+tag);return;} g=g.replace(a,b); n++;};

// fader shift-pill: same reserved-line rule as the knobs
sub("const FSUB = s(218);                     // fader secondary-label baseline",
    "const FSUB = FLBL + LROW + 4;            // fader secondary-label baseline", 'FSUB');
// LROW must be declared before FLBL/FSUB use it
sub("const FTOP = s(43);", "const LROW = Math.ceil(8.5 * TEXT * 1.05);   // reserved height of one label row\nconst FTOP = s(43);", 'LROW early');
sub("// The shift pill clears a whole label line rather than a constant that predates TEXT.\nconst LROW = Math.ceil(8.5 * TEXT * 1.05);   // reserved height of one label row\nconst SUB = LBL + LROW + 4;",
    "// The shift pill clears a whole label line rather than a constant that predates TEXT.\nconst SUB = LBL + LROW + 4;", 'LROW dedupe');

// rotary step labels vs the rotary's own caption
sub("const rd = 46, rcy = bt + 66, rly = bt + 108;", "const rd = 46, rcy = bt + 66, rly = bt + 114;", 'rly');

// the lower layer row needs the height the taller label block now occupies
sub("body += layerView(false, 252);", "body += layerView(false, 264);", 'row2');
sub("body += hrule(PL, 486, PR - PL, '#6F706E');", "body += hrule(PL, 498, PR - PL, '#6F706E');", 'hrule1');
sub("body += '<div style=\"position:absolute;left:0;top:494px;", "body += '<div style=\"position:absolute;left:0;top:506px;", 'strip top');
sub("body += hrule(PL, 602, PR - PL, '#8E8F8D');", "body += hrule(PL, 614, PR - PL, '#8E8F8D');", 'hrule2');
sub("body += lowerLeft(44, 610);", "body += lowerLeft(44, 622);", 'lowerLeft');
sub("body += vrule(408, 608, 376);", "body += vrule(408, 620, 376);", 'vrule');
sub("body += wordmark(624, PR);", "body += wordmark(636, PR);", 'wordmark');

// INIT PATCH pill clears the LOWER/UPPER button labels
sub("o += txtInv(44, B + 50, 72, 'INIT PATCH', { size: 6.6 });",
    "o += txtInv(44, B + 58, 72, 'INIT PATCH', { size: 6.6 });", 'init patch');

// MOD ASSIGN: the source labels were landing on the destination buttons
sub("o += button({ x: mx + i * step, y: B + 48, w: 24, on: i === 4, label: s, lsize: 6.4, led: false });",
    "o += button({ x: mx + i * step, y: B + 58, w: 24, on: i === 4, label: s, lsize: 6.4, led: false });", 'dst row');
sub("o += txt(mx - 34, B + 52, 30, 'DEST', { size: 7, align: 'right', color: INK2 });",
    "o += txt(mx - 34, B + 62, 30, 'DEST', { size: 7, align: 'right', color: INK2 });", 'dest label');
sub("const mx = 1150, step = 52;", "const mx = 1166, step = 52;   // clear of the MOD AMOUNT range legend", 'mx');

// wordmark strapline clears the GEMINUS cap height
sub("o += txt(wx + 98, y + 46, 300, '20 VOICE DUAL LAYER POLYPHONIC BINAURAL', { size: 7.2, align: 'left', color: INK2 });",
    "o += txt(wx + 98, y + 54, 300, '20 VOICE DUAL LAYER POLYPHONIC BINAURAL', { size: 7.2, align: 'left', color: INK2 });", 'strap1');
sub("o += txt(wx + 98, y + 60, 300, 'ANALOG-HYBRID SYNTHESIZER', { size: 7.2, align: 'left', color: INK2 });",
    "o += txt(wx + 98, y + 68, 300, 'ANALOG-HYBRID SYNTHESIZER', { size: 7.2, align: 'left', color: INK2 });", 'strap2');

// ENV 2 column pack: AH / DH captions were touching across columns
sub(`    EF(0.74, 'S', null, null, 'env2.sustain'), EF(0.36, 'R', 'orange', null, 'env2.release')
  ], 5);`,
`    EF(0.74, 'S', null, null, 'env2.sustain'), EF(0.36, 'R', 'orange', null, 'env2.release')
  ], 9);`, 'env2 gap');

// PORTAMENTO is a long caption on a narrow fader: give it room before GLIDE
sub(`    { w: 10, r: () => '' },
    { w: wSw3(40), r: (bx) => sw3({ x: bx, y: c1 - 20, h: 40, pos: 2, opts: ['ON', 'LEG', 'OFF'], label: 'GLIDE', ly: l1 }) }`,
`    { w: 30, r: () => '' },
    { w: wSw3(40), r: (bx) => sw3({ x: bx, y: c1 - 20, h: 40, pos: 2, opts: ['ON', 'LEG', 'OFF'], label: 'GLIDE', ly: l1 }) }`, 'porta gap');

// UNISON / BINAURAL sit shoulder to shoulder: trim the caption and part them
sub("o += button({ x: 664, y: B + 12, w: 26, on: false, label: 'UNISON', id: 'voice.unison', layered: true });",
    "o += button({ x: 664, y: B + 12, w: 26, on: false, label: 'UNISON', lsize: 6.2, id: 'voice.unison', layered: true });", 'unison');
sub("o += button({ x: 700, y: B + 12, w: 26, on: true, label: 'BINAURAL', id: 'voice.binaural', layered: true });",
    "o += button({ x: 708, y: B + 12, w: 26, on: true, label: 'BINAURAL', lsize: 6.2, id: 'voice.binaural', layered: true });", 'binaural');

fs.writeFileSync('gen.mjs', g);
console.log(n + '/19 applied');
