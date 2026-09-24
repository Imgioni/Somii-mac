#!/usr/bin/env bash
# Builds 002 by SPKR on macOS (universal: Apple Silicon + Intel), Release.
# Extra arguments go to "cmake --build", e.g.  scripts/build-mac.sh --target SGTests
set -euo pipefail
cd "$(dirname "$0")/.."

command -v cmake >/dev/null || { echo "cmake not found. Install Xcode command line tools and cmake (brew install cmake)."; exit 1; }
xcode-select -p >/dev/null 2>&1 || { echo "Xcode command line tools not found. Run: xcode-select --install"; exit 1; }

[ -d third_party/JUCE ] || { echo "third_party/JUCE is missing. See docs/BUILD-MACOS.md."; exit 1; }

if [ ! -f build-mac/CMakeCache.txt ]; then
    cmake -S . -B build-mac -G Ninja -DCMAKE_BUILD_TYPE=Release 2>/dev/null \
        || cmake -S . -B build-mac -DCMAKE_BUILD_TYPE=Release   # no Ninja: fall back to Make
fi
cmake --build build-mac "$@"
