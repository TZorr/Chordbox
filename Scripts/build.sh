#!/bin/bash
#
#  build.sh - configure and build Chordbox in Release, then verify it.
#
#  "It builds" and "it plays the right notes" are the same command here: a
#  chord generator that names Bb as A# or leaves a note hanging compiles
#  perfectly well.
#
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

# cmake and ninja live in ~/.buildtools on this machine, not on the PATH.
export PATH="$HOME/.buildtools/bin:$HOME/.buildtools/lib/python3.9/site-packages/cmake/data/bin:$PATH"

cd "$PROJECT_DIR"

cmake -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR"

echo
echo "Checking the theory engine:"
echo
"$BUILD_DIR/ChordboxCheck_artefacts/Release/ChordboxCheck"

echo
echo "Playing the processor and rendering the panel:"
echo
"$BUILD_DIR/EditorShot_artefacts/Release/EditorShot" "$BUILD_DIR/shots"

echo
echo "Built:"
ls -d "$BUILD_DIR/Chordbox_artefacts/Release/"*/*.{component,app} 2>/dev/null || true
echo "Panel: $BUILD_DIR/shots/chordbox.png"
