// Primitives for the Geminus panel. Every physical part is an image asset;
// only silkscreen (text, ticks, rules, black insets) is drawn.
// Colours are theme variables; the values for each theme are in gen.mjs (THEMES).
export const INK = 'var(--ink)';
export const INK2 = 'var(--ink2)';
export const ORANGE = 'var(--accent)';
export const BLACKPANEL = 'var(--inset)';
export const ONBLACK_INK = 'var(--oink)';
export const ONBLACK_INK2 = 'var(--oink2)';

export const A = {
  panel: 'refresh-panel.png', panelDark: 'refresh-panel-dark.svg', shading: 'refresh-shading.svg', glass: 'refresh-display.svg',
  knob: { cream: 'refresh-knob-cream.svg', orange: 'refresh-knob-orange.svg', dark: 'refresh-knob-dark.svg' },
  slot: 'refresh-rail.svg', ticks: 'ticks.png',
  cap: { grey: 'refresh-fader-grey.svg', orange: 'refresh-fader-orange.svg', dark: 'refresh-fader-dark.svg' },
  sw: ['refresh-switch-bot.svg', 'refresh-switch-mid.svg', 'refresh-switch-top.svg'],
  rocker: ['refresh-octave-left.svg', 'refresh-octave-center.svg', 'refresh-octave-right.svg'],
  btnOff: 'refresh-button-light.svg', btnOn: 'refresh-button-light-down.svg', btnDark: 'refresh-button-dark.svg', btnDarkOn: 'refresh-button-dark-down.svg',
  ledOff: 'refresh-led-off.svg', ledOn: 'refresh-led-on.svg',
  ribbon: 'refresh-ribbon.svg', ribbonLeft: 'refresh-ribbon-end.svg', ribbonRight: 'refresh-ribbon-end.svg', cheek: 'refresh-cheek.svg', octave: 'refresh-keyboard.svg',
  bender: 'refresh-bender-center.svg', dispWide: 'refresh-display.svg'
};
export const SW_ASPECT = 44 / 96;
export const TOGGLE_ASPECT = 300 / 1090;             // composed toggle sprite (tools/crop-ui-assets.ps1)
export const BTN_ASPECT = 84 / 120;

export const r1 = (n) => Math.round(n * 10) / 10;
export const esc = (s) => String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');

// Silkscreen scale. Sizes given to txt() are multiplied by this.
export const TEXT = 2.0;
export const lineH = (size) => (size || 8) * TEXT * 1.05;
// Bahnschrift condensed measures 0.468 em per character (measured in Chromium); letter-spacing adds.
export const wText = (t, size, ls) => (t ? String(t).length * (size || 8) * TEXT * (0.468 + (ls == null ? 0.06 : ls)) + 6 : 0);
export const FONT = "Bahnschrift,'Bahnschrift Condensed','SPKR Condensed','Arial Narrow',Arial,sans-serif";

// ---- binding ---------------------------------------------------------------
// Each interactive control gets one transparent hit element carrying the APVTS parameter id.
//   id        the APVTS parameter id, e.g. "upper.vcf.lpf"; with `layered`, the leaf ("voice.mode")
//   type      slider | toggle | combo   (data-juce-type)
//   ctl       knob | fader | sw3 | button | led | rocker
export function hit(o) {
  if (!o.id) return '';
  const extra = Object.entries(o.data || {})
    .map(([k, v]) => ' data-' + k + '="' + esc(v) + '"').join('');
  const layered = o.layered ? ' data-layered="' + esc(o.id) + '"' : '';
  const shift = o.shiftId ? ' data-shift="' + esc(o.shiftId) + '"' : '';
  const pid = o.layered ? 'upper.' + o.id : o.id;
  return '<div id="' + esc(o.domId || pid) + '" data-juce-type="' + o.type + '"'
    + (o.data?.wavecard ? ' role="button" tabindex="0" aria-label="' + esc(o.title || pid) + '"' : '')
    + ' data-param="' + esc(pid) + '"' + layered + shift + ' data-ctl="' + o.ctl + '"'
    + ' data-vis="' + esc(o.domId || pid) + '__v"' + extra
    + (o.title ? ' title="' + esc(o.title) + '"' : '')
    + ' style="position:absolute;left:' + r1(o.x) + 'px;top:' + r1(o.y) + 'px;'
    + 'width:' + r1(o.w) + 'px;height:' + r1(o.h) + 'px;cursor:' + (o.cursor || 'ns-resize') + ';'
    + 'z-index:5;touch-action:none;"></div>';
}
const visId = (o) => (o.id ? ' id="' + esc(o.domId || (o.layered ? 'upper.' + o.id : o.id)) + '__v"' : '');

// ---- silkscreen text -------------------------------------------------------
export function txt(x, y, w, s, o) {
  o = o || {};
  const size = (o.size || 9) * TEXT, weight = o.weight || 700, col = o.color || INK;
  const align = o.align || 'center', ls = o.ls == null ? 0.06 : o.ls;
  return '<div' + (o.id ? ' id="' + esc(o.id) + '"' : '') + (o.cls ? ' class="' + o.cls + '"' : '')
    + ' style="position:absolute;left:' + r1(x) + 'px;top:' + r1(y) + 'px;width:' + r1(w) + 'px;'
    + 'text-align:' + align + ';font-family:' + FONT + ';font-stretch:75%;'
    + 'font-weight:' + weight + ';font-size:' + r1(size) + 'px;line-height:1.05;color:' + col + ';'
    + 'letter-spacing:' + ls + 'em;text-transform:uppercase;white-space:nowrap;pointer-events:none;'
    + (o.style || '') + '">' + esc(s) + '</div>';
}

