# Third-party notices

Chordbox is © 2026 T'Zorr and is distributed under the GNU Affero General
Public License, version 3 (AGPLv3). See [LICENSE](LICENSE). The licence
follows from JUCE, the one third-party component Chordbox links against.

## JUCE

- **What:** the JUCE 9 framework (audio plugin client, GUI).
  - It is not included in this repository. `CMakeLists.txt` expects it at
    `~/JUCE` (or a path passed with `-DJUCE_PATH`) and links it as an
    external framework.
- **Copyright:** © Raw Material Software Limited, <https://juce.com>.
- **License:** JUCE 9 is dual-licensed, and this project uses the free tier,
  AGPLv3.
  - Chordbox links JUCE, so it is AGPLv3 in turn.
  - Full terms: <https://github.com/juce-framework/JUCE/blob/master/LICENSE.md>.
- **What AGPLv3 asks of anyone who distributes this plugin:** this includes a
  built AU, not only the source.
  - Make the complete corresponding source available under the same licence.
  - Preserve the copyright and licence notices.

  This repository already is that source.

## tonal (chord dictionary)

- **What:** the chord names and interval sets in
  `Source/Theory/ChordTypes.cpp` follow the chord-type dictionary of
  tonal.js (`@tonaljs/chord-type`).
  - The table was cut down to the chords listed in the README and rewritten
    as C++ data.
  - No code was copied.
- **Copyright:** © danigb and the tonal contributors,
  <https://github.com/tonaljs/tonal>.
- **License:** MIT, which is compatible with AGPLv3.

## Ideas, not code

[Ripchord](https://github.com/trackbout/ripchord) (GPL-3.0) is a JUCE MIDI
plugin that maps chords onto keys, and it was the starting point for thinking
about this one. No code was taken from it.

## Everything else

The rest is Chordbox's own code:

- the theory engine: scales, spelling, suggestions, voicing;
- the MIDI processing;
- the panel.

The panel, palette and pad design come from Kitbox, by the same author.
Nothing is downloaded at build or run time.
