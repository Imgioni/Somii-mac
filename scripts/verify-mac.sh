#!/usr/bin/env bash
# Check every shipping format, not just the architecture of the build host.
set -euo pipefail
cd "$(dirname "$0")/.."
art=build-mac/Geminus_artefacts/Release
for relative in VST3/Somii.vst3 AU/Somii.component CLAP/Somii.clap Standalone/Somii.app; do
    bundle="$art/$relative"
    executable=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$bundle/Contents/Info.plist")
    binary="$bundle/Contents/MacOS/$executable"
    test -x "$binary"
    lipo -verify_arch arm64 x86_64 "$binary"
    echo "Verified $relative (arm64 + x86_64)"
done
