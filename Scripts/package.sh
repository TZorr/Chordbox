#!/bin/bash
#
#  package.sh - build a macOS installer containing the AU.
#
#  Two things about this script are decisions rather than defaults.
#
#  **It builds before it packages, and build.sh runs ChordboxCheck and every
#  EditorShot check.** Packaging a binary nobody verified is how a hanging note
#  or a misspelt chord gets shipped. So there is
#  no way to reach an installer that skips them.
#
#  Copied from Kitbox's (itself FX Analyzer's) on 2026-10-06, without the VST3.
#  Note that the installer writes to /Library, while Scripts/install.sh writes
#  to ~/Library - installing both on one machine lists the plugin twice.
#
#  **The version is read out of the built bundle, not written here.** It comes
#  from project() in CMakeLists.txt, through JUCE, into Info.plist. A version
#  string typed into a packaging script is a second source of truth, and the two
#  disagree the first time somebody bumps one of them.
#
#  The Standalone is deliberately not in the installer. It exists to develop
#  against - see the note in CMakeLists.txt - and the deliverable is the AU.
#
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
ARTEFACTS="$BUILD_DIR/Chordbox_artefacts/Release"
STAGE="$BUILD_DIR/package"

AU_SOURCE="$ARTEFACTS/AU/Chordbox.component"

# Nothing gets packaged that has not just been built and verified.
"$PROJECT_DIR/Scripts/build.sh"

VERSION="$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" \
             "$AU_SOURCE/Contents/Info.plist")"

# The deployment target and the architecture the installer will refuse to
# install against, taken from the same place the compiler took them.
MIN_OS="$(/usr/libexec/PlistBuddy -c "Print :LSMinimumSystemVersion" \
            "$AU_SOURCE/Contents/Info.plist" 2>/dev/null || echo "26.5")"

echo
echo "Packaging Chordbox $VERSION"

rm -rf "$STAGE"
mkdir -p "$STAGE/au" "$STAGE/parts"

cp -R "$AU_SOURCE"   "$STAGE/au/"

pkgbuild --root "$STAGE/au" \
         --install-location "/Library/Audio/Plug-Ins/Components" \
         --identifier "TZorr.Chordbox.au" \
         --version "$VERSION" \
         "$STAGE/parts/Chordbox-AU.pkg" >/dev/null

# The architecture and OS checks are the point of using productbuild at all
# rather than shipping the component package. A plugin built arm64-only for
# macOS 26.5 that installs happily onto an Intel Mac running something older is a
# support question later; refusing at install time is one now.
cat > "$STAGE/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>Chordbox $VERSION</title>
    <options customize="allow" require-scripts="false" hostArchitectures="arm64"/>
    <domains enable_localSystem="true" enable_anywhere="false" enable_currentUserHome="false"/>
    <volume-check>
        <allowed-os-versions><os-version min="$MIN_OS"/></allowed-os-versions>
    </volume-check>
    <choices-outline>
        <line choice="au"/>
    </choices-outline>
    <choice id="au" title="Audio Unit"
            description="For Logic Pro and other AU hosts. Installs into /Library/Audio/Plug-Ins/Components.">
        <pkg-ref id="TZorr.Chordbox.au"/>
    </choice>
    <pkg-ref id="TZorr.Chordbox.au" version="$VERSION">Chordbox-AU.pkg</pkg-ref>
</installer-gui-script>
XML

UNSIGNED="$STAGE/Chordbox $VERSION (unsigned).pkg"
FINAL="$BUILD_DIR/Chordbox $VERSION.pkg"

productbuild --distribution "$STAGE/distribution.xml" \
             --package-path "$STAGE/parts" \
             "$UNSIGNED" >/dev/null

# Signed only if there is something to sign with. An Apple Development
# certificate is not that something: it is for running your own builds on your
# own machine, it is not accepted by Gatekeeper anywhere else, and signing with
# it would produce an installer that looks signed and still gets refused - which
# is worse than an honestly unsigned one.
IDENTITY="$(security find-identity -v 2>/dev/null \
              | sed -n 's/.*"\(Developer ID Installer:[^"]*\)".*/\1/p' | head -1)"

rm -f "$FINAL"

if [[ -n "$IDENTITY" ]]; then
    productsign --sign "$IDENTITY" "$UNSIGNED" "$FINAL"
    echo "Signed with: $IDENTITY"
    echo
    echo "Not yet notarised. To distribute it:"
    echo "  xcrun notarytool submit \"$FINAL\" --keychain-profile <profile> --wait"
    echo "  xcrun stapler staple \"$FINAL\""
else
    cp "$UNSIGNED" "$FINAL"
    echo
    echo "UNSIGNED: no Developer ID Installer certificate on this machine."
    echo "It installs here, and on any other Mac it will be refused by Gatekeeper"
    echo "until the user right-clicks it and chooses Open. For real distribution"
    echo "you need a Developer ID Installer certificate and notarisation."
fi

echo
echo "Built: $FINAL"
ls -lh "$FINAL" | awk '{print "  " $5}'
