import fs from 'node:fs';
let j = fs.readFileSync('geminus.js', 'utf8');
let n = 0;
const sub = (a, b, tag) => { if (!j.includes(a)) { console.log('MISS ' + tag); return; } j = j.replace(a, b); n++; };

// paint: a bit button lights when its own bit is set, so I+II lights both
sub(
`    case 'button': {
      const steps = parseInt(el.dataset.steps || '2', 10);
      const on = el.dataset.value != null
        ? Math.round(value * (steps - 1)) === parseInt(el.dataset.value, 10)
        : value >= 0.5;`,
`    case 'button': {
      const steps = parseInt(el.dataset.steps || '2', 10);
      const on = el.dataset.bit != null
        ? (Math.round(value * (steps - 1)) & parseInt(el.dataset.bit, 10)) !== 0
        : el.dataset.value != null
        ? Math.round(value * (steps - 1)) === parseInt(el.dataset.value, 10)
        : value >= 0.5;`,
  'paint');

// press: a bit button flips its own bit, so pressing a lit one turns that one off
sub(
`function bindButton(el) {
  const isChoice = el.dataset.value != null;
  const steps = parseInt(el.dataset.steps || '2', 10);
  const mine = parseInt(el.dataset.value || '0', 10);
  el.addEventListener('pointerdown', (e) => {
    if (e.button !== 0) return;
    e.preventDefault();
    if (isChoice) writeNorm(el, mine / (steps - 1));
    else writeNorm(el, readNorm(el) >= 0.5 ? 0 : 1);
  });
}`,
`function bindButton(el) {
  // CHORUS I and II are two bits of one 4-way parameter (OFF / I / II / I+II). Each key
  // flips its own bit, so pressing a lit one clears it and both can be on together.
  const bit = el.dataset.bit != null ? parseInt(el.dataset.bit, 10) : 0;
  const isChoice = el.dataset.value != null;
  const steps = parseInt(el.dataset.steps || '2', 10);
  const mine = parseInt(el.dataset.value || '0', 10);
  el.addEventListener('pointerdown', (e) => {
    if (e.button !== 0) return;
    e.preventDefault();
    if (bit) writeNorm(el, ((Math.round(readNorm(el) * (steps - 1)) ^ bit) & (steps - 1)) / (steps - 1));
    else if (isChoice) writeNorm(el, mine / (steps - 1));
    else writeNorm(el, readNorm(el) >= 0.5 ? 0 : 1);
  });
}`,
  'bindButton');

// routing: a bit button must not fall through to the plain step-through binder
sub(
"    if (el.dataset.ctl === 'button' && el.dataset.value == null && type === 'combo') bindStepped(el);",
"    if (el.dataset.ctl === 'button' && el.dataset.value == null && el.dataset.bit == null && type === 'combo') bindStepped(el);",
  'routing');

fs.writeFileSync('geminus.js', j);
console.log(n + '/3 applied');
