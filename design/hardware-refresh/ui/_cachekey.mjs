import fs from 'node:fs';
const p = 'C:/Users/w0nde/Desktop/Vst/src/plugin/WebEditor.cpp';
let s = fs.readFileSync(p, 'utf8');

// 1. the key helper, next to the other file-scope helpers
const anchor = '} // namespace';
if (!s.includes(anchor)) { console.log('MISS namespace'); process.exit(1); }
const helper = [
  '',
  '// WebView2 caches what the resource provider serves, and it keys that cache by the user data',
  '// folder. A fixed folder means a stale page can survive a rebuild and mask every UI change,',
  '// which looks exactly like a broken UI. Keying the folder to the content of the page makes a',
  '// changed ui/ land in a fresh cache instead.',
  'juce::String uiCacheKey()',
  '{',
  '    uint64_t h = 1469598103934665603ULL;              // FNV-1a over the served page',
  '    for (int i = 0; i < GeminusUiData::namedResourceListSize; ++i)',
  '    {',
  '        if (juce::String (GeminusUiData::originalFilenames[i]) != "index.html")',
  '            continue;',
  '',
  '        int size = 0;',
  '        if (const auto* d = GeminusUiData::getNamedResource (GeminusUiData::namedResourceList[i], size))',
  '            for (int j = 0; j < size; ++j)',
  '            {',
  '                h ^= static_cast<unsigned char> (d[j]);',
  '                h *= 1099511628211ULL;',
  '            }',
  '        break;',
  '    }',
  '    return juce::String::toHexString (static_cast<juce::int64> (h));',
  '}',
  '} // namespace'
].join('\n');
s = s.replace(anchor, helper);

// 2. use it for the user data folder
const oldF = '.withUserDataFolder (juce::File::getSpecialLocation (\n                                                juce::File::tempDirectory).getChildFile ("GeminusWebView")));';
const newF = '.withUserDataFolder (juce::File::getSpecialLocation (\n                                                juce::File::tempDirectory)\n                                                .getChildFile ("GeminusWebView-" + uiCacheKey())));';
if (!s.includes(oldF)) { console.log('MISS userDataFolder'); process.exit(1); }
s = s.replace(oldF, newF);

fs.writeFileSync(p, s);
console.log('cache folder now keyed to the page content');
