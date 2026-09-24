// Primitives for the Geminus panel. Every physical part is an image asset;
// only silkscreen (text, ticks, rules, black insets) is drawn.
export const INK = '#23252A';
export const INK2 = '#5A5C60';
export const ORANGE = '#F65A27';
export const BLACKPANEL = '#424243';

export const A = {
  panel: 'panel.png', panelDark: 'panel-dark.png', shading: 'shading.jpg', glass: 'glass.png',
  // knobs2: indicator line moulded into the cap, all three at 12 o'clock
  knob: { cream: 'k2-cream.png', orange: 'k2-orange.png', dark: 'k2-dark.png' },
  slot: 'fader-slot.png',
  cap: { grey: 'fader-cap-grey.png', orange: 'fader-cap-orange.png', dark: 'fader-cap-dark.png' },
  // switch3: real three-detent slide switch, one render per position
  sw: ['sw-bot.png', 'sw-mid.png', 'sw-top.png'],
  btnOff: 'btn-off.png', btnOn: 'btn-on.png',
  ledOff: 'led-off.png', ledOn: 'led-on.png',
  ribbon: 'ribbon.png', cheek: 'cheek.png', octave: 'octave.png'
};
export const SW_ASPECT = 44 / 96;

const r1 = (n) => Math.round(n * 10) / 10;
export const esc = (s) => String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');

const FONT = "'Barlow Condensed','Arial Narrow',Arial,sans-serif";

// ---- silkscreen text -------------------------------------------------------
export function txt(x, y, w, s, o) {
  o = o || {};
  const size = o.size || 9, weight = o.weight || 700, col = o.color || INK;
  const align = o.align || 'center', ls = o.ls == null ? 0.07 : o.ls;
  return '<div style="position:absolute;left:' + r1(x) + 'px;top:' + r1(y) + 'px;width:' + r1(w) + 'px;'
    + 'text-align:' + align + ';font-family:' + FONT + ';'
    + 'font-weight:' + weight + ';font-size:' + size + 'px;line-height:1.05;color:' + col + ';'
    + 'letter-spacing:' + ls + 'em;text-transform:uppercase;white-space:nowrap;">' + esc(s) + '</div>';
}

// secondary (shift) function: the hardware prints these inverted
export function txtInv(x, y, w, s, o) {
  o = o || {};
  const size = o.size || 7.5;
  return '<div style="position:absolute;left:' + r1(x) + 'px;top:' + r1(y) + 'px;width:' + r1(w) + 'px;'
    + 'text-align:center;font-family:' + FONT + ';'
    + 'font-weight:700;font-size:' + size + 'px;line-height:1.55;color:#E8E6E0;background:' + INK + ';'
    + 'letter-spacing:.07em;text-transform:uppercase;white-space:nowrap;border-radius:1px;">' + esc(s) + '</div>';
}

// ---- rules / frames --------------------------------------------------------
export function vrule(x, y, h, c) {
  return '<div style="position:absolute;left:' + r1(x) + 'px;top:' + r1(y) + 'px;width:1px;height:'
    + r1(h) + 'px;background:' + (c || '#9A9B99') + ';opacity:.75;"></div>';
}
export function hrule(x, y, w, c) {
  return '<div style="position:absolute;left:' + r1(x) + 'px;top:' + r1(y) + 'px;width:' + r1(w)
    + 'px;height:1px;background:' + (c || '#9A9B99') + ';opacity:.75;"></div>';
}
// inset sub-panels carry the supplied gray panel surface, not a flat fill
export function blackPanel(x, y, w, h) {
  return '<div style="position:absolute;left:' + r1(x) + 'px;top:' + r1(y) + 'px;width:' + r1(w) + 'px;height:' + r1(h) + 'px;'
    + 'background:' + BLACKPANEL + ' url(' + A.panelDark + ') repeat;border-radius:3px;'
    + 'box-shadow:inset 0 2px 5px rgba(0,0,0,.55),inset 0 0 0 1px rgba(0,0,0,.35),0 1px 0 rgba(255,255,255,.5);"></div>';
}

