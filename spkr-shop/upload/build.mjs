// Builds dist/ for Vercel: wraps index.html (kept as a fragment for the Claude artifact preview)
// in a full HTML document and copies the assets.
import fs from 'node:fs';

const src = fs.readFileSync('index.html', 'utf8');
const split = src.indexOf('<div class="grain"');
if (split < 0) throw new Error('index.html: could not find where the page body starts');

fs.rmSync('dist', { recursive: true, force: true });
fs.mkdirSync('dist');
fs.cpSync('assets', 'dist/assets', { recursive: true });
fs.writeFileSync('dist/index.html', `<!doctype html>
<html lang="en">
<head>
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
${src.slice(0, split)}</head>
<body>
${src.slice(split)}</body>
</html>
`);
console.log('built dist/');
