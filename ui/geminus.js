import { ParameterStore } from './parameter-store.js';
import { initFx } from './fx.js';
// value arc for the pop-overs' flat knobs (same geometry as flatKnob in ui/lib.mjs)
const arcD = (a0, a1) => { const p = a => [50+Math.sin(a*Math.PI/180)*46, 50-Math.cos(a*Math.PI/180)*46], [x0,y0] = p(a0), [x1,y1] = p(a1); return 'M'+x0.toFixed(1)+' '+y0.toFixed(1)+'A46 46 0 '+(Math.abs(a1-a0)>180?1:0)+' 1 '+x1.toFixed(1)+' '+y1.toFixed(1); };
const $ = id => document.getElementById(id), all = s => [...document.querySelectorAll(s)];
const clamp = (v, lo = 0, hi = 1) => Math.max(lo, Math.min(hi, v));
const text = (id, value) => { if ($(id)) $(id).textContent = value; };
const isText = el => !!el?.closest?.('input,textarea,[contenteditable=true]');
const metadata = JSON.parse($('parameter-spec').textContent);
const hosted = !!window.__JUCE__?.initialisationData?.__juce__functions?.length;
const Juce = hosted ? await import('./juce/index.js') : null;
const errors = [];
function report(e) { errors.push(String(e)); console.error(e); text('patch-status',String(e)); }
async function native(name, ...args) {
  if (!hosted) return null;
  if (!window.__JUCE__.initialisationData.__juce__functions.includes(name)) throw new Error('Missing native function: ' + name);
  return Juce.getNativeFunction(name)(...args);
}
const call = (name,...args) => native(name,...args).catch(report);
const store = new ParameterStore(metadata,Juce), controls = all('[data-param][data-juce-type]');
const srcs = ['dds2','lfo2','env1','vel','at','expr','ribbon','note'];
const fixed = ['lfo1Rate','xmod','wave','mix','hpf','res','env1Decay','dlyTime'];
// Keep in step with sgui::matrixDestSuffix in src/ui/UiHelpers.h.
const destinations = ['lfo1.rate','ddsMod.crossMod','ddsMod.pwmWave','mixer.mix','vcf.hpf','vcf.res','env1.decay','fx.delayTime',
  'dds2.tune','vcf.lpf','vcf.envAmt','vcf.lfo1Amt','vcf.dds2Amt','vca.envLevel','vca.lfo1Amt','vca.dds2Amt',
  'env1.attack','env1.sustain','env1.release','env2.attack','env2.decay','env2.sustain','env2.release',
  'lfo1.delay','lfo1.lrPhase','lfo2.rate','lfo2.delay','ddsMod.lfo1Amt','ddsMod.pwDetune','porta.time','fx.delaySend','fx.delayFeedback'];
// Four routes already have dedicated depth controls; there are deliberately no
// duplicate matrix parameters for these pairs (Matrix.h::matrixExcluded).
const route = (l,s,d) => d === 9 && s < 3 ? l + '.' + ['vcf.dds2Amt','lfo2.vcfAmt','vcf.envAmt'][s]
  : d === 25 && s === 4 ? l + '.lfo2.rateMod'
  : d < 8 ? `${l}.mtx.${srcs[s]}.${fixed[d]}` : `${l}.mtxd.${srcs[s]}.${d-7}`;
