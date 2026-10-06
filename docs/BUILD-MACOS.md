# Building Somii by SPKR on macOS

Development can stay on Windows. The actual Mac binaries must be compiled on a Mac,
including a GitHub Actions macOS runner. Renaming a Windows VST3 does not convert it.

The build produces VST3, Audio Unit, CLAP and a standalone app with both `arm64` and
`x86_64` slices. The deployment target is macOS 10.15 (Intel); Apple Silicon requires
macOS 11 or later. These are build targets, not a claim that every OS/DAW was tested.
The internal Geminus target and plug-in IDs remain stable for existing DAW projects.

## From Windows using GitHub Actions

Push the changes to the Somii repository, then open its **Actions > macOS build** run.
A successful run offers **Somii-macOS** under Artifacts, containing:

- `Somii-0.5.0-macOS-Setup.pkg`
- `Somii-0.5.0-macOS.zip`

The workflow compiles shipping formats and validation tools, runs DSP tests, loads the VST3 and renders audio in
memory, runs Apple's Audio Unit validator, and checks that all four bundles contain
both architectures. Validation failures stop packaging. The package script verifies
bundle signatures and uses the same signed bundles in the installer and ZIP.
The tests run natively on the runner; checking both binary slices does not substitute
for running the instrument on a real Intel Mac and an Apple Silicon Mac.

The web editor still needs a real DAW check: open, close and reopen it, play MIDI,
change controls, and save/reopen a project. The smoke test checks audio and state;
it does not open the web editor.

## By hand on a Mac

Install Xcode command line tools, CMake, and optionally Ninja. Fetch dependencies once:

```sh
xcode-select --install
brew install cmake ninja
git clone --depth 1 --branch 8.0.15 https://github.com/juce-framework/JUCE third_party/JUCE
git clone https://github.com/free-audio/clap-juce-extensions third_party/clap-juce-extensions
git -C third_party/clap-juce-extensions checkout 9fbefae3d9c3d130aafb558c1ec15427a4bd24be
git -C third_party/clap-juce-extensions submodule update --init --recursive
bash scripts/build-mac.sh
./build-mac/SGTests_artefacts/Release/SGTests
./build-mac/SGPluginSmoke_artefacts/Release/SGPluginSmoke "build-mac/Geminus_artefacts/Release/VST3/Somii.vst3"
auval -v aumu Gmns Spkr
bash scripts/verify-mac.sh
bash installer/mac/build_pkg.sh
```

The build copies plug-ins into the user Library plug-in folders. The installer uses
system Library folders; see `installer/mac/README.txt`. Build outputs are in `build-mac`,
and installers are in `dist`. The shipping UI is embedded, with no source-folder dependency.

## Signing and notarisation

Without Apple credentials, the package script ad-hoc signs the bundles. This is useful
for development but is not Developer ID signing or notarisation, and Gatekeeper may
block downloads. For distribution, supply these environment variables on the Mac:

| Variable | Purpose |
|---|---|
| `SPKR_SIGN_ID` | Developer ID Application identity in the keychain |
| `SPKR_INSTALLER_ID` | Developer ID Installer identity in the keychain |
| `SPKR_NOTARY_PROFILE` | Stored notarytool keychain profile |

The script signs the bundles and installer, then submits and staples the installer when
configured. The ZIP uses the signed bundles but is not separately notarized by this script.
Certificates must be imported into the CI runner keychain separately; the workflow does
not provision signing credentials.

## Platform differences

Windows uses WebView2; macOS uses the system WKWebView. The UI includes a condensed
fallback font for systems without Bahnschrift. Windows SDK paths and MSVC options
are guarded. The legacy native UI snapshot tool also compiles the processor's web
editor, so it needs the embedded UI resource target and browser support even though
its snapshots use the native panel.
