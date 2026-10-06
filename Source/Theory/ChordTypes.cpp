//
//  ChordTypes.cpp
//  Chordbox
//

#include "ChordTypes.h"

namespace Theory
{
    const std::vector<ChordType>& chordTypes()
    {
        using F = Family;

        static const std::vector<ChordType> table {
            { "",         "major",                   F::Triad,   { 0, 4, 7 } },
            { "m",        "minor",                   F::Triad,   { 0, 3, 7 } },
            { "dim",      "diminished",              F::Triad,   { 0, 3, 6 } },
            { "aug",      "augmented",               F::Triad,   { 0, 4, 8 } },

            { "sus2",     "suspended second",        F::Sus,     { 0, 2, 7 } },
            { "sus4",     "suspended fourth",        F::Sus,     { 0, 5, 7 } },
            { "7sus4",    "seventh sus4",            F::Sus,     { 0, 5, 7, 10 } },
            { "maj7sus2", "major seventh sus2",      F::Sus,     { 0, 2, 7, 11 } },
            { "maj7sus4", "major seventh sus4",      F::Sus,     { 0, 5, 7, 11 } },
            { "9sus4",    "ninth sus4",              F::Sus,     { 0, 5, 7, 10, 14 } },

            { "6",        "sixth",                   F::Sixth,   { 0, 4, 7, 9 } },
            { "m6",       "minor sixth",             F::Sixth,   { 0, 3, 7, 9 } },
            { "6/9",      "six nine",                F::Sixth,   { 0, 4, 7, 9, 14 } },

            { "maj7",     "major seventh",           F::Seventh, { 0, 4, 7, 11 } },
            { "7",        "dominant seventh",        F::Seventh, { 0, 4, 7, 10 } },
            { "m7",       "minor seventh",           F::Seventh, { 0, 3, 7, 10 } },
            { "m7b5",     "half-diminished",         F::Seventh, { 0, 3, 6, 10 } },
            { "mMaj7",    "minor major seventh",     F::Seventh, { 0, 3, 7, 11 } },
            { "maj7#5",   "augmented major seventh", F::Seventh, { 0, 4, 8, 11 } },
            { "dim7",     "diminished seventh",      F::Seventh, { 0, 3, 6, 9 } },

            { "add9",     "added ninth",             F::Colour,  { 0, 4, 7, 14 } },
            { "madd9",    "minor added ninth",       F::Colour,  { 0, 3, 7, 14 } },
            { "maj9",     "major ninth",             F::Colour,  { 0, 4, 7, 11, 14 } },
            { "9",        "dominant ninth",          F::Colour,  { 0, 4, 7, 10, 14 } },
            { "m9",       "minor ninth",             F::Colour,  { 0, 3, 7, 10, 14 } },
            { "maj7#11",  "lydian",                  F::Colour,  { 0, 4, 7, 11, 18 } },
            { "11",       "eleventh",                F::Colour,  { 0, 7, 10, 14, 17 } },
            { "m11",      "minor eleventh",          F::Colour,  { 0, 3, 7, 10, 14, 17 } },
            { "13",       "dominant thirteenth",     F::Colour,  { 0, 4, 7, 10, 14, 21 } },

            { "5",        "power chord",             F::Other,   { 0, 7 } },
            { "quartal",  "quartal",                 F::Other,   { 0, 5, 10, 15 } },
        };

        return table;
    }

    int numChordTypes()
    {
        return (int) chordTypes().size();
    }

    const ChordType& chordType (int index)
    {
        const auto& table = chordTypes();
        return table[(size_t) (index >= 0 && index < (int) table.size() ? index : 0)];
    }

    int findChordType (const std::string& symbol)
    {
        const auto& table = chordTypes();

        for (size_t i = 0; i < table.size(); ++i)
            if (table[i].symbol == symbol)
                return (int) i;

        return -1;
    }

    const char* familyName (Family family)
    {
        switch (family)
        {
            case Family::Triad:   return "Triads";
            case Family::Sus:     return "Sus";
            case Family::Sixth:   return "Sixths";
            case Family::Seventh: return "Sevenths";
            case Family::Colour:  return "Colour";
            case Family::Other:   return "Other";
        }

        return "";
    }

    const std::vector<Family>& families()
    {
        static const std::vector<Family> all { Family::Triad, Family::Sus, Family::Sixth,
                                               Family::Seventh, Family::Colour, Family::Other };
        return all;
    }

    int chordMask (int root, int typeIndex)
    {
        int mask = 0;

        for (auto interval : chordType (typeIndex).intervals)
            mask |= 1 << ((root + interval) % 12);

        return mask;
    }
}
