//
//  ChordTypes.h
//  Chordbox
//
//  The chord dictionary: every chord a pad can hold, as intervals above its
//  root in semitones.
//
//  The names and interval sets follow tonal.js's chord-type dictionary
//  (@tonaljs/chord-type, MIT - see THIRD_PARTY_NOTICES.md), cut down to the
//  chords a pad player reaches for. Intervals above the octave stay above it:
//  a 9 is 14, not 2, so add9 and sus2 are different chords, as they sound.
//
//  The order of the table is the order of the menus, and within a family it
//  is also the order Suggest tries types in when it falls back.
//
//  Plain C++ on purpose. ChordboxCheck tests this file without a plugin around
//  it.
//

#pragma once

#include <string>
#include <vector>

namespace Theory
{
    enum class Family { Triad, Sus, Sixth, Seventh, Colour, Other };

    struct ChordType
    {
        std::string symbol;          // what follows the root: "", "m", "maj7sus2"
        std::string name;            // for menus: "major", "major seventh sus2"
        Family family;
        std::vector<int> intervals;  // semitones above the root, ascending, starting at 0
    };

    const std::vector<ChordType>& chordTypes();

    int numChordTypes();
    const ChordType& chordType (int index);

    /** -1 if there is no such symbol. */
    int findChordType (const std::string& symbol);

    const char* familyName (Family);
    const std::vector<Family>& families();

    /** The chord's pitch classes on a root, as a 12-bit mask (bit 0 = C). */
    int chordMask (int root, int typeIndex);
}
