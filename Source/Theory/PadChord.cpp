//
//  PadChord.cpp
//  Chordbox
//

#include "PadChord.h"
#include "ChordTypes.h"

#include <algorithm>

namespace Theory
{
    namespace
    {
        // Seven bits per note, stored with this bias so that offsets from -32
        // to 95 fit: two octaves down, and a thirteenth inverted two octaves up.
        constexpr int bias = 32;
        constexpr int countShift = 56;
    }

    int PadChord::numInversions() const
    {
        return (int) chordType (type).intervals.size();
    }

    std::vector<int> PadChord::offsets() const
    {
        const auto& intervals = chordType (type).intervals;
        const auto lifted = std::clamp (inversion, 0, (int) intervals.size() - 1);

        std::vector<int> notes;
        notes.reserve (intervals.size());

        for (size_t i = 0; i < intervals.size(); ++i)
            notes.push_back (root + intervals[i] + 12 * octave + ((int) i < lifted ? 12 : 0));

        std::sort (notes.begin(), notes.end());
        return notes;
    }

    std::string PadChord::name (const Spelling& spelling) const
    {
        const auto& chord = chordType (type);
        auto text = spelling.name (root) + chord.symbol;

        const auto lifted = std::clamp (inversion, 0, (int) chord.intervals.size() - 1);

        if (lifted > 0)
            text += "/" + spelling.name (root + chord.intervals[(size_t) lifted]);

        return text;
    }

    std::string PadChord::numeral (int keyRoot, int scaleIndex) const
    {
        // Counted in the seven-note scale the key is spelt from, so a
        // pentatonic's chords still read as degrees of their key.
        const auto degree = degreeOf (root, keyRoot, scale (scaleIndex).spellAs);

        if (degree < 0)
            return {};

        static const char* const upper[7] = { "I", "II", "III", "IV", "V", "VI", "VII" };
        static const char* const lower[7] = { "i", "ii", "iii", "iv", "v", "vi", "vii" };

        const auto& chord = chordType (type);
        const auto has = [&chord] (int interval)
        {
            return std::find (chord.intervals.begin(), chord.intervals.end(), interval) != chord.intervals.end();
        };

        const auto minor = has (3) && ! has (4);
        std::string text = (minor ? lower : upper)[degree];

        if (chord.symbol == "dim" || chord.symbol == "dim7")
            text += "\xc2\xb0";   // degree sign
        else if (chord.symbol == "m7b5")
            text += "\xc3\xb8";   // o with stroke
        else if (chord.symbol == "aug" || chord.symbol == "maj7#5")
            text += "+";

        return text;
    }

    uint64_t packNotes (const std::vector<int>& offsets)
    {
        uint64_t packed = 0;
        int count = 0;

        for (auto offset : offsets)
        {
            if (count == maxPackedNotes)
                break;

            const auto stored = (uint64_t) std::clamp (offset + bias, 0, 127);
            packed |= stored << (7 * count);
            ++count;
        }

        return packed | ((uint64_t) count << countShift);
    }

    int unpackNotes (uint64_t packed, int* offsets)
    {
        const auto count = std::min ((int) ((packed >> countShift) & 0xf), maxPackedNotes);

        for (int i = 0; i < count; ++i)
            offsets[i] = (int) ((packed >> (7 * i)) & 0x7f) - bias;

        return count;
    }
}