let layer = 'upper', source = 0, destination = 0, shiftHeld = false, shiftLatch = false, splitLearn = false, desktopLayout = false;
const shift = () => shiftHeld || shiftLatch, li = () => +(layer === 'lower');
const gestures = new Set();
const custom=[null,null];let customLearn=false,customDrag=null;   // DDS 1 CUSTOM page state
const noteName = n => ['C','C♯','D','D♯','E','F','F♯','G','G♯','A','A♯','B'][n%12] + (Math.floor(n/12)-1);
function led(id,on) { if ($(id)) $(id).src = on ? 'new-led-on.png' : 'new-led-off.png'; }
function actionState(el,on) {
  if (typeof el === 'string') el = $(el);
  if (!el) return;
  if ($(el.dataset.vis)) $(el.dataset.vis).src = on ? el.dataset.on : el.dataset.off;
  led(el.dataset.led,on); el.setAttribute('aria-pressed',String(on));
}
for (const el of controls) { el._base = el.dataset.param; el._override = null; }
function resolve(el) {
  const secondary = el.dataset.shift || el.dataset.shiftparam;
  // SHIFT wins over everything, including the MODULATE page's routing override on MOD AMOUNT
  if (shift() && secondary) return metadata[secondary] ? secondary : layer + '.' + secondary;
  if (el._override) return el._override;
  return el.dataset.layered ? layer + '.' + el.dataset.layered : el._base;
}
function paint(el) {
  const id = el.dataset.param, spec = metadata[id]; if (!spec) return;
  const v = store.read(id), index = Math.round(v*(spec.steps-1)), vis = $(el.dataset.vis);
  if (vis) switch (el.dataset.ctl) {
    case 'knob': {
      if ($(el.dataset.ind)) $(el.dataset.ind).style.transform = `rotate(${-150+v*300}deg)`;
      // flat knobs (pop-over pages): the value arc; bipolar ranges grow from 12 o'clock
      const arc = el.dataset.arc && $(el.dataset.arc);
      if (arc) { const a = -150+v*300, bi = spec.range.trim().startsWith('-'); arc.setAttribute('d', bi ? arcD(Math.min(0,a), Math.max(0.5,a)) : arcD(-150, Math.max(-149.5,a))); }
      break;
    }
    case 'fader': vis.style.top = (+el.dataset.top+(1-v)*+el.dataset.travel)+'px'; break;
    case 'hfader': vis.style.left = (+el.dataset.left+v*+el.dataset.travel)+'px'; break;
    case 'rocker': vis.src = el.dataset.src1; break;
    case 'sw3': {
      // detent = the sprite/label slot for this value (labels are laid out in the same order as the sprites)
      const detent = el.dataset.map ? +el.dataset.map.split(',')[index] : index;
      vis.src = el.dataset['src'+detent];
      for (let i=0; i<3; i++) {
        const option = $(el.dataset.optbase + i);
        if (option) option.style.color = i === detent ? (el.dataset.optsel || 'var(--accent)') : el.dataset.optink;
      }
      break;
    }
    case 'button': case 'led': {
      const on = el.dataset.bit != null ? !!(index & +el.dataset.bit) : el.dataset.value != null ? index === +el.dataset.value : v > 0;
      if (el.dataset.wavecard) {
        el.style.boxShadow = on ? 'inset 0 0 0 2px var(--accent)' : 'none';
        el.style.background = on ? 'rgba(128,128,128,.08)' : 'transparent';
      } else vis.src = on ? el.dataset.on : el.dataset.off;
      led(el.dataset.led,on); el.setAttribute('aria-pressed',String(on)); break;
    }
  }
  const bipolar = spec.range.trim().startsWith('-1.');
  el.dataset.normalized = String(v); text(el.id+'__val',Math.round((bipolar ? v*2-1 : v)*100)); el.title = id + ': ' + store.display(id);
  const cell = $(el.id+'__cell');   // matrix: the cell lights, and its bar grows from the centre (left = negative)
  if (cell) { const a = bipolar ? v*2-1 : v, on = Math.abs(a) > .005; cell.classList.toggle('on',on); cell.classList.toggle('neg',a<0); cell.style.setProperty('--mag',Math.abs(a).toFixed(3)); $(el.id+'__val')?.classList.toggle('on',on); }
}
function refreshAltDisplays() {
  for (const l of ['upper','lower']) for (const ch of ['A','B']) {
    const id = `${l}.dds1.alt${ch}`;
    const spec = metadata[id];
    if (!spec) continue;
    const count = Math.max(1, (spec.steps || 2) - 1);
    const index = Math.round(store.read(id) * count);
    const names = (spec.range || '').split('|');
    text(`${id}-display`, names[index] || `W${index + 1}`);
  }
}
function status() {
  for (const l of ['upper','lower']) led(l+'.loopLed',store.read(l+'.env1.mode')>.75);
  led('transposeLed__v',Math.abs(store.read('global.transpose')-.5)>.01);
  text('split-readout',splitLearn ? 'PLAY A KEY' : noteName(Math.round(store.actual('perf.splitPoint'))));
  text('midich-readout',Math.round(store.actual('global.midiChannel')));
  const signed=(n)=>(n>0?'+':'')+n;
  text('finetune-readout',signed(Math.round(store.actual('global.fineTune')))+' CENTS');
  text('transpose-readout',signed(Math.round(store.actual('global.transpose')))+' ST');
  { let n=0; for (let a=0;a<8;a++) for (let b=0;b<8;b++) { const id=route(layer,a,b); if (metadata[id] && Math.abs(store.read(id)-metadata[id].def)>.003) n++; }
    text('mtx-count', layer.toUpperCase()+'   ·   '+(n ? n+(n===1?' ROUTE':' ROUTES')+' IN USE' : 'NO ROUTES YET - DRAG A KNOB UP OR DOWN')); }
  for (let i=0;i<8;i++) { actionState('mtxsrc'+i,i===source); actionState('mtxdst'+i,Math.abs(store.read(route(layer,source,i))-.5)>.001); $('mtxdst'+i)?.classList.toggle('seq-sel',i===destination); }
  // pop-over layer links name the layer you would switch to
  for (const id of ['mtx-layer-link','seq-layer-link','custom-layer-link']) text(id, layer==='upper' ? 'LOWER' : 'UPPER');
  refreshAltDisplays();
  refreshCustom();
  // VCF STYLE: show each layer's filter layout
  for (const l of ['upper','lower']) document.body.classList.toggle('vcf3w-'+l, store.read(l+'.vcf.style') > .5);
  // SYNC off: the arp knob shows the free RATE (ms) instead of the CLK DIV detents
  document.body.classList.toggle('arp-free', store.read(layer+'.arp.sync') < .5);
  fxUi?.refresh();
}
function retarget() {
  for (const el of controls) {
    const id = resolve(el);
    if (!metadata[id]) { if (!/^modslot|^modamt/.test(id)) report('Unknown parameter '+id); continue; }
    el.dataset.param=id; store.ensure(id); paint(el);
  }
  status();
}
export function setStripLayer(which) {
  layer=which; document.body.classList.toggle('layer-upper',layer==='upper'); document.body.classList.toggle('layer-lower',layer==='lower');
  if ($('modamt')) $('modamt')._override=route(layer,source,destination);
  retarget(); renderSequence();
}
function setShift() { document.body.classList.toggle('shift',shift()); actionState('act-shift',shift()); retarget(); }
function applyDesktopLayout(enabled) {
  desktopLayout=!!enabled;
  document.body.classList.toggle('desktop-layout',desktopLayout);
  actionState('act-desktop-layout',desktopLayout); actionState('act-desktop-top',desktopLayout);
  text('desktop-layout-state',desktopLayout?'ON':'OFF');
  text('desktop-layout-detail',desktopLayout?'LARGE CONTROLS · ONE LAYER AT A TIME':'FULL PANEL WITH KEYBOARD');
}
// GEMINI is the default theme; the choice is stored with the other global UI settings.
const THEMES = ['gemini','super6','dark'];
function applyTheme(name) {
  if (!THEMES.includes(name)) name = 'gemini';
  for (const t of THEMES) { document.body.classList.toggle('theme-'+t, t===name && t!=='gemini'); actionState('act-theme-'+t, t===name); }
  fxUi?.retheme();   // the FX faces that follow the theme pick up its colours
}
async function setTheme(name) { applyTheme((await native('uiTheme',name)) || name); }
// FX rack page: ui/fx.js (created once the helpers it needs exist)
let fxUi = null;
async function toggleDesktopLayout() {
  const saved=await native('desktopLayout',!desktopLayout);
  applyDesktopLayout(saved == null ? !desktopLayout : saved);
  fit(); call('pageSize',$('panel').offsetWidth,$('panel').offsetHeight);
}
store.onChange = id => { if (id==='perf.singleLayer') setStripLayer(store.read(id)>.5?'lower':'upper'); for (const el of controls) if (el.dataset.param===id) paint(el); status(); };
function oneWrite(id,v) { store.begin(id); store.write(id,v); store.end(id); }
function focus(target) { if (!isText(target) && !gestures.size) { if (isText(document.activeElement)) document.activeElement.blur(); call('hostFocus'); } }
function step(el,dir,e) {
  const id=el.dataset.param, spec=metadata[id]; if (!spec) return;
  if (shift() && /\.manual$/.test(el._base)) { init(+el._base.startsWith('lower')); return; }
  if (shift() && el.id==='perf.keyboardMode__2') { armSplit(); return; }
  if (shift() && el.dataset.layered==='arp.mode') { sequenceMemory('seqLoad'); return; }
  let n=store.read(id), count=spec.steps||25;
  if (el.dataset.ctl==='rocker') {
    const r=el.getBoundingClientRect(), direction=e?(e.clientX<r.left+r.width/2?-1:1):dir;
    oneWrite(id,n+direction/(count-1));
    const vis=$(el.dataset.vis); if(vis) vis.src=el.dataset[direction>0?'src2':'src0'];
    clearTimeout(el._returnTimer); el._returnTimer=setTimeout(()=>paint(el),140);
    return;
  }
  if (el.dataset.bit!=null) n=(Math.round(n*(count-1))^+el.dataset.bit)/(count-1);
  else if (el.dataset.value!=null) n=+el.dataset.value/(count-1);
  else if (spec.type==='toggle') n=1-n;
  else n=((Math.round(n*(count-1))+dir+count)%count)/(count-1);
  oneWrite(id,n);
}
for (const el of controls) {
  let drag=null;
  if (el.dataset.wavecard) el.addEventListener('keydown', e => {
    if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); step(el, 1, e); }
  });
  el.addEventListener('pointerdown',e=>{
    if(e.button!==0)return; e.preventDefault(); const id=el.dataset.param; if(!metadata[id])return;
    // Stepped rotary controls (for example ARP CLK DIV) are still draggable.
    // A click with no movement keeps the original one-step behaviour.
    const draggable = metadata[id].type==='slider' || (metadata[id].type==='combo' && el.dataset.ctl==='knob');
    if(!draggable||['button','led','rocker'].includes(el.dataset.ctl)){step(el,1,e);return;}
    try{el.setPointerCapture(e.pointerId);}catch{}
    // faders follow the pointer 1:1: their cap travel in screen pixels (layout travel x page zoom)
    const r=el.getBoundingClientRect(), zoom=r.height/el.offsetHeight||1, px=['fader','hfader'].includes(el.dataset.ctl)?+el.dataset.travel*zoom:220*zoom;
    drag={id,value:store.read(id),x:e.clientX,y:e.clientY,px}; gestures.add(el); store.begin(id);
  });
  el.addEventListener('pointermove',e=>{
    if (!drag) return;
    // Knobs and vertical faders are continuous, pixel-driven controls. Do not
    // quantise them to the printed tick marks; ticks are visual guidance only.
    if (Math.abs(e.clientX-drag.x)+Math.abs(e.clientY-drag.y)>3) drag.moved=true;
    const axis = el.dataset.ctl === 'hfader' ? e.clientX - drag.x : drag.y - e.clientY;
    const fine = e.ctrlKey || e.metaKey ? .25 : 1;
    const next = drag.value + axis / drag.px * fine;
    if (metadata[drag.id].type==='combo') {
      const steps = metadata[drag.id].steps || 2;
      store.write(drag.id, Math.round(clamp(next,0,1)*(steps-1))/(steps-1));
    } else store.write(drag.id, next);
  });
  const end=e=>{if(!drag)return;const finished=drag;store.end(finished.id);drag=null;gestures.delete(el);if(e&&el.hasPointerCapture(e.pointerId))el.releasePointerCapture(e.pointerId);if(!finished.moved&&metadata[finished.id].type==='combo'&&el.dataset.ctl==='knob')step(el,1,e);};
  for(const ev of ['pointerup','pointercancel','lostpointercapture'])el.addEventListener(ev,end);
  window.addEventListener('blur',()=>end());
  el.addEventListener('dblclick',e=>{e.preventDefault();const id=el.dataset.param;if(metadata[id])oneWrite(id,metadata[id].def);focus(el);});
  el.addEventListener('wheel',e=>{e.preventDefault();const id=el.dataset.param;if(!metadata[id])return;if(metadata[id].type==='slider')oneWrite(id,store.read(id)-Math.sign(e.deltaY)*(e.ctrlKey?.005:.02));else step(el,-Math.sign(e.deltaY));focus(el);},{passive:false});
  el.addEventListener('contextmenu',e=>{e.preventDefault();openModulate(el.dataset.param);});
}
function closePages(){all('.pop').forEach(el=>el.hidden=true);}
function openPage(id){closePages();if($(id))$(id).hidden=false;}
function openModulate(id){const l=id.split('.')[0],d=destinations.indexOf(id.slice(l.length+1));if(!['upper','lower'].includes(l)||d<0)return;for(let s=0;s<8;s++)$('modslot'+s)._override=route(l,s,d);text('mod-target',id.toUpperCase());retarget();openPage('pop-modulate');}
all('[data-open]').forEach(el=>el.addEventListener('click',()=>{openPage(el.dataset.open);if(el.dataset.open==='pop-patches'){refreshPatches().catch(report);setTimeout(()=>$('patch-list')?.focus({preventScroll:true}),0);}}));
all('[data-close]').forEach(el=>el.addEventListener('click',()=>{$(el.dataset.close).hidden=true;focus(el);}));
all('.pop').forEach(el=>el.addEventListener('click',e=>{if(e.target===el){el.hidden=true;focus(el);}}));
document.addEventListener('contextmenu',e=>e.preventDefault());
document.addEventListener('keydown',e=>{if(e.key==='Escape'){closePages();splitLearn=false;focus(e.target);}if(isText(e.target))return;if(e.key==='Shift'&&!shiftHeld){shiftHeld=true;setShift();}if(e.key==='F5'||e.key==='F12'||((e.ctrlKey||e.metaKey)&&['r','p','s','f','+','-','0'].includes(e.key.toLowerCase())))e.preventDefault();});
document.addEventListener('keyup',e=>{if(e.key==='Shift'){shiftHeld=false;setShift();}});
window.addEventListener('blur',()=>{shiftHeld=false;setShift();});
// Returning focus before click can swallow the action, so hand back after it.
document.addEventListener('click',e=>focus(e.target));
document.addEventListener('pointerup',e=>{if(!isText(e.target))setTimeout(()=>focus(e.target),0);});
document.addEventListener('pointercancel',e=>focus(e.target));
async function init(l){if(hosted)await native('initPatch',l);else for(const[id,s]of Object.entries(metadata))if(id.startsWith(l?'lower.':'upper.'))oneWrite(id,s.def);setPatchName('INIT');}
function armSplit(){splitLearn=true;status();}
function learnNote(n){if(splitLearn){splitLearn=false;oneWrite('perf.splitPoint',n/127);}if(customLearn){customLearn=false;oneWrite(layer+'.dds1.smpRoot',n/127);status();}}

