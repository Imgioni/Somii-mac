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
  if (el._override) return el._override;
  let id = el.dataset.layered ? layer + '.' + el.dataset.layered : el._base;
  const secondary = el.dataset.shift || el.dataset.shiftparam;
  if (shift() && secondary) id = metadata[secondary] ? secondary : layer + '.' + secondary;
  return id;
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
  for (let i=0;i<8;i++) { actionState('mtxsrc'+i,i===source); actionState('mtxdst'+i,Math.abs(store.read(route(layer,source,i))-.5)>.001); $('mtxdst'+i)?.classList.toggle('seq-sel',i===destination); }
  // pop-over layer links name the layer you would switch to
  for (const id of ['mtx-layer-link','seq-layer-link','custom-layer-link']) text(id, layer==='upper' ? 'LOWER' : 'UPPER');
  refreshAltDisplays();
  refreshCustom();
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
const THEMES = ['gemini','super6'];
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
all('[data-open]').forEach(el=>el.addEventListener('click',()=>{openPage(el.dataset.open);if(el.dataset.open==='pop-patches')refreshPatches().catch(report);}));
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
    const index=page*16+i,st=seq.steps[index]||emptyStep();text('seq-num'+i,index+1);text('seq-note'+i,st.n.length?noteName(st.n[0]):'—');text('seq-more'+i,st.n.length>1?'+'+(st.n.length-1)+' NOTES':st.n.length?'VEL '+Math.round(st.v*127):'EMPTY');
    $('seq-cell'+i)?.classList.toggle('seq-sel',index===selectedStep);$('seq-cell'+i)?.classList.toggle('seq-off',index>=seq.length);
    if($('seq-play'+i))$('seq-play'+i).style.opacity=(feed.running[l]&&feed.step[l]===index)||(feed.recording[l]&&feed.recStep[l]===index)?'1':'0';
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

