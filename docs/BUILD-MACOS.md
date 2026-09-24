# Building 002 by SPKR on macOS

The project is developed on Windows. macOS builds are produced either by GitHub Actions
(`.github/workflows/macos.yml`, no Mac needed) or by hand on a Mac. **None of this has been
compiled on a Mac yet** — the first CI run is the real test.

macOS gets four formats: **VST3**, **Audio Unit**, **CLAP** and the **standalone app**, as one
universal binary for Apple Silicon and Intel, back to macOS 10.15.

## By hand, on a Mac

Needs Xcode command line tools (`xcode-select --install`), CMake, and Ninja (optional).
`third_party/` is not in the repository, so fetch the dependencies once:

```
git clone --depth 1 --branch 8.0.15 https://github.com/juce-framework/JUCE third_party/JUCE
git clone --depth 1 --recurse-submodules https://github.com/free-audio/clap-juce-extensions third_party/clap-juce-extensions
bash scripts/build-mac.sh
./build-mac/SGTests_artefacts/Release/SGTests
bash installer/mac/build_pkg.sh
```

The build copies the plug-ins into `~/Library/Audio/Plug-Ins` (JUCE does this itself on macOS;
the Windows `Common Files` path in `CMakeLists.txt` is applied only on Windows). The package
lands in `dist/002-by-SPKR-<version>-macOS-Setup.pkg`, with a matching `.zip`.

## Through GitHub Actions

Every push builds and packages macOS, and the result is on the run page under **Artifacts**.
The workflow also runs the DSP tests, the plugin smoke test and Apple's `auval`; the last two
are informative only, since a CI runner has no audio device or real user session.

## Signing and notarisation

Unsigned builds install and run, but macOS says the developer is unidentified: the user must
right-click the `.pkg` and choose Open, or allow it in System Settings > Privacy & Security.
To ship signed, set these before `installer/mac/build_pkg.sh` (they need a paid Apple Developer
account, and on CI the certificates must be imported into the runner's keychain first):

| Variable | What it is |
|---|---|
| `SPKR_SIGN_ID` | `Developer ID Application: … (TEAMID)` — signs the plug-ins and the app |
| `SPKR_INSTALLER_ID` | `Developer ID Installer: … (TEAMID)` — signs the `.pkg` |
| `SPKR_NOTARY_PROFILE` | a stored `notarytool` keychain profile — notarises and staples |

## What differs from Windows

- **The UI font.** The panel is measured in Bahnschrift, which only Windows has. `ui/` carries
  Roboto Condensed (SIL OFL, `ui/FONT-LICENSE.txt`) as `SPKR Condensed`, next in the stack after
  Bahnschrift, so Windows is unchanged and macOS gets a condensed face with near-enough metrics.
  Measured over the 819 fixed-width labels of the panel: Bahnschrift 0 overflow, the bundled face
  4 labels over by at most 5 px (on a 3400 px canvas), the old fallback (Arial Narrow) 28 labels
  over by up to 22 px.
- **The WebView.** Windows uses WebView2 (statically linked loader); macOS uses WKWebView, which
  is part of the system. Nothing to install for users either way.
- **Windows-only pieces**, all guarded in CMake: the WebView2 SDK, the `GeminusLifecycle` tool,
  MSVC flags, and the Inno Setup installer in `installer/`.