// The audio thread owns real sequences. Preview uses a separate in-memory model.
const emptyStep=()=>({n:[],v:.8,t:false,a:false,r:false});
const emptySequence=()=>({length:16,steps:Array.from({length:64},emptyStep)});
const sequences=[emptySequence(),emptySequence()],memories=[[],[]];
let page=0,selectedStep=0,feed={recording:[false,false],step:[-1,-1],running:[false,false],recStep:[0,0]};
function renderSequence(){
  const l=li(),seq=sequences[l];
  for(let p=0;p<4;p++)actionState('seq-page'+p,p===page);
  for(let i=0;i<16;i++){
    const index=page*16+i,st=seq.steps[index]||emptyStep();text('seq-num'+i,index+1);text('seq-note'+i,st.n.length?noteName(st.n[0]):'—');text('seq-more'+i,st.n.length>1?'+'+(st.n.length-1)+' NOTES':st.n.length?'VEL '+Math.round(st.v*127):'');$('seq-note'+i)?.classList.toggle('empty',!st.n.length);
    $('seq-cell'+i)?.classList.toggle('seq-sel',index===selectedStep);$('seq-cell'+i)?.classList.toggle('seq-off',index>=seq.length);
    const now=(feed.running[l]&&feed.step[l]===index)||(feed.recording[l]&&feed.recStep[l]===index);
    if($('seq-play'+i))$('seq-play'+i).style.opacity=now?'1':'0';$('seq-cell'+i)?.classList.toggle('seq-now',now);
    ['t','a','r'].forEach((k,j)=>$('seq-flag'+i+'_'+j)?.classList.toggle('flag-on',!!st[k]));
  }
  text('seq-info',`${layer.toUpperCase()} · LENGTH ${seq.length} · STEP ${selectedStep+1}${feed.recording[l]?' · RECORDING':''}`);
  all('[data-action="seqRec"]').forEach(el=>actionState(el,!!feed.recording[l]));
}
function writeStep(i,st){const l=li();sequences[l].steps[i]=st;call('seqSetStep',l,i,st.n,st.v,+st.t,+st.a,+st.r);renderSequence();}
function sequenceMemory(fn){const l=li(),slot=Math.round(store.read(layer+'.seq.slot')*15);if(hosted)call(fn,l,slot);else if(fn==='seqStore')memories[l][slot]=structuredClone(sequences[l]);else if(memories[l][slot])sequences[l]=structuredClone(memories[l][slot]);renderSequence();text('seq-info',`${fn==='seqStore'?'STORED':'LOADED'} SLOT ${slot+1}`);}
function record(on){const l=li();feed.recording[l]=on;feed.recStep[l]=selectedStep;call('seqRecord',l,+on,on?selectedStep:-1);renderSequence();}
function transposeStep(dir){const st=structuredClone(sequences[li()].steps[selectedStep]);st.n=st.n.length?st.n.map(n=>clamp(n+dir,0,127)):[60];writeStep(selectedStep,st);}
all('[data-seqstep]').forEach(el=>el.addEventListener('click',()=>{selectedStep=page*16+(+el.dataset.seqstep);if(el.dataset.seqflag!=null){const st=structuredClone(sequences[li()].steps[selectedStep]),k=['t','a','r'][+el.dataset.seqflag];st[k]=!st[k];writeStep(selectedStep,st);}else renderSequence();}));

// ---- patch browser: a full-screen page after Arturia's. Sidebar views (ALL SOUNDS / LIKED / PACKS) and
// your packs, the list (TYPE chips, a sortable table) or the pack grid, and on the right the sound you are on. A pack is a folder inside the patch folder: typing a new pack
// name makes one, a folder made by hand in Explorer shows up, and a cover.png / .jpg in it is its cover.
const TYPES=['BASS','LEAD','KEYS','PAD','PLUCK','BRASS','STRINGS','ARP','SEQUENCE','ATMOSPHERE','PERCUSSION','FX'];
let folder='',files=[],banks=[],covers={},currentFile='',patch='INIT',abIsB=false,previewAB,artKey;
const pb={q:'',types:new Set(),pack:null,fav:false,key:'name',dir:1,view:'list'};
const HEART=$('patch-cur-fav')?.innerHTML||'';
const escHtml=s=>String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
function setPatchName(name){patch=name||'INIT';text('patch-name',patch);if(document.activeElement!==$('patch-name-input'))$('patch-name-input').value=patch;renderCurrent();}
const pbVal=(f,k)=>k==='fav'?(f.fav?0:1):String(f[k]||'').toLowerCase();
const inPack=f=>pb.pack===null||(f.bank||'')===pb.pack;
function pbShown(){
  const q=pb.q.toLowerCase();
  return files.filter(f=>(!q||(f.name+' '+(f.type||'')+' '+(f.bank||'')).toLowerCase().includes(q))&&inPack(f)
    &&(!pb.types.size||pb.types.has(f.type||''))&&(!pb.fav||f.fav))
    .sort((a,b)=>{const x=pbVal(a,pb.key),y=pbVal(b,pb.key);return (x<y?-1:x>y?1:0)*pb.dir||(a.name<b.name?-1:1);});
}
// A pack without an image gets a cover drawn from the instrument's own parts, seeded by its name:
// fader slots, a knob, one cycle of a wave or the keybed, in 002's colours, with the name set large.
// `wide` frames the same square art for the 2:1 panel on the right.
function genCover(name,wide){
  let h=2166136261;for(const ch of String(name||'002'))h=Math.imul(h^ch.charCodeAt(0),16777619)>>>0;
  const rnd=()=>{h=Math.imul(h^(h>>>15),2246822507)>>>0;h=Math.imul(h^(h>>>13),3266489909)>>>0;h=(h^(h>>>16))>>>0;return h/4294967296;};
  const P=[['#262627','#E7E2DA','#F65A27'],['#E7E2DA','#23252A','#F65A27'],['#18323D','#FFE8D1','#68C3D4'],['#568EA3','#FFFFFF','#2B1F18'],['#3A2B23','#E7E2DA','#F2A33A'],['#F65A27','#23252A','#E7E2DA']];
  const [bg,ink,acc]=P[Math.floor(rnd()*P.length)],m=Math.floor(rnd()*4);
  let a='';
  if(m===0){for(let i=0;i<7;i++){const x=34+i*22,v=.15+rnd()*.7;a+=`<rect x="${x}" y="28" width="3" height="104" rx="1.5" fill="${ink}" opacity=".3"/><rect x="${x-7}" y="${(28+v*94).toFixed(1)}" width="17" height="10" rx="2" fill="${i===3?acc:ink}"/>`;}}
  else if(m===1){const ang=(-150+rnd()*300)*Math.PI/180;for(let i=0;i<11;i++){const t=(-150+i*30)*Math.PI/180;a+=`<line x1="${(100+Math.sin(t)*52).toFixed(1)}" y1="${(80-Math.cos(t)*52).toFixed(1)}" x2="${(100+Math.sin(t)*(i%5?58:62)).toFixed(1)}" y2="${(80-Math.cos(t)*(i%5?58:62)).toFixed(1)}" stroke="${ink}" stroke-width="2.4" stroke-linecap="round"/>`;}
    a+=`<circle cx="100" cy="80" r="44" fill="${ink}"/><line x1="100" y1="80" x2="${(100+Math.sin(ang)*36).toFixed(1)}" y2="${(80-Math.cos(ang)*36).toFixed(1)}" stroke="${acc}" stroke-width="7" stroke-linecap="round"/>`;}
  else if(m===2){const k=[1,rnd()*.7,rnd()*.5,rnd()*.4,rnd()*.3];let d='';for(let x=0;x<=160;x+=2){const t=x/160*Math.PI*2;let y=0;k.forEach((g,i)=>y+=g*Math.sin(t*(i+1))/(i+1));d+=(x?'L':'M')+(20+x)+' '+(80-y*34).toFixed(1);}
    a+=`<path d="M20 80H180" stroke="${ink}" stroke-width="1.5" opacity=".3"/><path d="${d}" fill="none" stroke="${ink}" stroke-width="5" stroke-linejoin="round" stroke-linecap="round"/><circle cx="180" cy="80" r="7" fill="${acc}"/>`;}
  else{const lit=Math.floor(rnd()*10);for(let i=0;i<10;i++)a+=`<rect x="${22+i*16}" y="40" width="14" height="86" rx="2" fill="${i===lit?acc:ink}"/>`;[0,1,3,4,5,7,8].forEach(i=>{a+=`<rect x="${33+i*16}" y="40" width="10" height="52" rx="1.5" fill="${bg}"/>`;});}
  const label=String(name||'002').toUpperCase(),fs=Math.min(34,Math.max(16,300/Math.max(6,label.length)));
  return `<svg viewBox="${wide?'-100 0 400 200':'0 0 200 200'}" preserveAspectRatio="xMidYMid slice" aria-hidden="true"><rect x="-100" y="0" width="400" height="200" fill="${bg}"/>${a}`
    +`<text x="18" y="28" font-family="Bahnschrift,'SPKR Condensed',sans-serif" font-weight="700" font-size="12" letter-spacing="2" fill="${ink}" opacity=".6">002</text>`
    +`<text x="18" y="182" font-family="Bahnschrift,'SPKR Condensed',sans-serif" font-stretch="condensed" font-weight="700" font-size="${fs.toFixed(1)}" fill="${ink}">${escHtml(label)}</text></svg>`;
}
const packCover=(bank,wide)=>covers[bank]?`<img alt="" src="${covers[bank]}">`:genCover(bank||'LOOSE SOUNDS',wide);
const packNames=()=>[...new Set([...banks,...files.map(f=>f.bank||'')])].filter(b=>b||files.some(f=>!f.bank)).sort((a,b)=>a===''?1:b===''?-1:a.localeCompare(b));
// ALL PACKS: a mosaic of the first four covers
function allCover(wide){const n=packNames();if(n.length<4)return genCover('ALL PACKS',wide);
  return `<div style="display:grid;grid-template-columns:1fr 1fr;width:100%;height:100%">${n.slice(0,4).map(b=>`<div style="overflow:hidden">${packCover(b)}</div>`).join('')}</div>`;}
