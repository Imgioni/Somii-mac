// FX rack page (docs/fx/FX_PROMPTS.md). Each slot is a small "plug-in window" drawn here: the
// loaded effect's own face (skin and layout after the plug-in it is named for, ui/fxskins.js),
// flat controls (caps from tools/make-fx-controls.mjs), its live display, and the LAYER MIX
// fader along the bottom. Parameter meanings come from ui/fxdefs.mjs (embedded as #fx-spec).
import { SKINS, SKIN_OF, UPPER_COLOUR, LOWER_COLOUR } from './fxskins.js';

export function initFx(ctx) {
  const { $, all, store, oneWrite, native, hosted, actionState, text, openPage, report } = ctx;
  const { FX, DIVS } = JSON.parse($('fx-spec').textContent);
  const TYPES = FX.length + 1;                                     // NONE + effects
  const from = (q, n) => q.steps ? Math.round(n * (q.steps.length - 1)) : q.curve === 'log' ? q.lo * Math.pow(q.hi / q.lo, n) : q.lo + (q.hi - q.lo) * n;
  const to = (q, v) => q.steps ? (q.steps.length > 1 ? v / (q.steps.length - 1) : 0) : q.curve === 'log' ? Math.log(v / q.lo) / Math.log(q.hi / q.lo) : (v - q.lo) / (q.hi - q.lo);
  const pid = (s, p) => 'fx' + (s + 1) + '.' + p;
  const type = (s) => Math.round(store.read(pid(s, 'type')) * (TYPES - 1));
  const def = (s) => { const t = type(s); return t > 0 ? FX[t - 1] : null; };
  const param = (s, p) => def(s)?.params.find((q) => q.p === p);
  const val = (s, p) => { const q = param(s, p); return q ? from(q, store.read(pid(s, p))) : 0; };
  const view = { open: -1, band: [-1, -1, -1], vis: new Array(24).fill(0), ir: { name: 'DEFAULT HALL', wave: [], secs: 2.2 }, hist: [[], [], []], grains: [[], [], []], built: [-1, -1, -1], sband: [1, 1, 1], ambAdv: [false, false, false] };
  // PRO-Q: the band knobs ('band0..2') follow the selected band: FREQ, GAIN, Q
  const BANDS = [['p12', 'p13', 'p14'], ['p15', 'p16', 'p17'], ['p18', 'p19', 'p20'], ['p21', 'p22', 'p23'], ['p24', 'p25', 'p26'], ['p1', 'p2', 'p3']];
  const BAND_COLOURS = ['#FF7A7A', '#FFA94D', '#FFD43B', '#69DB7C', '#4DABF7', '#B197FC'];
  const STREAM_COLOURS = ['#6FA878', '#D9776F', '#D2AE52'];
  const ACCENT = {};                                               // per-effect accent over the skin's
  const skinName = (s) => { const d = def(s); return d ? SKIN_OF[d.id] || 'house' : 'house'; };
  // a skin colour may be a theme variable (the house skin follows GEMINI / SUPER SIX, darker)
  const cssVar = (v) => typeof v === 'string' && v.startsWith('var(') ? (getComputedStyle(document.body).getPropertyValue(v.slice(4, -1)).trim() || '#888888') : v;
  const skin = (s) => { const d = def(s), k = {}; for (const [a, b] of Object.entries(SKINS[skinName(s)])) k[a] = cssVar(b); if (d && ACCENT[d.id]) k.accent = ACCENT[d.id]; return k; };
  const FONTS = {
    sans: "'Segoe UI',Bahnschrift,sans-serif", cond: "Bahnschrift,'Arial Narrow',sans-serif",
    mono: "Consolas,'Courier New',monospace", wide: "'Arial Black','Segoe UI Black',sans-serif",
    small: "Bahnschrift,sans-serif", typewriter: "'Courier New',Courier,monospace", serif: "Georgia,'Times New Roman',serif"
  };
  const lum = (hex) => { if (!/^#[0-9a-f]{6}$/i.test(hex)) return 0; const n = parseInt(hex.slice(1), 16); return ((n >> 16) * 0.3 + ((n >> 8) & 255) * 0.59 + (n & 255) * 0.11) / 255; };
  const AMB_SUB = ['BACKWARD SWELLS', 'WOVEN CHORUS', 'OCTAVE SHIMMER', 'HARMONIC OCTAVES', 'DIGITAL ARTEFACTS', 'UNDERWATER'];
  const CASE = { house: 'lower', ocean: 'cap', ambient: 'upper', tuba: 'upper', fab: 'upper', vulf: 'lower', faraday: 'upper', chroma: 'lower', saturn: 'cap', valley: 'cap', nude: 'lower' };

  // ---- parameter keys: 'pN', 'mix', 'layer', 'on', or 'band0..2' (PRO-Q's selected band)
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
    if (q.p === 'layer') return Math.round(Math.min(1, 2 * (1 - n)) * 100) + ' / ' + Math.round(Math.min(1, 2 * n) * 100);
    const v = from(q, n);
    if (q.steps) return q.steps[v];
    switch (q.unit) {
      case 'Hz': return v >= 1000 ? (v / 1000).toFixed(v >= 10000 ? 1 : 2) + ' kHz' : (v < 10 ? v.toFixed(2) : Math.round(v)) + ' Hz';
      case 'ms': return v >= 1000 ? (v / 1000).toFixed(2) + ' s' : (v < 10 ? v.toFixed(2) : Math.round(v)) + ' ms';
      case 's': return v.toFixed(v < 10 ? 2 : 1) + ' s';
      case 'dB': return (v > 0.05 ? '+' : '') + v.toFixed(1) + ' dB';
      case '%': return Math.round(v) + ' %';
      case ':1': return v.toFixed(v < 10 ? 1 : 0) + ':1';
      default: return (Math.abs(v) < 10 && !Number.isInteger(v) ? v.toFixed(2) : Math.round(v)) + (q.unit ? ' ' + q.unit : '');
    }
  }
  const readout = (s, key) => { const q = spec(s, key); return q ? fmt(s, q, norm(s, key)) : ''; };

  // ---- one stylesheet for every face
  const css = document.createElement('style');
  css.textContent = `
  .fx-frame{cursor:grab;border-radius:12px;overflow:hidden;background:linear-gradient(180deg,var(--fbg),var(--fbg2));color:var(--fink);
    box-shadow:0 18px 40px rgba(0,0,0,.45),inset 0 0 0 1px rgba(255,255,255,.06);user-select:none}
  .fx-frame.fx-over{box-shadow:0 0 0 4px var(--facc),0 18px 40px rgba(0,0,0,.45)}
  .fx-frame.fx-dragging{opacity:.55}
  .fx-frame>*,.fx-frame .fxn>*,.fx-frame .fx-sub>*{position:absolute}
  .fx-head{left:0;top:0;right:0;height:64px;border-bottom:1px solid rgba(127,127,127,.18)}
  .fx-name,.fx-screen{cursor:default}.fx-name{cursor:pointer}
  .fx-grip{left:16px;top:18px;height:28px;font:600 17px/28px 'Segoe UI',sans-serif;color:var(--fink2);cursor:grab;letter-spacing:.04em;touch-action:none}
  .fx-name{left:140px;top:13px;height:38px;font-size:25px;line-height:38px;font-weight:700;cursor:pointer;white-space:nowrap;letter-spacing:.04em}
  .fx-name:hover,.fxs:hover,.fxg:hover{text-decoration:underline;text-underline-offset:5px}
  .fx-t{white-space:nowrap;pointer-events:none;line-height:1.15}
  .fxc{touch-action:none;cursor:ns-resize}
  .fxk svg{position:absolute;left:0;top:0;width:100%;height:100%;overflow:visible}
  .fxk .cap{position:absolute;pointer-events:none}
  .fxk .fx-inner{position:absolute;left:0;right:0;top:50%;transform:translateY(-50%);text-align:center;font-weight:700;color:#fff;pointer-events:none}
  .fxh{cursor:ew-resize}
  .fxv i,.fxh i,.fxv b,.fxh b{position:absolute;display:block;pointer-events:none}
  .fxs,.fxg,.fxp,.fxt{cursor:pointer}
  .fxs{white-space:nowrap;font-weight:700}
  .fxg{display:flex;align-items:center;justify-content:center;font-weight:700;letter-spacing:.04em;box-shadow:inset 0 0 0 1.5px currentColor}
  .fxb i{position:absolute;left:0;right:0;bottom:0;pointer-events:none}
  .fxb .fx-corner{position:absolute;left:0;top:0;width:100%;height:100%;pointer-events:none}
  .fx-screen{border-radius:6px;overflow:hidden}
  .fx-screen canvas{width:100%;height:100%;display:block}
  .fx-drop{position:absolute;inset:0;display:none;align-items:center;justify-content:center;font:700 24px 'Segoe UI',sans-serif;letter-spacing:.08em;
    color:var(--facc);background:rgba(0,0,0,.72);border:2px dashed var(--facc);border-radius:6px;pointer-events:none}
  .fx-screen.drag .fx-drop{display:flex}
  .fx-meter i{position:absolute;left:0;right:0;bottom:0;display:block}
  .fx-route.fx-over{box-shadow:0 0 0 3px var(--accent)}
  .fxp{border-radius:50%;box-shadow:inset 0 0 0 3px var(--fink2)}
  .fxp i{position:absolute;inset:0;background:var(--fink2);-webkit-mask:url(fx-power-glyph.svg) center/100% 100% no-repeat;mask:url(fx-power-glyph.svg) center/100% 100% no-repeat;pointer-events:none}
  .fxp.on{background:var(--facc);box-shadow:none}.fxp.on i{background:var(--fbg2)}
  .fx-frame.band .fxp{box-shadow:inset 0 0 0 3px rgba(255,255,255,.55)}.fx-frame.band .fxp i{background:rgba(255,255,255,.6)}
  .fx-frame.band .fxp.on{background:rgba(255,255,255,.3)}.fx-frame.band .fxp.on i{background:#fff}
  .fx-menu{position:absolute;z-index:20;min-width:190px;padding:6px 0;border-radius:8px;background:#1C1E23;box-shadow:0 12px 30px rgba(0,0,0,.55),inset 0 0 0 1px rgba(255,255,255,.1);font:600 15px 'Segoe UI',sans-serif;color:#E8EAF0}
  .fx-menu div{position:static;padding:6px 16px;cursor:pointer;white-space:nowrap}.fx-menu div:hover{background:rgba(255,255,255,.1)}
  .fx-menu .sep{height:1px;padding:0;margin:5px 0;background:rgba(255,255,255,.12);cursor:default}
  `;
  document.head.appendChild(css);

  // ---- builders: every position is relative to the slot frame
  const at = (x, y, w, h) => 'left:' + Math.round(x) + 'px;top:' + Math.round(y) + 'px;' + (w != null ? 'width:' + Math.round(w) + 'px;' : '') + (h != null ? 'height:' + Math.round(h) + 'px;' : '');
  const esc = (v) => String(v).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
  function caseText(s, str) {
    const c = CASE[skinName(s)];
    return c === 'lower' ? str.toLowerCase() : c === 'cap' ? str.toLowerCase().replace(/(^|[\s/-])\w/g, (m) => m.toUpperCase()) : str.toUpperCase();
  }
  const T = (x, y, w, str, o = {}) => '<div class="fx-t" style="' + at(x, y, w) + 'text-align:' + (o.align || 'center') + ';font-size:' + (o.size || 15) + 'px;font-weight:' + (o.weight || 600)
    + ';color:' + (o.color || 'var(--fink2)') + ';' + (o.font ? 'font-family:' + o.font + ';' : '') + (o.italic ? 'font-style:italic;' : '') + (o.ls != null ? 'letter-spacing:' + o.ls + 'em;' : '') + '"'
    + (o.val ? ' data-val="' + o.val + '"' : '') + '>' + esc(str) + '</div>';
  const arcPath = (a0, a1, r = 46) => {
    const p = (a) => [50 + Math.sin(a * Math.PI / 180) * r, 50 - Math.cos(a * Math.PI / 180) * r];
    const [x0, y0] = p(a0), [x1, y1] = p(a1);
    return 'M' + x0.toFixed(2) + ' ' + y0.toFixed(2) + 'A' + r + ' ' + r + ' 0 ' + (Math.abs(a1 - a0) > 180 ? 1 : 0) + ' ' + (a1 > a0 ? 1 : 0) + ' ' + x1.toFixed(2) + ' ' + y1.toFixed(2);
  };
  const A0 = -140, A1 = 140;
  function builders(s) {
    const K = skin(s), L = (key, fb) => caseText(s, fb || spec(s, key)?.label || '');
    return {
      K, T,
      // flat knob: generated cap rotating inside a live value arc; label and read-out beneath
      knob(key, x, y, d, o = {}) {
        const pad = Math.round(d * (o.pad ?? 0.12));
        let dots = '';
        if (o.dots) for (let i = 0; i <= 10; i++) { const a = (A0 + (A1 - A0) * i / 10) * Math.PI / 180; dots += '<circle cx="' + (50 + Math.sin(a) * 58).toFixed(1) + '" cy="' + (50 - Math.cos(a) * 58).toFixed(1) + '" r="' + (i % 5 ? 1.3 : 2.2) + '" fill="' + K.ink + '"/>'; }
        let h = '<div class="fxc fxk" data-key="' + key + '" data-kind="knob" style="' + at(x, y, d, d) + '">'
          + '<svg viewBox="0 0 100 100">' + dots + (o.noArc ? '' : '<path d="' + arcPath(A0, A1) + '" fill="none" stroke="' + K.ink2 + '" stroke-opacity=".28" stroke-width="' + (o.arcW || 4) + '" stroke-linecap="round"/>'
          + '<path class="val" fill="none" stroke="' + (o.colour || K.accent) + '" stroke-width="' + (o.arcW || 4) + '" stroke-linecap="round"/>') + '</svg>'
          + (o.ring ? '<svg viewBox="0 0 100 100"><circle cx="50" cy="50" r="40" fill="none" stroke="' + K.ink + '" stroke-width="2.2"/></svg>'
            : '<img class="cap" src="fx-knob-' + (o.cap || K.knob) + '.svg" alt="" style="' + at(pad, pad, d - 2 * pad, d - 2 * pad) + '">')
          + (o.inner ? '<div class="fx-inner" style="font-size:' + Math.round(d * (o.ring ? 0.2 : 0.15)) + 'px;' + (o.ring ? 'color:' + K.ink + ';font-weight:600' : '') + '" data-val="' + key + '"></div>' : '') + '</div>';
        const g = o.gap ?? 10, ls = o.lsize || 15;
        if (!o.noLabel) h += T(x - 60, y + d + g, d + 120, L(key, o.label), { size: ls });
        if (!o.inner && !o.noValue) h += T(x - 60, y + d + g + ls + 8, d + 120, '', { size: o.vsize || 16, weight: 700, color: 'var(--fink)', val: key });
        return h;
      },
      // hairline vertical slider; the handle is a small bar, or a value tag (Goodhertz-style)
      vslider(key, x, y, h, o = {}) {
        const hd = o.tag
          ? '<b class="hd" data-val="' + key + '" style="top:0;left:-14px;width:68px;height:30px;border-radius:5px;background:' + K.accent + ';color:#fff;font:700 15px/30px \'Segoe UI\',sans-serif;text-align:center"></b>'
          : '<b class="hd" style="top:0;left:11px;width:18px;height:8px;border-radius:2px;background:' + (o.colour || K.ink) + '"></b>';
        return '<div class="fxc fxv" data-key="' + key + '" data-kind="vs" data-travel="' + h + '" style="' + at(x - 20, y, 40, h) + '">'
          + '<i style="left:19px;width:2px;top:0;bottom:0;background:' + K.ink2 + ';opacity:.45"></i>'
          + (o.fill === false ? '' : '<i class="fill" style="left:18px;width:4px;bottom:0;background:' + (o.colour || K.accent) + '"></i>') + hd + '</div>'
          + (o.label === false ? '' : T(x - 70, y + h + 18, 140, L(key, o.label), { size: 15 }) + (o.tag || o.noValue ? '' : T(x - 70, y + h + 40, 140, '', { size: 16, weight: 700, color: 'var(--fink)', val: key })));
      },
      // horizontal: plain, gradient (LAYER MIX: blue UPPER -> salmon LOWER) or tagged
      hslider(key, x, y, w, o = {}) {
        const track = o.grad ? 'background:linear-gradient(90deg,' + (typeof o.grad === 'string' ? o.grad : UPPER_COLOUR) + ',' + LOWER_COLOUR + ');height:4px;top:16px' : 'background:' + K.ink2 + ';opacity:.5;height:2px;top:17px';
        const hd = o.tag
          ? '<b class="hd" data-val="' + key + '" style="top:1px;margin-left:-36px;width:72px;height:34px;border-radius:5px;background:' + K.accent + ';color:#fff;font:700 15px/34px \'Segoe UI\',sans-serif;text-align:center;transform:rotate(-35deg)"></b>'
          : '<b class="hd" style="top:4px;margin-left:-3px;width:6px;height:28px;border-radius:2px;background:' + (o.colour || '#FFFFFF') + '"></b>';
        return '<div class="fxc fxh" data-key="' + key + '" data-kind="hs" data-travel="' + w + '" style="' + at(x, y - 18, w, 36) + '"><i style="left:0;right:0;' + track + '"></i>' + hd + '</div>';
      },
      // a draggable number: value above, label below (Arturia-style side read-outs)
      num(key, x, y, o = {}) {
        const w = o.w || 150, al = o.align || 'left';
        return '<div class="fxc fxn" data-key="' + key + '" data-kind="num" style="' + at(x, y, w, 64) + '">'
          + T(0, 0, w, '', { size: o.size || 24, weight: 600, color: 'var(--fink)', val: key, align: al })
          + T(0, (o.size || 24) + 10, w, L(key, o.label), { size: 15, align: al }) + '</div>';
      },
      // square value box whose fill rises with the value (autochroma-style)
      box(key, x, y, w, h, o = {}) {
        const br = 'M0 12V0H12M' + (w - 12) + ' 0H' + w + 'V12M' + w + ' ' + (h - 12) + 'V' + h + 'H' + (w - 12) + 'M12 ' + h + 'H0V' + (h - 12);
        return '<div class="fxc fxb" data-key="' + key + '" data-kind="box" style="' + at(x, y, w, h) + '">'
          + '<i class="fill" style="background:' + (o.colour || K.accent) + ';opacity:.75"></i>'
          + '<svg class="fx-corner" viewBox="0 0 ' + w + ' ' + h + '"><path d="' + br + '" fill="none" stroke="rgba(62,74,63,.55)" stroke-width="3"/></svg>'
          + '<div class="fx-t" data-val="' + key + '" style="position:absolute;left:0;width:' + w + 'px;top:' + (h / 2 - 12) + 'px;text-align:center;font-size:20px;font-weight:700;color:var(--fink)"></div></div>'
          + T(x - 20, y + h + 8, w + 40, L(key, o.label), { size: 16 });
      },
      // eight squares round a value box, lit in turn as the value rises (Efx TONE)
      squares(key, x, y, d) {
        const c = d / 3; let h = '<div class="fxc fxq" data-key="' + key + '" data-kind="sq" style="' + at(x, y, d, d) + '">';
        [[0, 0], [1, 0], [2, 0], [2, 1], [2, 2], [1, 2], [0, 2], [0, 1]].forEach(([cx, cy], i) => {
          h += '<i data-sq="' + i + '" style="position:absolute;' + at(cx * c + c * 0.22, cy * c + c * 0.22, c * 0.56, c * 0.56) + 'box-sizing:border-box;border:2px solid ' + K.ink + '"></i>';
        });
        return h + '<div class="fx-t" data-val="' + key + '" style="position:absolute;' + at(c * 0.9, c * 1.18, c * 1.2, c * 0.64) + 'background:' + K.ink + ';color:#fff;font-size:' + Math.round(c * 0.3) + 'px;font-weight:700;text-align:center;line-height:' + Math.round(c * 0.64) + 'px"></div></div>';
      },
      // a small coloured square that steps a choice; shows a short label for the current step
      chip(key, x, y, shorts, colour) {
        return '<div class="fxc fxs" data-key="' + key + '" data-kind="chip" data-shorts="' + shorts.join(',') + '" title="Click to change, right-click to go back" style="' + at(x, y, 36, 36)
          + 'background:' + colour + ';color:#fff;font:700 16px/36px \'Segoe UI\',sans-serif;text-align:center;border-radius:3px"></div>';
      },
      // a stepped choice as a word: click steps on, right-click steps back
      sel(key, x, y, o = {}) {
        return '<div class="fxc fxs" data-key="' + key + '" data-kind="sel" data-arrows="' + (o.arrows ? 1 : '') + '" title="Click to change, right-click to go back" style="' + at(x, y)
          + 'height:32px;line-height:32px;font-size:' + (o.size || 18) + 'px;color:' + (o.colour || K.accent) + ';' + (o.align === 'right' ? 'transform:translateX(-100%);' : '') + '"></div>';
      },
      // every option shown, the current one filled (Efx-style mode list)
      grid(key, x, y, cols, cw, ch) {
        let h = '';
        spec(s, key).steps.forEach((name, i) => { h += '<div class="fxc fxg" data-key="' + key + '" data-kind="grid" data-idx="' + i + '" style="' + at(x + (i % cols) * cw, y + Math.floor(i / cols) * ch, cw, ch) + 'font-size:15px"></div>'.replace('></div>', '>' + esc(caseText(s, name)) + '</div>'); });
        return h;
      },
      // mini bat switch for a two-way choice (hardware-style skins)
      toggle(key, x, y, o = {}) {
        const q = spec(s, key);
        return T(x - 40, y - 26, 110, caseText(s, q.steps[1]), { size: 12 }) + '<img class="fxc fxt" data-key="' + key + '" data-kind="tog" alt="" src="fx-toggle-up.svg" style="' + at(x, y, 30, 54) + '">'
          + T(x - 40, y + 60, 110, caseText(s, q.steps[0]), { size: 12 }) + (o.label ? T(x - 40, y + 84, 110, caseText(s, o.label), { size: 13, color: 'var(--fink)' }) : '');
      },
      screen(x, y, w, h) {
        return '<div class="fx-screen" data-fx-screen="' + s + '" style="' + at(x, y, w, h) + 'background:' + K.screen + '"><canvas id="fx' + s + '-canvas" width="' + Math.round(w * 2) + '" height="' + Math.round(h * 2) + '"></canvas><div class="fx-drop">DROP A .WAV HERE</div></div>';
      },
      meter(x, y, w, h, idx, colour) {
        return '<div class="fx-meter" data-vis="' + idx + '" style="' + at(x, y, w, h) + 'background:rgba(0,0,0,.08)"><i style="background:' + colour + ';height:0"></i></div>';
      }
    };
  }

  // ---- the faces: the frame body between the header (64 px) and the LAYER MIX strip
  const has = (s, p) => !!param(s, p);
  const LAYOUTS = {
    // house (and anything unbranded): display on top, DRY / WET beside it, two rows of knobs
    grid(s, W, H, b) {
      let h = b.screen(24, 84, W - 140, 330);
      ['p9', 'p10', 'p11'].forEach((p, j) => { if (has(s, p)) h += b.sel(p, 42 + j * 210, 92); });
      h += b.vslider('mix', W - 62, 104, 270, { label: 'dry / wet' });
      const ks = ['p1', 'p2', 'p3', 'p4', 'p5', 'p6', 'p7', 'p8'].filter((p) => has(s, p)), cw = (W - 48) / 4;
      ks.forEach((p, i) => { h += b.knob(p, 24 + (i % 4) * cw + cw / 2 - 40, 480 + Math.floor(i / 4) * 170, 80); });
      return h;
    },
    // Rev-style: big central visual, thin sliders either side, three white knobs, side numbers
    ocean(s, W, H, b) {
      let h = b.screen(W * 0.17, 84, W * 0.66, 370);
      h += b.vslider('p4', 72, 110, 250, { colour: '#FFFFFF', fill: false, label: 'Macro', noValue: true }) + b.sel('p9', 24, 420, { arrows: true, colour: b.K.ink, size: 24 });
      h += b.vslider('mix', W - 72, 110, 250, { colour: '#FFFFFF', fill: false, label: 'Mix', noValue: true }) + b.sel('p10', W - 24, 420, { colour: b.K.ink2, size: 16, align: 'right' });
      ['p1', 'p2', 'p3'].forEach((p, i) => { h += b.knob(p, W / 2 - 55 + (i - 1) * 165, 500, 110, { pad: 0.14, arcW: 5, lsize: 16 }); });
      h += b.num('p5', 30, 500) + b.num('p8', 30, 610) + b.num('p7', W - 180, 500, { align: 'right' }) + b.num('p6', W - 180, 610, { align: 'right' });
      return h;
    },
    // Efx-style: a clean main view (mode title, TONE squares | MACRO field | SPACE rings in soft cards,
    // DRY / WET at the top right); the eight settings live behind ADVANCED, as in the original
    ambient(s, W, H, b) {
      const wide = { size: 30, weight: 900, color: 'var(--fink)', font: FONTS.wide, ls: 0.02 }, adv = view.ambAdv[s];
      const card = (x, y, w, h) => '<div style="' + at(x, y, w, h) + 'border-radius:14px;background:rgba(0,0,0,.045)"></div>';
      let h = b.sel('p9', 26, 78, { arrows: true, size: 30, colour: b.K.ink });
      h += b.T(30, 116, 300, '', { size: 13, weight: 700, color: b.K.accent, align: 'left', val: 'ambsub', ls: 0.1 });
      h += b.T(W - 230, 86, 170, '', { size: 22, weight: 700, color: 'var(--fink)', align: 'right', val: 'mix' }) + b.T(W - 230, 116, 170, 'DRY / WET', { size: 12, align: 'right' });
      h += b.vslider('mix', W - 38, 82, 70, { label: false, colour: b.K.ink });
      const top = 170, bottom = adv ? 548 : 760, ch = bottom - top;
      h += card(20, top, 200, ch) + card(232, top, W - 464, ch) + card(W - 220, top, 200, ch);
      h += b.T(20, top + 20, 200, 'TONE', wide) + b.T(W - 220, top + 20, 200, 'SPACE', wide) + b.T(232, top + 16, W - 464, 'MACRO', { size: 13, weight: 700, ls: 0.3 });
      h += b.squares('p12', 40, top + (ch - 160) / 2 + 20, 160);
      h += b.knob('p13', W - 200, top + (ch - 160) / 2 + 20, 160, { inner: true, noLabel: true, arcW: 3, pad: 0.04 });
      h += b.screen(250, top + 44, W - 500, ch - 64);
      h += '<div class="fxc fxs" data-kind="ambadv" data-key="ambadv" style="' + at(24, bottom + 14) + 'height:28px;line-height:28px;font-size:14px;letter-spacing:.2em;color:' + b.K.ink + '">' + (adv ? 'ADVANCED ▴' : 'ADVANCED ▾') + '</div>';
      if (adv) {
        const rows = ['p1', 'p2', 'p3', 'p4', 'p5', 'p6', 'p7', 'p8'], cw = (W - 60) / 2;
        rows.forEach((p, i) => {
          const x = 24 + Math.floor(i / 4) * (cw + 12), y = 616 + (i % 4) * 46;
          h += b.T(x, y - 10, 110, caseText(s, spec(s, p).label), { size: 13, align: 'left', color: 'var(--fink)' });
          h += b.hslider(p, x + 116, y, cw - 210, { colour: b.K.ink });
          h += b.T(x + cw - 88, y - 10, 84, '', { size: 14, weight: 700, color: 'var(--fink)', align: 'right', val: p });
        });
      }
      return h;
    },
    // VintageVerb-style: MIX / PREDELAY, a big ring DECAY, then outlined boxes of paired knobs
    valley(s, W, H, b) {
      const box = (x, y, w, h, title, c) => '<div style="' + at(x, y, w, h) + 'border:3px solid ' + c + ';border-radius:20px"></div>' + b.T(x, y + 14, w, title, { size: 17, weight: 800, color: 'var(--fink)', ls: 0.06 });
      const pair = (x, y, w, p1, p2, c, cap) => b.knob(p1, x + w / 2 - 32, y + 50, 64, { colour: c, cap, lsize: 13, vsize: 14, gap: 4 }) + b.knob(p2, x + w / 2 - 32, y + 190, 64, { colour: c, cap, lsize: 13, vsize: 14, gap: 4 });
      let h = b.knob('mix', 40, 100, 84, { label: 'MIX', colour: '#B07CFF', cap: 'violet', lsize: 15 }) + b.knob('p1', 40, 262, 84, { colour: '#B07CFF', cap: 'violet', lsize: 15 });
      h += b.knob('p2', 184, 108, 230, { colour: '#6A80FF', cap: 'vdecay', pad: 0, noArc: true, lsize: 18, vsize: 18 });
      h += box(446, 88, 190, 330, 'DAMPING', '#9B5CFF') + pair(446, 104, 190, 'p14', 'p15', '#B07CFF', 'violet');
      h += b.T(656, 128, W - 680, 'MODE', { size: 14, align: 'left', color: 'var(--fink2)', ls: 0.2 }) + b.sel('p9', 656, 150, { size: 22, colour: '#8C95FF' });
      h += b.T(656, 238, W - 680, 'COLOR', { size: 14, align: 'left', color: 'var(--fink2)', ls: 0.2 }) + b.sel('p10', 656, 260, { size: 22, colour: '#5FD6FF' });
      const bw = (W - 48 - 36) / 4, cols = [['SHAPE', 'p3', 'p4', '#7C6CFF', 'azure'], ['DIFF', 'p5', 'p6', '#6A80FF', 'azure'], ['MOD', 'p7', 'p8', '#4F9BFF', 'cyan'], ['EQ', 'p12', 'p13', '#2CD3FF', 'cyan']];
      cols.forEach(([t, a, c2, col, cap], i) => { const x = 24 + i * (bw + 12); h += box(x, 446, bw, 360, t, col) + pair(x, 470, bw, a, c2, col, cap); });
      return h;
    },
    // Nudistort-style: a white card; Distort | the painting | Delay; GLOBAL and GAIN STAGE along the bottom
    nude(s, W, H, b) {
      const it = { size: 28, weight: 700, color: 'var(--fink)', font: FONTS.serif, italic: true, align: 'left' };
      const ring = (p, x, y, d, label) => b.T(x - 40, y - 24, d + 80, caseText(s, label || spec(s, p).label), { size: 13, color: 'var(--fink2)' })
        + b.knob(p, x, y, d, { ring: true, inner: true, noLabel: true, arcW: 3, pad: 0 });
      let h = b.T(26, 74, 150, 'Distort', it) + b.T(W - 150, 74, 130, 'Delay', { ...it, align: 'right' });
      h += b.chip('p9', 30, 122, ['F', 'O', 'C', 'S'], '#1FB6C9') + b.chip('p10', 80, 122, ['T', 'U'], '#141414');
      h += b.T(26, 164, 110, 'type · mode', { size: 11, align: 'left' });
      [['mix', 'mix'], ['p1'], ['p2'], ['p3']].forEach(([p, lab], i) => { h += ring(p, 44, 220 + i * 118, 82, lab); });
      h += '<div style="' + at(186, 80, W - 372, 590) + 'box-shadow:0 0 0 2px #141414"></div>' + b.screen(186, 80, W - 372, 590);
      [['p4', 'mix'], ['p5', 'time'], ['p7'], ['p6']].forEach(([p, lab], i) => { h += ring(p, W - 126, 220 + i * 118, 82, lab); });
      h += '<div style="' + at(20, 700, W - 40, 118) + 'border-radius:12px;box-shadow:inset 0 0 0 2px #141414"></div>';
      h += b.T(20, 684, (W - 40) * 0.55, 'GLOBAL PARAMETERS', { size: 11, weight: 700, ls: 0.25 }) + b.T(20 + (W - 40) * 0.55, 684, (W - 40) * 0.45, 'GAIN STAGE', { size: 11, weight: 700, ls: 0.25 });
      [['p8'], ['p12'], ['p13']].forEach(([p], i) => { h += ring(p, 70 + i * 140, 736, 64); });
      [['p14'], ['p15']].forEach(([p], i) => { h += ring(p, 20 + (W - 40) * 0.55 + 60 + i * 140, 736, 64); });
      return h;
    },
    // console module:    // console module: no screen; one huge LEVEL knob with a dotted scale, small knobs, bat switches
    tuba(s, W, H, b) {
      let h = b.T(0, 84, W, 'TUBE CHANNEL', { size: 15, color: 'var(--fink)', ls: 0.3 }) + b.T(0, 106, W, 'MIC / LINE AMP · 2 BAND EQ', { size: 11, ls: 0.25 });
      h += b.knob('p1', 70, 170, 96, { pad: 0.03, noArc: true, dots: true });
      h += b.knob('p3', 44, 390, 70, { pad: 0.03, noArc: true, dots: true }) + b.knob('p4', 160, 390, 70, { pad: 0.03, noArc: true, dots: true });
      h += b.toggle('p9', 70, 590, { label: 'mode' }) + b.toggle('p10', 180, 590, { label: 'pad' });
      h += b.knob('p2', W / 2 - 140, 190, 280, { pad: 0.04, noArc: true, dots: true, lsize: 18, vsize: 18 });
      h += b.knob('p5', W - 250, 250, 110, { pad: 0.03, noArc: true, dots: true });
      h += b.vslider('mix', W - 70, 180, 300, { colour: '#FFFFFF', label: 'mix' });
      return h;
    },
    // FF-style: the graph fills the window, controls on a floating bar underneath
    fab(s, W, H, b) {
      const d = def(s);
      let h = b.screen(0, 64, W, 580);
      h += '<div style="' + at(20, 660, W - 40, 170) + 'border-radius:12px;background:rgba(255,255,255,.045);box-shadow:inset 0 0 0 1px rgba(255,255,255,.08)"></div>';
      if (d.id === 'proq') {
        h += '<div id="fx' + s + '-pqbar" class="fx-sub" style="left:0;top:0">' + pqBar(s, W, b) + '</div>';
      } else {
        h += b.sel('p9', 30, 80, { size: 20 });
        const ks = ['p1', 'p2', 'p3', 'p4', 'p5', 'p6', 'p7', 'p8', 'mix'], cw = (W - 60) / ks.length, band = { p3: '#FF8A3D', p4: '#FFC94A', p5: '#6EC6FF' };
        ks.forEach((p, i) => { h += b.knob(p, 30 + i * cw + cw / 2 - 28, 682, 56, { lsize: 11, vsize: 13, gap: 6, colour: band[p] }); });
      }
      return h;
    },
    // Saturn-style: burgundy, a borderless band display with a drag handle per band, and a
    // floating band panel whose big red-ringed DRIVE belongs to the selected band
    saturn(s, W, H, b) {
      let h = b.screen(0, 64, W, 470);
      h += '<div style="' + at(24, 552, W - 48, 262) + 'border-radius:14px;background:rgba(0,0,0,.34);box-shadow:inset 0 0 0 1px rgba(255,255,255,.08)"></div>';
      SAT_BANDS.forEach((name, i) => { h += '<div class="fxc fxs" data-key="sband" data-kind="sband" data-idx="' + i + '" style="' + at(48 + i * 74, 568) + 'height:30px;line-height:30px;font-size:16px"></div>'.replace('></div>', '>' + name + '</div>'); });
      h += b.sel('p9', 48, 612, { size: 18, colour: b.K.ink });
      h += b.knob('p6', 70, 672, 62, { colour: '#4FA3FF', lsize: 13, gap: 6 });
      h += b.knob('sdrive', W / 2 - 110, 574, 150, { label: 'DRIVE', colour: b.K.accent, arcW: 9, pad: 0.13, lsize: 15, gap: 6 });
      h += b.vslider('p7', W / 2 + 100, 590, 130, { label: 'TONE', colour: '#CFC8C4', fill: false });
      h += b.knob('p8', W - 250, 610, 70, { label: 'LEVEL', colour: b.K.accent, arcW: 7, lsize: 13, gap: 6 });
      h += b.knob('mix', W - 140, 610, 70, { label: 'MIX', colour: '#4FA3FF', arcW: 7, lsize: 13, gap: 6 });
      return h;
    },
    // Goodhertz-style: paper, columns of hairline faders with value tags, a tilted DRY / WET tag
    ghz(s, W, H, b) {
      const vulf = def(s).id === 'vulf', font = vulf ? FONTS.typewriter : FONTS.cond;
      const cols = vulf ? [['p1', 'in'], ['p2', 'comp'], ['p3', 'atk'], ['p4', 'rel'], ['p5', 'wow'], ['p6', 'lofi'], ['p7', 'out']]
        : [['p1', 'THRESH'], ['p2', 'RATIO'], ['p3', 'ATTACK'], ['p4', 'RELEASE'], ['p5', 'COLOR'], ['p6', 'WARMTH'], ['p7', 'VIBE'], ['p8', 'OUT']];
      const cw = (W - 40) / cols.length;
      let h = '';
      cols.forEach(([p, lab], i) => {
        const x = 20 + i * cw;
        if (i) h += '<div style="' + at(x, 80, 1, 520) + 'background:' + b.K.ink2 + ';opacity:.3"></div>';
        h += b.T(x, 92, cw, lab, { size: vulf ? 22 : 18, weight: vulf ? 400 : 700, color: 'var(--fink)', font, italic: vulf });
        h += b.vslider(p, x + cw / 2, 170, 330, { tag: true, label: false, fill: false });
      });
      if (!vulf) h += b.meter(20 + (cols.length - 1) * cw + cw / 2 + 34, 170, 8, 330, 1, '#E0454F') + b.sel('p9', W / 2, 548, { size: 17, colour: b.K.ink });
      h += '<div style="' + at(0, 610, W, 1) + 'background:' + b.K.ink2 + ';opacity:.35"></div>';
      h += b.T(24, 668, 90, 'dry', { size: 22, font, color: 'var(--fink)', weight: 400, align: 'left', italic: vulf }) + b.T(W - 114, 668, 90, 'wet', { size: 22, font, color: 'var(--fink)', weight: 400, align: 'right', italic: vulf });
      h += b.hslider('mix', 120, 684, W - 240, { tag: true });
      return h;
    },
    // autochroma-style: grain field on paper; one stream - its pitch big on the left, its settings as boxes
    chroma(s, W, H, b) {
      let h = b.screen(0, 64, W, 330);
      h += b.box('p9', 24, 424, 112, 112, { label: 'pitch', colour: STREAM_COLOURS[0] }) + b.box('p11', 24, 600, 112, 84, { label: 'fine' });
      const ks = ['p1', 'p2', 'p3', 'p4', 'p5', 'p6', 'p7', 'p8'], cw = (W - 320) / 4;
      ks.forEach((p, i) => { h += b.box(p, 160 + (i % 4) * cw + cw / 2 - 50, 440 + Math.floor(i / 4) * 170, 100, 84, { colour: p === 'p6' ? STREAM_COLOURS[1] : undefined }); });
      h += b.box('p10', W - 136, 424, 100, 84, { label: 'shape', colour: STREAM_COLOURS[2] }) + b.box('mix', W - 136, 560, 100, 150, { colour: '#9A9A96', label: 'mix' });
      return h;
    }
  };

  // PRO-Q's floating bar: the selected band's shape and FREQ / GAIN / Q, and OUTPUT
  function pqBar(s, W, b) {
    const on = Math.round(val(s, 'p27')), sel = view.band[s] >= 0 && (on & (1 << view.band[s])), acc = b.K.accent;
    let h = '';
    if (sel) {
      const gain = usesGain(val(s, PQ_SHAPE[view.band[s]]));
      h += '<div style="' + at(46, 690, 14, 14) + 'border-radius:50%;background:' + acc + '"></div>';
      h += b.T(70, 682, 160, '', { size: 18, weight: 700, color: 'var(--fink)', align: 'left', val: 'bandname' });
      h += b.sel('band3', 46, 718, { size: 16, colour: acc });
      h += '<div style="' + at(270, 676, 1, 130) + 'background:' + b.K.ink2 + ';opacity:.3"></div>';
      ['band0', 'band1', 'band2'].forEach((k, i) => { h += '<div class="fx-sub" style="position:absolute;left:0;top:0;' + (i === 1 && !gain ? 'opacity:.3;pointer-events:none' : '') + '">' + b.knob(k, 310 + i * 140, 680, 66, { label: ['FREQ', 'GAIN', 'Q'][i], lsize: 13, gap: 6 }) + '</div>'; });
    } else h += b.T(46, 694, 560, on ? 'click a band to edit it' : 'double-click the graph to add a band', { size: 18, align: 'left', color: 'var(--fink)' })
      + b.T(46, 728, 560, 'drag to move · wheel for Q · right-click for its shape · double-click to remove', { size: 13, align: 'left' });
    h += '<div style="' + at(W - 190, 676, 1, 130) + 'background:' + b.K.ink2 + ';opacity:.3"></div>';
    return h + b.knob('p4', W - 136, 680, 66, { lsize: 13, gap: 6 });
  }
  function pqRefresh(s) {
    const bar = $('fx' + s + '-pqbar'), fr = $('fx' + s + '-frame');
    if (bar && fr) bar.innerHTML = pqBar(s, +fr.dataset.w, builders(s));
    paint(s);
  }
  // ---- rendering a slot
  function render(s) {
    const fr = $('fx' + s + '-frame'); if (!fr) return;
    const d = def(s), K = skin(s), sk = skinName(s), W = +fr.dataset.w, H = +fr.dataset.h, b = builders(s);
    const colourHead = sk === 'vulf' || sk === 'faraday';   // those faces carry their colour in a band
    fr.classList.toggle('band', colourHead);
    for (const [k, v] of [['--fbg', K.bg], ['--fbg2', K.bg2], ['--fink', K.ink], ['--fink2', K.ink2], ['--facc', K.accent]]) fr.style.setProperty(k, v);
    fr.style.fontFamily = FONTS[K.font];
    let h = '<div class="fx-head" style="' + (colourHead ? 'background:' + K.accent + ';' : '') + '"></div>';
    h += '<div class="fx-grip" data-fx-grip="' + s + '" title="Drag onto another slot to swap their order" style="' + (colourHead ? 'color:rgba(255,255,255,.8)' : '') + '">⠿ fx ' + (s + 1) + '</div>';
    h += '<div class="fxc fxp" data-key="on" data-kind="pow" title="On / off" style="' + at(88, 16, 32, 32) + '"><i></i></div>';
    h += '<div class="fx-name" data-fx-open="' + s + '" title="Choose the effect for this slot" style="font-family:' + FONTS[K.nameFont || K.font] + ';' + (K.nameFont === 'serif' ? 'font-style:italic;font-weight:400;font-size:30px;' : '')
      + 'color:' + (K.nameColor || (colourHead ? '#FFFFFF' : lum(K.bg) > 0.5 ? K.ink : K.accent)) + '">' + esc(d ? caseText(s, d.name) : 'none') + ' ▾</div>';
    if (d) h += LAYOUTS[K.layout](s, W, H - 110, b);
    else h += b.T(0, H / 2 - 60, W, 'no effect loaded', { size: 22 }) + b.T(0, H / 2 - 24, W, 'click the name above to choose one', { size: 16 });
    // LAYER MIX: UPPER (blue) ... LOWER (salmon); the middle gives both layers the full effect
    h += '<div style="' + at(0, H - 106, W, 1) + 'background:' + K.ink2 + ';opacity:.25"></div>';
    h += b.T(24, H - 94, 200, caseText(s, 'layer mix'), { size: 13, align: 'left' });
    const upper = lum(K.bg) > 0.5 ? K.ink : UPPER_COLOUR;   // cream UPPER would vanish on a light face
    const lw = Math.round(W * 0.5), lx = Math.round((W - lw) / 2);   // double-click the line to centre it
    h += b.T(lx - 150, H - 66, 132, caseText(s, 'upper'), { size: 22, weight: 800, color: upper, align: 'right' });
    h += b.hslider('layer', lx, H - 52, lw, { grad: upper });
    h += b.T(lx + lw + 18, H - 66, 132, caseText(s, 'lower'), { size: 22, weight: 800, color: LOWER_COLOUR, align: 'left' });
    h += b.T(lx, H - 30, lw, '', { size: 14, weight: 700, color: 'var(--fink2)', val: 'layer' });
    fr.innerHTML = h;
    view.built[s] = type(s);
  }
  function paint(s) {
    const fr = $('fx' + s + '-frame'); if (!fr) return;
    for (const el of fr.querySelectorAll('[data-kind]')) {
      if (el.dataset.kind === 'ambadv') continue;
      if (el.dataset.kind === 'sband') { const on = +el.dataset.idx === view.sband[s]; el.style.color = on ? skin(s).accent : skin(s).ink2; el.style.textDecoration = on ? 'underline' : 'none'; continue; }
      const key = el.dataset.key, q = spec(s, key); if (!q) continue;
      const n = norm(s, key);
      switch (el.dataset.kind) {
        case 'knob': {
          const cap = el.querySelector('.cap'), v = el.querySelector('.val'), ang = A0 + (A1 - A0) * n;
          if (cap) cap.style.transform = 'rotate(' + ang.toFixed(1) + 'deg)';
          const bip = !q.steps && q.lo < 0 && q.hi > 0 && Math.abs(q.lo + q.hi) < 1e-6;   // symmetric ranges grow from 12 o'clock
          if (v) v.setAttribute('d', bip ? arcPath(Math.min(0, ang), Math.max(0.01, ang)) : arcPath(A0, Math.max(A0 + 0.5, ang)));
          break;
        }
        case 'vs': {
          const tr = +el.dataset.travel, hd = el.querySelector('.hd'), fill = el.querySelector('.fill');
          if (hd) hd.style.top = ((1 - n) * tr - (hd.offsetHeight || 8) / 2) + 'px';
          if (fill) fill.style.height = (n * tr) + 'px';
          break;
        }
        case 'hs': { const hd = el.querySelector('.hd'); if (hd) hd.style.left = (n * +el.dataset.travel) + 'px'; break; }
        case 'box': { const f = el.querySelector('.fill'); if (f) f.style.height = (n * 100) + '%'; break; }
        case 'sq': el.querySelectorAll('[data-sq]').forEach((q2, i) => { q2.style.background = i < Math.round(n * 8) ? skin(s).accent : 'transparent'; q2.style.borderColor = i < Math.round(n * 8) ? skin(s).accent : skin(s).ink; }); break;
        case 'sel': { const t = caseText(s, q.steps[from(q, n)]); el.textContent = el.dataset.arrows ? '‹  ' + t + '  ›' : t + ' ▾'; break; }
        case 'grid': { const on = +el.dataset.idx === from(q, n); el.style.background = on ? skin(s).accent : 'transparent'; el.style.color = on ? '#FFFFFF' : skin(s).ink; break; }
        case 'chip': el.textContent = el.dataset.shorts.split(',')[from(q, n)] || ''; el.title = q.steps[from(q, n)]; break;
        case 'tog': el.src = from(q, n) ? 'fx-toggle-up.svg' : 'fx-toggle-down.svg'; break;
        case 'pow': el.classList.toggle('on', n >= 0.5); break;
      }
    }
    for (const el of fr.querySelectorAll('[data-val]'))
      el.textContent = el.dataset.val === 'bandname' ? 'BAND ' + (pqBand(s) + 1) : el.dataset.val === 'ambsub' ? AMB_SUB[val(s, 'p9')] || '' : readout(s, el.dataset.val);
  }

  // ---- the routing column and the slots
  function refresh() {
    const serial = store.read('fx.mode') < 0.5;
    $('pop-fx').classList.toggle('fx-mode-serial', serial); $('pop-fx').classList.toggle('fx-mode-parallel', !serial);
    actionState('act-fxSerial', serial); actionState('act-fxParallel', !serial);
    text('fx-mode-note', (serial ? 'The sound runs through FX 1, then FX 2, then FX 3.' : 'Each slot hears the dry sound; the three results are mixed.') + ' Drag an effect onto another to swap them.');
    for (let s = 0; s < 3; s++) {
      text('fx-route-name' + s, def(s)?.name || 'NONE');
      if (view.built[s] !== type(s)) render(s);
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
    refresh();
  }
  function openPicker(s) {
    view.open = s; text('fx-picker-title', 'FX ' + (s + 1) + ' TYPE'); $('fx-picker').hidden = false;
    all('.fx-tile').forEach((t) => t.classList.toggle('sel', +t.dataset.fxTile === type(s)));
  }
  all('[data-fx-tile]').forEach((el) => el.addEventListener('click', () => { if (view.open >= 0) choose(view.open, +el.dataset.fxTile); $('fx-picker').hidden = true; view.open = -1; }));
  all('[data-fx-close]').forEach((el) => el.addEventListener('click', () => { $('fx-picker').hidden = true; view.open = -1; }));

  // ---- swapping: drag FX n (its routing box or its slot's grip) onto another
  const SLOT_PARAMS = ['type', 'on', 'mix', 'layer', ...Array.from({ length: 28 }, (_, i) => 'p' + (i + 1))];
  async function swap(a, b) {
    if (a === b) return;
    if (hosted) await native('fxSwap', a, b);   // the processor swaps without resetting to defaults
    else for (const p of SLOT_PARAMS) { const va = store.read(pid(a, p)), vb = store.read(pid(b, p)); oneWrite(pid(a, p), vb); oneWrite(pid(b, p), va); }
    for (const k of ['band', 'sband', 'hist', 'grains']) [view[k][a], view[k][b]] = [view[k][b], view[k][a]];
    view.built = [-1, -1, -1];
    refresh();
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
    if (from < 0 && frame && !e.target.closest('[data-kind],.fx-screen,[data-fx-open],.fx-menu')) from = +frame.id.slice(2, 3);
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
  // PRO-Q band helpers
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
    m.innerHTML = q.steps.map((n, i) => '<div data-shape="' + i + '">' + (i === cur ? '✓ ' : '') + n.toLowerCase() + '</div>').join('') + '<div class="sep"></div><div data-del="1">delete band</div>';
    m.addEventListener('pointerdown', (ev) => {
      ev.stopPropagation(); const it = ev.target.closest('[data-shape],[data-del]'); if (!it) return;
      if (it.dataset.del) pqRemove(s, b); else { oneWrite(pid(s, PQ_SHAPE[b]), +it.dataset.shape / (q.steps.length - 1)); pqRefresh(s); }
      m.remove();
    });
    fr.appendChild(m);
    setTimeout(() => document.addEventListener('pointerdown', () => m.remove(), { once: true }), 0);
  }
  const lastTap = [null, null, null], lastCtl = [null, null, null];   // double-click detection (graph / controls)                              // PRO-Q double-click detection
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
        if (kind === 'ambadv') { view.ambAdv[s] = !view.ambAdv[s]; render(s); paint(s); return; }
        if (!q) return;
        if (kind === 'sel' || kind === 'chip') { stepKey(s, key, 1); return; }
        if (kind === 'grid') { oneWrite(pid(s, keyP(s, key)), +el.dataset.idx / (q.steps.length - 1)); return; }
        if (kind === 'tog' || kind === 'pow') { oneWrite(pid(s, keyP(s, key)), norm(s, key) >= 0.5 ? 0 : 1); return; }
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
      } else if (d.id === 'saturn') {   // a dashed line moves its crossover; anywhere else picks the band and drags its drive
        const a = xOfF(val(s, 'p1'), 1), bb = xOfF(val(s, 'p2'), 1);
        if (Math.abs(u - a) < 0.025) drag = { kind: 'xo', p: 'p1', scr };
        else if (Math.abs(u - bb) < 0.025) drag = { kind: 'xo', p: 'p2', scr };
        else { view.sband[s] = u < a ? 0 : u < bb ? 1 : 2; paint(s); drag = { kind: 'sd', b: view.sband[s], scr }; }
      } else if (d.id === 'ambient') drag = { kind: 'xy', scr };
      else return;
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
        else { setReal(s, 'p12', clamp01(u) * 100); setReal(s, 'p13', (1 - clamp01(v)) * 100); }
        return;
      }
      const fine = e.ctrlKey || e.metaKey ? 0.25 : 1, dx = e.clientX - drag.x, dy = e.clientY - drag.y;
      // sliders move 1:1 with the pointer (travel in screen pixels); knobs, numbers, boxes: 250 px per full turn
      const n = drag.kind === 'vs' ? drag.n0 - dy / drag.px * fine : drag.kind === 'hs' ? drag.n0 + dx / drag.px * fine : drag.n0 - dy / (250 * drag.zoom) * fine;
      store.write(drag.id, quant(drag.q, n));
    }
    fr.addEventListener('pointermove', move);
    const end = () => { if (!drag) return; if (drag.id) store.end(drag.id); drag = null; };
    for (const ev of ['pointerup', 'pointercancel', 'lostpointercapture']) fr.addEventListener(ev, end);
    fr.addEventListener('dblclick', (e) => {
      const el = e.target.closest('[data-kind]');
      if (el && ['knob', 'vs', 'hs', 'num', 'box', 'sq'].includes(el.dataset.kind)) { oneWrite(pid(s, keyP(s, el.dataset.key)), defNorm(s, el.dataset.key)); return; }
      // (PRO-Q's graph double-click is handled on pointer-down, see lastTap)
    });
    fr.addEventListener('contextmenu', (e) => {
      const el = e.target.closest('[data-kind="sel"],[data-kind="chip"]'); if (el) { e.preventDefault(); stepKey(s, el.dataset.key, -1); return; }
      const scr = e.target.closest('.fx-screen');
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
      if (def(s)?.id === 'proq' && e.target.closest('.fx-screen')) {   // PRO-Q: the wheel sets the selected band's Q
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

  // ---- live displays, in each skin's colours
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
  // PRO-Q: each band's sections, in the order src/plugin/FxUnits.cpp builds them
  // PRO-Q: each band's sections, as src/plugin/FxUnits.cpp builds them from its shape
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
    return (x) => sh(k * x + bias) - sh(bias);
  }
  function draw(s, t) {
    const cv = $('fx' + s + '-canvas'), d = def(s); if (!cv || !d) return;
    const g = cv.getContext('2d'), W = cv.width, H = cv.height, K = skin(s);
    const light = lum(K.screen) > 0.5, acc = K.scrAcc || K.accent, ink = light ? '#2B2B28' : K.ink, ink2 = light ? 'rgba(40,40,36,.55)' : K.ink2;
    g.clearRect(0, 0, W, H);
    const vis = view.vis.slice(s * 8, s * 8 + 8), v = (p) => val(s, p), top = d.display === 'eq' || d.display === 'bands' ? 40 : 90;
    if (!['tail', 'xy', 'grains', 'eq'].includes(d.display)) {
      g.strokeStyle = light ? 'rgba(0,0,0,.07)' : 'rgba(255,255,255,.05)'; g.lineWidth = 2;
      for (let i = 1; i < 8; i++) { g.beginPath(); g.moveTo(i * W / 8, 0); g.lineTo(i * W / 8, H); g.stroke(); }
      for (let i = 1; i < 4; i++) { g.beginPath(); g.moveTo(0, i * H / 4); g.lineTo(W, i * H / 4); g.stroke(); }
    }
    g.strokeStyle = acc; g.fillStyle = acc; g.lineWidth = 4; g.lineJoin = 'round';
    const line = (fn, x0 = 0, x1 = W) => { g.beginPath(); for (let x = x0; x <= x1; x += 4) { const y = fn(x); x === x0 ? g.moveTo(x, y) : g.lineTo(x, y); } g.stroke(); };
    const glow = (y0, c = acc) => { const gr = g.createLinearGradient(0, y0, 0, H); gr.addColorStop(0, c + '66'); gr.addColorStop(1, c + '00'); return gr; };
    const label = (str, x, y, al = 'left') => { g.save(); g.fillStyle = ink2; g.font = '600 26px "Segoe UI",sans-serif'; g.textAlign = al; g.fillText(str, x, y); g.restore(); };
    switch (d.display) {
      case 'dots': {   // PS DELAY: one dot per repeat; size = level, height = accumulated pitch
        const fb = v('p3') / 100, n = 16, st = [12, -12, 7, 0, 0][v('p9')] * v('p5') / 100;
        for (let k = 0; k < n; k++) {
          const lvl = Math.pow(Math.max(fb, 0.001), k); if (lvl < 0.03) break;
          const x = 40 + k * (W - 80) / (n - 1), y = H * 0.62 - Math.max(-6, Math.min(6, (v('p9') === 3 ? (k % 2 ? -1 : 1) : 1) * st * k / 12)) * 22;
          g.globalAlpha = 0.25 + 0.75 * lvl; g.beginPath(); g.arc(x, y, 5 + 16 * lvl, 0, 7); g.fill();
        }
        g.globalAlpha = 1; label(readout(s, 'p1'), W - 24, H - 22, 'right');
        break;
      }
      case 'tail': {   // REV OCEAN: the tail as a dithered, moving sea of dots
        const dec = v('p2'), span = Math.max(1.5, dec * 1.3), x0 = v('p5') / 1000 / span * W, mode = v('p9'), mac = v('p4') / 100;
        const env = (x) => { const tt = (x - x0) / W * span; if (tt < 0) return 0; const att = mode === 2 ? Math.min(1, tt / (0.05 + mac * 0.6)) : Math.min(1, tt / 0.02); return att * Math.pow(10, -3 * tt / dec); };
        const step = 12;
        g.fillStyle = '#D6E0FF';
        for (let x = 0; x < W; x += step) {
          const hgt = (0.15 + 0.85 * env(x)) * (H - 40);
          for (let y = H - 6; y > H - hgt; y -= step) {
            const depth = (H - y) / hgt, wave = 0.5 + 0.5 * Math.sin(x * 0.021 + y * 0.034 + t / (500 - mode * 120)) * Math.sin(x * 0.007 - t / 1300 + y * 0.01);
            const a = (1 - depth) * 0.25 + wave * (0.35 + 0.4 * mac) * (1 - depth * 0.6);
            if (a < 0.08) continue;
            g.globalAlpha = Math.min(1, a); g.fillRect(x, y, step - 5, step - 5);
          }
        }
        g.globalAlpha = 1; if (v('p10')) label('frozen', W / 2, 44, 'center');
        break;
      }
      case 'xy': {   // AMBIENT: a dotted field that thickens round the TONE × SPACE puck
        const px = v('p12') / 100 * W, py = (1 - v('p13') / 100) * H, step = 14;
        for (let x = step / 2; x < W; x += step) for (let y = step / 2; y < H; y += step) {
          const dd = Math.hypot(x - px, y - py) / (W * 0.35), a = Math.max(0, 1 - dd) * (0.6 + 0.4 * Math.sin(x * 0.05 + y * 0.04 + t / 700)) + 0.22;
          g.globalAlpha = Math.min(1, a); g.fillStyle = dd < 0.45 ? acc : '#6E726F'; g.fillRect(x - 3, y - 3, 6, 6);
        }
        g.globalAlpha = 1;
        g.fillStyle = '#FFFFFF'; g.beginPath(); g.arc(px, py, 38, 0, 7); g.fill();
        g.strokeStyle = '#141414'; g.lineWidth = 4; g.beginPath(); g.arc(px, py, 38, 0, 7); g.stroke();
        g.fillStyle = '#141414'; g.beginPath(); g.arc(px, py, 22, 0, 7); g.fill();
        g.fillStyle = '#9A9A9A'; g.beginPath(); g.arc(px, py, 9, 0, 7); g.fill();
        break;
      }
      case 'echo': {   // ECHO DELAY: the repeats as bars - up for left, down for right (ping-pong alternates)
        const fb = v('p2') / 100, mode = v('p9'), mid = H / 2 + 20, span = W - 60;
        g.strokeStyle = 'rgba(255,255,255,.18)'; g.lineWidth = 2; g.beginPath(); g.moveTo(30, mid); g.lineTo(W - 30, mid); g.stroke();
        for (let k = 0; k < 14; k++) {
          const lvl = Math.pow(Math.max(fb, 0.001), k); if (lvl < 0.03) break;
          const x = 30 + (k + 0.5) * span / 14, hh = lvl * (H / 2 - 70);
          g.globalAlpha = 0.35 + 0.65 * lvl; g.fillStyle = acc;
          if (mode === 1) g.fillRect(x - 9, k % 2 ? mid : mid - hh, 18, hh);
          else if (mode === 2) g.fillRect(x - 9, mid - hh / 2, 18, hh);
          else { g.fillRect(x - 9, mid - hh, 18, hh); g.fillRect(x - 9, mid, 18, hh); }
        }
        g.globalAlpha = 1; label(readout(s, 'p1'), W - 24, H - 22, 'right'); label(['L / R', 'L ↔ R', 'MONO'][mode], 24, H - 22);
        break;
      }
      case 'imager': {   // STEREO IMAGER: a fan per band as wide as its width, and the live correlation
        const cx = W / 2, cy = H - 90, R = H - 170, bands = [['p1', 'LOW', 1], ['p2', 'MID', 0.66], ['p3', 'HIGH', 0.36]];
        g.strokeStyle = 'rgba(255,255,255,.12)'; g.lineWidth = 2;
        for (const a of [-90, -45, 0, 45, 90]) { const r0 = a * Math.PI / 180; g.beginPath(); g.moveTo(cx, cy); g.lineTo(cx + Math.sin(r0) * R, cy - Math.cos(r0) * R); g.stroke(); }
        bands.forEach(([p, name, rs]) => {
          const spread = Math.min(1, v(p) / 200) * Math.PI / 2, rr = R * rs;
          g.fillStyle = acc + '44'; g.beginPath(); g.moveTo(cx, cy); g.arc(cx, cy, rr, -Math.PI / 2 - spread, -Math.PI / 2 + spread); g.closePath(); g.fill();
          g.strokeStyle = acc; g.lineWidth = 3; g.beginPath(); g.arc(cx, cy, rr, -Math.PI / 2 - spread, -Math.PI / 2 + spread); g.stroke();
          label(name + ' ' + readout(s, p), cx, cy - rr - 10, 'center');
        });
        const corr = vis[0] || 0, bx = 60, bw = W - 120, by = H - 44;   // correlation, -1 ... +1
        g.fillStyle = 'rgba(255,255,255,.12)'; g.fillRect(bx, by, bw, 8);
        g.fillStyle = corr < 0 ? '#E0454F' : acc; g.fillRect(bx + bw / 2, by, corr * bw / 2, 8);
        label('-1', bx - 10, by + 10, 'right'); label('+1', bx + bw + 10, by + 10); label('correlation ' + corr.toFixed(2), cx, by - 10, 'center');
        break;
      }
      case 'painting': {   // NUDESTORT: a marbled painting that the drive, grain, type and mode keep repainting
        paint2(g, W, H, s, t);
        break;
      }
      case 'ir': {   // CONVOLVER: the impulse, trimmed and possibly reversed
        const wave = view.ir.wave.length ? view.ir.wave : Array.from({ length: 240 }, (_, i) => Math.exp(-i / 50) * (0.6 + 0.4 * Math.sin(i * 7.3)));
        const len = v('p2') / 100, rev = v('p9') === 1, n = wave.length, bw = W / n;
        for (let i = 0; i < n; i++) {
          const src = rev ? Math.floor(len * n) - 1 - i : i, a = src >= 0 && i < len * n ? Math.abs(wave[src] || 0) : 0, hh = a * (H - top - 40);
          g.globalAlpha = i < len * n ? 1 : 0.2; g.fillRect(i * bw, H / 2 + 20 - hh / 2, Math.max(1, bw - 1), Math.max(2, hh));
        }
        g.globalAlpha = 1; label(view.ir.name.toLowerCase(), W - 24, H - 22, 'right'); label('drop a .wav to change', 24, H - 22);
        break;
      }
      case 'curve': {   // DISTORTION: transfer curve and the live input level
        const shape = transfer(s), mid = H / 2 + 30, amp = (H - top - 40) / 2;
        g.strokeStyle = light ? 'rgba(0,0,0,.2)' : 'rgba(255,255,255,.2)'; g.lineWidth = 2; g.beginPath(); g.moveTo(0, mid); g.lineTo(W, mid); g.moveTo(W / 2, top); g.lineTo(W / 2, H); g.stroke();
        g.strokeStyle = acc; g.lineWidth = 4; line((x) => mid - Math.max(-1.1, Math.min(1.1, shape((x / W) * 2 - 1))) * amp);
        const lv = Math.min(1, vis[0]); g.beginPath(); g.arc(W / 2 + lv * W / 2, mid - shape(lv) * amp, 10, 0, 7); g.fill();
        break;
      }
      case 'bands': case 'mb': {   // SATURN / MULTIBAND: three bands on a log axis
        if (d.id === 'saturn') {
          const sb = view.sband[s], xa = xOfF(v('p1'), W), xb = xOfF(v('p2'), W), xs = [0, xa, xb, W];
          const hOf = (b) => (0.14 + v('p' + (3 + b)) / 36 * 0.66) * H, sig = (z) => 1 / (1 + Math.exp(-z));
          const grit = [0.35, 0.2, 0.3, 0.45, 0.6, 0.9, 1][v('p9')];   // how rough each style's curve looks
          const hAt = (x) => (hOf(0) + (hOf(1) - hOf(0)) * sig((x - xa) / 30) + (hOf(2) - hOf(1)) * sig((x - xb) / 30))
            * (1 + grit * 0.1 * Math.sin(x * 0.09 + t / 350) * Math.sin(x * 0.021 - t / 900));
          g.fillStyle = 'rgba(224,69,58,.16)'; g.fillRect(xs[sb], 0, xs[sb + 1] - xs[sb], H);
          const gr = g.createLinearGradient(0, H * 0.2, 0, H); gr.addColorStop(0, 'rgba(224,69,58,.45)'); gr.addColorStop(1, 'rgba(224,69,58,0)');
          g.fillStyle = gr; g.beginPath(); g.moveTo(0, H); for (let x = 0; x <= W; x += 4) g.lineTo(x, H - hAt(x)); g.lineTo(W, H); g.fill();
          g.strokeStyle = '#F2A08F'; g.lineWidth = 4; line((x) => H - hAt(x));
          g.setLineDash([10, 10]); g.strokeStyle = 'rgba(255,255,255,.4)'; g.lineWidth = 2;
          for (const [x, p] of [[xa, 'p1'], [xb, 'p2']]) {
            g.beginPath(); g.moveTo(x, 20); g.lineTo(x, H); g.stroke();
            g.save(); g.setLineDash([]); g.fillStyle = 'rgba(255,255,255,.6)'; g.beginPath(); g.moveTo(x - 10, 34); g.lineTo(x + 10, 34); g.lineTo(x, 48); g.fill(); g.restore();
            label(readout(s, p), x + 14, 46);
          }
          g.setLineDash([]);
          for (let b = 0; b < 3; b++) {   // the drag handle of each band sits on its level
            const cx = (xs[b] + xs[b + 1]) / 2, cy = H - hOf(b);
            g.fillStyle = b === sb ? '#EDE7E4' : '#958E8B'; g.beginPath(); g.roundRect(cx - 38, cy - 12, 76, 24, 8); g.fill();
            g.fillStyle = 'rgba(0,0,0,.25)'; g.fillRect(cx - 22, cy - 1, 44, 2);
            label(SAT_BANDS[b].toLowerCase() + ' ' + readout(s, 'p' + (3 + b)), cx, cy - 22, 'center');
          }
          break;
        }
        const sat = d.id === 'saturn', cols = sat ? ['#FF8A3D', '#FFC94A', '#6EC6FF'] : [acc, acc, acc];
        const xa = xOfF(sat ? v('p1') : v('p12'), W), xb = xOfF(sat ? v('p2') : v('p13'), W), xs = [0, xa, xb, W];
        for (let b = 0; b < 3; b++) {
          const lvl = sat ? v('p' + (3 + b)) / 36 : 0.5 + (vis[1 + b] || 0) / 24, hh = Math.max(0.03, Math.min(1, lvl)) * (H - top - 30);
          g.fillStyle = glow(H - hh, cols[b]); g.fillRect(xs[b] + 6, H - hh, xs[b + 1] - xs[b] - 12, hh);
          g.strokeStyle = cols[b]; g.lineWidth = 4; g.strokeRect(xs[b] + 6, H - hh, xs[b + 1] - xs[b] - 12, hh);
        }
        g.strokeStyle = ink; g.lineWidth = 3;
        for (const [x, p] of [[xa, sat ? 'p1' : 'p12'], [xb, sat ? 'p2' : 'p13']]) { g.beginPath(); g.moveTo(x, top); g.lineTo(x, H); g.stroke(); label(readout(s, p), x + 10, top + 30); }
        break;
      }
      case 'steps': {   // BITCRUSHER: a 440 Hz sine through the current bits and rate
        const levels = Math.pow(2, v('p1')) / 2, hold = Math.max(1, 48000 / v('p2')), amp = (H - top - 40) / 2, mid = H / 2 + 30;
        g.strokeStyle = light ? 'rgba(0,0,0,.2)' : 'rgba(255,255,255,.2)'; g.lineWidth = 2; line((x) => mid - Math.sin(x / W * 2 * Math.PI * 2 + t / 700) * amp);
        g.strokeStyle = acc; g.lineWidth = 4;
        line((x) => { const smp = Math.floor(x / W * 218 / hold) * hold; return mid - Math.round(Math.sin(smp / 218 * 2 * Math.PI * 2 + t / 700) * levels) / levels * amp; });
        break;
      }
      case 'gr': {   // compressors: scrolling gain reduction
        const h = view.hist[s]; h.push(vis[1] || 0); if (h.length > 200) h.shift();
        const y = (db) => top + Math.min(1, db / 24) * (H - top - 20);
        g.beginPath(); for (let i = h.length - 1; i >= 0; i--) { const x = W - (h.length - 1 - i) * W / 200; i === h.length - 1 ? g.moveTo(x, y(h[i])) : g.lineTo(x, y(h[i])); } g.stroke();
        label('gr ' + (vis[1] || 0).toFixed(1) + ' dB', W - 24, H - 22, 'right');
        break;
      }
      case 'lfo': {   // PHASER / FLANGER: two LFOs, apart by the stereo spread
        const rate = v('p1'), spread = d.id === 'phaser' ? v('p5') / 360 : (v('p9') ? 0.25 : 0), tri = d.id === 'flanger' && v('p10') === 1;
        const wave = (ph) => tri ? 1 - 4 * Math.abs(((ph % 1) + 1) % 1 - 0.5) : Math.sin(ph * 2 * Math.PI);
        const amp = (H - top - 40) / 2 * (v(d.id === 'phaser' ? 'p2' : 'p3') / 100), mid = H / 2 + 30, cyc = Math.max(1, Math.min(4, rate * 2));
        const f = (off) => (x) => mid - wave(x / W * cyc - t / 1000 * rate + off) * amp;
        g.fillStyle = glow(top); g.beginPath(); g.moveTo(0, f(0)(0)); for (let x = 0; x <= W; x += 4) g.lineTo(x, f(0)(x)); for (let x = W; x >= 0; x -= 4) g.lineTo(x, f(spread)(x)); g.fill();
        g.strokeStyle = acc; g.lineWidth = 4; line(f(0)); g.globalAlpha = 0.6; line(f(spread)); g.globalAlpha = 1;
        break;
      }
      case 'pan': {   // STEREO PAN: the position over the LFO shape
        const rate = v('p9') ? 0.5 : v('p1'), depth = v('p2') / 100, sh = v('p3') / 100, ph = t / 1000 * rate + v('p4') / 360;
        const lfo = (p) => { const sn = Math.sin(p * 2 * Math.PI), tr = 1 - 4 * Math.abs(((p % 1) + 1) % 1 - 0.5), sq = sn >= 0 ? 1 : -1; return sh < 0.5 ? sn + (tr - sn) * sh * 2 : tr + (sq - tr) * (sh - 0.5) * 2; };
        g.strokeStyle = ink2; g.lineWidth = 3; line((x) => top + 20 + (1 - (lfo(x / W * 2) * depth + 1) / 2) * (H - top - 60));
        const px = W / 2 + lfo(ph) * depth * (W / 2 - 40);
        g.fillStyle = acc; g.beginPath(); g.arc(px, H - 40, 18, 0, 7); g.fill();
        label('L', 24, H - 30); label('R', W - 24, H - 30, 'right');
        break;
      }
      case 'eq': {   // PRO-Q: frequency / dB grid, the summed curve filled from 0 dB, the selected band's own curve, nodes
        const secs = eqSections(s), y = (db) => eqY(db) * H, fAt = (x) => fOfX(x, W), grid = 'rgba(255,255,255,.07)';
        g.lineWidth = 2; g.font = '600 22px "Segoe UI",sans-serif';
        for (const hz of [30, 50, 100, 200, 500, 1000, 2000, 5000, 10000]) {
          const x = xOfF(hz, W); g.strokeStyle = grid; g.beginPath(); g.moveTo(x, 0); g.lineTo(x, H); g.stroke();
          g.fillStyle = ink2; g.textAlign = 'center'; g.fillText(hz >= 1000 ? hz / 1000 + 'k' : String(hz), x, H - 14);
        }
        for (const db of [-18, -12, -6, 0, 6, 12, 18]) {
          g.strokeStyle = db ? grid : 'rgba(255,255,255,.22)'; g.beginPath(); g.moveTo(0, y(db)); g.lineTo(W, y(db)); g.stroke();
          g.fillStyle = ink2; g.textAlign = 'right'; g.fillText((db > 0 ? '+' : '') + db, W - 12, y(db) - 6);
        }
        const on = Math.round(v('p27')), sb = view.band[s];
        if (sb >= 0 && secs[sb]?.length) {   // the selected band on its own, faint
          g.strokeStyle = acc; g.globalAlpha = 0.45; g.lineWidth = 3; line((x) => y(Math.max(-30, secs[sb].reduce((a, b) => a + magDb(b, fAt(x)), 0)))); g.globalAlpha = 1;
        }
        const total = (x) => Math.max(-30, secs.reduce((a, bs) => a + bs.reduce((c, b) => c + magDb(b, fAt(x)), 0), 0) + v('p4'));
        const pts = []; for (let x = 0; x <= W; x += 4) pts.push([x, y(total(x))]);
        g.fillStyle = acc + '38'; g.beginPath(); g.moveTo(0, y(0)); for (const [x, yy] of pts) g.lineTo(x, yy); g.lineTo(W, y(0)); g.fill();
        g.strokeStyle = acc; g.lineWidth = 5; g.beginPath(); pts.forEach(([x, yy], i) => i ? g.lineTo(x, yy) : g.moveTo(x, yy)); g.stroke();
        BANDS.forEach((b, i) => {
          if (!(on & (1 << i))) return;
          const x = xOfF(v(b[0]), W), yy = y(usesGain(v(PQ_SHAPE[i])) ? v(b[1]) : 0), sel = i === sb;
          g.fillStyle = sel ? ink : acc; g.beginPath(); g.arc(x, yy, sel ? 19 : 15, 0, 7); g.fill();
          if (sel) { g.strokeStyle = acc; g.lineWidth = 4; g.beginPath(); g.arc(x, yy, 25, 0, 7); g.stroke(); }
          g.fillStyle = K.bg2; g.font = '700 20px "Segoe UI",sans-serif'; g.textAlign = 'center'; g.fillText(String(i + 1), x, yy + 7);
          if (sel) {   // its value, in a tag above it
            const t = readout(s, 'band0') + (usesGain(v(PQ_SHAPE[i])) ? '   ' + readout(s, 'band1') : ''), tw = g.measureText(t).width + 28, ty = yy - 62 < 10 ? yy + 36 : yy - 62;
            const tx = Math.max(6, Math.min(W - tw - 6, x - tw / 2));
            g.fillStyle = 'rgba(0,0,0,.55)'; g.beginPath(); g.roundRect(tx, ty, tw, 34, 8); g.fill();
            g.fillStyle = ink; g.textAlign = 'left'; g.fillText(t, tx + 14, ty + 24);
          }
        });
        if (!on) { g.fillStyle = ink2; g.font = '600 30px "Segoe UI",sans-serif'; g.textAlign = 'center'; g.fillText('double-click to add a band', W / 2, y(0) - 30); }
        break;
      }
      case 'resp': {   // FILTER: the response, moving with its LFO
        const lfo = Math.sin(t / 1000 * v('p4') * 2 * Math.PI) * v('p5'), fc = Math.min(20000, Math.max(20, v('p1') * Math.pow(2, lfo + v('p6') * (vis[0] || 0))));
        const kind = ['lp', 'hp', 'bp', 'notch'][v('p9')], q = 0.5 + v('p2') / 100 * 12, b = biquad(kind, fc, q, 0), n = v('p10') ? 2 : 1;
        const y = (db) => top + (1 - (Math.max(-48, Math.min(24, db)) + 48) / 72) * (H - top - 10);
        g.fillStyle = glow(top); g.beginPath(); g.moveTo(0, H); for (let x = 0; x <= W; x += 4) g.lineTo(x, y(n * magDb(b, fOfX(x, W)))); g.lineTo(W, H); g.fill();
        line((x) => y(n * magDb(b, fOfX(x, W))));
        break;
      }
      case 'grains': {   // AUTOCHROMA: grains as coloured strokes; x = age, height = envelope, row = pitch
        const gs = view.grains[s], dens = v('p2'), size = v('p1'), pitches = [-12, -7, -5, 0, 5, 7, 12, 19, 24];
        if (Math.random() < dens / 30 * (v('p5') / 100)) gs.push({ born: t, st: pitches[v('p9')] + v('p11') / 100, k: Math.random() < v('p6') / 100 ? 1 : 0, x: Math.random() });   // k 1 = reversed
        while (gs.length && t - gs[0].born > size * 4 + 1500) gs.shift();
        g.strokeStyle = 'rgba(60,70,60,.12)'; g.lineWidth = 2;
        for (let i = 1; i < 12; i++) { g.beginPath(); g.moveTo(i * W / 12, 0); g.lineTo(i * W / 12, H); g.stroke(); }
        for (const gr of gs) {
          const age = (t - gr.born) / (size * 4 + 1500), env = Math.sin(Math.min(1, age) * Math.PI), x = gr.x * W * 0.3 + age * W * 0.7, y = H - 50 - (gr.st + 12) / 36 * (H - 120);
          g.globalAlpha = 0.35 + 0.65 * env; g.strokeStyle = STREAM_COLOURS[gr.k]; g.lineWidth = 4;
          g.beginPath(); g.moveTo(x, y - 12 - 40 * env); g.lineTo(x, y + 12 + 40 * env); g.stroke();
        }
        g.globalAlpha = 1;
        break;
      }
    }
  }
  // STORT's painting: domain-warped noise, one palette per TYPE; DRIVE warps harder, GRAIN makes
  // the bands finer, UNPREDICTABLE makes it drift faster. Drawn on the GPU at the display's full
  // resolution with anti-aliased band edges; without WebGL 2, a small CPU version scaled up.
  const PALETTES = [
    [[243, 227, 195], [240, 138, 93], [200, 53, 58], [47, 125, 79], [31, 58, 107]],
    [[28, 176, 170], [245, 110, 150], [250, 214, 80], [110, 70, 170], [20, 20, 28]],
    [[210, 214, 200], [120, 150, 110], [60, 90, 70], [180, 160, 110], [30, 34, 32]],
    [[250, 230, 90], [230, 60, 40], [150, 20, 30], [250, 140, 40], [15, 10, 12]]];
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
    if (!$('pop-fx').hidden) {
      for (let s = 0; s < 3; s++) {
        draw(s, t);
        for (const m of document.querySelectorAll('#fx' + s + '-frame .fx-meter')) m.firstChild.style.height = Math.min(100, (view.vis[s * 8 + +m.dataset.vis] || 0) / 20 * 100) + '%';
      }
    }
    requestAnimationFrame(loop);
  }
  requestAnimationFrame(loop);

  return {
    refresh, choose,
    feed(state) {
      if (state.fx) view.vis = state.fx;
      if (state.irName != null) view.ir = { name: state.irName, wave: state.irWave || [], secs: state.irSeconds || 0 };
    },
    open() { $('fx-picker').hidden = true; refresh(); openPage('pop-fx'); },
    retheme() { view.built = [-1, -1, -1]; refresh(); },
    serial() { oneWrite('fx.mode', 0); refresh(); },
    parallel() { oneWrite('fx.mode', 1); refresh(); }
  };
}
