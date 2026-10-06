Somii by SPKR — macOS

The installer includes universal Apple Silicon and Intel builds:
  VST3:       /Library/Audio/Plug-Ins/VST3/Somii.vst3
  Audio Unit: /Library/Audio/Plug-Ins/Components/Somii.component
  CLAP:       /Library/Audio/Plug-Ins/CLAP/Somii.clap
  Standalone: /Applications/Somii.app

For the ZIP, copy the plug-in bundles to the corresponding folders above,
or to the same Audio/Plug-Ins folders inside your home Library folder.
Copy Somii.app to Applications. Keep each bundle intact.

Restart your DAW, rescan its plug-ins, and load Somii on an instrument track.
Logic Pro uses the Audio Unit. Other hosts can use VST3 or CLAP if supported.
The interface uses macOS's built-in WKWebView; no WebView2 download is needed.

Development packages are ad-hoc signed, not Apple-notarized. macOS security
may block downloaded builds. Use signed, notarized releases for distribution.

To uninstall, close your DAW and remove the bundles listed above.

spkr.shop