// secondary (shift) function: the hardware prints these inverted
export function txtInv(x, y, w, s, o) {
  o = o || {};
  const size = (o.size || 7.5) * TEXT;
  return '<div' + (o.id ? ' id="' + esc(o.id) + '"' : '') + (o.cls ? ' class="' + o.cls + '"' : '') + ' style="position:absolute;left:' + r1(x) + 'px;top:' + r1(y) + 'px;width:' + r1(w) + 'px;'
    + 'text-align:center;font-family:' + FONT + ';font-stretch:75%;'
    + 'font-weight:700;font-size:' + r1(size) + 'px;line-height:1.5;color:' + (o.bg ? 'var(--badge-ink)' : INK2) + ';background:' + (o.bg || 'transparent') + ';'
    + 'letter-spacing:.06em;text-transform:uppercase;white-space:nowrap;border-radius:2px;pointer-events:none;">' + esc(s) + '</div>';
}

// ---- rules / frames --------------------------------------------------------
export function vrule(x, y, h, c) {
  return '<div style="position:absolute;left:' + r1(x) + 'px;top:' + r1(y) + 'px;width:1.5px;height:'
    + r1(h) + 'px;background:' + (c || 'var(--rule)') + ';opacity:.8;pointer-events:none;"></div>';
}
export function hrule(x, y, w, c, t) {
  return '<div style="position:absolute;left:' + r1(x) + 'px;top:' + r1(y) + 'px;width:' + r1(w)
    + 'px;height:' + (t || 1.5) + 'px;background:' + (c || 'var(--rule)') + ';opacity:.85;pointer-events:none;"></div>';
}
export function blackPanel(x, y, w, h) {
  return '<div style="position:absolute;left:' + r1(x) + 'px;top:' + r1(y) + 'px;width:' + r1(w) + 'px;height:' + r1(h) + 'px;'
    + 'background:' + BLACKPANEL + ';border-radius:4px;overflow:hidden;">'
    + '<div class="tex-dark" style="position:absolute;inset:0;background:url(' + A.panelDark + ') repeat;"></div>'
    + '<div style="position:absolute;inset:0;border-radius:4px;box-shadow:inset 0 2px 6px rgba(0,0,0,.6),inset 0 0 0 1px rgba(0,0,0,.4),0 1px 0 rgba(255,255,255,.5);"></div></div>';
}
// a recessed light well (list boxes, fields)
export function well(x, y, w, h, extra) {
  return '<div style="position:absolute;left:' + r1(x) + 'px;top:' + r1(y) + 'px;width:' + r1(w) + 'px;height:' + r1(h) + 'px;'
    + 'background:rgba(0,0,0,.07);border-radius:3px;box-shadow:inset 0 1px 3px rgba(0,0,0,.28),inset 0 0 0 1px rgba(0,0,0,.18);'
    + (extra || '') + '"></div>';
}

// ---- section header: title on a rule, as the hardware prints it -------------
export function sect(x, y, w, title, o) {
  o = o || {};
  const size = o.size || 11;
  // lifted 0.09em so the rule runs through the middle of the capitals, not the line box
  const rule = '<span style="flex:1;min-width:6px;height:' + (STYLE.flat ? '1px' : '1.5px') + ';position:relative;top:-0.09em;background:' + (o.ruleColor || (STYLE.flat ? 'var(--rule)' : ORANGE)) + ';opacity:.85"></span>';
  return '<div class="section-heading" style="position:absolute;left:' + r1(x) + 'px;top:' + r1(y)
    + 'px;width:' + r1(w) + 'px;height:' + r1(size * TEXT * 1.05) + 'px;display:flex;align-items:center;gap:8px;pointer-events:none;'
    + 'font-family:' + FONT + ';font-stretch:75%;font-size:' + r1(size * TEXT) + 'px;font-weight:700;letter-spacing:.1em;color:' + (o.color || INK) + '">'
    + (o.align === 'left' ? '' : rule) + '<span style="flex:none;white-space:nowrap">' + esc(title) + '</span>' + rule + '</div>';
}

