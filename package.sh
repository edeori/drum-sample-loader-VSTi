#!/usr/bin/env bash
# Builds, signs, packages, notarizes and staples Swarm (VST3 + AU +
# Standalone) into a single distributable .pkg installer.
#
# One-time setup this depends on (see Licensing-VST/docs/macos-signing-and-
# notarization.md for the full walkthrough): a "Developer ID Application"
# and a "Developer ID Installer" certificate in the login keychain, and an
# `xcrun notarytool store-credentials` profile. Defaults below match the
# existing Moth Production setup; override via env vars if needed.
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

PACKAGE_NAME="Swarm"
IDENTIFIER_BASE="com.mothproduction.swarm"
VERSION="0.1.0"

CODESIGN_IDENTITY="${CODESIGN_IDENTITY:-Developer ID Application: Márton Ferenczi (Y8272MS92K)}"
INSTALLER_IDENTITY="${INSTALLER_IDENTITY:-Developer ID Installer: Márton Ferenczi (Y8272MS92K)}"
NOTARY_PROFILE="${NOTARY_PROFILE:-moth-notary}"

BUILD_DIR="build"
ARTEFACTS_DIR="$BUILD_DIR/DrumSampleLoaderVSTi_artefacts/Release"
PKG_DIR="$BUILD_DIR/pkg"
COMPONENTS_DIR="$PKG_DIR/components"
OUTPUT_PKG="$BUILD_DIR/${PACKAGE_NAME}.pkg"

echo "==> Configuring..."
cmake -S . -B "$BUILD_DIR" >/dev/null

echo "==> Building VST3 + AU + Standalone (Release)..."
cmake --build "$BUILD_DIR" \
  --target DrumSampleLoaderVSTi_VST3 DrumSampleLoaderVSTi_AU DrumSampleLoaderVSTi_Standalone \
  -j "$(sysctl -n hw.ncpu)"

APP_SOURCE="$ARTEFACTS_DIR/Standalone/${PACKAGE_NAME}.app"
VST3_SOURCE="$ARTEFACTS_DIR/VST3/${PACKAGE_NAME}.vst3"
AU_SOURCE="$ARTEFACTS_DIR/AU/${PACKAGE_NAME}.component"

for path in "$APP_SOURCE" "$VST3_SOURCE" "$AU_SOURCE"; do
  if [[ ! -e "$path" ]]; then
    echo "Missing build artefact: $path" >&2
    exit 1
  fi
done

rm -rf "$PKG_DIR"
mkdir -p "$COMPONENTS_DIR"

# --- Standalone component ----------------------------------------------------
# No entitlements file: Swarm declares no input buses (sample playback +
# MIDI trigger only), so the hardened runtime needs no special permissions.
STANDALONE_STAGING="$PKG_DIR/staging-standalone"
STANDALONE_SCRIPTS="$PKG_DIR/scripts-standalone"
mkdir -p "$STANDALONE_STAGING/Applications/Moth Production" "$STANDALONE_SCRIPTS"
cp -R "$APP_SOURCE" "$STANDALONE_STAGING/Applications/Moth Production/"

codesign --force --deep --options runtime --timestamp \
  --sign "$CODESIGN_IDENTITY" \
  "$STANDALONE_STAGING/Applications/Moth Production/${PACKAGE_NAME}.app"

cat > "$STANDALONE_SCRIPTS/postinstall" <<EOF
#!/usr/bin/env bash
set -euo pipefail

# Refresh Launch Services so the standalone app appears immediately.
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister \\
  -f "/Applications/Moth Production/${PACKAGE_NAME}.app" >/dev/null 2>&1 || true

exit 0
EOF
chmod +x "$STANDALONE_SCRIPTS/postinstall"

pkgbuild \
  --root "$STANDALONE_STAGING" \
  --identifier "$IDENTIFIER_BASE.standalone" \
  --version "$VERSION" \
  --install-location "/" \
  --scripts "$STANDALONE_SCRIPTS" \
  "$COMPONENTS_DIR/standalone.pkg"

