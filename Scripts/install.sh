#!/bin/bash
#
#  install.sh - put the built plugin where the hosts look, and make sure the
#  hosts actually pick up the new build.
#
#  The cache flush is the part that matters. macOS keeps a registry of audio
#  units and Logic keeps a scan cache on top of it; replacing the bundle on
#  disk does not by itself make either let go of the old one.
#
#  In Logic, Chordbox is a MIDI effect: it appears in a software instrument
#  track's MIDI FX slot (above the instrument), not in the instrument slot.
#
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARTEFACTS="$PROJECT_DIR/build/Chordbox_artefacts/Release"

AU_SOURCE="$ARTEFACTS/AU/Chordbox.component"
APP_SOURCE="$ARTEFACTS/Standalone/Chordbox.app"

AU_DEST="$HOME/Library/Audio/Plug-Ins/Components"
APP_DEST="/Applications"

if [[ ! -d "$AU_SOURCE" ]]; then
    echo "No build found at $ARTEFACTS - run Scripts/build.sh first." >&2
    exit 1
fi

# Without the AudioComponents entry macOS does not register the unit, does not
# complain, and the plugin simply never appears in the host.
if ! /usr/libexec/PlistBuddy -c "Print :AudioComponents:0:subtype" \
        "$AU_SOURCE/Contents/Info.plist" >/dev/null 2>&1; then
    echo "The AU's Info.plist has no AudioComponents entry - rebuild from scratch:" >&2
    echo "    rm -rf build/Chordbox_artefacts && Scripts/build.sh" >&2
    exit 1
fi

# Refuse to install a build older than its sources.
stale="$(find "$PROJECT_DIR/Source" "$PROJECT_DIR/CMakeLists.txt" -type f \
             -newer "$AU_SOURCE/Contents/MacOS/Chordbox" -print -quit 2>/dev/null || true)"
if [[ -n "$stale" ]]; then
    echo "The build is older than ${stale#"$PROJECT_DIR"/} - run Scripts/build.sh first." >&2
    exit 1
fi

if pgrep -xq "Logic Pro"; then
    echo "Logic Pro is running. Quit it first: it holds the old plugin open and" >&2
    echo "will not rescan while it is." >&2
    exit 1
fi

mkdir -p "$AU_DEST"
rm -rf "$AU_DEST/Chordbox.component"
cp -R "$AU_SOURCE" "$AU_DEST/"
echo "Installed the AU to $AU_DEST"

if [[ -d "$APP_SOURCE" ]]; then
    rm -rf "$APP_DEST/Chordbox.app"
    cp -R "$APP_SOURCE" "$APP_DEST/"
    echo "Installed the standalone app to $APP_DEST"
fi

killall -9 AudioComponentRegistrar 2>/dev/null || true
sleep 1

echo
echo "Validating the Audio Unit:"
auval -v aumi Chbx Tzor
