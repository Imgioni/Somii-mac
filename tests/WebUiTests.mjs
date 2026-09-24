import assert from 'node:assert/strict';
import fs from 'node:fs';
import { ParameterStore } from '../ui/parameter-store.js';
const html = fs.readFileSync(new URL('../ui/index.html', import.meta.url), 'utf8');
const specs = JSON.parse(html.match(/<script id="parameter-spec" type="application\/json">(.*?)<\/script>/s)[1]);
const local = new ParameterStore(specs);
let checks = 0;
const check = (condition, message) => { assert.ok(condition, message); checks++; };
check(Object.keys(specs).length === 782, 'all parameters embedded');
for (const [id, spec] of Object.entries(specs)) {
  check(local.read(id) === spec.def, `${id} uses the APVTS default`);
  local.write(id, 2); check(local.read(id) === 1, `${id} clamps upper bound`);
  local.write(id, -1); check(local.read(id) === 0, `${id} clamps lower bound`);
  local.write(id, spec.def);
}
local.write('upper.voice.mode', 1/3);
check(local.display('upper.voice.mode') === 'LEGATO', 'choice labels use correct index');
check(Math.abs(local.read('lower.voice.mode') - specs['lower.voice.mode'].def) < .00001, 'layers are isolated');
local.write('upper.fx.chorus', 1/3);
check(local.actual('upper.fx.chorus') === 1, 'chorus I');
local.write('upper.fx.chorus', 0);
check(local.actual('upper.fx.chorus') === 0, 'chorus clears');
const event = () => ({ listeners: [], addListener(fn) { this.listeners.push(fn); }, emit() { this.listeners.forEach(fn=>fn()); } });
const calls = [], states = new Map();
function relay(id, type) {
  check(specs[id].type === type, 'relay type matches processor: ' + id);
  if (!states.has(id)) states.set(id, {
    n:specs[id].def, valueChangedEvent:event(), propertiesChangedEvent:event(),
    getNormalisedValue(){return this.n;}, getValue(){return !!this.n;}, getChoiceIndex(){return Math.round(this.n*(specs[id].steps-1));},
    setNormalisedValue(n){this.n=n;calls.push(['slider',id,n]);}, setValue(v){this.n=+v;calls.push(['toggle',id,v]);},
    setChoiceIndex(i){this.n=i/(specs[id].steps-1);calls.push(['combo',id,i]);},
    sliderDragStarted(){calls.push(['begin',id]);}, sliderDragEnded(){calls.push(['end',id]);}
  });
  return states.get(id);
}
const host = new ParameterStore(specs, {getSliderState:id=>relay(id,'slider'),getToggleState:id=>relay(id,'toggle'),getComboBoxState:id=>relay(id,'combo')});
const id = 'upper.vcf.lpf'; host.begin(id);host.write(id,.25);host.end(id);
check(JSON.stringify(calls.slice(-3)) === JSON.stringify([['begin',id],['slider',id,.25],['end',id]]), 'automation gesture is paired');
let notifications=0;host.onChange=()=>notifications++;
states.get(id).n=.9;states.get(id).valueChangedEvent.emit();
check(host.read(id)===.9 && notifications===1, 'host automation refreshes cached views');
host.write('upper.voice.mode',2/3);check(calls.at(-1)[2]===2, 'choice relay receives index, not normalized value');
host.write('global.ccRx',0);check(calls.at(-1)[2]===false, 'toggle relay receives bool');
host.ensure(id);check(states.get(id).valueChangedEvent.listeners.length===1, 'linked controls do not duplicate listeners');
const ids=[...html.matchAll(/\sid="([^"]+)"/g)].map(m=>m[1]);
check(ids.length===new Set(ids).size, 'all DOM ids are unique');
const available=new Set(ids);
for(const m of html.matchAll(/data-(?:vis|ind|led)="([^"]+)"/g)) check(available.has(m[1]), 'sprite target exists: '+m[1]);
check(!/fonts\.google|https?:\/\//.test(html), 'page has no external dependencies');
console.log(`${checks} Web UI checks passed.`);