// ---- knob ------------------------------------------------------------------
// val 0..1 maps to -150deg..+150deg. Rotate the supplied top face inside an
// elliptical projection, leaving the photographed sidewall and socket stationary.
// Pop-over pages use the dark flat look (memory: geminus-popover-dark-flat-look): while
// STYLE.flat is set, knobs and keys are drawn flat (generated caps from tools/make-fx-controls.mjs)
// instead of the photographed hardware parts the main panel uses.
export const STYLE = { flat: false };
const arcD = (a0, a1) => {
  const p = (a) => [50 + Math.sin(a * Math.PI / 180) * 46, 50 - Math.cos(a * Math.PI / 180) * 46];
  const [x0, y0] = p(a0), [x1, y1] = p(a1);
  return 'M' + r1(x0) + ' ' + r1(y0) + 'A46 46 0 ' + (Math.abs(a1 - a0) > 180 ? 1 : 0) + ' 1 ' + r1(x1) + ' ' + r1(y1);
};
export { arcD };
function flatKnob(o) {
  const x = o.x, y = o.y, d = o.d, dom = o.domId || o.id, pad = r1(d * 0.12);
  let out = '<svg aria-hidden="true" viewBox="0 0 100 100" style="position:absolute;left:' + r1(x) + 'px;top:' + r1(y) + 'px;width:' + r1(d) + 'px;height:' + r1(d) + 'px;overflow:visible;pointer-events:none">'
    + '<path d="' + arcD(-150, 150) + '" fill="none" stroke="var(--ink2)" stroke-opacity=".3" stroke-width="5" stroke-linecap="round"/>'
    + '<path' + (o.id ? ' id="' + esc(dom) + '__a"' : '') + ' d="' + arcD(-150, -149.5) + '" fill="none" stroke="var(--accent)" stroke-width="5" stroke-linecap="round"/></svg>';
  out += '<img src="fx-knob-house.svg" alt=""' + visId(o) + (o.id ? ' data-flat="1"' : '') + ' style="position:absolute;left:' + r1(x + pad) + 'px;top:' + r1(y + pad) + 'px;width:' + r1(d - 2 * pad) + 'px;height:' + r1(d - 2 * pad) + 'px;pointer-events:none">';
  out += hit({ id: o.id, domId: o.domId, layered: o.layered, shiftId: o.shiftId, type: o.hitType || 'slider', ctl: 'knob',
               x: x - 2, y: y - 2, w: d + 4, h: d + 4, title: o.title || o.label, cursor: o.hitType === 'combo' ? 'pointer' : 'ns-resize',
               data: Object.assign({ ind: dom + '__v', arc: dom + '__a' }, o.hitData || {}) });
  if (o.label) { const lw = o.lw || d + 80; out += txt(x + d / 2 - lw / 2, o.ly != null ? o.ly : y + d + 8, lw, o.label, { size: o.lsize || 8.5, color: o.labelColor || INK }); }
  return out;
}
export function knob(o) {
  if (STYLE.flat) return flatKnob(o);
  const x = o.x, y = o.y, d = o.d;
  const v = o.v || 'cream', val = o.val == null ? 0.5 : o.val;
  const deg = -150 + val * 300;
  const n = o.ticks == null ? 11 : o.ticks;
  const tkMaj = o.tick || INK, tkMin = o.tick || INK2;
  let out = '';
  const pad = Math.max(7, d * 0.2);
  if (n > 0) {
    const S = d + pad * 2, c = S / 2, R = d / 2;
    let lines = '';
    for (let i = 0; i < n; i++) {
      const t = n === 1 ? 0 : i / (n - 1);
      const ang = (-150 + t * 300) * Math.PI / 180;
      const major = (i === 0 || i === n - 1 || (n % 2 === 1 && i === (n - 1) / 2));
      const ra = R + 2, rb = R + (major ? pad * 0.85 : pad * 0.55);
      lines += '<line x1="' + r1(c + Math.sin(ang) * ra) + '" y1="' + r1(c - Math.cos(ang) * ra)
        + '" x2="' + r1(c + Math.sin(ang) * rb) + '" y2="' + r1(c - Math.cos(ang) * rb)
        + '" stroke="' + (major ? tkMaj : tkMin) + '" stroke-width="' + (major ? 1.6 : 1.1) + '" stroke-linecap="round"/>';
    }
    out += '<svg width="' + r1(S) + '" height="' + r1(S) + '" viewBox="0 0 ' + r1(S) + ' ' + r1(S) + '" '
      + 'style="position:absolute;left:' + r1(x - pad) + 'px;top:' + r1(y - pad) + 'px;overflow:visible;pointer-events:none;">' + lines + '</svg>';
  }
  if (o.minmax) {
    out += txt(x - pad - 30, y + d - 4, 40, o.minmax[0], { size: 6.5, color: o.lc2 || INK2, weight: 600, align: 'right', ls: 0.02 });
    out += txt(x + d + pad - 10, y + d - 4, 40, o.minmax[1], { size: 6.5, color: o.lc2 || INK2, weight: 600, align: 'left', ls: 0.02 });
  }
  if (o.scaleValues) {
    const radius = d / 2 + pad + 9;
    o.scaleValues.forEach((value, i) => {
      const angle = (-150 + 300 * i / (o.scaleValues.length - 1)) * Math.PI / 180;
      out += txt(x + d / 2 + Math.sin(angle) * radius - 10,
        y + d / 2 - Math.cos(angle) * radius - 5, 20, value,
        { size: 4.8, ls: 0, color: o.lc2 || INK2, weight: 600 });
    });
  }
  const sprite = A.knob[v];
  out += '<div' + visId(o) + ' style="position:absolute;left:' + r1(x) + 'px;top:' + r1(y) + 'px;width:' + r1(d) + 'px;height:' + r1(d) + 'px;pointer-events:none">'
    + '<img src="' + sprite + '" alt=""' + (o.id ? ' id="' + esc(o.domId || o.id) + '__i"' : '')
    + ' style="display:block;width:100%;height:100%;transform:rotate(' + r1(deg) + 'deg)"></div>';
  out += hit({ id: o.id, domId: o.domId, layered: o.layered, shiftId: o.shiftId, type: o.hitType || 'slider', ctl: 'knob',
               x: x - 2, y: y - 2, w: d + 4, h: d + 4, title: o.title || o.label,
               cursor: o.hitType === 'combo' ? 'pointer' : 'ns-resize',
               data: Object.assign({ ind: (o.domId || o.id) + '__i' }, o.hitData || {}) });
  if (o.label) { const lw = o.lw || d + 80; out += txt(x + d / 2 - lw / 2, o.ly != null ? o.ly : y + d + pad + 2, lw, o.label, { size: o.lsize || 8.5, color: o.labelColor || INK }); }
  if (o.sub) out += txtInv(x + d / 2 - wText(o.sub, 7, 0.06) / 2, o.sy != null ? o.sy : y + d + pad + 2 + lineH(8.5) + 2, wText(o.sub, 7, 0.06), o.sub, { size: 7 });
  return out;
}

