import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const root=path.dirname(fileURLToPath(import.meta.url));
const ui=path.join(root,'ui');
const atlas=fs.readFileSync(path.join(root,'assets/hardware-controls.png'));
const href='data:image/png;base64,'+atlas.toString('base64');
const width=atlas.readUInt32BE(16),height=atlas.readUInt32BE(20);
const crops={
  'knob-cream':[72,103,296,302], 'knob-orange':[478,103,299,302], 'knob-dark':[892,103,294,302],
  'fader-grey':[52,537,333,175], 'fader-orange':[462,537,333,175], 'fader-dark':[871,537,333,175],
  'button-light':[98,832,240,321], 'button-dark':[918,832,251,321]
};
for(const [name,box] of Object.entries(crops)) {
  for(const down of name.startsWith('button')?[false,true]:[false]) {
    const [x,y,w,h]=box;
    fs.writeFileSync(path.join(ui,'refresh-'+name+(down?'-down':'')+'.svg'),`<svg xmlns="http://www.w3.org/2000/svg" viewBox="${x} ${y} ${w} ${h}" width="${w}" height="${h}" preserveAspectRatio="none"><image width="${width}" height="${height}" href="${href}" style="${down?'filter:brightness(.82)':''}"/></svg>`);
  }
}
let lib=fs.readFileSync(path.resolve(root,'../../ui/lib.mjs'),'utf8');
const detail=fs.readFileSync(path.join(root,'assets/hardware-detail.png'));
const dw=detail.readUInt32BE(16),dh=detail.readUInt32BE(20);
const detailHref='data:image/png;base64,'+detail.toString('base64');
const parts={
  whitekey:[105,28,82,318], blackkey:[379,29,65,312],
  ribbon:[516,164,468,88], 'ribbon-end':[1047,714,112,233],
  'bender-center':[999,134,225,133], 'switch-bot':[115,407,85,269],
  'switch-mid':[427,406,87,272], 'switch-top':[740,406,85,272], rail:[1060,400,77,287],
  'octave-left':[50,770,253,145], 'octave-center':[355,770,253,145], 'octave-right':[665,770,253,145],
  'led-off':[98,1063,98,100], 'led-on':[336,1063,100,100], display:[529,1045,480,130], cheek:[1065,963,106,267]
};
function detailSvg(name,filter='') {
  const [x,y,w,h]=parts[name];
  return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="${x} ${y} ${w} ${h}" width="${w}" height="${h}" preserveAspectRatio="none"><image width="${dw}" height="${dh}" href="${detailHref}" style="${filter}"/></svg>`;
}
for(const name of Object.keys(parts)) fs.writeFileSync(path.join(ui,'refresh-'+name+'.svg'),detailSvg(name));
const replacements={
 'panel.png':'refresh-panel.png','panel-dark.png':'refresh-panel-dark.svg', 'shading.jpg':'refresh-shading.svg',
 'glass.png':'refresh-display.svg', 'disp-wide.png':'refresh-display.svg', 'cheek.png':'refresh-cheek.svg',
 'octave.png':'refresh-keyboard.svg','new-rail.png':'refresh-rail.svg',
 'new-ribbon-strip.png':'refresh-ribbon.svg','new-ribbon-left.png':'refresh-ribbon-end.svg','new-ribbon-right.png':'refresh-ribbon-end.svg'
};
for(const name of ['switch-bot','switch-mid','switch-top','octave-left','octave-center','octave-right','led-on','led-off','bender-center']) replacements['new-'+name+'.png']='refresh-'+name+'.svg';
for(const [a,b] of Object.entries(replacements)) lib=lib.replaceAll("'"+a+"'","'"+b+"'");
const panel=fs.readFileSync(path.join(ui,'refresh-panel.png')).toString('base64');
fs.writeFileSync(path.join(ui,'refresh-panel-dark.svg'),`<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512"><filter id="d"><feComponentTransfer><feFuncR type="linear" slope=".25"/><feFuncG type="linear" slope=".25"/><feFuncB type="linear" slope=".25"/></feComponentTransfer></filter><image href="data:image/png;base64,${panel}" width="512" height="512" filter="url(#d)"/></svg>`);
fs.writeFileSync(path.join(ui,'refresh-shading.svg'),'<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100"><defs><linearGradient id="s" x2="0" y2="1"><stop stop-color="#fff"/><stop offset="1" stop-color="#ddd"/></linearGradient></defs><path fill="url(#s)" d="M0 0H100V100H0Z"/></svg>');
// Individual generated keys match the existing note hit regions exactly.
const seams=[0,.962,1.940,2.946,3.938,4.944,5.965,6.985];
const blacks=[.955,2.078,3.916,5.031,6.154];
let keybed=`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 700 400" preserveAspectRatio="none"><defs><image id="atlas" width="${dw}" height="${dh}" href="${detailHref}"/></defs><path fill="#242525" d="M0 0H700V400H0Z"/>`;
const keyImage=(name,x,w,h)=>`<svg x="${x}" y="0" width="${w}" height="${h}" viewBox="${parts[name].join(' ')}" preserveAspectRatio="none"><use href="#atlas"/></svg>`;
for(let i=0;i<7;i++) keybed+=keyImage('whitekey',seams[i]*100,(seams[i+1]-seams[i])*100-1,400);
for(const at of blacks) keybed+=keyImage('blackkey',at*100-26,52,227.6);
fs.writeFileSync(path.join(ui,'refresh-keyboard.svg'),keybed+'</svg>');
for(const side of ['left','center','right']) for(const push of [false,true]) fs.writeFileSync(path.join(ui,`refresh-bender-${side}${push?'-push':''}.svg`),detailSvg('octave-'+side,push?'filter:brightness(.8)':''));
let runtime=fs.readFileSync(path.resolve(root,'../../ui/geminus.js'),'utf8');
runtime=runtime.replaceAll('new-led-on.png','refresh-led-on.svg').replaceAll('new-led-off.png','refresh-led-off.svg').replaceAll('new-bender-center.png','refresh-bender-center.svg').replace('`new-bender-${side}${y>.35?\'-push\':\'\'}.png`','`refresh-bender-${side}${y>.35?\'-push\':\'\'}.svg`');
fs.writeFileSync(path.join(ui,'geminus.js'),runtime);
for(const name of ['knob-cream','knob-orange','knob-dark','fader-grey','fader-orange','fader-dark','button-light','button-light-down','button-dark','button-dark-down'])
  lib=lib.replaceAll('new-'+name+'.png','refresh-'+name+'.svg');
const start=lib.indexOf('  const small = d <= 44');
const end=lib.indexOf('  out += hit(',start);
if(start<0||end<0) throw Error('Knob artwork section not found');
lib=lib.slice(0,start)+`  const sprite = A.knob[v];
  out += '<div' + visId(o) + ' style="position:absolute;left:' + r1(x) + 'px;top:' + r1(y) + 'px;width:' + r1(d) + 'px;height:' + r1(d) + 'px;pointer-events:none">'
    + '<img src="' + sprite + '" alt=""' + (o.id ? ' id="' + esc(o.domId || o.id) + '__i"' : '')
    + ' style="display:block;width:100%;height:100%;transform:rotate(' + r1(deg) + 'deg)"></div>';
`+lib.slice(end);
// Replace the thin guide artwork with a slim recessed track, retaining travel and hits.
const guideStart=lib.indexOf('  // a skinny guide');
const guideEnd=lib.indexOf("  out += '<img src=",guideStart);
if(guideStart<0||guideEnd<0) throw Error('Fader guide not found');
lib=lib.slice(0,guideStart)+`  out += '<img src="refresh-rail.svg" alt="" style="position:absolute;left:' + r1(x + sw / 2 - 3) + 'px;top:' + r1(y) + 'px;width:6px;height:' + r1(h) + 'px;pointer-events:none">';
`+lib.slice(guideEnd);
fs.writeFileSync(path.join(ui,'lib.mjs'),lib);
await import('./ui/gen.mjs');
const index=path.join(ui,'index.html');
let html=fs.readFileSync(index,'utf8');
html=html.replace('</head>',`<style>
/* Only the main face: internal page typography and controls are untouched. */
#panel > .tex {opacity:.38;background-size:480px!important}
#panel img {image-rendering:auto}
#panel [data-ctl="knob"]:hover,#panel [data-ctl="fader"]:hover {outline:1px solid #d7522870;outline-offset:3px;border-radius:5px}
#panel [data-action="patchPrev"],#panel [data-action="patchNext"] {font-size:0!important}
#panel [data-action="patchPrev"]::after,#panel [data-action="patchNext"]::after {content:'';position:absolute;left:45%;top:32%;width:9px;height:9px;border:2px solid currentColor;border-width:0 0 2px 2px;transform:rotate(45deg);border-radius:1px}
#panel [data-action="patchNext"]::after {transform:rotate(225deg)}
</style></head>`);
// Smooth bender direction glyphs while retaining their original label boxes.
html=html.replaceAll('>◁</div>','><svg width="18" height="18" viewBox="0 0 18 18"><path d="M12 3L6 9L12 15" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/></svg></div>').replaceAll('>▷</div>','><svg width="18" height="18" viewBox="0 0 18 18"><path d="M6 3L12 9L6 15" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/></svg></div>');
fs.writeFileSync(index,html);
