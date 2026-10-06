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

        /** How many letters above the root a chord tone sits: a third is 2, a
            fifth 4, a ninth 1. The sixth semitone is a flat fifth except in
            maj7#11, where it is a sharp eleventh; the eighth a sharp fifth in
            the augmented chords, a minor sixth nowhere else here. */
        int letterSteps (int interval, const std::string& symbol)
        {
            static constexpr int steps[12] = { 0, 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6 };

            if (interval == 18)
                return 3;

            if (interval % 12 == 8 && (symbol == "aug" || symbol == "maj7#5"))
                return 4;

            return steps[interval % 12];
        }
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

    std::vector<PadChord::SpeltNote> PadChord::speltNotes (const Spelling& spelling) const
    {
        const auto& chord = chordType (type);
        const auto lifted = std::clamp (inversion, 0, (int) chord.intervals.size() - 1);

        std::vector<SpeltNote> notes;
        notes.reserve (chord.intervals.size());

        for (size_t i = 0; i < chord.intervals.size(); ++i)
        {
            const auto interval = chord.intervals[i];
            notes.push_back ({ root + interval + 12 * octave + ((int) i < lifted ? 12 : 0),
                               spelling.above (root, interval, letterSteps (interval, chord.symbol)) });
        }

        std::sort (notes.begin(), notes.end(), [] (const SpeltNote& a, const SpeltNote& b) { return a.offset < b.offset; });
        return notes;
    }

    std::string PadChord::name (const Spelling& spelling) const
    {
        const auto& chord = chordType (type);
        auto text = spelling.name (root) + chord.symbol;

        const auto lifted = std::clamp (inversion, 0, (int) chord.intervals.size() - 1);

        if (lifted > 0)
        {
            const auto interval = chord.intervals[(size_t) lifted];
            text += "/" + spelling.above (root, interval, letterSteps (interval, chord.symbol));
        }

        return text;
    }

    std::string PadChord::numeral (int keyRoot, int scaleIndex) const
    {
        // Counted in the seven-note scale the key is spelt from, so a
        // pentatonic's chords still read as degrees of their key.
        const auto degree = degreeOf (root, keyRoot, scale (scaleIndex).spellAs);

        static const char* const upper[7] = { "I", "II", "III", "IV", "V", "VI", "VII" };
        static const char* const lower[7] = { "i", "ii", "iii", "iv", "v", "vi", "vii" };

        // Outside the key: by semitones above its root, on a major scale.
        static const char* const chromaticUpper[12] = { "I", "bII", "II", "bIII", "III", "IV",
                                                        "#IV", "V", "bVI", "VI", "bVII", "VII" };
        static const char* const chromaticLower[12] = { "i", "bii", "ii", "biii", "iii", "iv",
                                                        "#iv", "v", "bvi", "vi", "bvii", "vii" };

        const auto& chord = chordType (type);
        const auto has = [&chord] (int interval)
        {
            return std::find (chord.intervals.begin(), chord.intervals.end(), interval) != chord.intervals.end();
        };

        const auto minor = has (3) && ! has (4);
        const auto semitones = (((root - keyRoot) % 12) + 12) % 12;
        std::string text = degree >= 0 ? (minor ? lower : upper)[degree]
                                        : (minor ? chromaticLower : chromaticUpper)[semitones];

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