// stepped rotary (WAVEFORM / RANGE / CLK DIV) - ticks land on the labelled positions
export function rotary(o) {
  const x = o.x, y = o.y, d = o.d, steps = o.steps;
  const n = steps.length, sel = o.sel == null ? 0 : o.sel;
  const lc = o.lc || INK2;
  const pad = Math.max(7, d * 0.2);
  let out = knob({ x: x, y: y, d: d, v: o.v || 'cream', val: n === 1 ? 0.5 : sel / (n - 1), ticks: n, tick: o.tc,
                   id: o.id, domId: o.domId, layered: o.layered, hitType: 'combo', hitData: { steps: n }, title: o.label });
  const R = d / 2 + pad + 4, cx = x + d / 2, cy = y + d / 2;
  const fs = o.ssize || 7;
  const lh = lineH(fs);
  for (let i = 0; i < n; i++) {
    const ang = (-150 + (i / (n - 1)) * 300) * Math.PI / 180;
    const lx = cx + Math.sin(ang) * R, ly = cy - Math.cos(ang) * R;
    const s = Math.sin(ang);
    const align = s < -0.25 ? 'right' : (s > 0.25 ? 'left' : 'center');
    const w = 44, ox = align === 'right' ? -w : (align === 'left' ? 0 : -w / 2);
    const waves = {
      SIN: 'M1 6C4 -1 7 -1 10 6S16 13 19 6',
      SAW: 'M1 11L9 1V11L17 1V11',
      SQR: 'M1 11V1H10V11H19V1',
      TRI: 'M1 11L6 1L14 11L19 1',
      PLS: 'M1 11V1H6V11H19',
      NSE: 'M1 7L3 3L5 10L7 5L10 8L12 1L14 10L17 4L19 6'
    };
    if (o.waveSymbols && waves[steps[i]]) {
      const sx = align === 'right' ? lx - 20 : align === 'left' ? lx : lx - 10;
      out += '<svg aria-hidden="true" style="position:absolute;left:' + r1(sx) + 'px;top:' + r1(ly - 6)
        + 'px;width:20px;height:12px;overflow:visible;pointer-events:none" viewBox="0 0 20 12"><path d="'
        + waves[steps[i]] + '" fill="none" stroke="' + lc + '" stroke-width="1.3" stroke-linejoin="miter"/></svg>';
    } else {
      out += txt(lx + ox, ly - lh / 2, w, steps[i], { size: fs, color: lc, weight: 600, ls: 0.02, align: align });
    }
  }
  if (o.rangeOutline) {
    // Open, chamfered range boundary: clear of the scale labels and knob cap.
    out += '<svg aria-hidden="true" style="position:absolute;left:' + r1(cx - 72) + 'px;top:' + r1(cy - 60)
      + 'px;width:144px;height:108px;pointer-events:none;overflow:visible" viewBox="0 0 144 108">'
      + '<path d="M20 104L5 84V6H139V84L124 104M124 97V104H131" fill="none" stroke="'
      + lc + '" stroke-width="1.2" stroke-linejoin="miter"/></svg>';
  }
  if (o.label) out += txt(x - 40, o.ly != null ? o.ly : y + d + pad + 14, d + 80, o.label, { size: o.lsize || 8.5, color: o.labelColor || INK });
  return out;
}
// width a rotary needs, labels included
export const wRotary = (d, lw) => d + 2 * (Math.max(7, d * 0.2) + 4 + (lw == null ? 22 : lw));