// ---- preset loader bar -----------------------------------------------------
// host-style strip above the instrument: the gray panel surface, boxed with a
// border, carrying a preset field and a dropdown chevron on its right
export function presetBar(x, y, w) {
  const h = 44;
  let o = '<div style="position:absolute;left:' + r1(x) + 'px;top:' + r1(y) + 'px;width:' + r1(w) + 'px;height:' + h + 'px;'
    + 'background:' + BLACKPANEL + ' url(' + A.panelDark + ') repeat;border-radius:3px;'
    + 'box-shadow:inset 0 1px 0 rgba(255,255,255,.14),inset 0 0 0 1px rgba(0,0,0,.45),0 1px 2px rgba(0,0,0,.3);"></div>';
  const fw = 182, fh = 28, fx = x + 18, fy = y + (h - fh) / 2;
  o += '<div style="position:absolute;left:' + r1(fx) + 'px;top:' + r1(fy) + 'px;width:' + fw + 'px;height:' + fh + 'px;'
    + 'background:rgba(0,0,0,.26);border-radius:2px;'
    + 'box-shadow:inset 0 1px 2px rgba(0,0,0,.5),inset 0 0 0 1px rgba(255,255,255,.10);"></div>';
  o += txt(fx + 13, fy + 8.5, fw - 46, 'PATCH', { size: 10.5, align: 'left', color: '#D9D7D1', ls: 0.16 });
  const cx = fx + fw - 20, cy = fy + fh / 2;
  o += '<svg width="12" height="8" viewBox="0 0 12 8" style="position:absolute;left:' + r1(cx - 6) + 'px;top:' + r1(cy - 4) + 'px;">'
    + '<path d="M1 1.8 L6 6.2 L11 1.8" fill="none" stroke="#D9D7D1" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"/></svg>';
  return o;
}

// ---- knob ------------------------------------------------------------------
// val 0..1 maps to -150deg..+150deg. No pointer is drawn on the cap: the
// sprite rotates and the panel carries a printed tick scale, as on the UDO.
export function knob(o) {
  const x = o.x, y = o.y, d = o.d;
  const v = o.v || 'cream', val = o.val == null ? 0.5 : o.val;
  const deg = -150 + val * 300;
  const n = o.ticks == null ? 11 : o.ticks;
  const tkMaj = o.tick || INK, tkMin = o.tick || INK2;
  let out = '';
  if (n > 0) {
    const pad = Math.max(6, d * 0.18), S = d + pad * 2, c = S / 2, R = d / 2;
    let lines = '';
    for (let i = 0; i < n; i++) {
      const t = n === 1 ? 0 : i / (n - 1);
      const ang = (-150 + t * 300) * Math.PI / 180;
      const major = (i === 0 || i === n - 1 || (n % 2 === 1 && i === (n - 1) / 2));
      const ra = R + 1.5, rb = R + (major ? pad * 0.82 : pad * 0.5);
      lines += '<line x1="' + r1(c + Math.sin(ang) * ra) + '" y1="' + r1(c - Math.cos(ang) * ra)
        + '" x2="' + r1(c + Math.sin(ang) * rb) + '" y2="' + r1(c - Math.cos(ang) * rb)
        + '" stroke="' + (major ? tkMaj : tkMin) + '" stroke-width="' + (major ? 1.1 : 0.8) + '" stroke-linecap="round"/>';
    }
    out += '<svg width="' + r1(S) + '" height="' + r1(S) + '" viewBox="0 0 ' + r1(S) + ' ' + r1(S) + '" '
      + 'style="position:absolute;left:' + r1(x - pad) + 'px;top:' + r1(y - pad) + 'px;overflow:visible;">' + lines + '</svg>';
  }
  out += '<img src="' + A.knob[v] + '" alt="" style="position:absolute;left:' + r1(x) + 'px;top:' + r1(y) + 'px;'
    + 'width:' + r1(d) + 'px;height:' + r1(d) + 'px;transform:rotate(' + r1(deg) + 'deg);">';
  // indicator bar drawn on top of the cap and rotated with it
  const IND = { cream: '#2E3034', orange: '#6B1C0A', dark: '#E6E4DE' };
  const bw = Math.max(1.5, d * 0.052), bt = d * 0.10, bh = d * 0.28;
  out += '<div style="position:absolute;left:' + r1(x + d / 2 - bw / 2) + 'px;top:' + r1(y + bt) + 'px;'
    + 'width:' + r1(bw) + 'px;height:' + r1(bh) + 'px;background:' + IND[v] + ';border-radius:' + r1(bw / 2) + 'px;'
    + 'transform-origin:' + r1(bw / 2) + 'px ' + r1(d / 2 - bt) + 'px;transform:rotate(' + r1(deg) + 'deg);"></div>';
  if (o.label) out += txt(x - 24, o.ly != null ? o.ly : y + d + 4, d + 48, o.label, { size: o.lsize || 8.5, color: o.labelColor || INK });
  if (o.sub) out += txtInv(x + d / 2 - 21, o.sy != null ? o.sy : y + d + (o.label ? 14 : 4), 42, o.sub, { size: 7 });
  return out;
}