// ---- patch browser: search, TYPE / BANK filters, sorting, favourites, and saving into a bank.
// A bank is a folder inside the patch folder: typing a new bank name makes one, and a folder made
// by hand in Explorer shows up here as a bank.
const TYPES=['BASS','LEAD','KEYS','PAD','PLUCK','BRASS','STRINGS','ARP','SEQUENCE','ATMOSPHERE','PERCUSSION','FX'];
let folder='',files=[],banks=[],currentFile='',patch='INIT',abIsB=false,previewAB;
const pb={q:'',types:new Set(),banks:new Set(),fav:false,key:'name',dir:1};
function setPatchName(name){patch=name||'INIT';text('patch-name',patch);if(document.activeElement!==$('patch-name-input'))$('patch-name-input').value=patch;}
const pbVal=(f,k)=>k==='fav'?(f.fav?0:1):String(f[k]||'').toLowerCase();
function pbShown(){
  const q=pb.q.toLowerCase();
  return files.filter(f=>(!q||(f.name+' '+(f.type||'')+' '+(f.bank||'')).toLowerCase().includes(q))
    &&(!pb.types.size||pb.types.has(f.type||''))&&(!pb.banks.size||pb.banks.has(f.bank||''))&&(!pb.fav||f.fav))
    .sort((a,b)=>{const x=pbVal(a,pb.key),y=pbVal(b,pb.key);return (x<y?-1:x>y?1:0)*pb.dir||(a.name<b.name?-1:1);});
}
function renderPatches(){
  const list=$('patch-list');if(!list)return;
  const rows=pbShown();
  text('patch-count',rows.length+(rows.length===files.length?'':' / '+files.length)+' PATCHES');
  for(const el of all('.pb-head')){const on=el.dataset.sort===pb.key;el.querySelector('.pb-arrow').textContent=on?(pb.dir>0?'▲':'▼'):'';el.style.color=on?'var(--accent)':'var(--ink)';}
  $('patch-f-type').classList.toggle('on',!!pb.types.size);$('patch-f-bank').classList.toggle('on',!!pb.banks.size);$('patch-f-fav').classList.toggle('on',pb.fav);
  list.replaceChildren();
  if(!rows.length){const el=document.createElement('div');el.className='patch-empty';el.textContent=files.length?'Nothing matches those filters.':'No patches yet. Name this sound and choose SAVE.';list.append(el);return;}
  for(const file of rows){
    const el=document.createElement('div');el.className='patch-row'+(file.path===currentFile?' sel':'');
    const heart=document.createElement('span');heart.className='pb-fav'+(file.fav?' on':'');heart.textContent=file.fav?'♥':'♡';heart.title='Favourite';
    heart.addEventListener('click',async e=>{e.stopPropagation();file.fav=!file.fav;heart.className='pb-fav'+(file.fav?' on':'');heart.textContent=file.fav?'♥':'♡';
      if(hosted)await native('patchFavourite',file.path,file.fav);if(pb.fav)renderPatches();});
    const cell=(cls,t)=>{const s=document.createElement('span');s.className=cls;s.textContent=t;return s;};
    el.append(heart,cell('pb-name',file.name),cell('pb-type',file.type||'—'),cell('pb-bank',file.bank||'—'));
    el.addEventListener('click',()=>loadPatch(file).catch(report));
    el.addEventListener('dblclick',()=>{$('patch-name-input').value=file.name;$('patch-type-input').value=file.type||'';$('patch-bank-input').value=file.bank||'';});
    list.append(el);
  }
}
// a small drop-down under a control; picking an item runs `pick`
function pbPop(anchor,items,pick,marked){
  all('.pb-pop').forEach(el=>el.remove());
  if(!items.length)items=['(none yet)'];
  const box=document.createElement('div');box.className='pb-pop';
  const panel=anchor.closest('.pop-panel'),r=anchor.getBoundingClientRect(),p=panel.getBoundingClientRect(),z=p.width/panel.offsetWidth||1;
  box.style.left=((r.left-p.left)/z)+'px';box.style.top=((r.bottom-p.top)/z+6)+'px';
  for(const it of items){const val=typeof it==='string'?it:it.value,label=typeof it==='string'?it:it.label;
    const d=document.createElement('div');d.textContent=label;if(marked&&marked(val))d.className='on';
    d.addEventListener('pointerdown',e=>{e.stopPropagation();if(label!=='(none yet)')pick(val);box.remove();});box.append(d);}
  panel.append(box);
  setTimeout(()=>document.addEventListener('pointerdown',()=>box.remove(),{once:true}),0);
}
const patchTypes=()=>[...new Set([...TYPES,...files.map(f=>f.type).filter(Boolean)])].sort();
async function refreshPatches(){
  if(!hosted){text('patch-status','Browser preview: patch files need the plugin.');return;}
  if(!folder)folder=await native('patchFolder');
  files=await native('patchList',folder)||[];banks=await native('patchBanks',folder)||[];
  text('patch-folder-path',folder);
  if(!$('patch-type-input').value)$('patch-type-input').value=(await native('patchType'))||'';
  renderPatches();
}
async function loadPatch(file){
  const name=await native('patchLoad',file.path);if(!name)throw Error('Could not load this 002 patch.');
  currentFile=file.path;setPatchName(name);
  $('patch-type-input').value=file.type||'';$('patch-bank-input').value=file.bank||'';
  text('patch-status','LOADED: '+name+(file.bank?'  ·  '+file.bank:''));
  await refreshPatches();
}
async function savePatch(saveAs=false){
  if(!hosted){openPage('pop-patches');return;}
  if(!folder)folder=await native('patchFolder');
  const name=$('patch-name-input').value.trim();
  if(!name){text('patch-status','Enter a patch name first.');openPage('pop-patches');return;}
  const type=$('patch-type-input').value.trim(),bank=$('patch-bank-input').value.trim();
  const collision=files.find(f=>f.name.toLowerCase()===name.toLowerCase()&&(f.bank||'')===bank);
  if(collision&&collision.path!==currentFile)saveAs=true;
  const saved=saveAs?await native('patchSaveAs'):await native('patchSave',folder,name,bank,type);
  if(saved){currentFile=saved;setPatchName(await native('patchName'));text('patch-status','SAVED: '+saved);await refreshPatches();}
  else text('patch-status','Save cancelled or unsuccessful.');
}
async function adjacentPatch(dir){
  await refreshPatches();const rows=pbShown();
  if(rows.length){const i=rows.findIndex(f=>f.path===currentFile);await loadPatch(rows[(i+dir+rows.length)%rows.length]);}
}
$('patch-search').addEventListener('input',e=>{pb.q=e.target.value;renderPatches();});
$('patch-clear').addEventListener('click',()=>{pb.q='';pb.types.clear();pb.banks.clear();pb.fav=false;$('patch-search').value='';renderPatches();});
$('patch-f-fav').addEventListener('click',()=>{pb.fav=!pb.fav;renderPatches();});
const filterPop=(chip,set,values)=>pbPop(chip,values,v=>{set.has(v)?set.delete(v):set.add(v);renderPatches();},v=>set.has(v));
const named=v=>({value:v,label:v||'(none)'});
$('patch-f-type').addEventListener('click',()=>filterPop($('patch-f-type'),pb.types,[...new Set(files.map(f=>f.type||''))].sort().map(named)));
$('patch-f-bank').addEventListener('click',()=>filterPop($('patch-f-bank'),pb.banks,[...new Set(files.map(f=>f.bank||''))].sort().map(named)));
for(const el of all('.pb-head'))el.addEventListener('click',()=>{const k=el.dataset.sort;pb.dir=pb.key===k?-pb.dir:1;pb.key=k;renderPatches();});
$('patch-name-menu').addEventListener('click',()=>pbPop($('patch-name-menu'),pbShown().map(f=>f.name),v=>{$('patch-name-input').value=v;}));
$('patch-type-menu').addEventListener('click',()=>pbPop($('patch-type-menu'),patchTypes(),v=>{$('patch-type-input').value=v;},v=>v===$('patch-type-input').value));
// dev only (browser preview, no plugin): feed the browser sample rows to look at the layout
if(!hosted)window.__pb={rows:(f,b)=>{files=f;banks=b||[];renderPatches();}};
// dev only: '#pop=pop-patches&demo=1' fills the browser with sample rows so the layout can be checked
if(!hosted&&location.hash.includes('demo=1')){const mk=(n,t,b,f)=>({name:n,path:'C:/002/'+b+'/'+n+'.gpatch',type:t,bank:b,fav:!!f});
  files=[mk('Demacro','LOFI KEYS','FACTORY',1),mk('Gleaming Keys','PLUCKED KEYS','FACTORY'),mk('Seq Me Hard','PLUCKED BASS','PIZZA'),mk('Grape Keys','KEYS','FACTORY'),mk('33 Electric Piano','LOFI KEYS','PIZZA',1),mk('Power Sync','POLY LEAD','FACTORY'),mk('With Grace','STRINGS','MY SOUNDS'),mk('Memory','ATMOSPHERE','MY SOUNDS'),mk('Mars Magma','EVOLVING PAD','PIZZA'),mk('Everlast','PAD','FACTORY'),mk('Super Duper Saw','BIG LEAD','MY SOUNDS'),mk('Mambo Pad','ATMOSPHERE','')];
  banks=['FACTORY','MY SOUNDS','PIZZA'];$('patch-name-input').value='Velvet Keys';$('patch-type-input').value='LOFI KEYS';$('patch-bank-input').value='PIZZA';
  text('patch-folder-path','Documents / 002 / Patches');renderPatches();}