// ---- fader -----------------------------------------------------------------
export function fader(o) {
  const x = o.x, y = o.y, h = o.h;
  const val = o.val == null ? 0.5 : o.val;
  const sw = o.sw || 15;
  // cap drawn at 68 % of the render's height: slimmer caps, as requested (2026-09-17)
  // short faders (the performance depth faders) get a proportionally smaller cap
  const ck = h < 100 ? 0.72 : 1;
  const cw = r1(sw * 2.1 * ck), ch = r1(cw * 526 / 739 * 0.68);
  const cap = o.cap || 'grey';
  const travel = h - ch;
  let out = '';
  if (o.scale !== false) {
    const tw = o.tickW || 16;                    // narrow ladders where faders sit close together
    let ticks = '';
    // short faders get a sparse ladder: long ticks at the ends and middle, short between
    const n = h < 100 ? 4 : 20, major = h < 100 ? 2 : 5;
    for (let i = 0; i <= n; i++) {
      const ty = r1(i * travel / n);
      ticks += '<path d="M0 ' + ty + 'h' + (i % major === 0 ? tw : tw / 2) + '"/>';
    }
    out += '<svg aria-hidden="true" width="' + tw + '" height="' + r1(travel) + '" viewBox="0 0 ' + tw + ' ' + r1(travel) + '" style="position:absolute;overflow:visible;pointer-events:none;'
      + 'left:' + r1(x + sw + 3) + 'px;top:' + r1(y + ch / 2) + 'px;'
      + 'opacity:.9"><g fill="none" stroke="' + (o.onDark ? ONBLACK_INK2 : INK2) + '" stroke-width="1">' + ticks + '</g></svg>';
    if (o.minmax !== false) {
      out += txt(x + sw + tw + 4, y + ch / 2 - lineH(6.5) / 2, 30, o.minmax ? o.minmax[1] : '10', { size: 6.5, align: 'left', color: o.onDark ? ONBLACK_INK2 : INK2, weight: 600, ls: 0.02 });
      out += txt(x + sw + tw + 4, y + ch / 2 + travel - lineH(6.5) / 2, 30, o.minmax ? o.minmax[0] : '0', { size: 6.5, align: 'left', color: o.onDark ? ONBLACK_INK2 : INK2, weight: 600, ls: 0.02 });
    }
  }
  // no rail (user, 2026-09-17): only end stops where the cap's edges come to rest
  const stop = (sy) => '<div style="position:absolute;left:' + r1(x - (cw - sw) / 2) + 'px;top:' + r1(sy) + 'px;width:' + cw + 'px;height:2px;'
    + 'border-radius:1px;background:' + (o.onDark ? ONBLACK_INK2 : INK2) + ';pointer-events:none;"></div>';
  out += stop(y - 2) + stop(y + h);
  out += '<img src="refresh-rail.svg" alt="" style="position:absolute;left:' + r1(x + sw / 2 - 3) + 'px;top:' + r1(y) + 'px;width:6px;height:' + r1(h) + 'px;pointer-events:none">';
  out += '<img src="' + A.cap[cap] + '" alt=""' + visId(o) + ' style="position:absolute;left:' + r1(x - (cw - sw) / 2) + 'px;'
    + 'top:' + r1(y + (1 - val) * travel) + 'px;width:' + cw + 'px;height:' + ch + 'px;">';
  out += hit({ id: o.id, domId: o.domId, layered: o.layered, shiftId: o.shiftId, type: 'slider', ctl: 'fader',
               x: x - 8, y: y - 4, w: sw + 16, h: h + 8, title: o.label,
               data: { top: r1(y), travel: r1(travel) } });
  if (o.label) out += txt(x - 40, o.ly != null ? o.ly : y + h + 6, sw + 80, o.label, { size: o.lsize || 8, color: o.labelColor || INK });
  if (o.sub) out += txtInv(x + sw / 2 - wText(o.sub, 6.8, 0.06) / 2, o.sy != null ? o.sy : y + h + 6 + lineH(8) + 2, wText(o.sub, 6.8, 0.06), o.sub, { size: 6.8 });
  return out;
}

// horizontal fader: the same photographs rotated a quarter turn (uniform, no stretch)
export function hfader(o) {
  const x = o.x, y = o.y, w = o.w;
  const val = o.val == null ? 0.5 : o.val;
  const sw = o.sw || 15;
  const cw = r1(sw * 2.1), ch = r1(cw * 526 / 739);
  const travel = w - ch;
  let out = '';
  // no rail: end stops where the cap's edges come to rest
  const stop = (sx) => '<div style="position:absolute;left:' + r1(sx) + 'px;top:' + r1(y + sw / 2 - cw / 2) + 'px;width:2px;height:' + cw + 'px;'
    + 'border-radius:1px;background:' + INK2 + ';pointer-events:none;"></div>';
  out += stop(x - 3) + stop(x + w + 1);
  out += '<div style="position:absolute;left:' + r1(x) + 'px;top:' + r1(y + sw / 2 - 0.75) + 'px;width:' + r1(w) + 'px;height:1.5px;background:' + ORANGE + ';opacity:.85;pointer-events:none;"></div>';
  const tw = r1(w * 0.10);
  if (o.ticks === false) {   // wide faders: just a centre detent mark (the ladder image would be oversized)
    out += '<div style="position:absolute;left:' + r1(x + w / 2 - 1) + 'px;top:' + r1(y - 14) + 'px;width:2px;height:12px;background:' + INK2 + ';pointer-events:none;"></div>';
  } else out += '<img src="' + A.ticks + '" alt="" style="position:absolute;left:' + r1(x + ch / 2) + 'px;top:' + r1(y - tw - 3) + 'px;'
    + 'width:' + r1(tw) + 'px;height:' + r1(travel) + 'px;transform-origin:0 0;transform:translate(0,' + r1(tw) + 'px) rotate(-90deg);opacity:.9;">';
  // cap: a ch x cw box moved by the JS; the wide-and-shallow render sits inside it turned a quarter turn
  out += '<div' + visId(o) + ' style="position:absolute;left:' + r1(x + val * travel) + 'px;top:' + r1(y + sw / 2 - cw / 2) + 'px;'
    + 'width:' + ch + 'px;height:' + cw + 'px;pointer-events:none;">'
    + '<img src="' + A.cap[o.cap || 'grey'] + '" alt="" style="position:absolute;left:' + r1((ch - cw) / 2) + 'px;top:' + r1((cw - ch) / 2) + 'px;'
    + 'width:' + cw + 'px;height:' + ch + 'px;transform:rotate(90deg);"></div>';
  out += hit({ id: o.id, domId: o.domId, layered: o.layered, shiftId: o.shiftId, type: 'slider', ctl: 'hfader',
               x: x - 4, y: y - 12, w: w + 8, h: sw + 24, cursor: 'ew-resize', title: o.label,
               data: { left: r1(x), travel: r1(travel) } });
  if (o.label) out += txt(x - 20, o.ly != null ? o.ly : y + sw + 10, w + 40, o.label, { size: o.lsize || 8, color: o.labelColor || INK });
  out += txt(x - 32, y + 1, 28, o.minmax ? o.minmax[0] : '0', { size: 6.5, align: 'right', color: INK2, weight: 600, ls: 0.02 });
  out += txt(x + w + 4, y + 1, 28, o.minmax ? o.minmax[1] : '10', { size: 6.5, align: 'left', color: INK2, weight: 600, ls: 0.02 });
  return out;
}