// a cover with its CHANGE IMAGE button (hover); the button sets that pack's cover.png
const coverBox=(bank,cls)=>{const d=document.createElement('div');d.className=cls;d.innerHTML=packCover(bank);
  if(bank!==null){const c=document.createElement('div');c.className='pb-change';c.textContent='CHANGE IMAGE';c.title='Choose an image for this pack';
    c.addEventListener('click',e=>{e.stopPropagation();setPackCover(bank).catch(report);});d.append(c);}return d;};
const countOf=b=>files.filter(f=>(f.bank||'')===b).length,plural=n=>n+(n===1?' SOUND':' SOUNDS');
const showPack=(b)=>{pb.pack=b;pb.view='list';pb.fav=false;pb.types.clear();renderPatches();};
function renderPacks(){
  const names=packNames();
  // sidebar list: a small cover, the name, the count
  const list=$('patch-packlist');
  if(list)list.replaceChildren(...names.map(b=>{const d=document.createElement('div');d.className='pb-pl'+(pb.pack===b&&pb.view==='list'?' on':'');d.title='Show '+(b||'the sounds in no pack');
    d.innerHTML='<div class="pb-pl-cover">'+packCover(b)+'</div><div class="pb-pl-name"></div><div class="pb-pl-n">'+countOf(b)+'</div>';d.querySelector('.pb-pl-name').textContent=b||'NO PACK';
    d.addEventListener('click',()=>showPack(b));return d;}));
  // the grid of covers (PACKS view)
  const grid=$('patch-packs');
  if(grid)grid.replaceChildren(...names.map(b=>{const d=document.createElement('div');d.className='pb-pack';d.title='Open '+(b||'the sounds in no pack');
    const name=document.createElement('div');name.className='pb-pack-name';name.textContent=b||'NO PACK';
    const n=document.createElement('div');n.className='pb-pack-count';n.textContent=plural(countOf(b));
    d.append(coverBox(b,'pb-cover'),name,n);d.addEventListener('click',()=>showPack(b));return d;}));
  // the open pack's banner over the list
  const head=$('patch-packhead');
  if(head){head.hidden=pb.pack===null||pb.view!=='list';
    if(!head.hidden){head.replaceChildren(coverBox(pb.pack,'pb-ph-cover'));const t=document.createElement('div');
      t.innerHTML='<h3></h3><p>'+plural(countOf(pb.pack))+'</p>';t.querySelector('h3').textContent=pb.pack||'NO PACK';
      const acts=document.createElement('div');acts.className='pb-ph-acts';
      const all=document.createElement('div');all.className='pb-chip';all.textContent='ALL PACKS';all.addEventListener('click',()=>{pb.view='packs';renderPatches();});
      acts.append(all);head.append(t,acts);}}
  text('pb-n-all',files.length);text('pb-n-liked',files.filter(f=>f.fav).length);text('pb-n-packs',names.length);
  for(const el of all('.pb-nav')){const v=el.dataset.view;el.classList.toggle('on',v==='packs'?pb.view==='packs':pb.view==='list'&&pb.pack===null&&(v==='liked')===pb.fav);}
  $('pb-view-list').hidden=pb.view!=='list';$('pb-view-packs').hidden=pb.view!=='packs';
}
function renderTypes(){
  const box=$('patch-types');if(!box)return;
  const types=[...new Set(files.filter(inPack).map(f=>f.type||'').filter(Boolean))].sort();
  const chip=(t,label)=>{const d=document.createElement('div');d.className='pb-type'+((t===null?!pb.types.size:pb.types.has(t))?' on':'');d.textContent=label;
    d.addEventListener('click',()=>{if(t===null)pb.types.clear();else pb.types.has(t)?pb.types.delete(t):pb.types.add(t);renderPatches();});return d;};
  box.replaceChildren(chip(null,'ALL TYPES'),...types.map(t=>chip(t,t)));
}
// the sound you are on: its pack's cover, its name, type and pack, and its heart
function renderCurrent(){
  const nm=$('patch-cur-name');if(!nm)return;
  const f=files.find(x=>x.path===currentFile);
  nm.textContent=f?f.name:patch;
  text('patch-cur-meta',f?(f.type||'NO TYPE')+'   ·   '+(f.bank||'NO PACK'):patch==='INIT'?'THE INIT SOUND':'NOT SAVED IN THIS FOLDER YET');
  const fav=$('patch-cur-fav');fav.classList.toggle('on',!!f?.fav);fav.style.visibility=f?'visible':'hidden';
  const key=f?(f.bank||''):pb.pack===null?'*':pb.pack;
  if(key!==artKey){artKey=key;const art=$('patch-art');art.replaceChildren(...coverBox(key==='*'?null:key,'x').childNodes);if(key==='*')art.innerHTML=allCover();}
}
function likeToggle(file,heart){file.fav=!file.fav;heart.classList.toggle('on',file.fav);heart.classList.remove('pop-in');void heart.offsetWidth;if(file.fav)heart.classList.add('pop-in');
  if(hosted)native('patchFavourite',file.path,file.fav).catch(report);if(pb.fav)renderPatches();else renderCurrent();}
