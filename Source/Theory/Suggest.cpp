//
//  Suggest.cpp
//  Chordbox
//

#include "Suggest.h"
#include "ChordTypes.h"

namespace Theory
{
    namespace
    {
        using Row = std::vector<const char*>;

        struct StyleRows
        {
            const char* name;
            Row top, bottom;
        };

        const std::vector<StyleRows>& styles()
        {
            static const std::vector<StyleRows> table {
                { "Triads + 7ths",
                  { "", "m", "dim", "aug", "sus2", "sus4", "5" },
                  { "maj7", "7", "m7", "m7b5", "mMaj7", "maj7#5", "dim7", "7sus4", "maj7sus2", "6", "m6" } },

                { "Sus",
                  { "sus2", "sus4", "5" },
                  { "7sus4", "maj7sus2", "maj7sus4", "9sus4", "sus4", "sus2", "5" } },

                { "Colour",
                  { "add9", "madd9", "sus2", "sus4", "6", "m6", "5" },
                  { "maj9", "9", "m9", "maj7#11", "m11", "6/9", "maj7sus2", "7sus4", "m7", "m7b5" } },

                // Soft and settled: sixths where they fit, triads where not;
                // then six-nines and added ninths.
                { "Sixths",
                  { "6", "m6", "", "m", "dim", "sus2", "sus4", "5" },
                  { "6/9", "madd9", "add9", "m7", "7", "m7b5" } },

                // Ninths, then elevenths, thirteenths and the lydian maj7#11.
                { "Jazz",
                  { "maj9", "m9", "9", "m7b5", "maj7", "m7", "7" },
                  { "13", "m11", "maj7#11", "6/9", "7sus4", "m7b5" } },

                // Stacked fourths where the scale has them, open sus chords
                // where it does not: modal, unresolved.
                { "Quartal",
                  { "quartal", "sus4", "sus2", "5" },
                  { "7sus4", "maj7sus2", "sus2", "sus4", "quartal", "5" } },

                // Root and fifth, then the open sus shapes around them.
                { "Power",
                  { "5", "sus2", "sus4" },
                  { "sus2", "sus4", "add9", "madd9", "5" } },
            };

            return table;
        }

        int pick (const Row& row, int root, int keyRoot, int scaleIndex)
        {
            for (const auto* symbol : row)
            {
                const auto type = findChordType (symbol);

                if (type >= 0 && fitsScale (root, type, keyRoot, scaleIndex))
                    return type;
            }

            for (int type = 0; type < numChordTypes(); ++type)
                if (fitsScale (root, type, keyRoot, scaleIndex))
                    return type;

            return 0;
        }
    }

    int numStyles()
    {
        return (int) styles().size();
    }

    const char* styleName (int style)
    {
        return styles()[(size_t) (style >= 0 && style < numStyles() ? style : 0)].name;
    }

    std::array<PadChord, numPads> suggest (int keyRoot, int scaleIndex, int style)
    {
        const auto& rows = styles()[(size_t) (style >= 0 && style < numStyles() ? style : 0)];
        const auto& steps = scale (scaleIndex).steps;
        const auto degrees = (int) steps.size();

        keyRoot = ((keyRoot % 12) + 12) % 12;

        std::array<PadChord, numPads> pads;

        for (int pad = 0; pad < numPads; ++pad)
        {
            const auto column = pad % 8;
            const auto pitch = keyRoot + steps[(size_t) (column % degrees)] + 12 * (column / degrees);

            auto& chord = pads[(size_t) pad];
            chord.root = pitch % 12;
            chord.octave = pitch / 12;
            chord.inversion = 0;
            chord.type = pick (pad < 8 ? rows.top : rows.bottom, chord.root, keyRoot, scaleIndex);
        }

        return pads;
    }
}