// ---- three-position switch (one render per detent) -------------------------
export function sw3(o) {
  const x = o.x, y = o.y + (o.rocker ? 12 : 0);
  const h = o.rocker ? (o.h || 50) * .48 : o.h || 34, w = r1(o.rocker ? h * 2.416 : h * TOGGLE_ASPECT);
  const sprites = o.rocker ? A.rocker : A.sw;
  const pos = o.pos == null ? 1 : o.pos;            // 0 bottom, 1 mid, 2 top
  const dom = o.domId || (o.layered ? 'upper.' + o.id : o.id);
  let out = '<img src="' + sprites[pos] + '" alt=""' + visId(o) + ' style="position:absolute;left:' + r1(x) + 'px;top:' + r1(y) + 'px;'
    + 'width:' + w + 'px;height:' + r1(h) + 'px;">';
  out += hit({ id: o.id, domId: o.domId, layered: o.layered, shiftId: o.shiftId, type: 'combo', ctl: o.rocker ? 'rocker' : 'sw3', cursor: 'pointer',
               x: x - 4, y: y - 4, w: w + 8, h: h + 8, title: o.title || o.label,
               data: Object.assign({ steps: 3, src0: sprites[0], src1: sprites[1], src2: sprites[2], map: '2,1,0',
                                     optbase: dom + '__opt', optink: o.onDark ? ONBLACK_INK2 : (o.lc || 'var(--opt)'),
                                     optsel: o.onDark || o.lc === ONBLACK_INK2 ? 'var(--osel)' : 'var(--sel)' }, o.hitData || {}) });
  if (o.rocker) out += txt(x, y + h + 5, w, '− OCT +', { size: 5.5, color: INK2 });
  if (o.opts && !o.rocker) {
    const ss = o.ssize || 6.6, lh = lineH(ss), nOpt = o.opts.length;
    const pitch = Math.max(h * 0.34, lh + 2);
    for (let i = 0; i < nOpt; i++) {
      const ty = y + h / 2 + ((nOpt - 1) / 2 - i) * pitch - lh / 2;
      out += txt(x + w + 4, ty, 60, o.opts[i], { id: dom + '__opt' + i, size: ss, align: 'left', color: o.lc || INK2, weight: 600, ls: 0.02 });
    }
  }
  if (o.label) out += txt(x - 30, o.ly != null ? o.ly : y + h + 8, w + 60, o.label, { size: o.lsize || 7.6, color: o.labelColor || INK });
  return out;
}
// Two-position toggle, horizontal: the photographed vertical switch turned a quarter turn, so the
// actuator points left (first option) or right (second). Only the two end poses are used, which is
// what a real two-position toggle does. x / y are its centre.
export function sw2h(o) {
  const h = o.h || 30, w = r1(h * TOGGLE_ASPECT);        // the sprite's own upright box
  const dom = o.domId || (o.layered ? 'upper.' + o.id : o.id);
  const sprites = A.sw, pos = o.pos === 1 ? 0 : 2;       // 0 = bottom (right), 2 = top (left)
  const cx = o.x, cy = o.y;
  let out = '<img src="' + sprites[pos] + '" alt=""' + visId(o) + ' style="position:absolute;'
    + 'left:' + r1(cx - w / 2) + 'px;top:' + r1(cy - h / 2) + 'px;width:' + w + 'px;height:' + r1(h) + 'px;'
    + 'transform:rotate(-90deg);">';
  // paint() picks the sprite by detent, so the options are keyed to the sprite index, not the value
  out += hit({ id: o.id, domId: o.domId, layered: o.layered, type: 'combo', ctl: 'sw3', cursor: 'pointer',
               x: cx - h / 2 - 4, y: cy - w / 2 - 6, w: h + 8, h: w + 12, title: o.title || o.label,
               data: { steps: 2, src0: sprites[0], src2: sprites[2], map: '2,0',
                       optbase: dom + '__opt', optink: o.lc || 'var(--opt)', optsel: 'var(--sel)' } });
  const ss = o.ssize || 6.6, lh = lineH(ss), ty = cy - lh / 2;
  if (o.opts) {
    out += txt(cx - h / 2 - 46, ty, 40, o.opts[0], { id: dom + '__opt2', size: ss, align: 'right', color: o.lc || INK2, weight: 600 });
    out += txt(cx + h / 2 + 6, ty, 40, o.opts[1], { id: dom + '__opt1', size: ss, align: 'left', color: o.lc || INK2, weight: 600, style: 'display:none' });
    out += txt(cx + h / 2 + 6, ty, 40, o.opts[1], { id: dom + '__opt0', size: ss, align: 'left', color: o.lc || INK2, weight: 600 });
  }
  if (o.label) out += txt(cx - 60, o.ly != null ? o.ly : cy + w / 2 + 8, 120, o.label, { size: o.lsize || 7.2, color: o.labelColor || INK });
  return out;
}

export const wSw3 = (h, lw) => h * TOGGLE_ASPECT + 4 + (lw == null ? 44 : lw);