# --- VST3 component -----------------------------------------------------------
VST3_STAGING="$PKG_DIR/staging-vst3"
mkdir -p "$VST3_STAGING/Library/Audio/Plug-Ins/VST3"
cp -R "$VST3_SOURCE" "$VST3_STAGING/Library/Audio/Plug-Ins/VST3/"

codesign --force --options runtime --timestamp \
  --sign "$CODESIGN_IDENTITY" \
  "$VST3_STAGING/Library/Audio/Plug-Ins/VST3/${PACKAGE_NAME}.vst3"

pkgbuild \
  --root "$VST3_STAGING" \
  --identifier "$IDENTIFIER_BASE.vst3" \
  --version "$VERSION" \
  --install-location "/" \
  "$COMPONENTS_DIR/vst3.pkg"

# --- AU component --------------------------------------------------------------
AU_STAGING="$PKG_DIR/staging-au"
mkdir -p "$AU_STAGING/Library/Audio/Plug-Ins/Components"
cp -R "$AU_SOURCE" "$AU_STAGING/Library/Audio/Plug-Ins/Components/"

codesign --force --options runtime --timestamp \
  --sign "$CODESIGN_IDENTITY" \
  "$AU_STAGING/Library/Audio/Plug-Ins/Components/${PACKAGE_NAME}.component"

pkgbuild \
  --root "$AU_STAGING" \
  --identifier "$IDENTIFIER_BASE.au" \
  --version "$VERSION" \
  --install-location "/" \
  "$COMPONENTS_DIR/au.pkg"

# --- Distribution (lets the user choose which components to install) ---------
DIST_XML="$PKG_DIR/distribution.xml"
cat > "$DIST_XML" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>${PACKAGE_NAME}</title>
    <organization>com.mothproduction</organization>
    <domains enable_localSystem="true"/>
    <options customize="always" require-scripts="false" rootVolumeOnly="true"/>
    <choices-outline>
        <line choice="standalone"/>
        <line choice="vst3"/>
        <line choice="au"/>
    </choices-outline>
    <choice id="standalone" title="Standalone application" description="Installs the standalone ${PACKAGE_NAME} app into /Applications/Moth Production." start_selected="true">
        <pkg-ref id="$IDENTIFIER_BASE.standalone"/>
    </choice>
    <choice id="vst3" title="VST3 plug-in" description="Installs the ${PACKAGE_NAME} VST3 plug-in into /Library/Audio/Plug-Ins/VST3." start_selected="true">
        <pkg-ref id="$IDENTIFIER_BASE.vst3"/>
    </choice>
    <choice id="au" title="Audio Unit (AU) plug-in" description="Installs the ${PACKAGE_NAME} AU plug-in into /Library/Audio/Plug-Ins/Components." start_selected="true">
        <pkg-ref id="$IDENTIFIER_BASE.au"/>
    </choice>
    <pkg-ref id="$IDENTIFIER_BASE.standalone" version="$VERSION" onConclusion="none">standalone.pkg</pkg-ref>
    <pkg-ref id="$IDENTIFIER_BASE.vst3" version="$VERSION" onConclusion="none">vst3.pkg</pkg-ref>
    <pkg-ref id="$IDENTIFIER_BASE.au" version="$VERSION" onConclusion="none">au.pkg</pkg-ref>
</installer-gui-script>
XML

productbuild \
  --distribution "$DIST_XML" \
  --package-path "$COMPONENTS_DIR" \
  --sign "$INSTALLER_IDENTITY" \
  "$OUTPUT_PKG"

echo
echo "==> Submitting for notarization (this can take a few minutes)..."
xcrun notarytool submit "$OUTPUT_PKG" --keychain-profile "$NOTARY_PROFILE" --wait

echo
echo "==> Stapling notarization ticket..."
xcrun stapler staple "$OUTPUT_PKG"

echo
echo "==> Installer package created:"
echo "    $OUTPUT_PKG"
echo
echo "    Signed, notarized and stapled. Lets the user pick any combination of:"
echo "    /Applications/Moth Production/${PACKAGE_NAME}.app"
echo "    /Library/Audio/Plug-Ins/VST3/${PACKAGE_NAME}.vst3"
echo "    /Library/Audio/Plug-Ins/Components/${PACKAGE_NAME}.component"
echo "    (all three are selected by default)"