function renderPatches(){
  const list=$('patch-list');if(!list)return;
  const rows=pbShown();
  text('patch-count',rows.length+(rows.length===files.length?'':' OF '+files.length)+(files.length===1?' SOUND':' SOUNDS'));
  for(const el of all('.pb-head')){const on=el.dataset.sort===pb.key;el.querySelector('.pb-arrow').className='pb-arrow'+(on?(pb.dir>0?' up':' down'):'');el.style.color=on?'var(--ink)':'';}
  $('patch-f-fav').classList.toggle('on',pb.fav);
  renderPacks();renderTypes();
  list.replaceChildren();
  if(!rows.length){const el=document.createElement('div');el.className='patch-empty';
    el.textContent=!files.length?'No sounds here yet. Name this sound, give it a pack, and choose SAVE.':pb.fav?'No liked sounds here. Tap the heart on a sound to keep it close.':'Nothing matches. Try another pack or type, or RESET FILTERS.';
    list.append(el);renderCurrent();return;}
  for(const file of rows){
    const el=document.createElement('div');el.className='patch-row pb-grid'+(file.path===currentFile?' sel':'');
    const heart=document.createElement('span');heart.className='pb-fav'+(file.fav?' on':'');heart.innerHTML=HEART;heart.title='Like';
    heart.addEventListener('click',e=>{e.stopPropagation();likeToggle(file,heart);});
    const cell=(cls,t)=>{const s=document.createElement('span');s.className=cls;s.textContent=t;return s;};
    el.append(heart,cell('pb-name',file.name),cell('pb-type-c',file.type||'—'),cell('pb-bank',file.bank||'—'));
    el.addEventListener('click',()=>loadPatch(file).catch(report));
    el.addEventListener('dblclick',()=>{$('patch-name-input').value=file.name;$('patch-type-input').value=file.type||'';$('patch-bank-input').value=file.bank||'';});
    list.append(el);
  }
  list.querySelector('.sel')?.scrollIntoView({block:'nearest'});
  renderCurrent();
}
// a small drop-down under a control; picking an item runs `pick`
function pbPop(anchor,items,pick,marked){
  all('.pb-pop').forEach(el=>el.remove());
  if(!items.length)items=['(none yet)'];
  const box=document.createElement('div');box.className='pb-pop';
  const panel=anchor.closest('.pop-panel'),r=anchor.getBoundingClientRect(),p=panel.getBoundingClientRect(),z=p.width/panel.offsetWidth||1;
  box.style.left=Math.min((r.left-p.left)/z,panel.offsetWidth-260)+'px';box.style.top=((r.bottom-p.top)/z+6)+'px';
  for(const it of items){const val=typeof it==='string'?it:it.value,label=typeof it==='string'?it:it.label;
    const d=document.createElement('div');d.textContent=label;if(marked&&marked(val))d.className='on';
    d.addEventListener('pointerdown',e=>{e.stopPropagation();if(label!=='(none yet)')pick(val);box.remove();});box.append(d);}
  panel.append(box);
  setTimeout(()=>document.addEventListener('pointerdown',()=>box.remove(),{once:true}),0);
}
const patchTypes=()=>[...new Set([...TYPES,...files.map(f=>f.type).filter(Boolean)])].sort();
// withCovers: re-read the pack images too (opening the browser, REFRESH, a new folder) - not on every step
async function refreshPatches(withCovers=true){
  if(!hosted){text('patch-status','Browser preview: patch files need the plugin.');renderPatches();return;}
  if(!folder)folder=await native('patchFolder');
  files=await native('patchList',folder)||[];banks=await native('patchBanks',folder)||[];
  if(withCovers){covers=await native('patchCovers',folder)||{};artKey=undefined;}
  text('patch-folder-path',folder);
  if(!$('patch-type-input').value)$('patch-type-input').value=(await native('patchType'))||'';
  renderPatches();
}
async function loadPatch(file){
  const name=await native('patchLoad',file.path);if(!name)throw Error('Could not load this 002 patch.');
  currentFile=file.path;setPatchName(name);
  $('patch-type-input').value=file.type||'';$('patch-bank-input').value=file.bank||'';
  text('patch-status','LOADED  '+name+(file.bank?'  ·  '+file.bank:''));
  renderPatches();
}
async function savePatch(saveAs=false){
  if(!hosted){openPage('pop-patches');return;}
  if(!folder)folder=await native('patchFolder');
  const name=$('patch-name-input').value.trim();
  if(!name){text('patch-status','Give the sound a name first.');openPage('pop-patches');$('patch-name-input').focus();return;}
  const type=$('patch-type-input').value.trim(),bank=$('patch-bank-input').value.trim();
  const collision=files.find(f=>f.name.toLowerCase()===name.toLowerCase()&&(f.bank||'')===bank);
  if(collision&&collision.path!==currentFile)saveAs=true;
  const saved=saveAs?await native('patchSaveAs'):await native('patchSave',folder,name,bank,type);
  if(saved){currentFile=saved;setPatchName(await native('patchName'));text('patch-status','SAVED  '+name+(bank?'  ·  '+bank:''));await refreshPatches(false);}
  else text('patch-status','Not saved (cancelled, or the folder could not be written).');
}
async function adjacentPatch(dir){
  if(hosted)await refreshPatches(false);const rows=pbShown();
  if(rows.length){const i=rows.findIndex(f=>f.path===currentFile);await loadPatch(rows[(i+dir+rows.length)%rows.length]);}
}
async function setPackCover(bank){
  if(bank==null){const f=files.find(x=>x.path===currentFile);bank=f?(f.bank||''):(pb.pack||'');}
  if(!hosted){text('patch-status','Browser preview: covers need the plugin.');return;}
  if(!folder)folder=await native('patchFolder');
  const url=await native('patchSetCover',folder,bank);
  if(url){covers[bank]=url;artKey=undefined;renderPatches();text('patch-status','NEW COVER FOR '+(bank||'LOOSE SOUNDS'));}
}
$('patch-search').addEventListener('input',e=>{pb.q=e.target.value;renderPatches();});
$('patch-clear').addEventListener('click',()=>{pb.q='';pb.types.clear();pb.pack=null;pb.fav=false;pb.view='list';$('patch-search').value='';renderPatches();});
for(const el of all('.pb-nav'))el.addEventListener('click',()=>{const v=el.dataset.view;if(v==='packs')pb.view='packs';else{pb.view='list';pb.pack=null;pb.fav=v==='liked';pb.types.clear();}renderPatches();});
$('patch-art').addEventListener('click',()=>{if(artKey!=='*')setPackCover(artKey).catch(report);});
$('patch-f-fav').addEventListener('click',()=>{pb.fav=!pb.fav;pb.view='list';renderPatches();});
$('patch-cur-fav').addEventListener('click',()=>{const f=files.find(x=>x.path===currentFile);if(f)likeToggle(f,$('patch-cur-fav'));});
for(const el of all('.pb-head'))el.addEventListener('click',()=>{const k=el.dataset.sort;pb.dir=pb.key===k?-pb.dir:1;pb.key=k;renderPatches();});
// up / down walk the list and load as they go, as in the browsers this one follows
$('patch-list').addEventListener('keydown',e=>{if(e.key==='ArrowDown'||e.key==='ArrowUp'){e.preventDefault();adjacentPatch(e.key==='ArrowDown'?1:-1).catch(report);}});
$('patch-name-menu').addEventListener('click',()=>pbPop($('patch-name-menu'),pbShown().map(f=>f.name),v=>{$('patch-name-input').value=v;}));
$('patch-type-menu').addEventListener('click',()=>pbPop($('patch-type-menu'),patchTypes(),v=>{$('patch-type-input').value=v;},v=>v===$('patch-type-input').value));
$('patch-bank-menu').addEventListener('click',()=>pbPop($('patch-bank-menu'),packNames().filter(Boolean),v=>{$('patch-bank-input').value=v;},v=>v===$('patch-bank-input').value));
// dev only (browser preview): window.__cuPlay([0.31]) draws play heads on the sample page
if(!hosted)window.__cuPlay=(a)=>{cuPlay[li()]=a;drawCustom();};
// dev only (browser preview, no plugin): feed the browser sample rows to look at the layout
if(!hosted)window.__pb={rows:(f,b)=>{files=f;banks=b||[];renderPatches();}};
// dev only: '#pop=pop-patches&demo=1' fills the browser with sample rows so the layout can be checked
if(!hosted&&location.hash.includes('demo=1')){const mk=(n,t,b,f)=>({name:n,path:'C:/002/'+b+'/'+n+'.gpatch',type:t,bank:b,fav:!!f});
  files=[mk('Demacro','LOFI KEYS','FACTORY',1),mk('Gleaming Keys','PLUCKED KEYS','FACTORY'),mk('Seq Me Hard','PLUCKED BASS','PIZZA'),mk('Grape Keys','KEYS','FACTORY'),mk('33 Electric Piano','LOFI KEYS','PIZZA',1),mk('Power Sync','POLY LEAD','FACTORY'),mk('With Grace','STRINGS','MY SOUNDS'),mk('Memory','ATMOSPHERE','MY SOUNDS'),mk('Mars Magma','EVOLVING PAD','PIZZA'),mk('Everlast','PAD','FACTORY'),mk('Super Duper Saw','BIG LEAD','MY SOUNDS'),mk('Mambo Pad','ATMOSPHERE',''),mk('Night Drive','BASS','SYNTHWAVE'),mk('Chrome Arp','ARP','SYNTHWAVE')];
  banks=['FACTORY','MY SOUNDS','PIZZA','SYNTHWAVE'];currentFile=files[4].path;$('patch-name-input').value='Velvet Keys';$('patch-type-input').value='LOFI KEYS';$('patch-bank-input').value='PIZZA';
  text('patch-folder-path','Documents / 002 / Patches');renderPatches();}
{const pm=$('pop-matrix'),geo=$('mtx-geo'),cells=all('#pop-matrix .mtx-cell');let hot='';
  const set=(r,c)=>{const k=r+','+c;if(k===hot)return;hot=k;all('#pop-matrix .cross,#pop-matrix .hot').forEach(e=>e.classList.remove('cross','hot'));if(r<0)return;
    $('mtx-row'+r)?.classList.add('hot');$('mtx-col'+c)?.classList.add('hot');for(let i=0;i<8;i++){cells[r*8+i]?.classList.add('cross');cells[i*8+c]?.classList.add('cross');}};
  if(pm&&geo){pm.addEventListener('pointermove',e=>{const p=pm.querySelector('.pop-panel'),b=p.getBoundingClientRect(),z=b.width/p.offsetWidth||1;
      const c=Math.floor(((e.clientX-b.left)/z-geo.dataset.x0)/geo.dataset.cw),r=Math.floor(((e.clientY-b.top)/z-geo.dataset.y0)/geo.dataset.ch);
      if(r>=0&&r<8&&c>=0&&c<8)set(r,c);else set(-1,-1);});
    pm.addEventListener('pointerleave',()=>set(-1,-1));}}