// ---- button + LED ----------------------------------------------------------
// A push-key. `dark` uses the black cap (state shown by its LED only). A key is a toggle unless
// `value` marks it as one choice of a combo group, `bit` as one bit of a combo, or `cycle`
// steps through a combo. `action` makes an unbound key that the JS handles by name.
export function button(o) {
  if (STYLE.flat) o = { ...o, flat: true, led: false };
  // Action controls need the same addressable sprite/LED as parameter controls.
  if (o.action) o = { ...o, id: o.domId || ('act-' + o.action) };
  const x = o.x, y = o.y;
  const w = o.w || 25, h = r1(w * BTN_ASPECT);
  let out = '';
  const ld = o.ld || 10;
  const ledY = y - ld - 4;
  const hasLed = o.led !== false;
  if (hasLed) {
    out += '<img src="' + (o.on ? A.ledOn : A.ledOff) + '" alt=""'
      + (o.id ? ' id="' + esc(o.domId || o.id) + '__led"' : '') + ' style="position:absolute;'
      + 'left:' + r1(x + w / 2 - ld / 2) + 'px;top:' + r1(ledY) + 'px;width:' + ld + 'px;height:' + ld + 'px;pointer-events:none;">';
  }
  const off = o.flat ? 'flat-btn-off.svg' : o.dark ? A.btnDark : A.btnOff, on = o.flat ? 'flat-btn-on.svg' : o.dark ? A.btnDarkOn : A.btnOn;
  out += '<img src="' + (o.on ? on : off) + '" alt=""' + visId(o) + ' style="position:absolute;'
    + 'left:' + r1(x) + 'px;top:' + r1(y) + 'px;width:' + r1(w) + 'px;height:' + h + 'px;object-fit:contain;">';
  const lw = o.lw || Math.max(w + 40, wText(o.label, o.lsize || 7.6));
  if (o.label) out += txt(x + w / 2 - lw / 2, o.ly != null ? o.ly : y + h + 7, lw, o.label, { size: o.lsize || 7.6, color: o.labelColor || (o.onDark ? ONBLACK_INK : INK) });
  if (o.sub) out += txtInv(x + w / 2 - wText(o.sub, 6.6, 0.06) / 2, o.sy != null ? o.sy : (o.ly != null ? o.ly : y + h + 7) + lineH(7.6) + 2, wText(o.sub, 6.6, 0.06), o.sub, { size: 6.6 });
  if (o.action) {
    out += '<div id="' + esc(o.domId || ('act-' + o.action)) + '" data-action="' + esc(o.action) + '" data-vis="' + esc(o.domId || ('act-' + o.action)) + '__v" '
      + 'data-on="' + on + '" data-off="' + off + '"' + (hasLed ? ' data-led="' + esc(o.domId || ('act-' + o.action)) + '__led"' : '') + ' data-ledon="' + A.ledOn + '" data-ledoff="' + A.ledOff + '"'
      + (o.title ? ' title="' + esc(o.title) + '"' : '')
      + ' style="position:absolute;left:' + r1(x - 2) + 'px;top:' + r1(hasLed ? ledY - 2 : y - 2) + 'px;width:' + r1(w + 4) + 'px;height:' + r1(h + 4 + (hasLed ? ld + 4 : 0)) + 'px;cursor:pointer;z-index:5;touch-action:none;"></div>';
    return out;
  }
  out += hit({
    id: o.id, domId: o.domId, layered: o.layered, shiftId: o.shiftId, title: o.title || o.label,
    type: (o.value != null || o.bit != null || o.cycle) ? 'combo' : 'toggle',
    ctl: 'button', cursor: 'pointer',
    x: x - 2, y: hasLed ? ledY - 2 : y - 2, w: w + 4, h: h + 4 + (hasLed ? ld + 4 : 0),
    data: Object.assign(
      { on: on, off: off, ledon: A.ledOn, ledoff: A.ledOff, ...(hasLed ? { led: (o.domId || o.id) + '__led' } : {}) },
      o.value != null ? { value: o.value, steps: o.steps || 2 }
      : o.bit != null ? { bit: o.bit, steps: o.steps || 4 }
      : o.cycle ? { steps: o.steps || 3 } : {})
  });
  return out;
}

export function led(x, y, on, d) {
  d = d || 10;
  return '<img src="' + (on ? A.ledOn : A.ledOff) + '" alt="" style="position:absolute;'
    + 'left:' + r1(x) + 'px;top:' + r1(y) + 'px;width:' + d + 'px;height:' + d + 'px;pointer-events:none;">';
}

// A bound LED: lit when the combo parameter equals `value`; clicking selects that value.
// `ledId` names it so the JS can drive it; with `id` null it is display-only via data-led-of.
export function ledBound(o) {
  const d = o.d || 10;
  const dom = o.domId;
  let out = '<img src="' + (o.on ? A.ledOn : A.ledOff) + '" alt="" id="' + esc(dom) + '__v" style="position:absolute;'
    + 'left:' + r1(o.x) + 'px;top:' + r1(o.y) + 'px;width:' + d + 'px;height:' + d + 'px;pointer-events:none;">';
  if (o.label) out += txt(o.x + d + 5, o.y + d / 2 - lineH(o.lsize || 6.6) / 2, o.lw || 70, o.label, { size: o.lsize || 6.6, align: 'left', color: o.lc || INK2, weight: 600, ls: 0.02 });
  out += hit({ id: o.id, domId: dom, layered: o.layered, type: 'combo', ctl: 'led', cursor: 'pointer',
               x: o.x - 4, y: o.y - 4, w: d + 8 + (o.label ? (o.lw || 70) : 0), h: d + 8, title: o.label,
               data: { value: o.value, steps: o.steps, on: A.ledOn, off: A.ledOff } });
  return out;
}

