// One cache per APVTS id, irrespective of the number of controls showing it.
export class ParameterStore {
  constructor(specs, juce = null) { this.specs = specs; this.juce = juce; this.states = new Map(); this.values = new Map(); this.onChange = () => {}; }
  ensure(id) {
    if (this.states.has(id)) return this.states.get(id);
    const spec = this.specs[id];
    if (!spec) throw new Error('Unknown parameter: ' + id);
    this.values.set(id, spec.def);
    const state = !this.juce ? null : spec.type === 'slider' ? this.juce.getSliderState(id) : spec.type === 'toggle' ? this.juce.getToggleState(id) : this.juce.getComboBoxState(id);
    this.states.set(id, state);
    const update = () => {
      const n = spec.type === 'slider' ? state.getNormalisedValue() : spec.type === 'toggle' ? +state.getValue() : state.getChoiceIndex() / (spec.steps - 1);
      if (Number.isFinite(n)) this.values.set(id, Math.max(0, Math.min(1,n)));
      this.onChange(id);
    };
    state?.valueChangedEvent.addListener(update);
    state?.propertiesChangedEvent?.addListener(update);
    return state;
  }
  read(id) { this.ensure(id); return this.values.get(id); }
  write(id, n) {
    const state = this.ensure(id), spec = this.specs[id];
    if (!Number.isFinite(n)) return;
    n = Math.max(0, Math.min(1,n));
    if (spec.type === 'toggle') n = +(n >= .5);
    if (spec.type === 'combo') n = Math.round(n * (spec.steps - 1)) / (spec.steps - 1);
    this.values.set(id,n);
    if (spec.type === 'slider') state?.setNormalisedValue(n);
    else if (spec.type === 'toggle') state?.setValue(!!n);
    else state?.setChoiceIndex(Math.round(n * (spec.steps - 1)));
    this.onChange(id);
  }
  begin(id) { this.ensure(id)?.sliderDragStarted?.(); }
  end(id) { this.ensure(id)?.sliderDragEnded?.(); }
  actual(id) {
    const spec = this.specs[id], n = this.read(id);
    if (spec.type !== 'slider') return Math.round(n * (spec.steps - 1));
    const [lo,hi] = spec.range.split('..').map(Number);
    return lo + (hi - lo) * n;
  }
  display(id) {
    const spec = this.specs[id];
    if (spec.type === 'toggle') return this.read(id) ? 'ON' : 'OFF';
    if (spec.type === 'combo') return spec.range.trim().split('|')[this.actual(id)];
    // envelope times, as the engine reads them: 10 s × position², at least 1 ms (taper::envTime)
    if (/\.env[12]\.(attack|attackHold|decay|decayHold|release)$/.test(id)) {
      const n = this.read(id);
      if (/Hold$/.test(id) && n <= 0) return 'OFF';
      const t = Math.max(0.001, 10 * n * n);
      return t < 1 ? Math.round(t * 1000) + ' ms' : t.toFixed(2) + ' s';
    }
    return this.actual(id).toFixed(2);
  }
  snapshot() { return Object.fromEntries(Object.keys(this.specs).map(id => [id,this.read(id)])); }
}