fxUi=initFx({$,all,store,oneWrite,native,hosted,retarget,actionState,text,openPage,report});
// ---- DDS 1 CUSTOM: the sample page. The plugin sends each layer's name, length and outline
// (customUpper / customLower in the state feed) whenever that layer's sample changes.
function refreshCustom(){
  ['upper','lower'].forEach((l,i)=>{const c=custom[i],on=store.read(l+'.dds1.smpOn')>.5;
    text(l+'.dds1.custom-display',c?(on?c.name:c.name+' · OFF'):'NO SAMPLE');actionState(l+'.dds1.custom-open',!!c&&on);});
  const c=custom[li()],mode=cuMode();
  $('pop-custom')?.classList.toggle('cu-loop',mode===1);$('pop-custom')?.classList.toggle('cu-slice',mode===2);
  text('custom-name',c?c.name:'NO SAMPLE');
  text('custom-meta',c?layer.toUpperCase()+'   ·   '+c.seconds.toFixed(2)+' S   ·   '+(c.rate/1000).toFixed(1)+' KHZ':layer.toUpperCase()+'   ·   DROP A FILE BELOW');
  if($('custom-hint'))$('custom-hint').style.display=c?'none':'flex';
  text('custom-start-val',(store.actual(layer+'.dds1.smpStart')*100).toFixed(1)+' %');
  text('custom-end-val',(store.actual(layer+'.dds1.smpEnd')*100).toFixed(1)+' %');
  text('custom-loop-val',(store.actual(layer+'.dds1.smpLoopStart')*100).toFixed(1)+' %');
  text('custom-sense-val',Math.round(store.actual(layer+'.dds1.smpSense')*100)+' %');
  {const n=c?cuSlices(c).length:0,root=Math.round(store.actual(layer+'.dds1.smpRoot'));text('custom-slice-count',c?n+' SLICES   ·   '+noteName(root)+' – '+noteName(root+n-1):'');}
  {const x=store.actual(layer+'.dds1.smpLevel'),g=x<=.8?(x/.8)**2:10**(x-.8),db=20*Math.log10(g);text('custom-level-val',g<=1e-5?'-∞ DB':(db>0?'+':'')+db.toFixed(1)+' DB');}
  text('custom-root-val',customLearn?'PLAY A KEY':noteName(store.actual(layer+'.dds1.smpRoot')));
  const fine=Math.round(store.actual(layer+'.dds1.smpFine'));text('custom-fine-val',(fine>0?'+':'')+fine+' CT');
  actionState('act-customLearn',customLearn);
  drawCustom();
}
const cuMode=()=>Math.round(store.read(layer+'.dds1.smpLoop')*2);   // 0 ONE SHOT, 1 LOOP, 2 SLICE
// SLICE: the slices of START..END as [from, to] fractions - the same rule as Dds1::setSlice in the
// engine (equal slices, or cuts at the transients at least Sample::sliceThreshold strong, 30 ms apart)
function cuSlices(c){
  const v=k=>store.actual(layer+'.dds1.'+k),lo=v('smpStart'),hi=Math.max(lo+1e-6,v('smpEnd'));
  const n=[0,4,8,16,32,64][Math.round(store.read(layer+'.dds1.smpSlices')*5)];
  if(n)return Array.from({length:n},(_,k)=>[lo+(hi-lo)*k/n,lo+(hi-lo)*(k+1)/n]);
  const t=1-v('smpSense'),thr=.02+.9*t*t,gap=.03/Math.max(.01,c.seconds),on=c.onsets||[],cuts=[lo];
  for(let j=0;j<on.length;j+=2){const p=on[j];if(p<=cuts[cuts.length-1]+gap||p>=hi-gap||on[j+1]<thr)continue;cuts.push(p);}
  cuts.push(hi);return cuts.slice(0,-1).map((a,i)=>[a,cuts[i+1]]);
}
let cuHover=-1,cuHeard=-1,cuPlay=[[],[]],cuCache=null;
// the waveform, drawn once per sample / region / mode / theme into a cache; play heads go on top
function cuWave(c,W,H,top,bot,css){
  const v=k=>store.actual(layer+'.dds1.'+k),key=[li(),c.name,c.seconds,v('smpStart'),v('smpEnd'),document.body.className,W,H].join('|');
  if(cuCache?.key===key)return cuCache.cv;
  const cv=cuCache?.cv||document.createElement('canvas');cv.width=W;cv.height=H;const g=cv.getContext('2d');g.clearRect(0,0,W,H);
  const rgb=k=>{const h=(css.getPropertyValue(k).trim()||'#F65A27').replace('#','');return [0,2,4].map(i=>parseInt(h.slice(i,i+2),16));};
  const LO=rgb('--wave-lo'),MID=rgb('--wave-mid'),HI=rgb('--wave-hi');
  const n=c.wave.length/2,mid=(top+bot)/2,amp=(bot-top)/2-6,a=v('smpStart')*W,b=v('smpEnd')*W,bands=c.bands,rms=c.rms;
  const at=(arr,stride,o,t)=>{const i=Math.floor(t),f=t-i,j=Math.min(n-1,i+1);return arr[i*stride+o]*(1-f)+arr[j*stride+o]*f;};
  for(let x=0;x<W;x+=2){
    const t=Math.min(n-1,x/W*(n-1)),lo=at(c.wave,2,0,t),hi=at(c.wave,2,1,t);
    let col=MID;
    if(bands){const l=at(bands,3,0,t),m=at(bands,3,1,t),h=at(bands,3,2,t);col=[0,1,2].map(i=>Math.round(LO[i]*l+MID[i]*m+HI[i]*h));}
    const inside=x>=a&&x<=b,fill='rgb('+col.join(',')+')';
    g.fillStyle=fill;g.globalAlpha=inside?.42:.12;g.fillRect(x,mid-hi*amp,2,Math.max(1.5,(hi-lo)*amp));
    const r=rms?at(rms,1,0,t):(hi-lo)*.35;
    g.globalAlpha=inside?1:.25;g.fillRect(x,mid-r*amp,2,Math.max(1.5,2*r*amp));
  }
  g.globalAlpha=1;cuCache={key,cv};return cv;
}
function drawCustom(){
  const cv=$('custom-canvas');if(!cv||$('pop-custom')?.hidden)return;
  const g=cv.getContext('2d'),W=cv.width,H=cv.height,c=custom[li()],css=getComputedStyle(cv);
  const accent=css.getPropertyValue('--pop-accent').trim()||'#F65A27',ink=css.getPropertyValue('--pop-ink').trim()||'#E7E2DA',ink2=css.getPropertyValue('--pop-ink2').trim()||'#A8A49C',bg=css.getPropertyValue('--pop-bg2').trim()||'#262627';
  const font=(px)=>"700 "+px+"px Bahnschrift,'SPKR Condensed',sans-serif";
  g.clearRect(0,0,W,H);
  const top=64,bot=H-56,mid=(top+bot)/2;   // a strip for the flags on top, a time ruler underneath
  g.fillStyle=ink2;g.globalAlpha=.3;g.fillRect(0,mid,W,1.5);g.globalAlpha=1;
  if(!c)return;
  const v=k=>store.actual(layer+'.dds1.'+k),a=v('smpStart')*W,b=v('smpEnd')*W,mode=cuMode();
  // time ruler
  g.fillStyle=ink2;g.font=font(22);g.textAlign='center';
  const steps=[.05,.1,.25,.5,1,2,5,10,20],step=steps[steps.findIndex(s=>c.seconds/s<=12)]||30;
  for(let s=0;s<=c.seconds+1e-9;s+=step){const x=s/c.seconds*W;g.globalAlpha=.5;g.fillRect(x,bot+4,2,12);g.globalAlpha=.8;g.fillText((step<1?s.toFixed(2):s.toFixed(0))+'s',Math.min(W-40,Math.max(40,x)),bot+44);}
  g.globalAlpha=1;
  const slices=mode===2?cuSlices(c):[],heads=(cuPlay[li()]||[]).filter(p=>p>=0);
  const playing=new Set(heads.map(p=>slices.findIndex(([s,e])=>p>=s&&p<e)));
  // slices: alternate shading; the hovered one and the ones playing are lit
  slices.forEach(([p,q],k)=>{const lit=playing.has(k)||k===cuHeard;g.fillStyle=lit||k===cuHover?accent:ink;g.globalAlpha=lit?.2:k===cuHover?.12:k%2?.05:0;g.fillRect(p*W,top,(q-p)*W,bot-top);});
  g.globalAlpha=1;
  g.drawImage(cuWave(c,W,H,top,bot,css),0,0);
  const flag=(x,t,colour,textInk,right)=>{g.fillStyle=colour;g.fillRect(x-1.5,top,3,bot-top);const w=g.measureText(t).width+28;g.beginPath();g.roundRect(right?x-w:x,4,w,top-12,[6]);g.fill();g.fillStyle=textInk;g.textAlign='left';g.fillText(t,(right?x-w:x)+14,top-22);};
  g.font=font(26);
  if(mode===2){
    const root=Math.round(v('smpRoot'));
    slices.forEach(([p],k)=>{const x=p*W;if(k){g.fillStyle=ink;g.globalAlpha=.55;g.fillRect(x-1,top,2,bot-top);g.globalAlpha=1;}
      const w=Math.max(0,(slices[k][1]-p)*W);if(w>54){g.fillStyle=playing.has(k)||k===cuHover?accent:ink2;g.font=font(w>110?24:20);g.textAlign='left';g.fillText(noteName(root+k),x+10,bot-14);}});
    g.font=font(26);
  }
  if(mode===1){const ls=Math.min(v('smpLoopStart')*W,b);g.fillStyle=ink;g.globalAlpha=.08;g.fillRect(ls,top,b-ls,bot-top);g.globalAlpha=1;flag(ls,'LOOP',ink,bg,false);}
  flag(a,'START',accent,bg,false);flag(b,'END',accent,bg,true);
  // the play heads of the sounding notes
  for(const p of heads){const x=p*W;g.fillStyle=ink;g.fillRect(x-2,top,4,bot-top);g.beginPath();g.moveTo(x-10,top);g.lineTo(x+10,top);g.lineTo(x,top+14);g.fill();}
}
async function loadCustomFile(file){
  if(!file)return;
  if(!/\.(wav|aiff?|flac|ogg|mp3)$/i.test(file.name)){text('custom-status','Drop a WAV, AIFF, FLAC, OGG or MP3 file.');return;}
  if(file.size>120e6){text('custom-status','That file is too large (120 MB max).');return;}
  text('custom-status','LOADING '+file.name.toUpperCase()+' …');
  const bytes=new Uint8Array(await file.arrayBuffer()),l=li(),name=file.name.replace(/\.[^.]+$/,'');
  if(hosted){
    let bin='';for(let i=0;i<bytes.length;i+=0x8000)bin+=String.fromCharCode.apply(null,bytes.subarray(i,i+0x8000));
    if(!await native('customLoad',l,btoa(bin),file.name)){text('custom-status','Could not read '+file.name+'.');return;}
  }else{   // browser preview: decode here so the page can be checked without the plugin (same onset rule as Sample::findOnsets)
    const buf=await new OfflineAudioContext(1,1,48000).decodeAudioData(bytes.buffer),d=buf.getChannelData(0).slice(0,Math.floor(90*buf.sampleRate)),wave=[],rms=[],bands=[];
    {const P=1024,k1=Math.exp(-2*Math.PI*200/buf.sampleRate),k2=Math.exp(-2*Math.PI*2500/buf.sampleRate);let l1=0,l2=0,top=1e-9;
      for(let p=0;p<P;p++){const a=Math.floor(p*d.length/P),e=Math.max(a+1,Math.floor((p+1)*d.length/P));let lo=0,hi=0,sq=0,el=0,em=0,eh=0;
        for(let i=a;i<e;i++){const x=d[i];lo=Math.min(lo,x);hi=Math.max(hi,x);sq+=x*x;l1=x+k1*(l1-x);l2=x+k2*(l2-x);el+=l1*l1;em+=(l2-l1)**2;eh+=(x-l2)**2;}
        const s=el+em+eh+1e-12;wave.push(lo,hi);rms.push(Math.sqrt(sq/(e-a)));bands.push(el/s,em/s,eh/s);top=Math.max(top,-lo,hi);}
      for(let i=0;i<wave.length;i++)wave[i]/=top;for(let i=0;i<rms.length;i++)rms[i]=Math.min(1,rms[i]/top);}
    const hop=Math.max(32,Math.floor(buf.sampleRate*.005)),m=Math.floor(d.length/hop),e=[],rise=[],peaks=[],gap=Math.max(1,Math.floor(buf.sampleRate*.06/hop));
    for(let k=0;k<m;k++){let s=0;for(let i=k*hop;i<(k+1)*hop;i++)s+=d[i]*d[i];e.push(10*Math.log10(s/hop+1e-9));}
    for(let k=0;k<m;k++)rise.push(k<2||e[k]<=-60?0:Math.max(0,e[k]-Math.max(e[k-1],e[k-2])));
    for(let k=2;k<m;k++){if(rise[k]<3)continue;let top2=true;for(let j=Math.max(0,k-gap);j<=Math.min(m-1,k+gap)&&top2;j++)top2=j===k||rise[j]<rise[k]||(rise[j]===rise[k]&&j>k);if(top2)peaks.push([rise[k],k]);}
    peaks.sort((x,y)=>y[0]-x[0]);peaks.length=Math.min(128,peaks.length);const best=peaks[0]?.[0]||1;peaks.sort((x,y)=>x[1]-y[1]);
    custom[l]={name,seconds:d.length/buf.sampleRate,rate:buf.sampleRate,wave,rms,bands,onsets:peaks.flatMap(([r,k])=>[Math.max(0,(k-1)*hop)/d.length,r/best])};
  }
  oneWrite((l?'lower':'upper')+'.dds1.smpOn',1);   // a new sample is meant to be heard
  text('custom-status',cuMode()===2?'LOADED. EACH KEY FROM THE ROOT KEY UP PLAYS ONE SLICE.':'LOADED. PLAY ITS ROOT KEY TO HEAR IT AT PITCH.');status();
}
{
  const drop=$('custom-drop'),input=$('custom-file');
  const fracAt=e=>{const r=drop.getBoundingClientRect();return clamp((e.clientX-r.left)/r.width);};
  const sliceAt=x=>{const c=custom[li()];return c&&cuMode()===2?cuSlices(c).findIndex(([p,q])=>x>=p&&x<q):-1;};
  drop.addEventListener('dragover',e=>{e.preventDefault();drop.style.boxShadow='inset 0 0 0 3px var(--pop-accent)';});
  drop.addEventListener('dragleave',()=>drop.style.boxShadow='');
  drop.addEventListener('drop',e=>{e.preventDefault();drop.style.boxShadow='';loadCustomFile(e.dataTransfer?.files?.[0]).catch(report);});
  input.addEventListener('change',()=>{loadCustomFile(input.files[0]).catch(report);input.value='';});
  // empty: a click browses. Loaded: drag the nearer flag; in SLICE, a click away from START / END plays that slice
  const move=e=>{
    if(!customDrag){const k=sliceAt(fracAt(e));if(k!==cuHover){cuHover=k;drawCustom();}return;}
    if(customDrag.note!=null)return;
    const x=fracAt(e),v=k=>store.actual(layer+'.dds1.'+k);
    const lim={smpStart:[0,v('smpEnd')-.002],smpLoopStart:[v('smpStart'),v('smpEnd')-.002],smpEnd:[Math.max(v('smpStart'),v('smpLoopStart'))+.002,1]}[customDrag.key];
    store.write(customDrag.id,clamp(x,lim[0],lim[1]));};
  drop.addEventListener('pointerdown',e=>{
    if(e.button!==0)return;e.preventDefault();
    if(!custom[li()]){input.click();return;}
    const x=fracAt(e),v=k=>store.actual(layer+'.dds1.'+k),cands=['smpStart','smpEnd'];
    if(cuMode()===1)cands.push('smpLoopStart');
    const r=drop.getBoundingClientRect(),inFlags=e.clientY<r.top+r.height*.16;
    const key=cands.reduce((best,k)=>Math.abs(x-v(k))<Math.abs(x-v(best))?k:best);
    const k=sliceAt(x);
    if(k>=0&&!inFlags&&Math.abs(x-v(key))>.008){   // audition the slice under the pointer
      const note=Math.round(v('smpRoot'))+k;customDrag={note};cuHeard=k;call('noteOn',note,.8);drop.setPointerCapture(e.pointerId);drawCustom();return;}
    customDrag={id:layer+'.dds1.'+key,key};store.begin(customDrag.id);drop.setPointerCapture(e.pointerId);gestures.add(drop);move(e);
  });
  drop.addEventListener('pointermove',move);
  drop.addEventListener('pointerleave',()=>{if(cuHover>=0){cuHover=-1;drawCustom();}});
  const end=()=>{if(!customDrag)return;if(customDrag.note!=null){call('noteOff',customDrag.note);cuHeard=-1;drawCustom();}else{store.end(customDrag.id);gestures.delete(drop);}customDrag=null;};
  for(const ev of ['pointerup','pointercancel','lostpointercapture'])drop.addEventListener(ev,end);
}
const actions={
  shift:()=>{shiftLatch=!shiftLatch;setShift();},openMatrix:()=>openPage('pop-matrix'),openSeq:()=>{renderSequence();openPage('pop-seq');},openSettings:()=>openPage('pop-settings'),openPatches:async()=>{openPage('pop-patches');await refreshPatches();},
  altA:el=>{setStripLayer(el.id.startsWith('lower')?'lower':'upper');openPage('pop-altA');},
  custom:el=>{setStripLayer(el.id.startsWith('lower')?'lower':'upper');text('custom-status','');openPage('pop-custom');drawCustom();},
  customBrowse:()=>$('custom-file').click(),customLearn:()=>{customLearn=!customLearn;status();},
  customRootDown:()=>oneWrite(layer+'.dds1.smpRoot',clamp(store.read(layer+'.dds1.smpRoot')-1/127)),customRootUp:()=>oneWrite(layer+'.dds1.smpRoot',clamp(store.read(layer+'.dds1.smpRoot')+1/127)),
  customClear:async()=>{if(hosted)await native('customClear',li());else custom[li()]=null;oneWrite(layer+'.dds1.smpOn',0);text('custom-status','CLEARED.');status();},
  layerToggle:()=>setStripLayer(layer==='upper'?'lower':'upper'),mtxLayer0:()=>setStripLayer('upper'),mtxLayer1:()=>setStripLayer('lower'),seqLayer0:()=>setStripLayer('upper'),seqLayer1:()=>setStripLayer('lower'),
  mtxSrc:el=>{source=+el.id.slice(-1);setStripLayer(layer);},mtxDst:el=>{destination=+el.id.slice(-1);setStripLayer(layer);},
  mtxClear:()=>{for(let d=0;d<(shift()?32:1);d++){const id=route(layer,source,shift()?d:destination);oneWrite(id,metadata[id].def);}},
  seqRec:()=>shift()?sequenceMemory('seqStore'):record(!feed.recording[li()]),seqRecStop:()=>record(false),seqLoad:()=>sequenceMemory('seqLoad'),seqStore:()=>sequenceMemory('seqStore'),
  seqClear:()=>{sequences[li()]=emptySequence();call('seqClear',li());renderSequence();},seqStepClear:()=>writeStep(selectedStep,emptyStep()),seqUp:()=>transposeStep(1),seqDown:()=>transposeStep(-1),
  seqLength:()=>{sequences[li()].length=selectedStep+1;call('seqSetLength',li(),selectedStep+1);renderSequence();},
  splitLearn:armSplit,panic:()=>{call('allNotesOff');releaseKeys();},initUpper:()=>init(0),initLower:()=>init(1),
  resetAll:async()=>{call('allNotesOff');await init(0);await init(1);for(const[id,s]of Object.entries(metadata))if(/^(global|perf)\./.test(id))oneWrite(id,s.def);},
  patchRefresh:refreshPatches,patchFolderUser:async()=>{folder=await native('patchFolder');await refreshPatches();},patchFolderChoose:async()=>{const c=await native('patchChooseFolder');if(c)folder=c;await refreshPatches();},patchSave:()=>savePatch(shift()),patchSaveAs:()=>savePatch(true),patchOpen:async()=>{const f=await native('patchOpen');if(f){currentFile=f;setPatchName(await native('patchName'));await refreshPatches();}else if(!hosted)await refreshPatches();},patchPrev:()=>adjacentPatch(-1),patchNext:()=>adjacentPatch(1),patchSetCover:setPackCover,
  ab:async()=>{if(hosted){if(shift())await native('abCopy');else abIsB=!!(await native('abToggle'));}else if(shift()||!previewAB)previewAB=store.snapshot();else{const current=store.snapshot();for(const[id,v]of Object.entries(previewAB))oneWrite(id,v);previewAB=current;abIsB=!abIsB;}actionState('act-ab',abIsB);},
  toggleDesktopLayout,themeSuper6:()=>setTheme('super6'),themeDark:()=>setTheme('dark'),openFx:()=>fxUi.open(),fxSerial:()=>fxUi.serial(),fxParallel:()=>fxUi.parallel(),themeGemini:()=>setTheme('gemini')
};
for(let p=0;p<4;p++)actions['seqPage'+p]=()=>{page=p;selectedStep=p*16;renderSequence();};
all('[data-action]').forEach(el=>{const fn=actions[el.dataset.action];if(!fn){report('Unbound action: '+el.dataset.action);return;}el.addEventListener('click',async()=>{try{await fn(el);}catch(e){report(e);}finally{focus(el);}});});

