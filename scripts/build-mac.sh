#!/usr/bin/env bash
# Builds Somii on macOS (universal: Apple Silicon + Intel), Release.
# Extra arguments go to "cmake --build", e.g.  scripts/build-mac.sh --target SGTests
set -euo pipefail
cd "$(dirname "$0")/.."

[ "$(uname -s)" = Darwin ] || { echo "Run on macOS or use the GitHub Actions macOS workflow."; exit 1; }
command -v cmake >/dev/null || { echo "cmake not found. Install Xcode command line tools and cmake (brew install cmake)."; exit 1; }
xcode-select -p >/dev/null 2>&1 || { echo "Xcode command line tools not found. Run: xcode-select --install"; exit 1; }

[ -d third_party/JUCE ] || { echo "third_party/JUCE is missing. See docs/BUILD-MACOS.md."; exit 1; }

[ -f third_party/clap-juce-extensions/CMakeLists.txt ] || { echo "clap-juce-extensions is missing. See docs/BUILD-MACOS.md."; exit 1; }

# Choose a generator before configuring, and preserve real configure errors.
generator=()
if [ ! -f build-mac/CMakeCache.txt ]; then
    if command -v ninja >/dev/null; then generator=(-G Ninja); else generator=(-G "Unix Makefiles"); fi
fi
cmake -S . -B build-mac "${generator[@]}" -DCMAKE_BUILD_TYPE=Release \
    '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15 -DGEMINUS_DEV_UI=OFF
cmake --build build-mac --config Release --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-3}" "$@"
