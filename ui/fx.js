// FX rack page (docs/fx/FX_PROMPTS.md). Each slot is a small module drawn here, all on one SPKR
// chassis: a header (grip, power, name, module number, activity light), the effect's own face
// (colours and layout from ui/fxskins.js, flat controls from tools/make-fx-controls.mjs, a live
// display) and a shared footer with DRY / WET and the LAYER MIX. Parameter meanings come from
// ui/fxdefs.mjs (embedded as #fx-spec).
import { SKINS, SKIN_OF, UPPER_COLOUR, LOWER_COLOUR } from './fxskins.js';

export function initFx(ctx) {
  const { $, all, store, oneWrite, native, hosted, actionState, text, openPage, report } = ctx;
  const { FX, DIVS, NP, CARVE_BEATS, CARVE_SHAPERS, CARVE_HINTS, FX_MOD_SOURCES, FX_EXT } = JSON.parse($('fx-spec').textContent);
  const NSRC = FX_MOD_SOURCES.length;
  const NV = 16;                                                   // live display values per slot (fx::Unit::kVis)
  const TYPES = FX.length + 1;                                     // NONE + effects
  const from = (q, n) => q.steps ? Math.round(n * (q.steps.length - 1)) : q.curve === 'log' ? q.lo * Math.pow(q.hi / q.lo, n) : q.lo + (q.hi - q.lo) * n;
  const to = (q, v) => q.steps ? (q.steps.length > 1 ? v / (q.steps.length - 1) : 0) : q.curve === 'log' ? Math.log(v / q.lo) / Math.log(q.hi / q.lo) : (v - q.lo) / (q.hi - q.lo);
  const pid = (s, p) => 'fx' + (s + 1) + '.' + p;
  const type = (s) => Math.round(store.read(pid(s, 'type')) * (TYPES - 1));
  const def = (s) => { const t = type(s); return t > 0 ? FX[t - 1] : null; };
  const param = (s, p) => def(s)?.params.find((q) => q.p === p);
  const val = (s, p) => { const q = param(s, p); return q ? from(q, store.read(pid(s, p))) : 0; };
  const view = { open: -1, band: [-1, -1, -1], vis: new Array(3 * NV).fill(0), spec: [], specSm: [[], [], []], level: [0, 0, 0],
    ir: { name: 'DEFAULT HALL', wave: [], secs: 2.2 }, hist: [[], [], []], grains: [[], [], []], built: [-1, -1, -1], sband: [1, 1, 1],
    // each slot's extra values (CARVE's waves), its modulation routes (target * NSRC + source -> amount) and
    // where the plugin is pushing each modulated control now; CARVE's selected shaper
    ext: [0, 1, 2].map(() => new Array(FX_EXT).fill(0)), mods: [new Map(), new Map(), new Map()], modNow: [new Map(), new Map(), new Map()],
    shaper: [8, 8, 8], extDirty: [new Set(), new Set(), new Set()] };
  // CONTOUR: the band knobs ('band0..2') follow the selected band: FREQ, GAIN, Q
  const BANDS = [['p12', 'p13', 'p14'], ['p15', 'p16', 'p17'], ['p18', 'p19', 'p20'], ['p21', 'p22', 'p23'], ['p24', 'p25', 'p26'], ['p1', 'p2', 'p3']];
  const skinName = (s) => { const d = def(s); return d ? SKIN_OF[d.id] || 'house' : 'house'; };
  // a skin colour may be a theme variable (the house skin follows GEMINI / SUPER SIX, darker)
  const cssVar = (v) => typeof v === 'string' && v.startsWith('var(') ? (getComputedStyle(document.body).getPropertyValue(v.slice(4, -1)).trim() || '#888888') : v;
  const skin = (s) => { const k = {}; for (const [a, b] of Object.entries(SKINS[skinName(s)])) k[a] = cssVar(b); return k; };
  const FONT = "Bahnschrift,'SPKR Condensed','Arial Narrow',sans-serif";
  const lum = (hex) => { if (!/^#[0-9a-f]{6}$/i.test(hex)) return 0; const n = parseInt(hex.slice(1), 16); return ((n >> 16) * 0.3 + ((n >> 8) & 255) * 0.59 + (n & 255) * 0.11) / 255; };
  const alpha = (hex, a) => /^#[0-9a-f]{6}$/i.test(hex) ? hex + Math.round(a * 255).toString(16).padStart(2, '0') : hex;
  const lower = (str) => String(str).toLowerCase();

  // ---- parameter keys: 'pN', 'mix', 'layer', 'on', 'band0..3' (CONTOUR's selected band) or 'sdrive' (HEAT's selected band)
  const PQ_SHAPE = ['p5', 'p6', 'p7', 'p8', 'p10', 'p11'];          // each band's SHAPE slot (ui/fxdefs.mjs)
  const pqBand = (s) => Math.max(0, view.band[s]);
  const keyP = (s, key) => key.startsWith('band') ? (+key[4] < 3 ? BANDS[pqBand(s)][+key[4]] : PQ_SHAPE[pqBand(s)]) : key === 'sdrive' ? 'p' + (3 + view.sband[s]) : key;
  const SAT_BANDS = ['LOW', 'MID', 'HIGH'];
  function spec(s, key) {
    const p = keyP(s, key);
    if (p === 'mix') return { p, label: 'DRY / WET', lo: 0, hi: 100, unit: '%', curve: 'lin', def: def(s)?.mix ?? 100 };
    if (p === 'layer') return { p, label: 'LAYER', lo: 0, hi: 1, unit: '', curve: 'lin', def: 0.5 };
    if (p === 'on') return { p, label: 'ON', steps: ['OFF', 'ON'], def: 1 };
    return param(s, p);
  }
  const norm = (s, key) => store.read(pid(s, keyP(s, key)));
  const defNorm = (s, key) => { const q = spec(s, key); return !q ? 0 : q.p === 'layer' ? 0.5 : to(q, q.def); };
  function fmt(s, q, n) {
    const d = def(s);
    if (d?.sync && q.p === d.sync.knob && val(s, d.sync.p) === 1) return DIVS[Math.round(n * (DIVS.length - 1))];
    if (q.p === 'layer') return 'UPPER ' + Math.round(Math.min(1, 2 * (1 - n)) * 100) + '  ·  LOWER ' + Math.round(Math.min(1, 2 * n) * 100);
    const v = from(q, n);
    if (q.steps) return q.steps[v];
    switch (q.unit) {
      case 'Hz': return v >= 1000 ? (v / 1000).toFixed(v >= 10000 ? 1 : 2) + ' kHz' : (v < 10 ? v.toFixed(2) : Math.round(v)) + ' Hz';
      case 'ms': return v >= 1000 ? (v / 1000).toFixed(2) + ' s' : (v < 10 ? v.toFixed(2) : Math.round(v)) + ' ms';
      case 's': return v.toFixed(v < 10 ? 2 : 1) + ' s';
      case 'dB': return (v > 0.05 ? '+' : '') + v.toFixed(1) + ' dB';
      case '%': return Math.round(v) + ' %';
      case ':1': return v.toFixed(v < 10 ? 1 : 0) + ':1';
      case 'st': return (Math.round(v) > 0 ? '+' : '') + Math.round(v) + ' st';
      default: return (Math.abs(v) < 10 && !Number.isInteger(v) ? v.toFixed(2) : Math.round(v)) + (q.unit ? ' ' + q.unit : '');
    }
  }
  const readout = (s, key) => { const q = spec(s, key); return q ? fmt(s, q, norm(s, key)) : ''; };

  // ---- one stylesheet for every module
  const css = document.createElement('style');
  css.textContent = `
  .fx-frame{cursor:grab;border-radius:10px;overflow:hidden;background:var(--fbg);color:var(--fink);font-family:${FONT};font-stretch:75%;
    box-shadow:0 22px 44px -18px rgba(0,0,0,.75),0 0 0 1px rgba(255,255,255,.07);user-select:none;transition:box-shadow .15s,opacity .15s}
  .fx-frame.fx-over{box-shadow:0 0 0 3px var(--facc),0 22px 44px -18px rgba(0,0,0,.75)}
  .fx-frame.fx-dragging{opacity:.55}
  .fx-frame>*,.fx-body>*,.fx-frame .fx-sub>*{position:absolute}
  .fx-body{left:0;top:0;width:100%;height:100%;transition:opacity .25s ease-out,filter .25s ease-out}
  .fx-frame.bypassed .fx-body{opacity:.38;filter:saturate(.15)}
  .fx-frame.fx-in .fx-body{animation:fx-in .38s cubic-bezier(.16,1,.3,1)}
  @keyframes fx-in{from{opacity:0;translate:0 12px}}
  .fx-head{left:0;top:0;right:0;height:64px;border-bottom:1px solid color-mix(in srgb,var(--fink2) 28%,transparent)}
  .fx-foot{left:0;right:0;border-top:1px solid color-mix(in srgb,var(--fink2) 28%,transparent);background:color-mix(in srgb,var(--fbg),#000 8%)}
  .fx-grip{left:12px;top:18px;width:22px;height:28px;color:var(--fink2);cursor:grab;touch-action:none;border-radius:4px;transition:color .12s}
  .fx-grip:hover{color:var(--fink)}
  .fx-name{left:92px;top:13px;height:38px;font-size:26px;line-height:38px;font-weight:700;letter-spacing:.06em;cursor:pointer;white-space:nowrap;display:flex;align-items:center;gap:10px;border-radius:4px}
  .fx-name svg{opacity:.55;transition:opacity .12s,translate .15s}.fx-name:hover svg{opacity:1;translate:0 2px}
  .fx-code{right:46px;top:24px;font-size:13px;font-weight:600;letter-spacing:.16em;color:var(--fink2);white-space:nowrap}
  .fx-led{right:22px;top:27px;width:10px;height:10px;border-radius:50%;background:var(--facc);opacity:.12;transition:opacity .07s linear}
  .fx-empty{display:flex;flex-direction:column;align-items:center;justify-content:center;gap:12px;cursor:pointer;border-radius:10px;
    box-shadow:inset 0 0 0 1px color-mix(in srgb,var(--fink2) 40%,transparent);color:var(--fink2);font-size:20px;font-weight:700;letter-spacing:.12em;transition:box-shadow .15s,color .15s,background-color .15s}
  .fx-empty:hover{box-shadow:inset 0 0 0 2px var(--facc);color:var(--fink);background:color-mix(in srgb,var(--facc) 8%,transparent)}
  .fx-empty b{font-size:44px;line-height:1;font-weight:400}
  .fx-t{white-space:nowrap;pointer-events:none;line-height:1.15}
  .fx-hd{display:flex;align-items:center;gap:10px;font-size:13px;font-weight:700;letter-spacing:.18em;color:var(--fink2);pointer-events:none}
  .fx-hd i{flex:1;height:1px;background:currentColor;opacity:.35}
  .fxc{touch-action:none;cursor:ns-resize}
  .fxk svg{position:absolute;left:0;top:0;width:100%;height:100%;overflow:visible}
  .fxk .cap{position:absolute;pointer-events:none}
  .fxk:hover .cap{filter:brightness(1.08)}
  .fxh{cursor:ew-resize}
  .fxv i,.fxh i,.fxv b,.fxh b{position:absolute;display:block;pointer-events:none}
  .fxs,.fxg,.fxp{cursor:pointer}
  .fxs{white-space:nowrap;font-weight:700;letter-spacing:.06em;border-radius:4px}
  .fxs:hover{text-decoration:underline;text-underline-offset:5px;text-decoration-thickness:1px}
  .fxg{display:flex;align-items:center;justify-content:center;font-weight:700;letter-spacing:.08em;border-radius:4px;
    box-shadow:inset 0 0 0 1px color-mix(in srgb,var(--fink2) 45%,transparent);transition:background-color .12s,color .12s}
  .fxg:hover{background:color-mix(in srgb,var(--fink) 8%,transparent)}
  .fx-screen{border-radius:6px;overflow:hidden;cursor:default}
  .fx-screen canvas{width:100%;height:100%;display:block}
  .fx-drop{position:absolute;inset:0;display:none;align-items:center;justify-content:center;font-size:24px;font-weight:700;letter-spacing:.1em;
    color:var(--facc);background:rgba(0,0,0,.72);box-shadow:inset 0 0 0 2px var(--facc);border-radius:6px;pointer-events:none}
  .fx-screen.drag .fx-drop{display:flex}
  .fx-route.fx-over{box-shadow:0 0 0 3px var(--accent)}
  .fxp{border-radius:50%;box-shadow:inset 0 0 0 2.5px var(--fink2);transition:background-color .12s,box-shadow .12s}
  .fxp i{position:absolute;inset:0;background:var(--fink2);-webkit-mask:url(fx-power-glyph.svg) center/100% 100% no-repeat;mask:url(fx-power-glyph.svg) center/100% 100% no-repeat;pointer-events:none}
  .fxp.on{background:var(--facc);box-shadow:none}.fxp.on i{background:var(--fbg)}
  .fx-menu{position:absolute;z-index:20;min-width:190px;padding:6px 0;border-radius:8px;background:#1C1D20;box-shadow:0 16px 32px -10px rgba(0,0,0,.7),0 0 0 1px rgba(255,255,255,.1);
    font:700 16px ${FONT};font-stretch:75%;letter-spacing:.04em;color:#E7E2DA;animation:fx-menu .14s ease-out}
  @keyframes fx-menu{from{opacity:0;translate:0 -6px}}
  .fx-menu div{position:static;padding:7px 16px;cursor:pointer;white-space:nowrap}.fx-menu div:hover{background:rgba(255,255,255,.1)}
  .fx-menu .sep{height:1px;padding:0;margin:5px 0;background:rgba(255,255,255,.12);cursor:default}
  .fxn.modded>.fx-t:first-child{color:var(--facc)!important}
  .fx-tab{flex-direction:column;gap:4px}.fx-tab i{position:static;width:7px;height:7px;border-radius:50%;background:currentColor;opacity:.25}
  .fx-tab.lit i{opacity:1;background:var(--facc)}.fx-tab.lit.cur i{background:var(--fbg)}
  .fx-mod{position:absolute;z-index:25;width:420px;padding:14px 0 10px;border-radius:8px;background:#1C1D20;box-shadow:0 16px 32px -10px rgba(0,0,0,.7),0 0 0 1px rgba(255,255,255,.1);
    font:700 15px ${FONT};font-stretch:75%;color:#E7E2DA;animation:fx-menu .14s ease-out;cursor:default}
  .fx-mod>*{position:relative}
  .fx-mod h4{margin:0 18px 10px;font-size:14px;font-weight:700;letter-spacing:.14em;color:#A8A49C;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
  .fx-mod .r{height:31px;display:flex;align-items:center;padding:0 18px;gap:12px;cursor:ew-resize;touch-action:none}
  .fx-mod .r:hover{background:rgba(255,255,255,.06)}
  .fx-mod .n{width:112px;letter-spacing:.06em;white-space:nowrap}
  .fx-mod .t{flex:1;height:4px;background:rgba(255,255,255,.14);border-radius:2px;pointer-events:none}
  .fx-mod .t b{position:absolute;top:0;height:4px;border-radius:2px;background:var(--pop-accent,#F65A27)}
  .fx-mod .t i{position:absolute;left:50%;top:-5px;width:1px;height:14px;background:rgba(255,255,255,.35)}
  .fx-mod .v{width:58px;text-align:right;color:#A8A49C;white-space:nowrap}
  .fx-mod .r.on .n,.fx-mod .r.on .v{color:var(--pop-accent,#F65A27)}
  .fx-mod .f{display:flex;justify-content:space-between;padding:10px 18px 0;font-size:13px;color:#A8A49C;letter-spacing:.1em}
  .fx-mod .f span{cursor:pointer}.fx-mod .f span:hover{color:#E7E2DA}
  @media (prefers-reduced-motion: reduce){.fx-frame *,.fx-frame{animation:none!important;transition:none!important}}
  `;
  document.head.appendChild(css);

  // ---- builders: every position is relative to the slot frame
  const at = (x, y, w, h) => 'left:' + Math.round(x) + 'px;top:' + Math.round(y) + 'px;' + (w != null ? 'width:' + Math.round(w) + 'px;' : '') + (h != null ? 'height:' + Math.round(h) + 'px;' : '');
  const esc = (v) => String(v).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
  const T = (x, y, w, str, o = {}) => '<div class="fx-t" style="' + at(x, y, w) + 'text-align:' + (o.align || 'center') + ';font-size:' + (o.size || 15) + 'px;font-weight:' + (o.weight || 600)
    + ';color:' + (o.color || 'var(--fink2)') + ';' + (o.ls != null ? 'letter-spacing:' + o.ls + 'em;' : 'letter-spacing:.06em;') + '"'
    + (o.val ? ' data-val="' + o.val + '"' : '') + '>' + esc(str) + '</div>';
  const arcPath = (a0, a1, r = 46) => {
    const p = (a) => [50 + Math.sin(a * Math.PI / 180) * r, 50 - Math.cos(a * Math.PI / 180) * r];
    const [x0, y0] = p(a0), [x1, y1] = p(a1);
    return 'M' + x0.toFixed(2) + ' ' + y0.toFixed(2) + 'A' + r + ' ' + r + ' 0 ' + (Math.abs(a1 - a0) > 180 ? 1 : 0) + ' ' + (a1 > a0 ? 1 : 0) + ' ' + x1.toFixed(2) + ' ' + y1.toFixed(2);
  };
  const A0 = -140, A1 = 140;
  const CHEVRON = '<svg width="16" height="16" viewBox="0 0 16 16" aria-hidden="true"><path d="M3.5 6 8 10.5 12.5 6" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/></svg>';
  const GRIP = '<svg width="22" height="28" viewBox="0 0 22 28" aria-hidden="true">' + [6, 14, 22].flatMap((y) => [7, 15].map((x) => '<circle cx="' + x + '" cy="' + y + '" r="2.2" fill="currentColor"/>')).join('') + '</svg>';
  function builders(s) {
    const K = skin(s), L = (key, fb) => lower(fb || spec(s, key)?.label || '');
    return {
      K, T,
      // flat knob: a generated cap rotating inside a live value arc; label and read-out beneath
      knob(key, x, y, d, o = {}) {
        const pad = Math.round(d * (o.pad ?? 0.12)), aw = o.arcW || (d >= 110 ? 5 : 4);
        let h = '<div class="fxc fxk" data-key="' + key + '" data-kind="knob" title="Drag up or down · double-click resets" style="' + at(x, y, d, d) + '">'
          + '<svg viewBox="0 0 100 100"><path d="' + arcPath(A0, A1) + '" fill="none" stroke="' + K.ink2 + '" stroke-opacity=".28" stroke-width="' + aw + '" stroke-linecap="round"/>'
          + '<path class="val" fill="none" stroke="' + (o.colour || K.accent) + '" stroke-width="' + aw + '" stroke-linecap="round"/>'
          // modulation: the range the routed sources can push it through, and where it is now
          + '<path class="mod" fill="none" stroke="' + K.ink + '" stroke-width="2.5" stroke-linecap="round" opacity=".8"/><circle class="mdot" r="0" fill="' + K.ink + '"/></svg>'
          + '<img class="cap" src="fx-knob-' + (o.cap || K.knob) + '.svg" alt="" style="' + at(pad, pad, d - 2 * pad, d - 2 * pad) + '"></div>';
        const g = o.gap ?? 10, ls = o.lsize || 14;
        if (!o.noLabel) h += T(x - 60, y + d + g, d + 120, L(key, o.label), { size: ls });
        if (!o.noValue) h += T(x - 60, y + d + g + ls + 7, d + 120, '', { size: o.vsize || 16, weight: 700, color: 'var(--fink)', val: key });
        return h;
      },
      // hairline vertical fader; the handle is a short bar
      vslider(key, x, y, h, o = {}) {
        return '<div class="fxc fxv" data-key="' + key + '" data-kind="vs" data-travel="' + h + '" style="' + at(x - 20, y, 40, h) + '">'
          + '<i style="left:19px;width:2px;top:0;bottom:0;background:' + K.ink2 + ';opacity:.4"></i>'
          + '<i class="fill" style="left:18px;width:4px;bottom:0;background:' + (o.colour || K.accent) + '"></i>'
          + '<i class="mr" style="left:26px;width:3px;bottom:0;height:0;border-radius:2px;background:' + K.ink + ';opacity:.8"></i>'
          + '<b class="hd" style="top:0;left:6px;width:28px;height:8px;border-radius:2px;background:' + K.ink + '"></b></div>'
          + T(x - 70, y + h + 18, 140, L(key, o.label), { size: 14 }) + T(x - 70, y + h + 40, 140, '', { size: 16, weight: 700, color: 'var(--fink)', val: key });
      },
      // horizontal fader. 'fill' lights the track up to the handle; 'split' colours it UPPER | LOWER
      hslider(key, x, y, w, o = {}) {
        return '<div class="fxc fxh" data-key="' + key + '" data-kind="hs" data-mode="' + (o.split ? 'split' : 'fill') + '" data-travel="' + w + '" data-c1="' + (o.c1 || K.accent) + '" data-c2="' + (o.c2 || alpha(K.ink2, 0.3)) + '" style="' + at(x, y - 18, w, 36) + '">'
          + '<i class="tr" style="left:0;right:0;top:16px;height:4px;border-radius:2px"></i>'
          + '<i class="mr" style="left:0;width:0;top:24px;height:3px;border-radius:2px;background:' + K.ink + ';opacity:.8"></i>'
          + '<b class="hd" style="top:5px;margin-left:-4px;width:8px;height:26px;border-radius:2px;background:' + K.ink + '"></b></div>';
      },
      // a draggable number: value above, label below
      num(key, x, y, o = {}) {
        const w = o.w || 150, al = o.align || 'center';
        return '<div class="fxc fxn" data-key="' + key + '" data-kind="num" title="Drag up or down · double-click resets" style="' + at(x, y, w, 64) + '">'
          + T(0, 0, w, '', { size: o.size || 26, weight: 700, color: 'var(--fink)', val: key, align: al, ls: 0.02 })
          + T(0, (o.size || 26) + 10, w, L(key, o.label), { size: 14, align: al }) + '</div>';
      },
      // a stepped choice as a word: click steps on, right-click steps back
      sel(key, x, y, o = {}) {
        return '<div class="fxc fxs" data-key="' + key + '" data-kind="sel" title="Click to change, right-click to go back" style="' + at(x, y)
          + 'height:32px;line-height:32px;font-size:' + (o.size || 17) + 'px;color:' + (o.colour || K.accent) + ';' + (o.align === 'right' ? 'transform:translateX(-100%);' : '') + '"></div>';
      },
      // every option shown, the current one filled
      grid(key, x, y, cols, cw, ch, o = {}) {
        const short = o.short || ((n) => n);
        return spec(s, key).steps.map((name, i) => '<div class="fxc fxg" data-key="' + key + '" data-kind="grid" data-idx="' + i + '" style="'
          + at(x + (i % cols) * cw + 3, y + Math.floor(i / cols) * ch + 3, cw - 6, ch - 6) + 'font-size:' + (o.size || 14) + 'px">' + esc(lower(short(name))) + '</div>').join('');
      },
      // a section label on a hairline, as Somii prints its sections
      head(x, y, w, label) { return '<div class="fx-hd" style="' + at(x, y, w, 18) + '"><span>' + esc(label.toUpperCase()) + '</span><i></i></div>'; },
      // a row of knobs on n equal columns between x0 and x1
      row(keys, y, d, o = {}, n = keys.length, x0 = 24, x1 = null) {
        const cw = ((x1 ?? o.W) - x0) / n;
        return keys.filter((p) => spec(s, p)).map((p, i) => this.knob(p, x0 + i * cw + cw / 2 - d / 2, y, d, o)).join('');
      },
      screen(x, y, w, h) {
        return '<div class="fx-screen" data-fx-screen="' + s + '" style="' + at(x, y, w, h) + 'background:' + K.screen + '"><canvas id="fx' + s + '-canvas" width="' + Math.round(w * 2) + '" height="' + Math.round(h * 2) + '"></canvas><div class="fx-drop">DROP A .WAV HERE</div></div>';
      }
    };
  }

  // ---- the faces: the body between the header (64 px) and the footer (the last 120 px)
  const has = (s, p) => !!param(s, p);
  const R = (b, W) => (keys, y, d, o = {}, n, x0, x1) => b.row(keys, y, d, { ...o, W: W - 24 }, n, x0, x1);
  // the smaller knobs of a face: never under 64 px (user, 2026-10-05: knobs were too small to use)
  const small = { lsize: 13, vsize: 14, gap: 6 };
  const LAYOUTS = {
    // the theme-coloured modules: choices on top, the display, four main knobs, then the rest smaller
    grid(s, W, H, b) {
      const row = R(b, W);
      let h = ['p9', 'p10', 'p11'].filter((p) => has(s, p)).map((p, j) => b.sel(p, 24 + j * 230, 80)).join('');
      h += b.screen(24, 124, W - 48, 300);
      const ks = ['p1', 'p2', 'p3', 'p4', 'p5', 'p6', 'p7', 'p8'].filter((p) => has(s, p));
      h += row(ks.slice(0, 4), 454, 84, {}, 4);
      if (ks.length > 4) h += b.head(24, 612, W - 48, 'more') + row(ks.slice(4), 640, 68, small, 4);
      return h;
    },
    // CARVE: the eleven shapers as tabs (the dot is lit when one is on); the selected shaper's own wave fills
    // the display, then its switch, trigger and rate, curve and band, its modes, its mix and settings; the
    // crossovers, the trigger sensitivity and the output under them all
    carve(s, W, H, b) {
      const row = R(b, W), k = view.shaper[s], name = CARVE_SHAPERS[k], P = (j) => 'p' + (1 + 5 * k + j), tw = (W - 48) / 11, own = CARVE_OWN[name];
      let h = CARVE_SHAPERS.map((n, i) => '<div class="fxc fxg fx-tab" data-key="tab" data-kind="tab" data-idx="' + i + '" title="' + n.toUpperCase() + ' shaper" style="'
        + at(24 + i * tw + 2, 76, tw - 4, 42) + 'font-size:13px">' + n + '<i></i></div>').join('');
      h += b.screen(24, 126, W - 48, 210);
      h += b.T(24, 342, W - 48, 'wave: ' + CARVE_HINTS[name], { size: 12, align: 'left', ls: 0.08 });
      h += b.grid(P(0), 24, 362, 2, 64, 38) + b.grid(P(3), 164, 362, 4, 92, 38) + b.sel(P(2), W - 24, 364, { align: 'right', size: 17 });
      h += ['steps', 'lines', 'smooth'].map((c, i) => '<div class="fxc fxg" data-key="xcurve" data-kind="xcurve" data-idx="' + i + '" style="' + at(24 + i * 84 + 3, 409, 78, 32) + 'font-size:14px">' + c + '</div>').join('');
      h += b.T(W - 24 - 4 * 72 - 70, 416, 62, 'band', { size: 13, align: 'right', ls: 0.14 }) + b.grid(P(4), W - 24 - 4 * 72, 406, 4, 72, 38);
      if (own.sels.length) {   // its modes, every option in view
        const cells = own.sels.reduce((a, p) => a + spec(s, p).steps.length, 0), cw = Math.min(110, (W - 48 - 16 * (own.sels.length - 1)) / cells);
        let x = 24;
        for (const p of own.sels) { h += b.grid(p, x, 452, spec(s, p).steps.length, cw, 38); x += spec(s, p).steps.length * cw + 16; }
      }
      h += b.head(24, 504, W - 48, name + ' · mix and settings');
      h += row([P(1), ...own.knobs], 530, 64, small, 6);
      h += b.head(24, 650, W - 48, 'carve');
      h += row(['p86', 'p87', 'p88', 'p89'], 676, 64, small, 6);
      return h;
    },
    // POISE: the balance it leans towards and what it is doing about it, the four TONE handles on the
    // graph (their readings under it); AMOUNT big, then its timing, squash and trim; the channel and DELTA keys
    poise(s, W, H, b) {
      const row = R(b, W), cw = (W - 48) / 4;
      let h = b.screen(24, 80, W - 48, 360);
      [['p12', 'p13'], ['p14', 'p15'], ['p16', 'p17'], ['p18', 'p19']].forEach(([f, g], i) => {
        h += b.T(24 + i * cw, 452, cw, lower(spec(s, g).label), { size: 13, ls: 0.12 });
        h += b.T(24 + i * cw, 472, cw / 2 - 4, '', { size: 15, weight: 700, color: 'var(--fink)', val: f, align: 'right' }) + b.T(24 + i * cw + cw / 2 + 4, 472, cw / 2 - 4, '', { size: 15, weight: 700, color: b.K.accent, val: g, align: 'left' });
      });
      h += b.knob('p1', 52, 528, 136, { cap: b.K.hero });
      h += row(['p2', 'p3', 'p4', 'p5'], 540, 72, {}, 4, 220);
      h += b.grid('p9', 220, 712, 2, 150, 42, { short: (n) => n.replace(' / ', '/') }) + b.grid('p10', W - 24 - 2 * 150, 712, 2, 150, 42, { short: (n) => n.replace('DELTA ', 'delta ') });
      return h;
    },
    // RIFT: the portal is the pad (SCATTER across, BLOOM up); the scale, the grain shape and FREEZE; the
    // grains, the delay and the space on one row; stereo and tone under them
    rift(s, W, H, b) {
      const row = R(b, W), q = (W - 48) / 4;
      let h = b.screen(24, 80, W - 48, 290);
      h += b.grid('p9', 24, 382, 6, (W - 48) / 6, 40);
      h += b.grid('p10', 24, 428, 4, 104, 40) + b.grid('p17', W - 24 - 2 * 116, 428, 2, 116, 40);
      h += b.head(24, 484, (W - 48) * 5 / 8 - 12, 'grains') + b.head(24 + (W - 48) * 5 / 8 + 12, 484, (W - 48) * 3 / 8 - 12, 'delay · space');
      h += row(['p1', 'p2', 'p3', 'p4', 'p14', 'p6', 'p5', 'p8'], 510, 64, small, 8);
      h += b.head(24, 630, W - 48, 'stereo · tone');
      h += row(['p7', 'p15', 'p16'], 656, 64, small, 4) + b.grid('p11', 24 + 3 * q + 10, 668, 2, (q - 20) / 2, 40);
      return h;
    },
    // SPACES: the echogram of the space; DECAY big, then the shape, diffusion, movement and tone
    spaces(s, W, H, b) {
      const row = R(b, W);
      let h = b.sel('p9', 24, 82, { size: 22, colour: b.K.ink }) + b.grid('p10', W - 24 - 3 * 116, 78, 3, 116, 42);
      h += b.screen(24, 132, W - 48, 250);
      h += b.knob('p2', 44, 408, 136, { cap: b.K.hero });
      h += row(['p1', 'p3', 'p4'], 428, 76, {}, 3, 220);
      h += b.head(24, 606, (W - 60) / 2, 'diffusion · motion') + b.head(W / 2 + 6, 606, (W - 60) / 2, 'tone');
      h += row(['p5', 'p6', 'p7', 'p8', 'p12', 'p13', 'p14', 'p15'], 634, 66, small, 8);
      return h;
    },
    // MARBLE: the painting it makes of the sound, then drive and its colour, the delay and the gain stage
    marble(s, W, H, b) {
      const row = R(b, W);
      let h = b.grid('p9', 24, 78, 4, 100, 42) + b.grid('p10', W - 24 - 2 * 130, 78, 2, 130, 42);
      h += b.screen(24, 132, W - 48, 300);
      h += b.knob('p1', 44, 456, 112, { cap: b.K.hero });
      h += row(['p2', 'p3', 'p7', 'p8'], 466, 68, {}, 4, 190);
      h += b.head(24, 632, (W - 60) * 3 / 7, 'delay') + b.head(24 + (W - 48) * 3 / 7 + 12, 632, (W - 60) * 4 / 7, 'gain stage');
      h += row(['p4', 'p5', 'p6', 'p12', 'p13', 'p14', 'p15'], 660, 68, small, 7);
      return h;
    },
    // VALVE: the valve itself lights with the drive; GAIN big, LEVEL and OUTPUT, the tone and input
    valve(s, W, H, b) {
      const row = R(b, W);
      let h = b.screen(24, 80, W - 48, 250);
      h += b.knob('p1', 52, 356, 150, { cap: b.K.hero });
      h += b.knob('p2', W / 2 - 50, 372, 100) + b.knob('p5', W - 52 - 96, 376, 96);
      h += b.head(24, 606, W / 2 - 36, 'tone') + row(['p3', 'p4'], 636, 72, small, 2, 24, W / 2 - 12);
      h += b.head(W / 2 + 12, 606, W / 2 - 36, 'input');
      h += b.grid('p9', W / 2 + 12, 638, 2, (W / 2 - 36) / 2, 46) + b.grid('p10', W / 2 + 12, 694, 2, (W / 2 - 36) / 2, 46);
      return h;
    },
    // HEAT: three bands on the display (drag a line to move a crossover, a band to heat it); the
    // selected band's DRIVE big; style, dynamics, tone, output
    heat(s, W, H, b) {
      const row = R(b, W);
      let h = b.screen(24, 80, W - 48, 330);
      SAT_BANDS.forEach((name, i) => { h += '<div class="fxc fxs" data-key="sband" data-kind="sband" data-idx="' + i + '" style="' + at(24 + i * 92, 426) + 'height:32px;line-height:32px;font-size:16px">' + lower(name) + ' band</div>'; });
      h += b.grid('p9', 24, 468, 7, (W - 48) / 7, 42);
      h += b.knob('sdrive', 44, 546, 140, { label: 'drive', cap: b.K.hero });
      h += row(['p6', 'p7', 'p8'], 566, 74, {}, 3, 230);
      return h;
    },
    // PUMP: gain reduction on top, seven faders
    pump(s, W, H, b) {
      let h = b.screen(24, 80, W - 48, 250);
      const ks = ['p1', 'p2', 'p3', 'p4', 'p5', 'p6', 'p7'], cw = (W - 48) / ks.length;
      ks.forEach((p, i) => { h += b.vslider(p, 24 + i * cw + cw / 2, 376, 250); });
      return h;
    },
    // CEILING: gain reduction and the output against the ceiling; eight knobs; AUTO GAIN
    ceiling(s, W, H, b) {
      const row = R(b, W);
      let h = b.screen(24, 80, W - 48, 260);
      h += b.grid('p9', W - 24 - 2 * 150, 352, 2, 150, 42, { short: (n) => n.replace('AUTO GAIN ', 'auto ') });
      h += row(['p1', 'p2', 'p3', 'p4'], 420, 88, {}, 4) + row(['p5', 'p6', 'p7', 'p8'], 618, 72, small, 4);
      return h;
    },
    // PRISM: one stream of grains; the pitch as a keyboard of intervals; shape, fine, then the stream's settings
    prism(s, W, H, b) {
      const row = R(b, W);
      let h = b.screen(24, 80, W - 48, 270);
      h += b.head(24, 370, W - 48, 'pitch');
      h += b.grid('p9', 24, 396, 9, (W - 48) / 9, 46, { size: 16 });
      h += b.grid('p10', 24, 452, 4, 118, 40) + b.num('p11', W - 24 - 170, 446, { w: 170, size: 22, align: 'right' });
      h += row(['p1', 'p2', 'p3', 'p4', 'p5', 'p6', 'p7', 'p8'], 560, 66, small, 8);
      return h;
    },
    // PARLOUR: the plate ringing; the preamp and the room on one line; the post EQ under it
    parlour(s, W, H, b) {
      const row = R(b, W), cw = (W - 48) / 5, at5 = (i, d) => 24 + i * cw + cw / 2 - d / 2;
      let h = b.screen(24, 80, W - 48, 250);
      h += b.head(24, 352, cw * 2 - 12, 'preamp') + b.head(24 + cw * 2 + 12, 352, cw * 3 - 12, 'room');
      h += b.knob('p1', at5(0, 100), 384, 100, { cap: b.K.hero }) + b.knob('p2', at5(1, 64), 402, 64);
      h += b.knob('p4', at5(2, 64), 402, 64) + b.knob('p5', at5(3, 100), 384, 100, { cap: b.K.hero }) + b.knob('p3', at5(4, 64), 402, 64);
      h += b.head(24, 588, W - 48, 'post eq');
      h += row(['p6', 'p7', 'p8', 'p12'], 616, 72, small, 4);
      return h;
    },
    // CONTOUR: the graph fills the module; the selected band's controls on a bar underneath
    eq(s, W, H, b) {
      let h = b.screen(0, 64, W, 560);
      h += '<div style="' + at(20, 640, W - 40, 150) + 'border-radius:8px;background:color-mix(in srgb,var(--fbg),#000 12%)"></div>';
      h += '<div id="fx' + s + '-pqbar" class="fx-sub" style="left:0;top:0">' + pqBar(s, W, b) + '</div>';
      return h;
    }
  };

  // CONTOUR's bar: the selected band's shape and FREQ / GAIN / Q, and OUTPUT
  function pqBar(s, W, b) {
    const on = Math.round(val(s, 'p27')), sel = view.band[s] >= 0 && (on & (1 << view.band[s])), acc = b.K.accent, Y = 656;
    let h = '';
    if (sel) {
      const gain = usesGain(val(s, PQ_SHAPE[view.band[s]]));
      h += '<div style="' + at(46, Y + 10, 14, 14) + 'border-radius:50%;background:' + acc + '"></div>';
      h += b.T(70, Y + 4, 160, '', { size: 18, weight: 700, color: 'var(--fink)', align: 'left', val: 'bandname' });
      h += b.sel('band3', 46, Y + 38, { size: 16, colour: acc });
      h += '<div style="' + at(270, Y - 4, 1, 126) + 'background:' + b.K.ink2 + ';opacity:.3"></div>';
      ['band0', 'band1', 'band2'].forEach((k, i) => { h += '<div class="fx-sub" style="position:absolute;left:0;top:0;' + (i === 1 && !gain ? 'opacity:.3;pointer-events:none' : '') + '">' + b.knob(k, 310 + i * 140, Y, 68, { label: ['FREQ', 'GAIN', 'Q'][i], ...small }) + '</div>'; });
    } else h += b.T(46, Y + 14, 560, on ? 'click a band to edit it' : 'double-click the graph to add a band', { size: 18, align: 'left', color: 'var(--fink)' })
      + b.T(46, Y + 48, 560, 'drag to move · wheel for Q · right-click for its shape · double-click to remove', { size: 13, align: 'left' });
    h += '<div style="' + at(W - 190, Y - 4, 1, 126) + 'background:' + b.K.ink2 + ';opacity:.3"></div>';
    return h + b.knob('p4', W - 136, Y, 68, small);
  }
  function pqRefresh(s) {
    const bar = $('fx' + s + '-pqbar'), fr = $('fx' + s + '-frame');
    if (bar && fr) bar.innerHTML = pqBar(s, +fr.dataset.w, builders(s));
    paint(s);
  }
  // ---- rendering a slot
  function render(s, animate) {
    const fr = $('fx' + s + '-frame'); if (!fr) return;
    const d = def(s), K = skin(s), W = +fr.dataset.w, H = +fr.dataset.h, b = builders(s), t = type(s);
    for (const [k, v] of [['--fbg', K.bg], ['--fink', K.ink], ['--fink2', K.ink2], ['--facc', K.accent]]) fr.style.setProperty(k, v);
    const FY = H - 120;
    let body = '';
    if (d) body = LAYOUTS[K.layout](s, W, FY, b);
    // empty slot: the affordance is the thing you click
    else body = '<div class="fx-empty" data-fx-open="' + s + '" title="Choose the effect for this slot" style="' + at(40, 100, W - 80, FY - 200) + '"><b>+</b>CHOOSE AN EFFECT<span style="font-size:14px;font-weight:600;letter-spacing:.1em">OR DROP ONE HERE FROM ANOTHER SLOT</span></div>';
    // the face first: it spans the whole frame, so the header and footer must be drawn over it to stay clickable
    let h = '<div class="fx-body">' + body + '</div><div class="fx-head"></div>';
    h += '<div class="fx-grip" data-fx-grip="' + s + '" title="Drag onto another slot to swap their order">' + GRIP + '</div>';
    h += '<div class="fxc fxp" data-key="on" data-kind="pow" title="On / bypass" style="' + at(44, 16, 32, 32) + '"><i></i></div>';
    h += '<div class="fx-name" data-fx-open="' + s + '" title="Choose the effect for this slot">' + esc(d ? d.name : 'EMPTY SLOT') + CHEVRON + '</div>';
    if (d) h += '<div class="fx-code fx-t">' + esc(d.group.replace(' / ', '/') + '  ·  ' + String(t).padStart(2, '0')) + '</div><i class="fx-led" data-led="' + s + '"></i>';
    // footer, the same on every module: DRY / WET on the left, LAYER MIX on the right (middle = both layers fully)
    const upper = lum(K.bg) > 0.5 ? K.ink : cssVar(UPPER_COLOUR), lowerC = K.lower || cssVar(LOWER_COLOUR), half = W / 2;
    h += '<div class="fx-foot" style="' + at(0, FY, W, 120) + '"></div>';
    if (d) {
      h += b.T(24, FY + 24, 200, 'dry / wet', { size: 13, align: 'left', ls: 0.12 }) + b.T(half - 160, FY + 22, 136, '', { size: 16, weight: 700, color: 'var(--fink)', align: 'right', val: 'mix' });
      h += b.hslider('mix', 24, FY + 74, half - 48);
    }
    h += b.T(half + 12, FY + 24, 200, 'layer mix', { size: 13, align: 'left', ls: 0.12 }) + b.T(W - 24 - 300, FY + 24, 300, '', { size: 13, weight: 700, color: 'var(--fink)', align: 'right', val: 'layer' });
    h += b.hslider('layer', half + 12, FY + 74, half - 36, { split: true, c1: upper, c2: lowerC });
    h += b.T(half + 12, FY + 94, 120, 'upper', { size: 12, align: 'left', color: upper, weight: 700 }) + b.T(W - 24 - 120, FY + 94, 120, 'lower', { size: 12, align: 'right', color: lowerC, weight: 700 });
    fr.innerHTML = h;
    if (animate) { fr.classList.remove('fx-in'); void fr.offsetWidth; fr.classList.add('fx-in'); }
    view.built[s] = t;
  }
  function paint(s) {
    const fr = $('fx' + s + '-frame'); if (!fr) return;
    const K = skin(s);
    for (const el of fr.querySelectorAll('[data-kind]')) {
      if (el.dataset.kind === 'sband') { const on = +el.dataset.idx === view.sband[s]; el.style.color = on ? K.accent : K.ink2; el.style.textDecoration = on ? 'underline' : 'none'; continue; }
      if (el.dataset.kind === 'tab' || el.dataset.kind === 'xcurve') {   // CARVE's shaper tabs and curve keys
        const i = +el.dataset.idx, on = el.dataset.kind === 'tab' ? i === view.shaper[s] : i === Math.round(carvePt(s, view.shaper[s], 16));
        if (el.dataset.kind === 'tab') { el.classList.toggle('cur', on); el.classList.toggle('lit', val(s, 'p' + (1 + 5 * i)) === 1); }
        el.style.background = on ? K.accent : ''; el.style.color = on ? K.bg : K.ink; el.style.boxShadow = on ? 'none' : '';
        continue;
      }
      const key = el.dataset.key, q = spec(s, key); if (!q) continue;
      const n = norm(s, key), mr = modRange(s, key);
      el.classList.toggle('modded', !!mr);
      switch (el.dataset.kind) {
        case 'knob': {
          const cap = el.querySelector('.cap'), v = el.querySelector('.val'), ang = A0 + (A1 - A0) * n;
          if (cap) cap.style.transform = 'rotate(' + ang.toFixed(1) + 'deg)';
          const bip = !q.steps && q.lo < 0 && q.hi > 0 && Math.abs(q.lo + q.hi) < 1e-6;   // symmetric ranges grow from 12 o'clock
          if (v) v.setAttribute('d', bip ? arcPath(Math.min(0, ang), Math.max(0.01, ang)) : arcPath(A0, Math.max(A0 + 0.5, ang)));
          // modulation: the reachable range inside the value arc, and a dot where the plugin has it now
          const mp = el.querySelector('.mod'), md = el.querySelector('.mdot'), angOf = (x) => A0 + (A1 - A0) * clamp01(x);
          if (mp) { if (mr) mp.setAttribute('d', arcPath(angOf(n + mr[0]), Math.max(angOf(n + mr[0]) + 0.5, angOf(n + mr[1])), 37)); else mp.removeAttribute('d'); }
          const now = mr ? view.modNow[s].get(targetOf(s, key)) : undefined;
          if (md) { if (now != null) { const a = angOf(n + now) * Math.PI / 180; md.setAttribute('cx', (50 + Math.sin(a) * 37).toFixed(1)); md.setAttribute('cy', (50 - Math.cos(a) * 37).toFixed(1)); md.setAttribute('r', '5'); } else md.setAttribute('r', '0'); }
          break;
        }
        case 'vs': {
          const tr = +el.dataset.travel, hd = el.querySelector('.hd'), fill = el.querySelector('.fill');
          if (hd) hd.style.top = ((1 - n) * tr - 4) + 'px';
          if (fill) fill.style.height = (n * tr) + 'px';
          const mrEl = el.querySelector('.mr');
          if (mrEl) { mrEl.style.bottom = (mr ? clamp01(n + mr[0]) * tr : 0) + 'px'; mrEl.style.height = (mr ? (clamp01(n + mr[1]) - clamp01(n + mr[0])) * tr : 0) + 'px'; }
          break;
        }
        case 'hs': {
          const hd = el.querySelector('.hd'), tr = el.querySelector('.tr'), pc = (n * 100).toFixed(2) + '%';
          if (hd) hd.style.left = (n * +el.dataset.travel) + 'px';
          // two solid colours meeting at the handle - not a blend
          if (tr) tr.style.background = 'linear-gradient(90deg,' + el.dataset.c1 + ' 0 ' + pc + ',' + el.dataset.c2 + ' ' + pc + ' 100%)';
          const mrEl = el.querySelector('.mr'), trav = +el.dataset.travel;
          if (mrEl) { mrEl.style.left = (mr ? clamp01(n + mr[0]) * trav : 0) + 'px'; mrEl.style.width = (mr ? (clamp01(n + mr[1]) - clamp01(n + mr[0])) * trav : 0) + 'px'; }
          break;
        }
        case 'sel': { const t = lower(q.steps[from(q, n)]); el.innerHTML = esc(t) + ' <span style="display:inline-block;vertical-align:-2px">' + CHEVRON + '</span>'; break; }
        case 'grid': { const on = +el.dataset.idx === from(q, n); el.style.background = on ? K.accent : ''; el.style.color = on ? K.bg : K.ink; el.style.boxShadow = on ? 'none' : ''; break; }
        case 'pow': el.classList.toggle('on', n >= 0.5); fr.classList.toggle('bypassed', !!def(s) && n < 0.5); break;
      }
    }
    for (const el of fr.querySelectorAll('[data-val]'))
      el.textContent = el.dataset.val === 'bandname' ? 'BAND ' + (pqBand(s) + 1) : readout(s, el.dataset.val);
  }

  // ---- the routing column and the slots
  function refresh(animate) {
    const serial = store.read('fx.mode') < 0.5;
    $('pop-fx').classList.toggle('fx-mode-serial', serial); $('pop-fx').classList.toggle('fx-mode-parallel', !serial);
    actionState('act-fxSerial', serial); actionState('act-fxParallel', !serial);
    text('fx-mode-note', (serial ? 'The sound runs through FX 1, then FX 2, then FX 3.' : 'Each slot hears the dry sound; the three results are mixed.') + ' Drag an effect onto another to swap them.');
    for (let s = 0; s < 3; s++) {
      text('fx-route-name' + s, def(s)?.name || 'EMPTY');
      if (view.built[s] !== type(s)) render(s, animate);
      paint(s);
    }
  }

  // ---- type picker
  function choose(s, t) {
    oneWrite(pid(s, 'type'), t / (TYPES - 1));
    const d = t > 0 ? FX[t - 1] : null;
    if (d) {   // the processor resets the slot too; doing it here keeps the page right without a host
      for (const q of d.params) oneWrite(pid(s, q.p), to(q, q.def));
      oneWrite(pid(s, 'mix'), d.mix / 100);
    }
    view.band[s] = -1; view.sband[s] = 1; view.hist[s] = []; view.grains[s] = [];
    // a new effect: its own extra values and no modulation (the processor does the same)
    view.ext[s] = Array.from({ length: FX_EXT }, (_, i) => d?.ext?.[i] ?? 0); view.extDirty[s].clear();
    view.mods[s] = new Map(); view.modNow[s] = new Map(); view.shaper[s] = 8;
    refresh(true);
  }
  function openPicker(s) {
    view.open = s; text('fx-picker-title', 'FX ' + (s + 1) + ' · CHOOSE AN EFFECT'); $('fx-picker').hidden = false;
    all('[data-fx-tile]').forEach((t) => t.classList.toggle('sel', +t.dataset.fxTile === type(s)));
  }
  all('[data-fx-tile]').forEach((el) => el.addEventListener('click', () => { if (view.open >= 0) choose(view.open, +el.dataset.fxTile); $('fx-picker').hidden = true; view.open = -1; }));
  all('[data-fx-close]').forEach((el) => el.addEventListener('click', () => { $('fx-picker').hidden = true; view.open = -1; }));

  // ---- swapping: drag FX n (its routing box or its slot's grip) onto another
  const SLOT_PARAMS = ['type', 'on', 'mix', 'layer', ...Array.from({ length: NP }, (_, i) => 'p' + (i + 1))];
  async function swap(a, b) {
    if (a === b) return;
    if (hosted) await native('fxSwap', a, b);   // the processor swaps without resetting to defaults
    else for (const p of SLOT_PARAMS) { const va = store.read(pid(a, p)), vb = store.read(pid(b, p)); oneWrite(pid(a, p), vb); oneWrite(pid(b, p), va); }
    for (const k of ['band', 'sband', 'hist', 'grains', 'ext', 'mods', 'modNow', 'shaper', 'extDirty']) [view[k][a], view[k][b]] = [view[k][b], view[k][a]];
    view.built = [-1, -1, -1];
    refresh(true);
  }
  let moving = null;
  const dropAt = (e) => document.elementFromPoint(e.clientX, e.clientY)?.closest('[data-fx-drop]');
  const clearOver = () => all('.fx-over').forEach((el) => el.classList.remove('fx-over'));
  document.addEventListener('pointerdown', (e) => {
    if (e.button) return;
    const g = e.target.closest?.('[data-fx-grip]');
    let from = g ? +g.dataset.fxGrip : -1;
    // a window's own background also picks it up (not its controls, graph, name or menus)
    const frame = e.target.closest?.('.fx-frame');
    if (from < 0 && frame && !e.target.closest('[data-kind],.fx-screen,[data-fx-open],.fx-menu,.fx-mod')) from = +frame.id.slice(2, 3);
    if (from < 0) return;
    e.preventDefault(); e.stopPropagation();
    moving = { from, x: e.clientX, y: e.clientY, live: !!g };
    if (moving.live) startMove();
  }, true);
  function startMove() { moving.live = true; $('fx' + moving.from + '-frame')?.classList.add('fx-dragging'); document.body.style.cursor = 'grabbing'; }
  document.addEventListener('pointermove', (e) => {
    if (!moving) return;
    if (!moving.live) { if (Math.hypot(e.clientX - moving.x, e.clientY - moving.y) < 8) return; startMove(); }
    const t = dropAt(e); clearOver();
    if (t && +t.dataset.fxDrop !== moving.from) { t.classList.add('fx-over'); $('fx' + t.dataset.fxDrop + '-frame')?.classList.add('fx-over'); }
  });
  document.addEventListener('pointerup', (e) => {
    if (!moving) return;
    const t = moving.live ? dropAt(e) : null, a = moving.from;
    moving = null; document.body.style.cursor = ''; clearOver();
    all('.fx-dragging').forEach((el) => el.classList.remove('fx-dragging'));
    if (t) swap(a, +t.dataset.fxDrop).catch(report);
  });

  // ---- controls inside the frames: one delegated handler per frame
  const FMIN = 20, FMAX = 20000;
  const xOfF = (f, w) => Math.log(f / FMIN) / Math.log(FMAX / FMIN) * w;
  const fOfX = (x, w) => FMIN * Math.pow(FMAX / FMIN, Math.max(0, Math.min(1, x / w)));
  const clamp01 = (v) => Math.max(0, Math.min(1, v));
  const quant = (q, n) => q?.steps ? Math.round(clamp01(n) * (q.steps.length - 1)) / (q.steps.length - 1) : clamp01(n);
  function setReal(s, p, v) { const q = param(s, p); if (q) store.write(pid(s, p), clamp01(to(q, Math.max(q.lo, Math.min(q.hi, v))))); }
  function stepKey(s, key, dir) { const q = spec(s, key); if (!q?.steps) return; const K = q.steps.length, i = from(q, norm(s, key)); oneWrite(pid(s, keyP(s, key)), ((i + dir + K) % K) / (K - 1)); }
  const eqY = (db) => 0.5 - db / 24 * 0.42;
  // CONTOUR band helpers
  function pqHit(s, u, v) {
    const on = Math.round(val(s, 'p27')); let best = -1, bd = 0.05;
    BANDS.forEach((bb, i) => { if (!(on & (1 << i))) return; const du = Math.abs(xOfF(val(s, bb[0]), 1) - u), dv = Math.abs(eqY(usesGain(val(s, PQ_SHAPE[i])) ? val(s, bb[1]) : 0) - v); if (du + dv * 0.5 < bd) { bd = du + dv * 0.5; best = i; } });
    return best;
  }
  function pqAdd(s, u, v) {
    const on = Math.round(val(s, 'p27')); let b = 0; while (b < 6 && (on & (1 << b))) b++;
    if (b === 6) { report('All six bands are in use - remove one first'); return; }
    const f = fOfX(u, 1), shape = f < 40 ? 3 : f > 14000 ? 6 : 0;   // a band dropped at the very ends starts as a cut
    const pq = param(s, PQ_SHAPE[b]);
    oneWrite(pid(s, PQ_SHAPE[b]), shape / (pq.steps.length - 1));
    oneWrite(pid(s, BANDS[b][0]), clamp01(to(param(s, BANDS[b][0]), f)));
    oneWrite(pid(s, BANDS[b][1]), clamp01(to(param(s, BANDS[b][1]), shape ? 0 : Math.max(-24, Math.min(24, (0.5 - v) / 0.42 * 24)))));
    oneWrite(pid(s, BANDS[b][2]), clamp01(to(param(s, BANDS[b][2]), shape ? 0.71 : 1)));
    oneWrite(pid(s, 'p27'), (on | (1 << b)) / 63);
    view.band[s] = b; pqRefresh(s);
  }
  function pqRemove(s, b) {
    oneWrite(pid(s, 'p27'), (Math.round(val(s, 'p27')) & ~(1 << b)) / 63);
    view.band[s] = -1; pqRefresh(s);
  }
  function pqMenu(s, fr, e, b) {
    fr.querySelector('.fx-menu')?.remove();
    const r = fr.getBoundingClientRect(), z = r.width / fr.offsetWidth || 1, q = param(s, PQ_SHAPE[b]), cur = val(s, PQ_SHAPE[b]);
    const m = document.createElement('div'); m.className = 'fx-menu';
    m.style.left = Math.min(fr.offsetWidth - 210, (e.clientX - r.left) / z) + 'px'; m.style.top = Math.min(fr.offsetHeight - 440, (e.clientY - r.top) / z) + 'px';
    m.innerHTML = q.steps.map((n, i) => '<div data-shape="' + i + '"' + (i === cur ? ' style="color:' + skin(s).accent + '"' : '') + '>' + n.toLowerCase() + '</div>').join('') + '<div class="sep"></div><div data-del="1">delete band</div>';
    m.addEventListener('pointerdown', (ev) => {
      ev.stopPropagation(); const it = ev.target.closest('[data-shape],[data-del]'); if (!it) return;
      if (it.dataset.del) pqRemove(s, b); else { oneWrite(pid(s, PQ_SHAPE[b]), +it.dataset.shape / (q.steps.length - 1)); pqRefresh(s); }
      m.remove();
    });
    fr.appendChild(m);
    setTimeout(() => document.addEventListener('pointerdown', () => m.remove(), { once: true }), 0);
  }
  // CARVE: shaper k's wave lives in the slot's extra values (16 points at k*17, the curve at k*17+16), read
  // the way src/plugin/FxUnits.cpp reads it; the shapes the menu draws; each shaper's own controls
  const CARVE_OWN = {
    pitch: { knobs: ['p56'], sels: ['p57'] }, reverb: { knobs: ['p58', 'p59', 'p60'], sels: [] }, time: { knobs: ['p62'], sels: ['p61', 'p63'] },
    drive: { knobs: ['p64', 'p66'], sels: ['p65'] }, noise: { knobs: ['p67', 'p70'], sels: ['p68', 'p69'] }, liquid: { knobs: ['p72', 'p73', 'p74'], sels: ['p71'] },
    filter: { knobs: ['p76', 'p77', 'p78', 'p79'], sels: ['p75'] }, crush: { knobs: ['p80', 'p81', 'p82'], sels: [] },
    volume: { knobs: ['p83'], sels: [] }, pan: { knobs: ['p84'], sels: [] }, width: { knobs: ['p85'], sels: [] } };
  const carvePt = (s, k, i) => view.ext[s][k * 17 + i] ?? 0;
  function carveAt(s, k, ph) {
    const x = Math.max(0, Math.min(0.99999, ph)) * 16, i = Math.floor(x), f = x - i, a = carvePt(s, k, i), b = carvePt(s, k, (i + 1) & 15), curve = Math.round(carvePt(s, k, 16));
    return curve === 0 ? a : a + (b - a) * (curve === 1 ? f : 0.5 - 0.5 * Math.cos(Math.PI * f));
  }
  // writes to the extra values: shown at once, sent to the plugin once per frame per shaper
  function setExt(s, i, v) { view.ext[s][i] = v; view.extDirty[s].add(Math.floor(i / 17)); }
  function flushExt() {
    for (let s = 0; s < 3; s++) {
      if (!view.extDirty[s].size) continue;
      if (hosted) for (const k of view.extDirty[s]) native('fxExt', s, k * 17, view.ext[s].slice(k * 17, k * 17 + 17)).catch(report);
      view.extDirty[s].clear();
    }
  }
  const PUMP = [0, 0.32, 0.56, 0.72, 0.82, 0.89, 0.93, 0.96, 0.98, 1, 1, 1, 1, 1, 1, 1];
  const CARVE_SHAPES = {
    'pump': (i) => PUMP[i], 'ramp up': (i) => i / 15, 'ramp down': (i) => 1 - i / 15,
    'sine': (i) => 0.5 - 0.5 * Math.cos(2 * Math.PI * i / 16), 'triangle': (i) => 1 - Math.abs(i - 8) / 8,
    'square': (i) => (i < 8 ? 1 : 0), 'gate 1/16': (i) => (i % 2 ? 0 : 1), 'stairs': (i) => Math.floor(i / 4) / 3,
    'chop': (i) => [1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0][i], 'random': () => Math.round(Math.random() * 8) / 8,
    'flat middle': () => 0.5, 'flat top': () => 1
  };
  function carveMenu(s, fr, e) {
    fr.querySelector('.fx-menu')?.remove();
    const r = fr.getBoundingClientRect(), z = r.width / fr.offsetWidth || 1, k = view.shaper[s];
    const m = document.createElement('div'); m.className = 'fx-menu';
    m.style.left = Math.min(fr.offsetWidth - 210, (e.clientX - r.left) / z) + 'px'; m.style.top = Math.min(fr.offsetHeight - 470, (e.clientY - r.top) / z) + 'px';
    m.innerHTML = Object.keys(CARVE_SHAPES).map((n) => '<div data-shape="' + n + '">' + n + '</div>').join('');
    m.addEventListener('pointerdown', (ev) => {
      ev.stopPropagation(); const it = ev.target.closest('[data-shape]'); if (!it) return;
      for (let i = 0; i < 16; i++) setExt(s, k * 17 + i, clamp01(CARVE_SHAPES[it.dataset.shape](i)));
      m.remove();
    });
    fr.appendChild(m);
    setTimeout(() => document.addEventListener('pointerdown', () => m.remove(), { once: true }), 0);
  }
  // POISE: the four TONE handles (FREQ, GAIN) and where a gain sits on the graph (±12 dB over the height)
  const TONES = [['p12', 'p13'], ['p14', 'p15'], ['p16', 'p17'], ['p18', 'p19']];
  const toneY = (db) => 0.5 - db / 12 * 0.42;
  const POISE_FC = Array.from({ length: 12 }, (_, k) => 30 * Math.pow(2, 0.82 * k));   // its band centres (FxUnits.cpp)
  // ---- FX modulation (user, 2026-10-05: right-click any control of an effect and assign it to whatever can
  // move it; kept out of the matrix page). A route adds amount x source to the control's normalised value,
  // as the matrix does; the plugin applies it per layer (src/plugin/FxRack.cpp).
  const BIPOLAR = new Set(FX_MOD_SOURCES.map((n, k) => (/^LFO|NOTE|BENDER|RANDOM/.test(n) ? k : -1)));   // the ones that swing both ways
  const targetOf = (s, key) => { const p = keyP(s, key); return p === 'mix' ? NP : /^p\d+$/.test(p) ? +p.slice(1) - 1 : -1; };
  const modOf = (s, t, k) => view.mods[s].get(t * NSRC + k) || 0;
  function setMod(s, t, k, a) {
    a = Math.round(Math.max(-1, Math.min(1, a)) * 100) / 100;
    if (a) view.mods[s].set(t * NSRC + k, a); else { view.mods[s].delete(t * NSRC + k); if (!modRange(s, t)) view.modNow[s].delete(t); }
    if (hosted) native('fxMod', s, t, k, a).catch(report);
  }
  // how far below and above its value the routed sources can take a control (null = not modulated)
  function modRange(s, keyOrT) {
    const t = typeof keyOrT === 'number' ? keyOrT : targetOf(s, keyOrT); if (t < 0) return null;
    let lo = 0, hi = 0, any = false;
    for (let k = 0; k < NSRC; k++) {
      const a = modOf(s, t, k); if (!a) continue;
      any = true; const sl = BIPOLAR.has(k) ? -a : 0;
      lo += Math.min(sl, a, 0); hi += Math.max(sl, a, 0);
    }
    return any ? [lo, hi] : null;
  }
  function modPanel(s, fr, e, key) {
    fr.querySelector('.fx-mod')?.remove(); fr.querySelector('.fx-menu')?.remove();
    const t = targetOf(s, key), q = spec(s, key), r = fr.getBoundingClientRect(), z = r.width / fr.offsetWidth || 1, PH = 64 + NSRC * 31 + 44;
    const m = document.createElement('div'); m.className = 'fx-mod';
    m.style.left = Math.max(8, Math.min(fr.offsetWidth - 428, (e.clientX - r.left) / z - 30)) + 'px';
    m.style.top = Math.max(8, Math.min(fr.offsetHeight - PH - 8, (e.clientY - r.top) / z - 30)) + 'px';
    // CARVE's own settings carry their shaper's name ("TIME RANGE"); its common ones already do ("TIME MIX")
    const own = def(s)?.id === 'carve' ? CARVE_SHAPERS.find((n) => CARVE_OWN[n].knobs.includes(keyP(s, key))) : null;
    const what = own ? own + ' ' + q.label : q.label.startsWith('DRY') ? 'dry / wet' : q.label;
    m.innerHTML = '<h4>MODULATE · ' + esc(((def(s)?.name || '') + ' · ' + what).toUpperCase()) + '</h4>'
      + FX_MOD_SOURCES.map((n, k) => '<div class="r" data-src="' + k + '" title="Drag sideways: how much ' + n + ' moves it · wheel for fine steps · double-click clears">'
        + '<span class="n">' + esc(lower(n)) + '</span><span class="t"><i></i><b></b></span><span class="v"></span></div>').join('')
      + '<div class="f"><span data-clear>clear all</span><span data-close>close</span></div>';
    const show = () => {
      m.querySelectorAll('.r').forEach((row) => {
        const a = modOf(s, t, +row.dataset.src), bar = row.querySelector('b');
        row.classList.toggle('on', a !== 0);
        bar.style.left = (50 + Math.min(0, a) * 50) + '%'; bar.style.width = Math.abs(a) * 50 + '%';
        row.querySelector('.v').textContent = a ? (a > 0 ? '+' : '') + Math.round(a * 100) + ' %' : '-';
      });
      paint(s);
    };
    let drag = null, lastDown = null;
    m.addEventListener('pointerdown', (ev) => {
      ev.stopPropagation();
      if (ev.target.closest('[data-close]')) { m.remove(); return; }
      if (ev.target.closest('[data-clear]')) { for (let k = 0; k < NSRC; k++) if (modOf(s, t, k)) setMod(s, t, k, 0); show(); return; }
      const row = ev.target.closest('.r'); if (!row || ev.button) return;
      const k = +row.dataset.src, now = performance.now();
      if (lastDown?.k === k && now - lastDown.t < 350) { setMod(s, t, k, 0); show(); lastDown = null; return; }
      lastDown = { k, t: now };
      drag = { k, x: ev.clientX, a0: modOf(s, t, k), half: row.querySelector('.t').getBoundingClientRect().width / 2 };
      try { row.setPointerCapture(ev.pointerId); } catch { /* synthetic pointer */ }
    });
    m.addEventListener('pointermove', (ev) => { if (!drag) return; setMod(s, t, drag.k, drag.a0 + (ev.clientX - drag.x) / drag.half * (ev.ctrlKey || ev.metaKey ? 0.25 : 1)); show(); });
    for (const ev of ['pointerup', 'pointercancel']) m.addEventListener(ev, () => { drag = null; });
    m.addEventListener('wheel', (ev) => { const row = ev.target.closest('.r'); if (!row) return; ev.preventDefault(); ev.stopPropagation(); const k = +row.dataset.src; setMod(s, t, k, modOf(s, t, k) - Math.sign(ev.deltaY) * (ev.ctrlKey ? 0.01 : 0.05)); show(); }, { passive: false });
    m.addEventListener('contextmenu', (ev) => { ev.preventDefault(); ev.stopPropagation(); });
    m.addEventListener('dblclick', (ev) => ev.stopPropagation());
    fr.appendChild(m); show();
    const away = (ev) => { if (!m.contains(ev.target)) { m.remove(); document.removeEventListener('pointerdown', away, true); } };
    setTimeout(() => document.addEventListener('pointerdown', away, true), 0);
  }
  const lastTap = [null, null, null], lastCtl = [null, null, null];   // double-click detection (graph / controls)
  const capture = (el, e) => { try { el.setPointerCapture(e.pointerId); } catch { /* synthetic or already released pointer */ } };
  for (let s = 0; s < 3; s++) {
    const fr = $('fx' + s + '-frame');
    let drag = null;
    fr.addEventListener('pointerdown', (e) => {
      if (e.button !== 0) return;
      if (e.target.closest('[data-fx-open]')) { openPicker(s); return; }
      const el = e.target.closest('[data-kind]'), scr = e.target.closest('.fx-screen');
      if (el) {
        const key = el.dataset.key, kind = el.dataset.kind, q = spec(s, key);
        if (kind === 'sband') { view.sband[s] = +el.dataset.idx; paint(s); return; }
        if (kind === 'tab') { view.shaper[s] = +el.dataset.idx; view.built[s] = -1; render(s); paint(s); return; }
        if (kind === 'xcurve') { setExt(s, view.shaper[s] * 17 + 16, +el.dataset.idx); paint(s); return; }
        if (!q) return;
        if (kind === 'sel') { stepKey(s, key, 1); return; }
        if (kind === 'grid') { oneWrite(pid(s, keyP(s, key)), +el.dataset.idx / (q.steps.length - 1)); return; }
        if (kind === 'pow') { oneWrite(pid(s, keyP(s, key)), norm(s, key) >= 0.5 ? 0 : 1); return; }
        const now = performance.now(), last = lastCtl[s];
        lastCtl[s] = { t: now, key };
        if (last && last.key === key && now - last.t < 400) { lastCtl[s] = null; e.preventDefault(); oneWrite(pid(s, keyP(s, key)), defNorm(s, key)); return; }
        e.preventDefault(); capture(fr, e);
        const id = pid(s, keyP(s, key)), r = el.getBoundingClientRect();
        drag = { kind, id, q, n0: norm(s, key), x: e.clientX, y: e.clientY, zoom: r.width / el.offsetWidth || 1 };
        store.begin(id);
        // sliders: the handle jumps under the pointer and then follows it exactly
        if (kind === 'vs' || kind === 'hs') {
          drag.px = +el.dataset.travel * drag.zoom;
          drag.n0 = kind === 'vs' ? 1 - (e.clientY - r.top) / r.height : (e.clientX - r.left) / r.width;
          store.write(id, quant(q, drag.n0));
        }
        return;
      }
      if (!scr) return;
      const d = def(s); if (!d) return;
      const r = scr.getBoundingClientRect(), u = (e.clientX - r.left) / r.width, v = (e.clientY - r.top) / r.height;
      if (d.id === 'proq') {
        const best = pqHit(s, u, v), now = performance.now(), last = lastTap[s];
        lastTap[s] = { t: now, u, v };
        if (last && now - last.t < 400 && Math.abs(u - last.u) < 0.02 && Math.abs(v - last.v) < 0.03) {   // a double-click
          lastTap[s] = null; e.preventDefault();
          if (best >= 0) pqRemove(s, best); else pqAdd(s, u, v);
          return;
        }
        if (view.band[s] !== best) { view.band[s] = best; pqRefresh(s); }
        if (best < 0) return;
        drag = { kind: 'eq', b: best, scr };
      } else if (d.id === 'mbcomp') {
        const a = xOfF(val(s, 'p12'), 1), bb = xOfF(val(s, 'p13'), 1);
        drag = { kind: 'xo', p: Math.abs(u - a) < Math.abs(u - bb) ? 'p12' : 'p13', scr };
      } else if (d.id === 'saturn') {   // a crossover line moves; anywhere else picks the band and drags its drive
        const a = xOfF(val(s, 'p1'), 1), bb = xOfF(val(s, 'p2'), 1);
        if (Math.abs(u - a) < 0.025) drag = { kind: 'xo', p: 'p1', scr };
        else if (Math.abs(u - bb) < 0.025) drag = { kind: 'xo', p: 'p2', scr };
        else { view.sband[s] = u < a ? 0 : u < bb ? 1 : 2; paint(s); drag = { kind: 'sd', b: view.sband[s], scr }; }
      } else if (d.id === 'rift') drag = { kind: 'xy', scr };
      else if (d.id === 'carve') drag = { kind: 'draw', scr, last: -1 };
      else if (d.id === 'poise') {   // the nearest TONE handle follows the pointer
        let best = 0, bd = 1e9;
        TONES.forEach(([f, g], i) => { const dd = Math.abs(xOfF(val(s, f), 1) - u) + Math.abs(toneY(val(s, g)) - v) * 0.4; if (dd < bd) { bd = dd; best = i; } });
        drag = { kind: 'tone', h: best, scr };
      } else return;
      e.preventDefault(); capture(fr, e);
      move(e);
    });
    function move(e) {
      if (!drag) return;
      if (drag.scr) {
        const r = drag.scr.getBoundingClientRect(), u = (e.clientX - r.left) / r.width, v = (e.clientY - r.top) / r.height;
        if (drag.kind === 'eq') { const bb = BANDS[drag.b]; setReal(s, bb[0], fOfX(u, 1)); if (usesGain(val(s, PQ_SHAPE[drag.b]))) setReal(s, bb[1], (0.5 - v) / 0.42 * 24); }
        else if (drag.kind === 'xo') setReal(s, drag.p, fOfX(u, 1));
        else if (drag.kind === 'sd') setReal(s, 'p' + (3 + drag.b), ((1 - v) - 0.14) / 0.66 * 36);
        else if (drag.kind === 'draw') {   // CARVE: paint the points under the pointer, filling any it skipped
          const i = Math.max(0, Math.min(15, Math.floor(u * 16))), y = clamp01(1 - (v - 0.06) / 0.88), from = drag.last < 0 ? i : drag.last;
          const y0 = drag.last < 0 ? y : drag.y;
          const sk = view.shaper[s];
          for (let k = Math.min(from, i); k <= Math.max(from, i); k++) setExt(s, sk * 17 + k, from === i ? y : y0 + (y - y0) * (k - from) / (i - from));
          drag.last = i; drag.y = y;
        } else if (drag.kind === 'tone') { const [f, g] = TONES[drag.h]; setReal(s, f, fOfX(u, 1)); setReal(s, g, (0.5 - v) / 0.42 * 12); }
        else { setReal(s, 'p12', clamp01(u) * 100); setReal(s, 'p13', (1 - clamp01(v)) * 100); }
        return;
      }
      const fine = e.ctrlKey || e.metaKey ? 0.25 : 1, dx = e.clientX - drag.x, dy = e.clientY - drag.y;
      // sliders move 1:1 with the pointer (travel in screen pixels); knobs and numbers: 250 px per full turn
      const n = drag.kind === 'vs' ? drag.n0 - dy / drag.px * fine : drag.kind === 'hs' ? drag.n0 + dx / drag.px * fine : drag.n0 - dy / (250 * drag.zoom) * fine;
      store.write(drag.id, quant(drag.q, n));
    }
    fr.addEventListener('pointermove', move);
    const end = () => { if (!drag) return; if (drag.id) store.end(drag.id); drag = null; };
    for (const ev of ['pointerup', 'pointercancel', 'lostpointercapture']) fr.addEventListener(ev, end);
    fr.addEventListener('dblclick', (e) => {
      const el = e.target.closest('[data-kind]');
      if (el && ['knob', 'vs', 'hs', 'num'].includes(el.dataset.kind)) oneWrite(pid(s, keyP(s, el.dataset.key)), defNorm(s, el.dataset.key));
      // (CONTOUR's graph double-click is handled on pointer-down, see lastTap)
    });
    fr.addEventListener('contextmenu', (e) => {
      // a continuous control (knob, fader, number, DRY / WET): its modulation
      const ctl = e.target.closest('[data-kind="knob"],[data-kind="vs"],[data-kind="hs"],[data-kind="num"]');
      if (ctl) { const q = spec(s, ctl.dataset.key); if (q && !q.steps && targetOf(s, ctl.dataset.key) >= 0) { e.preventDefault(); modPanel(s, fr, e, ctl.dataset.key); return; } }
      const el = e.target.closest('[data-kind="sel"]'); if (el) { e.preventDefault(); stepKey(s, el.dataset.key, -1); return; }
      const scr = e.target.closest('.fx-screen');
      if (def(s)?.id === 'carve' && scr) { e.preventDefault(); carveMenu(s, fr, e); return; }
      if (def(s)?.id !== 'proq' || !scr) return;
      e.preventDefault();
      const r = scr.getBoundingClientRect(), b = pqHit(s, (e.clientX - r.left) / r.width, (e.clientY - r.top) / r.height);
      if (b >= 0) { view.band[s] = b; pqRefresh(s); pqMenu(s, fr, e, b); }
    });
    fr.addEventListener('wheel', (e) => {
      const el = e.target.closest('[data-kind]');
      if (el) {
        e.preventDefault(); const key = el.dataset.key, q = spec(s, key); if (!q) return;
        if (q.steps) stepKey(s, key, -Math.sign(e.deltaY)); else oneWrite(pid(s, keyP(s, key)), clamp01(norm(s, key) - Math.sign(e.deltaY) * (e.ctrlKey ? 0.005 : 0.02)));
        return;
      }
      if (def(s)?.id === 'proq' && e.target.closest('.fx-screen') && view.band[s] >= 0) {   // CONTOUR: the wheel sets the selected band's Q
        e.preventDefault(); const p = BANDS[view.band[s]][2];
        oneWrite(pid(s, p), clamp01(store.read(pid(s, p)) - Math.sign(e.deltaY) * 0.03));
      }
    }, { passive: false });
    // CONVOLVER: drop a WAV on the display
    fr.addEventListener('dragover', (e) => { const scr = e.target.closest('.fx-screen'); if (!scr || def(s)?.id !== 'convolver') return; e.preventDefault(); scr.classList.add('drag'); });
    fr.addEventListener('dragleave', (e) => e.target.closest('.fx-screen')?.classList.remove('drag'));
    fr.addEventListener('drop', async (e) => {
      const scr = e.target.closest('.fx-screen'); scr?.classList.remove('drag');
      if (!scr || def(s)?.id !== 'convolver') return;
      e.preventDefault();
      const file = e.dataTransfer?.files?.[0]; if (!file) return;
      if (!/\.(wav|aif|aiff)$/i.test(file.name)) { report('Drop a WAV or AIFF file'); return; }
      const bytes = new Uint8Array(await file.arrayBuffer());
      let bin = ''; for (let i = 0; i < bytes.length; i += 0x8000) bin += String.fromCharCode.apply(null, bytes.subarray(i, i + 0x8000));
      if (hosted) { const ok = await native('fxLoadIr', btoa(bin), file.name); if (!ok) report('Could not read ' + file.name); }
      else view.ir = { name: file.name.replace(/\.[^.]+$/, '').toUpperCase(), wave: [], secs: 0 };
    });
  }

  // ---- live displays, in each module's colours. Flat marks only: lines, solid fills, see-through fills
  function biquad(kind, f, q, g, fs = 48000) {
    const w = 2 * Math.PI * Math.min(f, fs * 0.49) / fs, c = Math.cos(w), sn = Math.sin(w), al = sn / (2 * q), A = Math.pow(10, g / 40), sq = 2 * Math.sqrt(A) * al;
    switch (kind) {
      case 'lp': return [(1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al];
      case 'hp': return [(1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al];
      case 'bp': return [al, 0, -al, 1 + al, -2 * c, 1 - al];
      case 'notch': return [1, -2 * c, 1, 1 + al, -2 * c, 1 - al];
      case 'bell': return [1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A];
      case 'ls': return [A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq), (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq];
      default: return [A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq), (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq];
    }
  }
  function magDb(b, f, fs = 48000) {
    const w = 2 * Math.PI * f / fs, c1 = Math.cos(w), s1 = Math.sin(w), c2 = Math.cos(2 * w), s2 = Math.sin(2 * w);
    const nr = b[0] + b[1] * c1 + b[2] * c2, ni = -(b[1] * s1 + b[2] * s2), dr = b[3] + b[4] * c1 + b[5] * c2, di = -(b[4] * s1 + b[5] * s2);
    return 10 * Math.log10((nr * nr + ni * ni) / (dr * dr + di * di) + 1e-20);
  }
  // CONTOUR: each band's sections, as src/plugin/FxUnits.cpp builds them from its shape
  const usesGain = (shape) => shape <= 2;
  function eqSections(s) {
    const on = Math.round(val(s, 'p27')), out = [];
    BANDS.forEach((b, i) => {
      if (!(on & (1 << i))) { out.push([]); return; }
      const f = val(s, b[0]), g = val(s, b[1]), q = val(s, b[2]), sh = val(s, PQ_SHAPE[i]);
      if (sh >= 3 && sh <= 8) out.push(Array.from({ length: 1 << ((sh - 3) % 3) }, (_, k) => biquad(sh <= 5 ? 'hp' : 'lp', f, k ? 0.7071 : q, 0)));
      else out.push([biquad(['bell', 'ls', 'hs'][sh] || (sh === 9 ? 'notch' : 'bp'), f, q, g)]);
    });
    return out;
  }
  // DISTORTION transfer, mirroring the shapers in src/plugin/FxUnits.cpp
  function transfer(s) {
    const v = (p) => val(s, p), k = Math.pow(10, v('p1') / 20), bias = v('p2') / 100 * 0.5, ty = v('p9');
    const sh = [(x) => Math.tanh(x), (x) => Math.max(-1, Math.min(1, x)), (x) => Math.sin(x), (x) => x > 0 ? Math.tanh(x) : Math.tanh(x * 0.4) * 0.6, (x) => x > 0 ? 1 - Math.exp(-x) : -(1 - Math.exp(x)) * 0.8][ty];
    const amt = Math.min(1, Math.max(0, v('p1') / 6));   // 0 dB is clean, as in the DSP
    return (x) => x + (sh(k * x + bias) - sh(bias) - x) * amt;
  }
  // the analyser (plugin only): the slot's output, 48 log bands from 20 Hz, smoothed for the eye
  function spectrum(s) {
    const raw = view.spec.slice(s * 48, s * 48 + 48); if (raw.length < 48) return null;
    const sm = view.specSm[s];
    for (let i = 0; i < 48; i++) sm[i] = sm[i] == null ? raw[i] : raw[i] > sm[i] ? raw[i] : sm[i] * 0.86 + raw[i] * 0.14;
    return sm.some((d) => d > -84) ? sm : null;
  }
  const bandHz = (i) => 20 * Math.pow(1000, (i + 0.5) / 48);
  function draw(s, t) {
    const cv = $('fx' + s + '-canvas'), d = def(s); if (!cv || !d) return;
    const g = cv.getContext('2d'), W = cv.width, H = cv.height, K = skin(s);
    const light = lum(K.screen) > 0.5, acc = K.accent, ink = K.ink, ink2 = K.ink2, faint = light ? 'rgba(0,0,0,.08)' : 'rgba(255,255,255,.06)';
    g.clearRect(0, 0, W, H);
    const vis = view.vis.slice(s * NV, s * NV + NV), v = (p) => val(s, p), top = 60;
    if (!['grains', 'eq', 'painting', 'valve', 'echogram', 'gr', 'plate', 'carve', 'poise', 'rift'].includes(d.display)) {
      g.strokeStyle = faint; g.lineWidth = 2;
      for (let i = 1; i < 8; i++) { g.beginPath(); g.moveTo(i * W / 8, 0); g.lineTo(i * W / 8, H); g.stroke(); }
      for (let i = 1; i < 4; i++) { g.beginPath(); g.moveTo(0, i * H / 4); g.lineTo(W, i * H / 4); g.stroke(); }
    }
    g.strokeStyle = acc; g.fillStyle = acc; g.lineWidth = 4; g.lineJoin = 'round'; g.globalAlpha = 1;
    const line = (fn, x0 = 0, x1 = W) => { g.beginPath(); for (let x = x0; x <= x1; x += 4) { const y = fn(x); x === x0 ? g.moveTo(x, y) : g.lineTo(x, y); } g.stroke(); };
    const area = (fn, base, c) => { g.fillStyle = c; g.beginPath(); g.moveTo(0, base); for (let x = 0; x <= W; x += 4) g.lineTo(x, fn(x)); g.lineTo(W, base); g.fill(); };
    const label = (str, x, y, al = 'left', c = ink2, size = 26) => { g.save(); g.fillStyle = c; g.font = '700 ' + size + 'px ' + FONT; g.textAlign = al; g.fillText(String(str).toUpperCase(), x, y); g.restore(); };
    switch (d.display) {
      case 'dots': {   // PS DELAY: one mark per repeat; size = level, height = accumulated pitch
        const fb = v('p3') / 100, n = 16, st = [12, -12, 7, 0, 0][v('p9')] * v('p5') / 100;
        g.strokeStyle = faint; g.beginPath(); g.moveTo(40, H * 0.6); g.lineTo(W - 40, H * 0.6); g.stroke();
        for (let k = 0; k < n; k++) {
          const lvl = Math.pow(Math.max(fb, 0.001), k); if (lvl < 0.03) break;
          const x = 50 + k * (W - 100) / (n - 1), y = H * 0.6 - Math.max(-6, Math.min(6, (v('p9') === 3 ? (k % 2 ? -1 : 1) : 1) * st * k / 12)) * 22;
          g.globalAlpha = 0.3 + 0.7 * lvl; g.beginPath(); g.arc(x, y, 6 + 18 * lvl, 0, 7); g.fill();
        }
        g.globalAlpha = 1; label(readout(s, 'p1'), W - 24, H - 22, 'right');
        break;
      }
      case 'carve': {   // CARVE: the selected shaper's wave, its points, and where that shaper is reading it now
        const k = view.shaper[s], P = (j) => 'p' + (1 + 5 * k + j), on = v(P(0)) === 1, trig = v(P(3)), follow = trig === 3;
        const x0 = 30, x1 = W - 30, yT = H * 0.06, yB = H * 0.94, Y = (w) => yB - w * (yB - yT), X = (ph) => x0 + ph * (x1 - x0);
        for (let i = 0; i <= 16; i++) { g.strokeStyle = i % 4 ? faint : alpha(ink2, 0.35); g.lineWidth = 2; g.beginPath(); g.moveTo(X(i / 16), yT); g.lineTo(X(i / 16), yB); g.stroke(); }
        for (const w of [0, 0.5, 1]) { g.strokeStyle = faint; g.beginPath(); g.moveTo(x0, Y(w)); g.lineTo(x1, Y(w)); g.stroke(); }
        g.globalAlpha = on ? 1 : 0.4;
        const pts = []; for (let x = x0; x <= x1; x += 3) pts.push([x, Y(carveAt(s, k, (x - x0) / (x1 - x0)))]);
        g.fillStyle = alpha(acc, 0.22); g.beginPath(); g.moveTo(x0, yB); for (const [x, y] of pts) g.lineTo(x, y); g.lineTo(x1, yB); g.fill();
        g.strokeStyle = acc; g.lineWidth = 5; g.beginPath(); pts.forEach(([x, y], i) => i ? g.lineTo(x, y) : g.moveTo(x, y)); g.stroke();
        for (let i = 0; i < 16; i++) { g.fillStyle = ink; g.beginPath(); g.arc(X(i / 16), Y(carvePt(s, k, i)), 8, 0, 7); g.fill(); }
        g.globalAlpha = 1;
        // the playhead: the plugin sends where each shaper is; on its own the page runs it at 120 BPM
        const beats = CARVE_BEATS[v(P(2))] || 1, ph = hosted ? vis[k] || 0 : trig >= 2 ? 0 : (t / 1000 * 2 / beats) % 1;
        if (on) {
          g.strokeStyle = ink; g.lineWidth = 3; g.beginPath(); g.moveTo(X(ph), yT); g.lineTo(X(ph), yB); g.stroke();
          g.fillStyle = ink; g.beginPath(); g.arc(X(ph), Y(carveAt(s, k, ph)), 14, 0, 7); g.fill();
          g.strokeStyle = acc; g.lineWidth = 4; g.beginPath(); g.arc(X(ph), Y(carveAt(s, k, ph)), 21, 0, 7); g.stroke();
        } else label(CARVE_SHAPERS[k] + ' is off', x1 - 6, yT + 30, 'right', ink, 24);
        if (follow) { label('quiet', x0 + 6, yB - 14, 'left', ink2, 22); label('loud', x1 - 6, yB - 14, 'right', ink2, 22); }
        else label('draw · right-click for shapes', x0 + 6, yB - 14, 'left', ink2, 22);
        break;
      }
      case 'poise': {   // POISE: where it leans (dashed), what it is doing now (filled), the TONE handles
        const y = (db) => toneY(db) * H, fAt = (x) => fOfX(x, W);
        g.lineWidth = 2; g.font = '600 22px ' + FONT;
        const sp = spectrum(s);
        if (sp) {   // the sound coming out, behind everything
          const ys = (db) => H - Math.max(0, Math.min(1, (db + 72) / 72)) * H * 0.9;
          g.fillStyle = alpha(ink2, 0.14); g.beginPath(); g.moveTo(0, H); sp.forEach((db, i) => g.lineTo(xOfF(bandHz(i), W), ys(db))); g.lineTo(W, H); g.closePath(); g.fill();
        }
        for (const hz of [50, 100, 200, 500, 1000, 2000, 5000, 10000]) {
          const x = xOfF(hz, W); g.strokeStyle = faint; g.beginPath(); g.moveTo(x, 0); g.lineTo(x, H); g.stroke();
          g.fillStyle = ink2; g.textAlign = 'center'; g.fillText(hz >= 1000 ? hz / 1000 + 'k' : String(hz), x, H - 14);
        }
        for (const db of [-6, 0, 6]) { g.strokeStyle = db ? faint : alpha(ink2, 0.45); g.beginPath(); g.moveTo(0, y(db)); g.lineTo(W, y(db)); g.stroke(); }
        // the target: a gentle smile plus the handles, around its own average (as the DSP sees it)
        const shape = (f) => 0.45 * Math.abs(Math.log2(f / 1000)) + TONES.reduce((a, [fp, gp]) => { const o = Math.log2(f / v(fp)); return a + v(gp) * Math.exp(-o * o / (2 * 0.64)); }, 0);
        const meanT = POISE_FC.reduce((a, f) => a + shape(f), 0) / 12;
        g.strokeStyle = ink; g.lineWidth = 3; g.setLineDash([12, 10]); line((x) => y(shape(fAt(x)) - meanT)); g.setLineDash([]);
        // the correction now: the twelve bells it sets (sent by the plugin)
        const secs = POISE_FC.map((f, k) => biquad('bell', f, 1.1, v('p1') > 0 ? vis[k] || 0 : 0));
        const total = (x) => Math.max(-14, Math.min(14, secs.reduce((a, b) => a + magDb(b, fAt(x)), 0)));
        g.fillStyle = alpha(acc, 0.22); g.beginPath(); g.moveTo(0, y(0)); for (let x = 0; x <= W; x += 4) g.lineTo(x, y(total(x))); g.lineTo(W, y(0)); g.fill();
        g.strokeStyle = acc; g.lineWidth = 5; line((x) => y(total(x)));
        TONES.forEach(([fp, gp], i) => {
          const x = xOfF(v(fp), W), yy = y(v(gp));
          g.fillStyle = K.bg; g.beginPath(); g.arc(x, yy, 24, 0, 7); g.fill();
          g.strokeStyle = ink; g.lineWidth = 4; g.beginPath(); g.arc(x, yy, 24, 0, 7); g.stroke();
          g.fillStyle = ink; g.font = '700 22px ' + FONT; g.textAlign = 'center'; g.fillText(String(i + 1), x, yy + 8);
        });
        label('lean', 20, 40, 'left', ink2, 22);
        if ((vis[12] || 0) < -0.1) label('squash ' + vis[12].toFixed(1) + ' db', W - 20, 40, 'right', acc, 22);
        if (v('p10') === 1) label('delta', W - 20, 72, 'right', acc, 22);
        break;
      }
      case 'rift': {   // RIFT: a tunnel of rings with the grains pulled through it; the pad point is SCATTER × BLOOM
        const sc = v('p12') / 100, bl = v('p13') / 100, frozen = v('p17') === 1, cx = W / 2, cy = H / 2;
        const px = 30 + sc * (W - 60), py = H - 30 - bl * (H - 60), speed = frozen ? 0 : 0.00012 + v('p2') / 100 * 0.0004;
        const rings = 10 + Math.round(bl * 8);
        for (let i = rings - 1; i >= 0; i--) {   // far rings small and faint; they drift towards you
          const z = ((i / rings + t * speed) % 1), r = 30 + z * z * Math.max(W, H) * 0.62;
          const wob = sc * 26 * Math.sin(t / 900 + i * 1.7), ox = (px - cx) * (1 - z) * 0.5, oy = (py - cy) * (1 - z) * 0.5;
          g.globalAlpha = 0.12 + 0.6 * z * (1 - z) * 2; g.strokeStyle = i % 4 ? ink2 : acc; g.lineWidth = 2 + 3 * z;
          g.beginPath(); g.ellipse(cx + ox, cy + oy, r * 1.25 + wob, r * 0.82 - wob * 0.5, 0, 0, 7); g.stroke();
        }
        g.globalAlpha = 1;
        // grains: born at the rim, pulled to the centre (reversed ones fly out), size = SIZE, spread = SCATTER
        const gs = view.grains[s], life = 700 + v('p1') * 2;
        if (!frozen && Math.random() < v('p2') / 60) gs.push({ born: t, a: Math.random() * 7, rev: Math.random() < v('p14') / 100, j: (Math.random() - 0.5) * sc });
        while (gs.length && t - gs[0].born > life) gs.shift();
        for (const gr of gs) {
          const k = (t - gr.born) / life, z = gr.rev ? k : 1 - k, r = 40 + z * Math.max(W, H) * 0.45, a = gr.a + gr.j * 3 + (1 - z) * (1 + bl * 2);
          g.globalAlpha = Math.sin(Math.PI * k); g.fillStyle = gr.rev ? ink : acc;
          g.beginPath(); g.arc(cx + Math.cos(a) * r * 1.25, cy + Math.sin(a) * r * 0.82, 4 + v('p1') / 1000 * 10 * z, 0, 7); g.fill();
        }
        g.globalAlpha = 1;
        g.strokeStyle = alpha(ink, 0.25); g.lineWidth = 2; g.beginPath(); g.moveTo(px, 0); g.lineTo(px, H); g.moveTo(0, py); g.lineTo(W, py); g.stroke();
        g.fillStyle = acc; g.beginPath(); g.arc(px, py, 20, 0, 7); g.fill();
        g.strokeStyle = ink; g.lineWidth = 3; g.beginPath(); g.arc(px, py, 32, 0, 7); g.stroke();
        label('bloom ' + Math.round(v('p13')), 20, 40); label('scatter ' + Math.round(v('p12')), W - 20, H - 20, 'right');
        if (frozen) label('frozen', W - 20, 40, 'right', acc);
        break;
      }
      case 'echogram': {   // SPACES: early reflections as bars, then the dense tail under its envelope
        const pre = v('p1') / 1000, dec = v('p2'), size = v('p3') / 100, att = v('p4') / 100, ed = v('p5') / 100, ld = v('p6') / 100;
        const span = Math.max(0.6, pre + Math.min(dec, 12) * 0.9), X = (sec) => 24 + sec / span * (W - 48), base = H - 50, tall = H - 110;
        g.strokeStyle = faint; g.lineWidth = 2; g.beginPath(); g.moveTo(24, base); g.lineTo(W - 24, base); g.stroke();
        g.fillStyle = ink2;
        for (let x = X(pre); x < W - 24; x += 5) {
          const tt = (x - 24) / (W - 48) * span - pre, e = Math.min(1, tt / (0.01 + att * 0.35)) * Math.pow(10, -3 * tt / dec);
          const hh = e * tall * 0.78 * (1 - ld * 0.45 + ld * 0.45 * hash(x, 7)); if (hh < 1) continue;
          g.globalAlpha = 0.55; g.fillRect(x, base - hh, 2.5, hh);
        }
        g.fillStyle = acc; g.globalAlpha = 1;
        for (let i = 0; i < 12; i++) { const tt = pre + (i + 1) * 0.006 * (1 + size * 3) * (0.6 + hash(i, 3) * 0.8), a = Math.pow(0.8, i) * (1 - ed * 0.55); g.fillRect(X(tt) - 3, base - a * tall, 6, a * tall); }
        g.strokeStyle = ink; g.lineWidth = 3; g.globalAlpha = 0.8;
        line((x) => { const tt = (x - 24) / (W - 48) * span - pre; return tt < 0 ? base : base - Math.min(1, tt / (0.01 + att * 0.35)) * Math.pow(10, -3 * tt / dec) * tall * 0.8; }, Math.round(X(pre)), W - 24);
        g.globalAlpha = 1; label('pre ' + readout(s, 'p1'), 24, 40); label('decay ' + readout(s, 'p2'), W - 24, 40, 'right');
        break;
      }
      case 'valve': {   // VALVE: the glass, its plates, and a filament that glows with drive and level
        const lvl = Math.min(1, vis[0] || 0), drive = v('p1') / 30, glow = 0.18 + 0.82 * Math.min(1, drive * 0.45 + lvl * 0.9);
        const cx = W * 0.3, tw = W * 0.2, th = H - 70, y0 = 30;
        g.strokeStyle = ink2; g.lineWidth = 3; g.beginPath(); g.roundRect(cx - tw / 2, y0, tw, th - 30, [tw / 2, tw / 2, 14, 14]); g.stroke();
        g.fillStyle = alpha(ink2, 0.35); g.fillRect(cx - tw * 0.32, y0 + th * 0.22, tw * 0.14, th * 0.42); g.fillRect(cx + tw * 0.18, y0 + th * 0.22, tw * 0.14, th * 0.42);
        g.strokeStyle = acc; g.lineWidth = 5; g.globalAlpha = glow; g.beginPath();
        for (let i = 0; i <= 8; i++) { const yy = y0 + th * 0.24 + i * th * 0.05; i ? g.lineTo(cx + (i % 2 ? 10 : -10), yy) : g.moveTo(cx - 10, yy); } g.stroke();
        g.fillStyle = acc; g.globalAlpha = glow * 0.18; g.beginPath(); g.ellipse(cx, y0 + th * 0.45, tw * 0.3, th * 0.3, 0, 0, 7); g.fill();
        g.globalAlpha = 1; g.strokeStyle = ink2; g.lineWidth = 4;
        for (let i = 0; i < 5; i++) { const x = cx - tw * 0.3 + i * tw * 0.15; g.beginPath(); g.moveTo(x, y0 + th - 30); g.lineTo(x, y0 + th - 8); g.stroke(); }
        // its curve: how hard the stage is pushed, and where the signal sits on it now
        const gx = W * 0.5, gw = W * 0.44, gy = 40, gh = H - 90, k = 1 + drive * 5, amt = Math.min(1, v('p1') / 6), cur = (x) => x + (Math.tanh(k * x) / Math.tanh(k) - x) * amt;
        g.strokeStyle = faint; g.lineWidth = 2; g.strokeRect(gx, gy, gw, gh);
        g.strokeStyle = acc; g.lineWidth = 4; line((x) => gy + gh / 2 - cur((x - gx) / gw * 2 - 1) * gh / 2 * 0.9, Math.round(gx), Math.round(gx + gw));
        g.fillStyle = ink; g.beginPath(); g.arc(gx + gw / 2 + lvl * gw / 2, gy + gh / 2 - cur(lvl) * gh / 2 * 0.9, 9, 0, 7); g.fill();
        label(readout(s, 'p1'), gx + gw, H - 16, 'right');
        break;
      }
      case 'plate': {   // PARLOUR: the plate on its four springs, ringing with the input for as long as DECAY says; the post EQ along the bottom
        const lvl = Math.min(1, vis[0] || 0), dec = v('p5'), px = 60, py = 40, pw = W * 0.56, ph = H - 80;
        view.ring = view.ring || [0, 0, 0];
        view.ring[s] = Math.max(lvl, view.ring[s] * Math.pow(10, -3 * (1 / 60) / Math.max(0.1, dec)));   // decays at the set RT60 (per frame)
        const amp = 6 + 26 * view.ring[s];
        g.strokeStyle = ink2; g.lineWidth = 3;
        for (const [cx, cy] of [[px, py], [px + pw, py], [px, py + ph], [px + pw, py + ph]]) { g.beginPath(); g.moveTo(cx, cy); g.lineTo(cx + (cx === px ? -30 : 30), cy + (cy === py ? -26 : 26)); g.stroke(); }
        g.strokeRect(px, py, pw, ph);
        g.save(); g.beginPath(); g.rect(px, py, pw, ph); g.clip();
        for (let i = 1; i < 12; i++) {   // the plate's modes: lines bowing with the ringing
          const yy = py + i * ph / 12, ph2 = t / (140 + i * 9);
          g.strokeStyle = i % 4 ? alpha(ink, 0.45) : ink; g.lineWidth = i % 4 ? 2 : 3;
          line((x) => yy + Math.sin((x - px) / pw * Math.PI * (1 + (i % 3))) * Math.sin(ph2 + i) * amp * Math.sin(i / 12 * Math.PI), Math.round(px), Math.round(px + pw));
        }
        g.restore();
        // post EQ: the two shelves on a log axis, 20 Hz - 20 kHz
        const ex = px + pw + 60, ew = W - ex - 30, ey = H / 2, eh = H * 0.36;
        const loS = biquad('ls', v('p6'), 0.7071, v('p7')), hiS = biquad('hs', v('p8'), 0.7071, v('p12'));
        g.strokeStyle = alpha(ink, 0.3); g.lineWidth = 2; g.beginPath(); g.moveTo(ex, ey); g.lineTo(ex + ew, ey); g.stroke();
        g.strokeStyle = ink; g.lineWidth = 4; line((x) => { const f = fOfX(x - ex, ew); return ey - Math.max(-20, Math.min(20, magDb(loS, f) + magDb(hiS, f))) / 20 * eh; }, Math.round(ex), Math.round(ex + ew));
        label('post eq', ex, py + 16, 'left', ink2); label(readout(s, 'p5'), px + pw, H - 6, 'right', ink);
        break;
      }
      case 'echo': {   // ECHO DELAY: the repeats as bars - up for left, down for right (ping-pong alternates)
        const fb = v('p2') / 100, mode = v('p9'), mid = H / 2 + 10, span = W - 60;
        g.strokeStyle = faint; g.lineWidth = 2; g.beginPath(); g.moveTo(30, mid); g.lineTo(W - 30, mid); g.stroke();
        for (let k = 0; k < 14; k++) {
          const lvl = Math.pow(Math.max(fb, 0.001), k); if (lvl < 0.03) break;
          const x = 30 + (k + 0.5) * span / 14, hh = lvl * (H / 2 - 60);
          g.globalAlpha = 0.35 + 0.65 * lvl; g.fillStyle = acc;
          if (mode === 1) g.fillRect(x - 9, k % 2 ? mid : mid - hh, 18, hh);
          else if (mode === 2) g.fillRect(x - 9, mid - hh / 2, 18, hh);
          else { g.fillRect(x - 9, mid - hh, 18, hh); g.fillRect(x - 9, mid, 18, hh); }
        }
        g.globalAlpha = 1; label(readout(s, 'p1'), W - 24, H - 22, 'right'); label(['L / R', 'L ↔ R', 'MONO'][mode], 24, H - 22);
        break;
      }
      case 'imager': {   // STEREO IMAGER: a fan per band as wide as its width, and the live correlation
        const cx = W / 2, cy = H - 90, Rr = H - 170, bands = [['p1', 'LOW', 1], ['p2', 'MID', 0.66], ['p3', 'HIGH', 0.36]];
        g.strokeStyle = faint; g.lineWidth = 2;
        for (const a of [-90, -45, 0, 45, 90]) { const r0 = a * Math.PI / 180; g.beginPath(); g.moveTo(cx, cy); g.lineTo(cx + Math.sin(r0) * Rr, cy - Math.cos(r0) * Rr); g.stroke(); }
        bands.forEach(([p, name, rs]) => {
          const spread = Math.min(1, v(p) / 200) * Math.PI / 2, rr = Rr * rs;
          g.fillStyle = alpha(acc, 0.22); g.beginPath(); g.moveTo(cx, cy); g.arc(cx, cy, rr, -Math.PI / 2 - spread, -Math.PI / 2 + spread); g.closePath(); g.fill();
          g.strokeStyle = acc; g.lineWidth = 3; g.beginPath(); g.arc(cx, cy, rr, -Math.PI / 2 - spread, -Math.PI / 2 + spread); g.stroke();
          label(name + ' ' + readout(s, p), cx, cy - rr - 10, 'center');
        });
        const corr = vis[0] || 0, bx = 60, bw = W - 120, by = H - 44;   // correlation, -1 ... +1
        g.fillStyle = faint; g.fillRect(bx, by, bw, 8);
        g.fillStyle = corr < 0 ? '#E0454F' : acc; g.fillRect(bx + bw / 2, by, corr * bw / 2, 8);
        label('-1', bx - 10, by + 10, 'right'); label('+1', bx + bw + 10, by + 10); label('correlation ' + corr.toFixed(2), cx, by - 10, 'center');
        break;
      }
      case 'painting': paint2(g, W, H, s, t); break;   // MARBLE
      case 'ir': {   // CONVOLVER: the impulse, trimmed and possibly reversed
        const wave = view.ir.wave.length ? view.ir.wave : Array.from({ length: 240 }, (_, i) => Math.exp(-i / 50) * (0.6 + 0.4 * Math.sin(i * 7.3)));
        const len = v('p2') / 100, rev = v('p9') === 1, n = wave.length, bw = W / n;
        for (let i = 0; i < n; i++) {
          const src = rev ? Math.floor(len * n) - 1 - i : i, a = src >= 0 && i < len * n ? Math.abs(wave[src] || 0) : 0, hh = a * (H - top - 40);
          g.globalAlpha = i < len * n ? 1 : 0.2; g.fillRect(i * bw, H / 2 + 10 - hh / 2, Math.max(1, bw - 1), Math.max(2, hh));
        }
        g.globalAlpha = 1; label(view.ir.name, W - 24, H - 22, 'right'); label('drop a .wav to change', 24, H - 22);
        break;
      }
      case 'curve': {   // DISTORTION: transfer curve and the live input level
        const shape = transfer(s), mid = H / 2 + 10, amp = (H - top - 40) / 2;
        g.strokeStyle = faint; g.lineWidth = 2; g.beginPath(); g.moveTo(0, mid); g.lineTo(W, mid); g.moveTo(W / 2, 20); g.lineTo(W / 2, H); g.stroke();
        g.strokeStyle = acc; g.lineWidth = 4; line((x) => mid - Math.max(-1.1, Math.min(1.1, shape((x / W) * 2 - 1))) * amp);
        const lv = Math.min(1, vis[0]); g.beginPath(); g.arc(W / 2 + lv * W / 2, mid - shape(lv) * amp, 10, 0, 7); g.fill();
        break;
      }
      case 'bands': case 'mb': {   // HEAT / MULTIBAND: three bands on a log axis
        if (d.id === 'saturn') {
          const sb = view.sband[s], xa = xOfF(v('p1'), W), xb = xOfF(v('p2'), W), xs = [0, xa, xb, W];
          const hOf = (b) => (0.14 + v('p' + (3 + b)) / 36 * 0.66) * H, sig = (z) => 1 / (1 + Math.exp(-z));
          const grit = [0.35, 0.2, 0.3, 0.45, 0.6, 0.9, 1][v('p9')];   // how rough each style's curve looks
          const hAt = (x) => (hOf(0) + (hOf(1) - hOf(0)) * sig((x - xa) / 30) + (hOf(2) - hOf(1)) * sig((x - xb) / 30))
            * (1 + grit * 0.1 * Math.sin(x * 0.09 + t / 350) * Math.sin(x * 0.021 - t / 900));
          g.fillStyle = alpha(acc, 0.1); g.fillRect(xs[sb], 0, xs[sb + 1] - xs[sb], H);
          area((x) => H - hAt(x), H, alpha(acc, 0.28));
          g.strokeStyle = acc; g.lineWidth = 4; line((x) => H - hAt(x));
          g.strokeStyle = alpha(ink, 0.45); g.lineWidth = 2; g.setLineDash([10, 10]);
          for (const [x, p] of [[xa, 'p1'], [xb, 'p2']]) { g.beginPath(); g.moveTo(x, 20); g.lineTo(x, H); g.stroke(); label(readout(s, p), x + 14, 46); }
          g.setLineDash([]);
          for (let b = 0; b < 3; b++) {   // each band's handle sits on its level
            const cx = (xs[b] + xs[b + 1]) / 2, cy = H - hOf(b);
            g.fillStyle = b === sb ? ink : alpha(ink, 0.5); g.beginPath(); g.roundRect(cx - 36, cy - 11, 72, 22, 6); g.fill();
            label(SAT_BANDS[b] + ' ' + readout(s, 'p' + (3 + b)), cx, cy - 22, 'center', b === sb ? ink : ink2);
          }
          break;
        }
        const xa = xOfF(v('p12'), W), xb = xOfF(v('p13'), W), xs = [0, xa, xb, W];
        for (let b = 0; b < 3; b++) {
          const lvl = 0.5 + (vis[1 + b] || 0) / 24, hh = Math.max(0.03, Math.min(1, lvl)) * (H - top - 30);
          g.fillStyle = alpha(acc, 0.22); g.fillRect(xs[b] + 6, H - hh, xs[b + 1] - xs[b] - 12, hh);
          g.strokeStyle = acc; g.lineWidth = 4; g.strokeRect(xs[b] + 6, H - hh, xs[b + 1] - xs[b] - 12, hh);
        }
        g.strokeStyle = ink; g.lineWidth = 3;
        for (const [x, p] of [[xa, 'p12'], [xb, 'p13']]) { g.beginPath(); g.moveTo(x, top); g.lineTo(x, H); g.stroke(); label(readout(s, p), x + 10, top + 30); }
        break;
      }
      case 'steps': {   // BITCRUSHER: a sine through the current bits and rate
        const levels = Math.pow(2, v('p1')) / 2, hold = Math.max(1, 48000 / v('p2')), amp = (H - top - 40) / 2, mid = H / 2 + 10;
        g.strokeStyle = faint; g.lineWidth = 2; line((x) => mid - Math.sin(x / W * 2 * Math.PI * 2 + t / 700) * amp);
        g.strokeStyle = acc; g.lineWidth = 4;
        line((x) => { const smp = Math.floor(x / W * 218 / hold) * hold; return mid - Math.round(Math.sin(smp / 218 * 2 * Math.PI * 2 + t / 700) * levels) / levels * amp; });
        break;
      }
      case 'gr': {   // PUMP / CEILING: gain reduction scrolling right to left, the reading now large
        const h = view.hist[s]; h.push(vis[1] || 0); if (h.length > 200) h.shift();
        const y = (db) => 30 + Math.min(1, db / 24) * (H - 60);
        g.strokeStyle = faint; g.lineWidth = 2;
        for (const db of [3, 6, 12, 18]) { g.beginPath(); g.moveTo(0, y(db)); g.lineTo(W, y(db)); g.stroke(); label('-' + db, W - 14, y(db) - 6, 'right', ink2, 20); }
        const xAt = (i) => W - (h.length - 1 - i) * W / 200;
        g.fillStyle = alpha(acc, 0.22); g.beginPath(); g.moveTo(xAt(0), y(0)); h.forEach((dv, i) => g.lineTo(xAt(i), y(dv))); g.lineTo(W, y(0)); g.fill();
        g.strokeStyle = acc; g.lineWidth = 4; g.beginPath(); h.forEach((dv, i) => i ? g.lineTo(xAt(i), y(dv)) : g.moveTo(xAt(i), y(dv))); g.stroke();
        label((vis[1] || 0).toFixed(1), 24, 70, 'left', ink, 56); label('db reduction', 24, 102);
        break;
      }
      case 'lfo': {   // PHASER / FLANGER: two LFOs, apart by the stereo spread
        const rate = v('p1'), spread = d.id === 'phaser' ? v('p5') / 360 : (v('p9') ? 0.25 : 0), tri = d.id === 'flanger' && v('p10') === 1;
        const wave = (ph) => tri ? 1 - 4 * Math.abs(((ph % 1) + 1) % 1 - 0.5) : Math.sin(ph * 2 * Math.PI);
        const amp = (H - top - 40) / 2 * (v(d.id === 'phaser' ? 'p2' : 'p3') / 100), mid = H / 2 + 10, cyc = Math.max(1, Math.min(4, rate * 2));
        const f = (off) => (x) => mid - wave(x / W * cyc - t / 1000 * rate + off) * amp;
        g.fillStyle = alpha(acc, 0.2); g.beginPath(); g.moveTo(0, f(0)(0)); for (let x = 0; x <= W; x += 4) g.lineTo(x, f(0)(x)); for (let x = W; x >= 0; x -= 4) g.lineTo(x, f(spread)(x)); g.fill();
        g.strokeStyle = acc; g.lineWidth = 4; line(f(0)); g.globalAlpha = 0.55; line(f(spread)); g.globalAlpha = 1;
        break;
      }
      case 'pan': {   // STEREO PAN: the position over the LFO shape
        const rate = v('p9') ? 0.5 : v('p1'), depth = v('p2') / 100, sh = v('p3') / 100, ph = t / 1000 * rate + v('p4') / 360;
        const lfo = (p) => { const sn = Math.sin(p * 2 * Math.PI), tr = 1 - 4 * Math.abs(((p % 1) + 1) % 1 - 0.5), sq = sn >= 0 ? 1 : -1; return sh < 0.5 ? sn + (tr - sn) * sh * 2 : tr + (sq - tr) * (sh - 0.5) * 2; };
        g.strokeStyle = ink2; g.lineWidth = 3; line((x) => top + (1 - (lfo(x / W * 2) * depth + 1) / 2) * (H - top - 60));
        const px = W / 2 + lfo(ph) * depth * (W / 2 - 40);
        g.fillStyle = acc; g.beginPath(); g.arc(px, H - 40, 18, 0, 7); g.fill();
        label('L', 24, H - 30); label('R', W - 24, H - 30, 'right');
        break;
      }
      case 'eq': {   // CONTOUR: grid, the live analyser behind, the summed curve filled from 0 dB, the selected band's own curve, nodes
        const secs = eqSections(s), y = (db) => eqY(db) * H, fAt = (x) => fOfX(x, W);
        g.lineWidth = 2; g.font = '600 22px ' + FONT;
        const sp = spectrum(s);
        if (sp) {   // the sound coming out, -72 ... 0 dB from the bottom up
          const ys = (db) => H - Math.max(0, Math.min(1, (db + 72) / 72)) * H * 0.9;
          g.fillStyle = alpha(ink2, 0.16); g.strokeStyle = alpha(ink2, 0.45); g.beginPath(); g.moveTo(0, H);
          sp.forEach((db, i) => g.lineTo(xOfF(bandHz(i), W), ys(db))); g.lineTo(W, H); g.closePath(); g.fill();
          g.beginPath(); sp.forEach((db, i) => i ? g.lineTo(xOfF(bandHz(i), W), ys(db)) : g.moveTo(xOfF(bandHz(i), W), ys(db))); g.stroke();
        }
        for (const hz of [30, 50, 100, 200, 500, 1000, 2000, 5000, 10000]) {
          const x = xOfF(hz, W); g.strokeStyle = faint; g.beginPath(); g.moveTo(x, 0); g.lineTo(x, H); g.stroke();
          g.fillStyle = ink2; g.textAlign = 'center'; g.fillText(hz >= 1000 ? hz / 1000 + 'k' : String(hz), x, H - 14);
        }
        for (const db of [-18, -12, -6, 0, 6, 12, 18]) {
          g.strokeStyle = db ? faint : alpha(ink2, 0.4); g.beginPath(); g.moveTo(0, y(db)); g.lineTo(W, y(db)); g.stroke();
          g.fillStyle = ink2; g.textAlign = 'right'; g.fillText((db > 0 ? '+' : '') + db, W - 12, y(db) - 6);
        }
        const on = Math.round(v('p27')), sb = view.band[s];
        if (sb >= 0 && secs[sb]?.length) {   // the selected band on its own, faint
          g.strokeStyle = acc; g.globalAlpha = 0.45; g.lineWidth = 3; line((x) => y(Math.max(-30, secs[sb].reduce((a, b) => a + magDb(b, fAt(x)), 0)))); g.globalAlpha = 1;
        }
        const total = (x) => Math.max(-30, secs.reduce((a, bs) => a + bs.reduce((c, b) => c + magDb(b, fAt(x)), 0), 0) + v('p4'));
        const pts = []; for (let x = 0; x <= W; x += 4) pts.push([x, y(total(x))]);
        g.fillStyle = alpha(acc, 0.22); g.beginPath(); g.moveTo(0, y(0)); for (const [x, yy] of pts) g.lineTo(x, yy); g.lineTo(W, y(0)); g.fill();
        g.strokeStyle = acc; g.lineWidth = 5; g.beginPath(); pts.forEach(([x, yy], i) => i ? g.lineTo(x, yy) : g.moveTo(x, yy)); g.stroke();
        BANDS.forEach((b, i) => {
          if (!(on & (1 << i))) return;
          const x = xOfF(v(b[0]), W), yy = y(usesGain(v(PQ_SHAPE[i])) ? v(b[1]) : 0), sel = i === sb;
          g.fillStyle = sel ? ink : acc; g.beginPath(); g.arc(x, yy, sel ? 19 : 15, 0, 7); g.fill();
          if (sel) { g.strokeStyle = acc; g.lineWidth = 4; g.beginPath(); g.arc(x, yy, 25, 0, 7); g.stroke(); }
          g.fillStyle = K.bg; g.font = '700 20px ' + FONT; g.textAlign = 'center'; g.fillText(String(i + 1), x, yy + 7);
          if (sel) {   // its value, in a tag above it
            const tx0 = readout(s, 'band0') + (usesGain(v(PQ_SHAPE[i])) ? '   ' + readout(s, 'band1') : ''), tw = g.measureText(tx0).width + 28, ty = yy - 62 < 10 ? yy + 36 : yy - 62;
            const tx = Math.max(6, Math.min(W - tw - 6, x - tw / 2));
            g.fillStyle = alpha(K.bg, 0.92); g.beginPath(); g.roundRect(tx, ty, tw, 34, 6); g.fill();
            g.fillStyle = ink; g.textAlign = 'left'; g.fillText(tx0, tx + 14, ty + 24);
          }
        });
        if (!on) { g.fillStyle = ink2; g.font = '700 30px ' + FONT; g.textAlign = 'center'; g.fillText('DOUBLE-CLICK TO ADD A BAND', W / 2, y(0) - 30); }
        break;
      }
      case 'resp': {   // FILTER: the response, moving with its LFO
        const lfo = Math.sin(t / 1000 * v('p4') * 2 * Math.PI) * v('p5'), fc = Math.min(20000, Math.max(20, v('p1') * Math.pow(2, lfo + v('p6') * (vis[0] || 0))));
        const kind = ['lp', 'hp', 'bp', 'notch'][v('p9')], q = 0.5 + v('p2') / 100 * 12, b = biquad(kind, fc, q, 0), n = v('p10') ? 2 : 1;
        const y = (db) => top + (1 - (Math.max(-48, Math.min(24, db)) + 48) / 72) * (H - top - 10);
        area((x) => y(n * magDb(b, fOfX(x, W))), H, alpha(acc, 0.2));
        line((x) => y(n * magDb(b, fOfX(x, W))));
        break;
      }
      case 'grains': {   // PRISM: grains as strokes; x = age, height = envelope, row = pitch
        const gs = view.grains[s], dens = v('p2'), size = v('p1'), pitches = [-12, -7, -5, 0, 5, 7, 12, 19, 24];
        if (Math.random() < dens / 30 * (v('p5') / 100)) gs.push({ born: t, st: pitches[v('p9')] + v('p11') / 100, k: Math.random() < v('p6') / 100 ? 1 : 0, x: Math.random() });   // k 1 = reversed
        while (gs.length && t - gs[0].born > size * 4 + 1500) gs.shift();
        g.strokeStyle = faint; g.lineWidth = 2;
        for (const p of pitches) { const yy = H - 50 - (p + 12) / 36 * (H - 120); g.beginPath(); g.moveTo(0, yy); g.lineTo(W, yy); g.stroke(); }
        for (const gr of gs) {
          const age = (t - gr.born) / (size * 4 + 1500), env = Math.sin(Math.min(1, age) * Math.PI), x = gr.x * W * 0.3 + age * W * 0.7, yy = H - 50 - (gr.st + 12) / 36 * (H - 120);
          g.globalAlpha = 0.35 + 0.65 * env; g.strokeStyle = gr.k ? '#F65A27' : acc; g.lineWidth = 4;
          g.beginPath(); g.moveTo(x, yy - 12 - 40 * env); g.lineTo(x, yy + 12 + 40 * env); g.stroke();
        }
        g.globalAlpha = 1; label(readout(s, 'p9') + ' st', 24, 44); label('reversed', W - 24, 44, 'right', '#F65A27');
        break;
      }
    }
  }
  // MARBLE's painting: domain-warped noise, one palette per TYPE; DRIVE warps harder, GRAIN makes
  // the bands finer, RESTLESS makes it drift faster. Drawn on the GPU at the display's full
  // resolution with anti-aliased band edges; without WebGL 2, a small CPU version scaled up.
  // The palettes are Somii's own colours.
  const PALETTES = [
    [[231, 226, 218], [246, 90, 39], [35, 37, 42], [104, 195, 212], [130, 98, 81]],
    [[255, 232, 209], [86, 142, 163], [24, 50, 61], [246, 90, 39], [231, 226, 218]],
    [[231, 226, 218], [66, 66, 67], [168, 164, 156], [242, 163, 58], [38, 38, 39]],
    [[246, 90, 39], [242, 163, 58], [35, 37, 42], [231, 226, 218], [184, 64, 26]]];
  const arts = [0, 1, 2].map(() => ({ cv: null, at: -1e9 }));   // one cached painting per slot
  const hash = (x, y) => { const h = Math.sin(x * 127.1 + y * 311.7) * 43758.5453; return h - Math.floor(h); };
  const vnoise = (x, y) => {
    const xi = Math.floor(x), yi = Math.floor(y), xf = x - xi, yf = y - yi, u = xf * xf * (3 - 2 * xf), w = yf * yf * (3 - 2 * yf);
    const a = hash(xi, yi), b = hash(xi + 1, yi), c = hash(xi, yi + 1), d = hash(xi + 1, yi + 1);
    return a + (b - a) * u + (c - a) * w + (a - b - c + d) * u * w;
  };
  const fbm = (x, y) => vnoise(x, y) * 0.5 + vnoise(x * 2.03, y * 2.03) * 0.3 + vnoise(x * 4.1, y * 4.1) * 0.2;
  let glp;   // the WebGL painter, made on first use (null when WebGL 2 is missing)
  function glPainter() {
    const cv = document.createElement('canvas'), gl = cv.getContext('webgl2', { antialias: false, premultipliedAlpha: false, preserveDrawingBuffer: true });
    if (!gl) return null;
    const sh = (type, src) => { const o = gl.createShader(type); gl.shaderSource(o, src); gl.compileShader(o); if (!gl.getShaderParameter(o, gl.COMPILE_STATUS)) throw new Error(gl.getShaderInfoLog(o)); return o; };
    const prog = gl.createProgram();
    gl.attachShader(prog, sh(gl.VERTEX_SHADER, '#version 300 es\nin vec2 p;void main(){gl_Position=vec4(p,0.,1.);}'));
    gl.attachShader(prog, sh(gl.FRAGMENT_SHADER, `#version 300 es
precision highp float;
uniform vec2 uRes; uniform float uTime, uDrive, uGrain; uniform vec3 uPal[5]; out vec4 o;
float h(vec2 p){ return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float vn(vec2 p){ vec2 i = floor(p), f = fract(p), u = f * f * (3. - 2. * f);
  return mix(mix(h(i), h(i + vec2(1, 0)), u.x), mix(h(i + vec2(0, 1)), h(i + vec2(1, 1)), u.x), u.y); }
float fbm(vec2 p){ float v = 0., a = .5; for (int i = 0; i < 4; i++) { v += a * vn(p); p = p * 2.03 + vec2(1.7, 9.2); a *= .5; } return v; }
void main(){
  vec2 uv = vec2(gl_FragCoord.x, uRes.y - gl_FragCoord.y) / uRes.x * 1.9;   // broad strokes, like a painting
  vec2 q = vec2(fbm(uv + vec2(uTime, 0.)), fbm(uv + vec2(5.2, 1.3) - uTime * .7));
  float k = 1.5 + uDrive * 5.;
  vec2 r = vec2(fbm(uv + k * q + vec2(1.7, 9.2)), fbm(uv + k * q + vec2(8.3, 2.8)));
  float band = fract(fbm(uv + k * r) * (4. + uGrain * 10.) + q.x) * 5.;
  float i0 = floor(band), fr = band - i0, w = fwidth(band) * 1.5;
  float m = smoothstep(.55 - w, 1., fr);   // soft band edges, never narrower than a pixel
  int a = int(i0) % 5, b = (a + 1) % 5;
  o = vec4(mix(uPal[a], uPal[b], m), 1.);
}`));
    gl.linkProgram(prog);
    if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) throw new Error(gl.getProgramInfoLog(prog));
    gl.useProgram(prog);
    gl.bindBuffer(gl.ARRAY_BUFFER, gl.createBuffer());
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 3, -1, -1, 3]), gl.STATIC_DRAW);   // one triangle covers the view
    const at = gl.getAttribLocation(prog, 'p'); gl.enableVertexAttribArray(at); gl.vertexAttribPointer(at, 2, gl.FLOAT, false, 0, 0);
    const u = (n) => gl.getUniformLocation(prog, n);
    const loc = { res: u('uRes'), time: u('uTime'), drive: u('uDrive'), grain: u('uGrain'), pal: u('uPal') };
    return (W, H, time, drive, grain, pal) => {
      if (cv.width !== W || cv.height !== H) { cv.width = W; cv.height = H; gl.viewport(0, 0, W, H); }
      gl.uniform2f(loc.res, W, H); gl.uniform1f(loc.time, time); gl.uniform1f(loc.drive, drive); gl.uniform1f(loc.grain, grain);
      gl.uniform3fv(loc.pal, pal.flat().map((c) => c / 255));
      gl.drawArrays(gl.TRIANGLES, 0, 3);
      return cv;
    };
  }
  function paint2(g, W, H, s, t) {
    const art = arts[s], d = (p) => val(s, p), type = d('p9'), wild = d('p10') === 1, drive = d('p1') / 48, grain = d('p2') / 100;
    if (glp === undefined) { try { glp = glPainter(); } catch (e) { report(e); glp = null; } }
    if (glp) { g.drawImage(glp(W, H, t / (wild ? 1800 : 6000), drive, grain, PALETTES[type]), 0, 0, W, H); return; }
    const cw = 180, chh = Math.round(180 * H / W);
    // the noise is the page's heaviest drawing: repaint it ~8 times a second, reuse it in between
    if (art.cv && t - art.at < 120) { g.imageSmoothingEnabled = true; g.drawImage(art.cv, 0, 0, W, H); return; }
    art.at = t;
    if (!art.cv) { art.cv = document.createElement('canvas'); }
    if (art.cv.width !== cw || art.cv.height !== chh) { art.cv.width = cw; art.cv.height = chh; }
    const x2 = art.cv.getContext('2d'), img = x2.createImageData(cw, chh), pal = PALETTES[type], time = t / (wild ? 1800 : 6000);
    for (let y = 0; y < chh; y++) for (let x = 0; x < cw; x++) {
      const nx = x / cw * 3, ny = y / chh * 3 * (chh / cw);
      const q = fbm(nx + time, ny), r = fbm(nx + 5.2, ny + 1.3 - time * 0.7);
      const k = 1.5 + drive * 5;
      const val2 = fbm(nx + k * q, ny + k * r);
      const band = (val2 * (4 + grain * 10) + q) % 1 * pal.length, i0 = Math.floor(band) % pal.length, i1 = (i0 + 1) % pal.length, fr = band - Math.floor(band);
      const e = Math.max(0, (fr - 0.55) / 0.45), m = e * e * (3 - 2 * e), o = (y * cw + x) * 4;   // soft edges between the colour bands
      for (let c = 0; c < 3; c++) img.data[o + c] = pal[i0][c] * (1 - m) + pal[i1][c] * m;
      img.data[o + 3] = 255;
    }
    x2.putImageData(img, 0, 0);
    g.imageSmoothingEnabled = true; g.drawImage(art.cv, 0, 0, W, H);
  }
  function loop(t) {
    flushExt();
    if (!$('pop-fx').hidden) {
      for (let s = 0; s < 3; s++) {
        draw(s, t);
        // the activity light: the slot's output level (sent by the plugin)
        const led = document.querySelector('[data-led="' + s + '"]');
        if (led) led.style.opacity = (0.12 + 0.88 * Math.min(1, (view.level[s] || 0) * 1.6)).toFixed(2);
      }
    }
    requestAnimationFrame(loop);
  }
  requestAnimationFrame(loop);

  return {
    refresh, choose,
    feed(state) {
      if (state.fx) view.vis = state.fx;
      if (state.fxSpec) view.spec = state.fxSpec;
      if (state.fxLevel) view.level = state.fxLevel;
      if (state.irName != null) view.ir = { name: state.irName, wave: state.irWave || [], secs: state.irSeconds || 0 };
      // the slots' extra values and routes (sent when the plugin changed them), and where modulation has the controls now
      if (state.fxExt) state.fxExt.forEach((a, s) => { if (!view.extDirty[s].size) view.ext[s] = a.slice(); });
      if (state.fxMods) { view.mods = [new Map(), new Map(), new Map()]; for (const [s, t, k, a] of state.fxMods) view.mods[s].set(t * NSRC + k, a); }
      if (state.fxModNow) {
        const had = view.modNow.map((m) => m.size);
        view.modNow = [new Map(), new Map(), new Map()];
        for (const [s, t, o] of state.fxModNow) view.modNow[s].set(t, o);
        if (!$('pop-fx').hidden) for (let s = 0; s < 3; s++) if (view.modNow[s].size || had[s]) paint(s);
      }
      if ((state.fxExt || state.fxMods) && !$('pop-fx').hidden) for (let s = 0; s < 3; s++) paint(s);
    },
    open() { $('fx-picker').hidden = true; refresh(); openPage('pop-fx'); },
    retheme() { view.built = [-1, -1, -1]; refresh(); },
    serial() { oneWrite('fx.mode', 0); refresh(); },
    parallel() { oneWrite('fx.mode', 1); refresh(); }
  };
}
