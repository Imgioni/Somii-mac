// Run with: node tests/UiSyntaxTests.mjs   (also imported by WebUiTests.mjs)
// A parse error in any imported module disables all controls and window fitting.
//
// `node --check` is NOT usable here: on Node 24 it silently exits 0 for any file containing an
// `import` statement, even when that file has a syntax error (verified 2026-10-04) — and every
// module in ui/ has imports. Parse each file as a real ES module instead, without running it,
// which needs --experimental-vm-modules; without the flag the work is handed to a child process.
import { readdirSync, readFileSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { join, resolve } from 'node:path';
import vm from 'node:vm';

const self = fileURLToPath(import.meta.url);
const isMain = !!process.argv[1] && resolve(process.argv[1]) === self;

if (!vm.SourceTextModule) {
  // Re-run with the flag. Exiting here would also end whatever imported us, so when we are only
  // a dependency, report by throwing instead.
  const r = spawnSync(process.execPath, ['--experimental-vm-modules', '--no-warnings', self], { stdio: 'inherit' });
  if (isMain) process.exit(r.status ?? 1);
  if (r.status !== 0) throw new Error('UI JavaScript failed to parse (see above)');
} else {
  const ui = fileURLToPath(new URL('../ui/', import.meta.url));
  const scripts = readdirSync(ui, { recursive: true }).filter(f => f.endsWith('.js') || f.endsWith('.mjs'));
  let failed = 0;
  for (const file of scripts) {
    try { new vm.SourceTextModule(readFileSync(join(ui, file), 'utf8'), { identifier: file }); }
    catch (e) { console.error(`SYNTAX ERROR  ${file}: ${e.message.split('\n')[0]}`); failed++; }
  }
  if (failed) { console.error(`${failed} UI JavaScript file(s) failed to parse.`); process.exit(1); }
  console.log(`${scripts.length} UI JavaScript files parsed cleanly as ES modules.`);
}