// Cleanup runs on up, cancellation, lost capture, and blur: never leave expression
// or a note held when the pointer leaves the WebView or the editor closes.
function pointerGesture(el,start,move,finish){
  if(!el)return;let pointer=null;
  const end=()=>{if(pointer==null)return;const id=pointer;pointer=null;finish();gestures.delete(el);if(el.hasPointerCapture(id))el.releasePointerCapture(id);};
  el.addEventListener('pointerdown',e=>{if(e.button!==0||pointer!=null)return;e.preventDefault();pointer=e.pointerId;gestures.add(el);el.setPointerCapture(pointer);start(e);});
  el.addEventListener('pointermove',e=>{if(pointer===e.pointerId)move(e);});for(const ev of ['pointerup','pointercancel','lostpointercapture'])el.addEventListener(ev,end);window.addEventListener('blur',end);
}
let heldNote=null;
function releaseKeys(){if(heldNote!=null)call('noteOff',heldNote);heldNote=null;all('[data-press]').forEach(el=>el.style.opacity='0');}
function playKey(e){
  const hit=document.elementFromPoint(e.clientX,e.clientY)?.closest('[data-note]'),n=hit?+hit.dataset.note:null;if(n===heldNote)return;releaseKeys();if(n==null)return;learnNote(n);heldNote=n;
  const r=$('keybed').getBoundingClientRect(),v=e.shiftKey?clamp((e.clientY-r.top)/r.height,.1,1):.8;call('noteOn',n,v);all(`[data-press="${n}"]`).forEach(el=>el.style.opacity='1');
  if(!hosted&&feed.recording[li()]){writeStep(feed.recStep[li()],{...emptyStep(),n:[n],v});feed.recStep[li()]=(feed.recStep[li()]+1)%64;renderSequence();}
}
pointerGesture($('keybed'),playKey,playKey,releaseKeys);
// The ribbon [p.77]: ACROSS bends the pitch from wherever the finger lands (relative: the engine
// bends by the distance from the landing point, a full sweep = 12 semitones). PUSH UP is aftertouch,
// measured from the touch point, so it works on a short strip: the pointer keeps tracking above it,
// and RIB_AT_TRAVEL design px up is full pressure (user, 2026-10-05: most keyboards have no aftertouch).
const RIB_AT_TRAVEL=110;
let rib=null;
function ribbon(e,first){
  const el=$('ribbon-input'),r=el.getBoundingClientRect(),z=r.width/el.offsetWidth||1,x=clamp((e.clientX-r.left)/r.width);
  if(first)rib={x0:x,y0:e.clientY,at:-1};
  const at=clamp((rib.y0-e.clientY)/z/RIB_AT_TRAVEL);
  call(first?'ribbonTouch':'ribbonMove',x);
  if(Math.abs(at-rib.at)>.01){rib.at=at;call('ribbonPressure',at);}
  const left=parseFloat(el.style.left),w=el.offsetWidth,mx=left+x*w,ax=left+rib.x0*w;
  const show=(id,css)=>Object.assign($(id).style,css);
  show('ribbon-marker',{left:mx+'px',opacity:'1'});show('ribbon-anchor',{left:ax+'px',opacity:'.9'});
  show('ribbon-span',{left:Math.min(ax,mx)+'px',width:Math.abs(mx-ax)+'px',opacity:'.35'});
  show('ribbon-at',{left:mx+'px',height:(at*90)+'px',opacity:at>0?'1':'0'});
  el.style.cursor='none';
  const st=(x-rib.x0)*12;text('ribbon-readout','BEND '+(st>=0?'+':'')+st.toFixed(1)+' ST   ·   AT '+Math.round(at*100)+' %');
  show('ribbon-readout',{left:mx+'px',top:(parseFloat(el.style.top)-14-at*90)+'px',opacity:'1'});
}
pointerGesture($('ribbon-input'),e=>ribbon(e,true),e=>ribbon(e,false),()=>{call('ribbonRelease');rib=null;
  for(const id of ['ribbon-marker','ribbon-anchor','ribbon-span','ribbon-at','ribbon-readout'])$(id).style.opacity='0';$('ribbon-input').style.cursor='pointer';text('ribbon-readout','');});
