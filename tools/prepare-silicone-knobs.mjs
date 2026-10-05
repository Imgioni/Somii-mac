import fs from 'node:fs';
// Keep the generated material and its illumination stationary. Only the UI's
// separate vector index rotates; all knob sizes share these three bodies.
const png=fs.readFileSync(new URL('../design/hardware-refresh/assets/knobs-unmarked.png',import.meta.url));
const width=png.readUInt32BE(16),height=png.readUInt32BE(20);
for(const [color,x] of [['cream',72],['orange',478],['dark',892]]) {
  const svg=`<svg xmlns="http://www.w3.org/2000/svg" width="296" height="302" viewBox="${x} 103 296 302" preserveAspectRatio="none"><image width="${width}" height="${height}" href="data:image/png;base64,${png.toString('base64')}"/></svg>`;
  fs.writeFileSync(new URL(`../ui/silicone-knob-${color}.svg`,import.meta.url),svg);
}