// stepped rotary (WAVEFORM / RANGE) - ticks land on the labelled positions
export function rotary(o) {
  const x = o.x, y = o.y, d = o.d, steps = o.steps;
  const n = steps.length, sel = o.sel == null ? 0 : o.sel;
  const lc = o.lc || INK2;                       // light on black insets
  const tc = o.tc || null;                       // tick colour override
  let out = knob({ x: x, y: y, d: d, v: o.v || 'cream', val: n === 1 ? 0.5 : sel / (n - 1), ticks: n, tick: tc });
  const pad = Math.max(6, d * 0.18), R = d / 2 + pad + 7, cx = x + d / 2, cy = y + d / 2;
  const fs = o.ssize || 6.2;
  for (let i = 0; i < n; i++) {
    const ang = (-150 + (i / (n - 1)) * 300) * Math.PI / 180;
    const lx = cx + Math.sin(ang) * R, ly = cy - Math.cos(ang) * R;
    // anchor outward so labels fan away from the cap instead of colliding
    const s = Math.sin(ang);
    const align = s < -0.25 ? 'right' : (s > 0.25 ? 'left' : 'center');
    const ox = align === 'right' ? -34 : (align === 'left' ? 0 : -17);
    out += txt(lx + ox, ly - fs / 2, 34, steps[i], { size: fs, color: lc, weight: 600, ls: 0.02, align: align });
  }
  if (o.label) out += txt(x - 26, o.ly != null ? o.ly : y + d + pad + 11, d + 52, o.label, { size: o.lsize || 8.5, color: o.labelColor || INK });
  return out;
}

// ---- fader -----------------------------------------------------------------
export function fader(o) {
  const x = o.x, y = o.y, h = o.h;
  const val = o.val == null ? 0.5 : o.val;
  const sw = o.sw || 15;
  // cap:slot width ratio is the real one from the render (280:203); cap height
  // follows the cap's TRUE aspect (456:280) rather than a stretched sprite
  const cw = r1(sw * 1.38), ch = r1(sw * 1.38 * (91 / 56));
  const cap = o.cap || 'grey';
  const travel = h - ch;
  let out = '';
  if (o.scale !== false) {
    const n = 11; let lines = '';
    for (let i = 0; i < n; i++) {
      const ty = (ch / 2) + (travel * i / (n - 1));
      const major = (i === 0 || i === n - 1 || i === (n - 1) / 2);
      lines += '<line x1="0" y1="' + r1(ty) + '" x2="' + (major ? 5 : 3.2) + '" y2="' + r1(ty)
        + '" stroke="' + (major ? INK : INK2) + '" stroke-width="' + (major ? 1.1 : 0.8) + '" stroke-linecap="round"/>';
    }
    out += '<svg width="6" height="' + r1(h) + '" viewBox="0 0 6 ' + r1(h) + '" style="position:absolute;'
      + 'left:' + r1(x + sw + 2) + 'px;top:' + r1(y) + 'px;overflow:visible;">' + lines + '</svg>';
  }
  out += '<img src="' + A.slot + '" alt="" style="position:absolute;left:' + r1(x) + 'px;top:' + r1(y) + 'px;'
    + 'width:' + r1(sw) + 'px;height:' + r1(h) + 'px;">';
  out += '<img src="' + A.cap[cap] + '" alt="" style="position:absolute;left:' + r1(x - (cw - sw) / 2) + 'px;'
    + 'top:' + r1(y + (1 - val) * travel) + 'px;width:' + cw + 'px;height:' + ch + 'px;">';
  if (o.label) out += txt(x - 22, o.ly != null ? o.ly : y + h + 3.5, sw + 44, o.label, { size: o.lsize || 8, color: o.labelColor || INK });
  if (o.sub) out += txtInv(x - 14, o.sy != null ? o.sy : y + h + 13.5, sw + 28, o.sub, { size: 6.8 });
  return out;
}