// A vertical ladder of bound LEDs (RANGE 1-4, MODE UP/DOWN/..., voice modes).
export function ledLadder(o) {
  const d = o.d || 10, pitch = o.pitch || Math.max(d + 6, lineH(o.lsize || 6.6) + 3);
  let out = '';
  o.items.forEach((label, i) => {
    out += ledBound({ x: o.x, y: o.y + i * pitch, d: d, label: label, lsize: o.lsize, lw: o.lw, lc: o.lc,
                      id: o.id, domId: (o.layered ? 'upper.' + o.id : o.id) + '__led' + i, layered: o.layered,
                      value: i, steps: o.items.length, on: o.sel === i });
  });
  return out;
}

// ---- pop-over ----------------------------------------------------------------
// An overlay page on the panel surface. Hidden until its opener is clicked.
export function popover(id, title, x, y, w, h, content, o) {
  o = o || {};
  let out = '<div id="' + esc(id) + '" class="pop" hidden style="position:absolute;left:0;top:0;'
    + 'width:100%;height:100%;z-index:40;background:rgba(12,13,15,.6);">';
  out += '<div class="pop-panel" style="position:absolute;left:' + r1(x) + 'px;top:' + r1(y) + 'px;'
    + 'width:' + r1(w) + 'px;height:' + r1(h) + 'px;'
    + 'background:var(--base);border-radius:6px;overflow:hidden;'
    + 'box-shadow:0 14px 50px rgba(0,0,0,.6),inset 0 0 0 1px rgba(0,0,0,.35),inset 0 1px 0 rgba(255,255,255,.7);">';
  // flat chrome: one solid header band on a hairline, no texture, no printed orange rule
  out += '<div class="pop-head" style="position:absolute;left:0;top:0;width:100%;height:52px;pointer-events:none"></div>';
  out += txt(24, 15, 600, title, { size: 12, align: 'left', ls: 0.14 });
  if (o.subtitle) out += txt(24 + wText(title, 12, 0.14) + 18, 21, 1100, o.subtitle, { size: 7.4, align: 'left', color: INK2, weight: 600, ls: 0.1 });
  out += '<div data-close="' + esc(id) + '" class="pop-close" title="Close (Esc)" style="position:absolute;right:10px;top:8px;'
    + 'width:36px;height:36px;cursor:pointer;z-index:6;">'
    + '<svg width="36" height="36" viewBox="0 0 36 36"><path d="M12.5 12.5 L23.5 23.5 M23.5 12.5 L12.5 23.5" '
    + 'stroke="currentColor" stroke-width="2.2" stroke-linecap="round" fill="none"/></svg></div>';
  out += content;
  out += '</div></div>';
  return out;
}

// ---- text button (pop-over furniture: a photographed key with the legend printed on it) -----
export function keyButton(x, y, w, label, action, o) {
  o = o || {};
  if (STYLE.flat && !o.link) o = { ...o, link: true, cls: ((o.cls || '') + ' pill').trim(), h: o.h || 40, size: o.size || 8 };
  // link: the legend alone is the control (no key cap); colour var(--link), lit = aria-pressed
  if (o.link) {
    const size = (o.size || 8.6) * TEXT, h = o.h || Math.round(size * 1.6);
    return '<div id="' + esc(o.domId || ('act-' + action)) + '" class="link' + (o.cls ? ' ' + o.cls : '') + '" data-action="' + esc(action) + '"'
      + (o.title ? ' title="' + esc(o.title) + '"' : '')
      + ' style="position:absolute;left:' + r1(x) + 'px;top:' + r1(y) + 'px;width:' + r1(w) + 'px;height:' + h + 'px;'
      + 'line-height:' + h + 'px;text-align:' + (o.align || 'center') + ';font-family:' + FONT + ';font-stretch:75%;font-weight:700;'
      + 'font-size:' + r1(size) + 'px;letter-spacing:.08em;text-transform:uppercase;white-space:nowrap;cursor:pointer;z-index:7;'
      + (o.color ? 'color:' + o.color + ';' : '') + '">' + esc(label) + '</div>';
  }
  // Fit the photograph inside the button footprint without distorting the cap.
  // Wide actions use a small photographed key with a silkscreen legend beside it.
  const capW = Math.min(w, y < 20 ? 28 : 44), h = Math.round(capW * BTN_ASPECT);
  const capX = w > 150 ? x : x + (w - capW) / 2;
  const dom = o.domId || ('act-' + action);
  return '<img src="' + (o.dark ? A.btnDark : A.btnOff) + '" alt="" id="' + esc(dom) + '__v" style="position:absolute;left:' + capX + 'px;top:' + y + 'px;width:' + capW + 'px;height:' + h + 'px;object-fit:contain;">'
    + txt(w > 150 ? x + capW + 12 : x - 5, w > 150 ? y + 7 : y + h + 5, w > 150 ? w - capW - 12 : w + 10, label, { size: o.size || 7.6, align: w > 150 ? 'left' : 'center', color: o.dark ? ONBLACK_INK : INK, ls: 0.05 })
    + '<div id="' + esc(dom) + '" data-action="' + esc(action) + '" data-vis="' + esc(dom) + '__v" data-on="' + (o.dark ? A.btnDarkOn : A.btnOn) + '" data-off="' + (o.dark ? A.btnDark : A.btnOff) + '"'
    + (o.title ? ' title="' + esc(o.title) + '"' : '')
    + ' style="position:absolute;left:' + x + 'px;top:' + y + 'px;width:' + w + 'px;height:' + (w > 150 ? h : h + 23) + 'px;cursor:pointer;z-index:5;touch-action:none;"></div>';
}