function bender(e){
  const r=$('bender-input').getBoundingClientRect();
  // The hardware lever is absolute: its neutral point is the centre of the
  // slot, so grabbing a different spot does not change the control's meaning.
  const x=clamp((e.clientX-(r.left+r.width/2))/(r.width/2),-1,1);
  const y=clamp(((r.top+r.height/2)-e.clientY)/(r.height/2));
  call('bend',x);call('push',y);
  const side=x < -.2 ? 'left' : x > .2 ? 'right' : 'center';
  $('bender-img').src=`new-bender-${side}${y>.35?'-push':''}.png`;
}
pointerGesture($('bender-input'),bender,bender,()=>{call('bend',0);call('push',0);$('bender-img').src='new-bender-center.png';});
function stateFeed(s){
  if(!s)return;
  if('customUpper' in s||'customLower' in s){if('customUpper' in s)custom[0]=s.customUpper;if('customLower' in s)custom[1]=s.customLower;status();}fxUi?.feed(s);if(s.customPlay){const was=JSON.stringify(cuPlay);cuPlay=s.customPlay;if(JSON.stringify(cuPlay)!==was)drawCustom();}feed={...feed,...s};if(s.seq)s.seq.forEach((seq,i)=>{if(seq)sequences[i]=seq;});if(s.patch!=null)setPatchName(s.patch);if(s.lastNote!=null&&s.lastNote>=0)learnNote(s.lastNote);
  text('voice-count',(s.voices||[0,0]).reduce((a,b)=>a+b,0));if(s.bpm!=null)text('bpm-readout',Math.round(s.bpm));
  for(const el of all('[data-meter]'))el.style.transform=`scaleX(${clamp(el.dataset.meter==='l'?s.peakL||0:s.peakR||0)})`;
  for(let i=0;i<2;i++)led(i?'led-lower':'led-upper',(s.voices?.[i]||0)>0);renderSequence();
}
window.__JUCE__?.backend?.addEventListener('geminusState',stateFeed);
function fit(){const root=$('panel');document.documentElement.style.zoom=String(Math.min(innerWidth/root.offsetWidth,innerHeight/root.offsetHeight));document.documentElement.style.overflow='hidden';}
window.addEventListener('resize',fit);window.geminusFit=fit;
applyDesktopLayout(!!(await native('desktopLayout')));applyTheme((await native('uiTheme')) || 'gemini');
for(const id of Object.keys(metadata))store.ensure(id);setStripLayer(store.read('perf.singleLayer')>.5?'lower':'upper');retarget();renderSequence();fit();call('pageSize',$('panel').offsetWidth,$('panel').offsetHeight);
if(hosted)native('patchName').then(setPatchName).catch(report);
window.geminusDiagnostics={hosted,errors,controlCount:controls.length,actionCount:all('[data-action]').length};console.log('002 ready',window.geminusDiagnostics);
// dev: #fx opens the FX page; #fx=1,2,3 also loads those types into the slots
if(location.hash.startsWith('#fx')){(location.hash.split('=')[1]||'').split('&')[0].split(',').forEach((t,i)=>{if(t)fxUi.choose(i,+t);});actions.openFx();}
// dev: #pop=pop-matrix opens a pop-over; add &theme=super6 to preview a theme
{const h=new URLSearchParams(location.hash.slice(1));if(h.get('theme'))applyTheme(h.get('theme'));if(h.get('pop'))openPage(h.get('pop'));}
