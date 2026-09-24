import fs from 'node:fs';
for (const f of ['Main','LayerRow','Components']) {
  const s = fs.readFileSync(f + '.dc.html', 'utf8');
  const body = s.slice(s.indexOf('</helmet>') + 9, s.indexOf('</x-dc>'));
  const head = s.slice(s.indexOf('<helmet>') + 8, s.indexOf('</helmet>'));
  fs.writeFileSync('_preview-' + f + '.html',
    '<!doctype html><html><head><meta charset="utf-8">' + head +
    '</head><body style="margin:0;background:#4a4a4a;padding:0">' + body +
    (f === 'Main' ? '<script type="module" src="geminus.js"></script>' : '') +
    '</body></html>');
}
console.log('previews written (relative refs)');