// ---- three-position switch (switch3.png, one render per detent) ------------
export function sw3(o) {
  const x = o.x, y = o.y;
  const h = o.h || 34, w = r1(h * SW_ASPECT);
  const pos = o.pos == null ? 1 : o.pos;            // 0 bottom, 1 mid, 2 top
  let out = '<img src="' + A.sw[pos] + '" alt="" style="position:absolute;left:' + r1(x) + 'px;top:' + r1(y) + 'px;'
    + 'width:' + w + 'px;height:' + r1(h) + 'px;">';
  if (o.opts) {
    // detent labels sit level with the three cap positions
    for (let i = 0; i < o.opts.length; i++) {
      const ty = y + h * 0.16 + (1 - i / 2) * (h * 0.66) - 3.4;
      out += txt(x + w + 2.5, ty, 34, o.opts[i],
        { size: o.ssize || 6.2, align: 'left', color: o.lc || INK2, weight: 600, ls: 0.02 });
    }
  }
  if (o.label) out += txt(x - 20, o.ly != null ? o.ly : y + h + 3, w + 40, o.label, { size: o.lsize || 7.6, color: o.labelColor || INK });
  return out;
}

// ---- button + LED ----------------------------------------------------------
export function button(o) {
  const x = o.x, y = o.y;
  const w = o.w || 25, h = r1(w * (84 / 120));
  let out = '';
  if (o.led !== false) {
    const ld = o.ld || 8;
    out += '<img src="' + (o.on ? A.ledOn : A.ledOff) + '" alt="" style="position:absolute;'
      + 'left:' + r1(x + w / 2 - ld / 2) + 'px;top:' + r1(y - ld - 3) + 'px;width:' + ld + 'px;height:' + ld + 'px;">';
  }
  out += '<img src="' + (o.on ? A.btnOn : A.btnOff) + '" alt="" style="position:absolute;'
    + 'left:' + r1(x) + 'px;top:' + r1(y) + 'px;width:' + r1(w) + 'px;height:' + h + 'px;">';
  if (o.label) out += txt(x - 18, o.ly != null ? o.ly : y + h + 3, w + 36, o.label, { size: o.lsize || 7.6, color: o.dark ? '#D8D6D0' : INK });
  return out;
}

export function led(x, y, on, d) {
  d = d || 8;
  return '<img src="' + (on ? A.ledOn : A.ledOff) + '" alt="" style="position:absolute;'
    + 'left:' + r1(x) + 'px;top:' + r1(y) + 'px;width:' + d + 'px;height:' + d + 'px;">';
}

// ---- section header --------------------------------------------------------
export function sect(x, y, w, title, o) {
  o = o || {};
  const size = o.size || 10.5;
  let out = txt(x, y, w, title, { size: size, align: o.align || 'left', color: o.color || INK, ls: 0.1 });
  if (o.rule !== false) out += hrule(x, y + size + 3, w, o.ruleColor || '#8E8F8D');
  return out;
}