$('patch-bank-menu').addEventListener('click',()=>pbPop($('patch-bank-menu'),banks,v=>{$('patch-bank-input').value=v;},v=>v===$('patch-bank-input').value));
fxUi=initFx({$,all,store,oneWrite,native,hosted,retarget,actionState,text,openPage,report});
// ---- DDS 1 CUSTOM: the sample page. The plugin sends each layer's name, length and outline
// (customUpper / customLower in the state feed) whenever that layer's sample changes.
function refreshCustom(){
  ['upper','lower'].forEach((l,i)=>{const c=custom[i],on=store.read(l+'.dds1.smpOn')>.5;
    text(l+'.dds1.custom-display',c?(on?c.name:c.name+' · OFF'):'NO SAMPLE');actionState(l+'.dds1.custom-open',!!c&&on);});
  const c=custom[li()];
  text('custom-name',c?c.name:'NO SAMPLE');
  text('custom-meta',c?layer.toUpperCase()+'  ·  '+c.seconds.toFixed(2)+' S  ·  '+(c.rate/1000).toFixed(1)+' KHZ':layer.toUpperCase());
  if($('custom-hint'))$('custom-hint').style.display=c?'none':'flex';
  text('custom-start-val',(store.actual(layer+'.dds1.smpStart')*100).toFixed(1)+' %');
  text('custom-end-val',(store.actual(layer+'.dds1.smpEnd')*100).toFixed(1)+' %');
  text('custom-loop-val',(store.actual(layer+'.dds1.smpLoopStart')*100).toFixed(1)+' %');
  {const x=store.actual(layer+'.dds1.smpLevel'),g=x<=.8?(x/.8)**2:10**(x-.8),db=20*Math.log10(g);text('custom-level-val',g<=1e-5?'-∞ DB':(db>0?'+':'')+db.toFixed(1)+' DB');}
  text('custom-root-val',customLearn?'PLAY A KEY':noteName(store.actual(layer+'.dds1.smpRoot')));
  const fine=Math.round(store.actual(layer+'.dds1.smpFine'));text('custom-fine-val',(fine>0?'+':'')+fine+' CT');
  actionState('act-customLearn',customLearn);
  drawCustom();
}
function drawCustom(){
  const cv=$('custom-canvas');if(!cv||$('pop-custom')?.hidden)return;
  const g=cv.getContext('2d'),W=cv.width,H=cv.height,c=custom[li()],css=getComputedStyle(cv);
  const accent=css.getPropertyValue('--pop-accent').trim()||'#F65A27',ink=css.getPropertyValue('--pop-ink2').trim()||'#A8A49C';
  g.clearRect(0,0,W,H);
  g.fillStyle=ink;g.globalAlpha=.35;g.fillRect(0,H/2,W,1.5);g.globalAlpha=1;
  if(!c)return;
  const a=store.actual(layer+'.dds1.smpStart')*W,b=store.actual(layer+'.dds1.smpEnd')*W,n=c.wave.length/2,pad=24;
  g.fillStyle=accent;
  for(let x=0;x<W;x+=2){const i=Math.min(n-1,Math.floor(x/W*n)),lo=c.wave[i*2],hi=c.wave[i*2+1];g.globalAlpha=x>=a&&x<=b?.9:.25;g.fillRect(x,H/2-hi*(H/2-pad),2,Math.max(1.5,(hi-lo)*(H/2-pad)));}
  const loop=store.read(layer+'.dds1.smpLoop')>.5,ls=Math.min(store.actual(layer+'.dds1.smpLoopStart')*W,b);
  g.globalAlpha=.12;g.fillRect(a,0,b-a,H);g.globalAlpha=1;
  if(loop){g.fillStyle='#fff';g.globalAlpha=.1;g.fillRect(ls,0,b-ls,H);g.globalAlpha=1;}
  const tag=(x,t,left,y,fill,ink)=>{g.fillStyle=fill;g.fillRect(x-2,0,4,H);g.fillRect(left?x:x-176,y,176,48);g.fillStyle=ink;g.font='700 30px Bahnschrift,'SPKR Condensed',sans-serif';g.textAlign='center';g.fillText(t,left?x+88:x-88,y+35);};
  tag(a,'START',true,0,accent,'#fff');tag(b,'END',false,0,accent,'#fff');
  if(loop)tag(ls,'LOOP START',true,H-48,'#fff','#222');
  g.fillStyle='#fff';g.font='700 30px Bahnschrift,'SPKR Condensed',sans-serif';g.textAlign='center';g.fillText(loop?'⟲  LOOP':'ONE SHOT  →',loop?(ls+b)/2:(a+b)/2,loop?H/2+10:H-22);
}
async function loadCustomFile(file){
  if(!file)return;
  if(!/\.(wav|aiff?|flac|ogg|mp3)$/i.test(file.name)){text('custom-status','Drop a WAV, AIFF, FLAC, OGG or MP3 file.');return;}
  if(file.size>60e6){text('custom-status','That file is too large (60 MB max).');return;}
  text('custom-status','LOADING '+file.name.toUpperCase()+' …');
  const bytes=new Uint8Array(await file.arrayBuffer()),l=li(),name=file.name.replace(/\.[^.]+$/,'');
  if(hosted){
    let bin='';for(let i=0;i<bytes.length;i+=0x8000)bin+=String.fromCharCode.apply(null,bytes.subarray(i,i+0x8000));
    if(!await native('customLoad',l,btoa(bin),file.name)){text('custom-status','Could not read '+file.name+'.');return;}
  }else{   // browser preview: decode here so the page can be checked without the plugin
    const buf=await new OfflineAudioContext(1,1,48000).decodeAudioData(bytes.buffer),d=buf.getChannelData(0),wave=[];
    let top=1e-6;for(const v of d)top=Math.max(top,Math.abs(v));
    for(let p=0;p<600;p++){let lo=0,hi=0;for(let i=Math.floor(p*d.length/600);i<Math.floor((p+1)*d.length/600);i++){lo=Math.min(lo,d[i]/top);hi=Math.max(hi,d[i]/top);}wave.push(lo,hi);}
    custom[l]={name,seconds:buf.duration,rate:buf.sampleRate,wave};
  }
  oneWrite((l?'lower':'upper')+'.dds1.smpOn',1);   // a new sample is meant to be heard
  text('custom-status','LOADED. PLAY ITS ROOT KEY TO HEAR IT AT PITCH.');status();
}
{
  const drop=$('custom-drop'),input=$('custom-file');
  const fracAt=e=>{const r=drop.getBoundingClientRect();return clamp((e.clientX-r.left)/r.width);};
  drop.addEventListener('dragover',e=>{e.preventDefault();drop.style.boxShadow='inset 0 0 0 3px var(--pop-accent)';});
  drop.addEventListener('dragleave',()=>drop.style.boxShadow='');
  drop.addEventListener('drop',e=>{e.preventDefault();drop.style.boxShadow='';loadCustomFile(e.dataTransfer?.files?.[0]).catch(report);});
  input.addEventListener('change',()=>{loadCustomFile(input.files[0]).catch(report);input.value='';});
  // empty: a click browses. Loaded: drag the nearer of START / END (they never cross).
  const move=e=>{if(!customDrag)return;const x=fracAt(e),v=k=>store.actual(layer+'.dds1.'+k);
    const lim={smpStart:[0,v('smpEnd')-.002],smpLoopStart:[v('smpStart'),v('smpEnd')-.002],smpEnd:[Math.max(v('smpStart'),v('smpLoopStart'))+.002,1]}[customDrag.key];
    store.write(customDrag.id,clamp(x,lim[0],lim[1]));};
  drop.addEventListener('pointerdown',e=>{
    if(e.button!==0)return;e.preventDefault();
    if(!custom[li()]){input.click();return;}
    const x=fracAt(e),v=k=>store.actual(layer+'.dds1.'+k),cands=['smpStart','smpEnd'];
    if(store.read(layer+'.dds1.smpLoop')>.5)cands.push('smpLoopStart');
    // the LOOP START tag hangs at the bottom: a grab down there takes it even where it overlaps START
    const r=drop.getBoundingClientRect(),low=cands.length>2&&e.clientY>r.bottom-r.height*.2;
    const key=low?'smpLoopStart':cands.reduce((best,k)=>Math.abs(x-v(k))<Math.abs(x-v(best))?k:best);
    customDrag={id:layer+'.dds1.'+key,key};store.begin(customDrag.id);drop.setPointerCapture(e.pointerId);gestures.add(drop);move(e);
  });
  drop.addEventListener('pointermove',move);
  const end=()=>{if(!customDrag)return;store.end(customDrag.id);customDrag=null;gestures.delete(drop);};
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
  patchRefresh:refreshPatches,patchFolderUser:async()=>{folder=await native('patchFolder');await refreshPatches();},patchFolderChoose:async()=>{const c=await native('patchChooseFolder');if(c)folder=c;await refreshPatches();},patchSave:()=>savePatch(shift()),patchSaveAs:()=>savePatch(true),patchOpen:async()=>{const f=await native('patchOpen');if(f){currentFile=f;setPatchName(await native('patchName'));await refreshPatches();}else if(!hosted)await refreshPatches();},patchPrev:()=>adjacentPatch(-1),patchNext:()=>adjacentPatch(1),
  ab:async()=>{if(hosted){if(shift())await native('abCopy');else abIsB=!!(await native('abToggle'));}else if(shift()||!previewAB)previewAB=store.snapshot();else{const current=store.snapshot();for(const[id,v]of Object.entries(previewAB))oneWrite(id,v);previewAB=current;abIsB=!abIsB;}actionState('act-ab',abIsB);},
  toggleDesktopLayout,themeSuper6:()=>setTheme('super6'),openFx:()=>fxUi.open(),fxSerial:()=>fxUi.serial(),fxParallel:()=>fxUi.parallel(),themeGemini:()=>setTheme('gemini')
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
// The ribbon bends pitch from wherever the finger lands [p.77]: slide right OR up to bend up, left
// or down to bend down (the manual plays it bottom-to-top); a full sweep either way is the full range.
let ribbonStart=.5,ribbonStartY=.5;
function ribbon(e,first){const el=$('ribbon-input'),r=el.getBoundingClientRect(),x=clamp((e.clientX-r.left)/r.width),y=(e.clientY-r.top)/r.height;if(first){ribbonStart=x;ribbonStartY=y;}
  const pos=clamp(ribbonStart+(x-ribbonStart)+(ribbonStartY-y));call(first?'ribbonTouch':'ribbonMove',first?x:pos);$('ribbon-marker').style.left=(parseFloat(el.style.left)+x*el.offsetWidth)+'px';$('ribbon-marker').style.opacity='1';el.style.cursor='none';const st=Math.round((pos-ribbonStart)*12);text('ribbon-readout',`BEND ${st>0?'+':''}${st} ST`);}
pointerGesture($('ribbon-input'),e=>ribbon(e,true),e=>ribbon(e,false),()=>{call('ribbonRelease');$('ribbon-marker').style.opacity='0';$('ribbon-input').style.cursor='pointer';text('ribbon-readout','');});
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
  if('customUpper' in s||'customLower' in s){if('customUpper' in s)custom[0]=s.customUpper;if('customLower' in s)custom[1]=s.customLower;status();}fxUi?.feed(s);feed={...feed,...s};if(s.seq)s.seq.forEach((seq,i)=>{if(seq)sequences[i]=seq;});if(s.patch!=null)setPatchName(s.patch);if(s.lastNote!=null&&s.lastNote>=0)learnNote(s.lastNote);
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
