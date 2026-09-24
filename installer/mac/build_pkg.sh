#!/usr/bin/env bash
# Packages the macOS build into dist/002-by-SPKR-<version>-macOS-Setup.pkg and a matching .zip.
# Run scripts/build-mac.sh first (or pass --build).
#
# Signing is optional and off unless these are set in the environment:
#   SPKR_SIGN_ID        Developer ID Application: Name (TEAMID)   - signs the plug-ins and app
#   SPKR_INSTALLER_ID   Developer ID Installer: Name (TEAMID)     - signs the .pkg
#   SPKR_NOTARY_PROFILE name of a stored notarytool keychain profile - notarises and staples
# Without them the package still installs, but macOS warns that it is from an unidentified
# developer (right-click > Open, or System Settings > Privacy & Security > Open Anyway).
set -euo pipefail
cd "$(dirname "$0")/../.."

[ "${1:-}" = "--build" ] && scripts/build-mac.sh

VERSION=$(sed -n 's/^#define AppVersion  *"\([^"]*\)".*/\1/p' installer/002.iss)
[ -n "$VERSION" ] || { echo "could not read the version from installer/002.iss"; exit 1; }
ART=build-mac/Geminus_artefacts/Release
IDENT=com.spkr.002
STAGE=build-mac/pkg
DIST=dist
NAME="002 by SPKR"

VST3="$ART/VST3/$NAME.vst3"
AU="$ART/AU/$NAME.component"
APP="$ART/Standalone/$NAME.app"
CLAP="$ART/CLAP/$NAME.clap"
[ -d "$VST3" ] || { echo "$VST3 not found - build first: scripts/build-mac.sh"; exit 1; }

rm -rf "$STAGE"; mkdir -p "$STAGE/pkgs" "$DIST"

# Apple wants each destination in its own component package; productbuild joins them.
component () {   # component <built bundle> <install folder> <id suffix>
    local src="$1" dest="$2" id="$3"
    [ -d "$src" ] || { echo "  (skipping $id: not built)"; return; }
    local root="$STAGE/root-$id"
    mkdir -p "$root"
    cp -R "$src" "$root/"
    if [ -n "${SPKR_SIGN_ID:-}" ]; then
        codesign --force --deep --options runtime --timestamp --sign "$SPKR_SIGN_ID" "$root/$(basename "$src")"
    fi
    pkgbuild --root "$root" --install-location "$dest" --identifier "$IDENT.$id" --version "$VERSION" \
             "$STAGE/pkgs/$id.pkg" >/dev/null
    echo "  $id"
}

echo "== components =="
component "$VST3" "/Library/Audio/Plug-Ins/VST3" vst3
component "$AU"   "/Library/Audio/Plug-Ins/Components" au
component "$CLAP" "/Library/Audio/Plug-Ins/CLAP" clap
component "$APP"  "/Applications" app

cat > "$STAGE/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>002 by SPKR</title>
    <organization>com.spkr</organization>
    <options customize="always" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <volume-check><allowed-os-versions><os-version min="10.15"/></allowed-os-versions></volume-check>
    <license file="LICENSE.txt"/>
    <readme file="README.txt"/>
    <choices-outline>
$(for p in "$STAGE"/pkgs/*.pkg; do id=$(basename "$p" .pkg); echo "        <line choice=\"$IDENT.$id\"/>"; done)
    </choices-outline>
$(for p in "$STAGE"/pkgs/*.pkg; do id=$(basename "$p" .pkg)
  case $id in vst3) t="VST3 plug-in";; au) t="Audio Unit";; clap) t="CLAP plug-in";; app) t="Standalone app";; *) t=$id;; esac
  echo "    <choice id=\"$IDENT.$id\" title=\"$t\" visible=\"true\"><pkg-ref id=\"$IDENT.$id\"/></choice>"
  echo "    <pkg-ref id=\"$IDENT.$id\" version=\"$VERSION\">$id.pkg</pkg-ref>"
done)
</installer-gui-script>
XML
cp installer/LICENSE.txt installer/README.txt "$STAGE/"

PKG="$DIST/002-by-SPKR-$VERSION-macOS-Setup.pkg"
echo "== installer =="
if [ -n "${SPKR_INSTALLER_ID:-}" ]; then
    productbuild --distribution "$STAGE/distribution.xml" --package-path "$STAGE/pkgs" \
                 --resources "$STAGE" --sign "$SPKR_INSTALLER_ID" "$PKG"
else
    productbuild --distribution "$STAGE/distribution.xml" --package-path "$STAGE/pkgs" \
                 --resources "$STAGE" "$PKG"
fi

if [ -n "${SPKR_NOTARY_PROFILE:-}" ]; then
    echo "== notarising =="
    xcrun notarytool submit "$PKG" --keychain-profile "$SPKR_NOTARY_PROFILE" --wait
    xcrun stapler staple "$PKG"
fi

# the same payload as a zip, for people who would rather drag the bundles into place themselves
ZIPDIR="$STAGE/zip/002 by SPKR"
mkdir -p "$ZIPDIR"
for b in "$VST3" "$AU" "$CLAP" "$APP"; do [ -d "$b" ] && cp -R "$b" "$ZIPDIR/"; done
cp installer/LICENSE.txt installer/README.txt "$ZIPDIR/"
(cd "$STAGE/zip" && zip -qry "../../../$DIST/002-by-SPKR-$VERSION-macOS.zip" "002 by SPKR")

echo
ls -lh "$DIST" | grep macOS
