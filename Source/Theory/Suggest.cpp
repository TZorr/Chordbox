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

        /** What fills the second row. Most styles pick from a list of types
            that fit the scale; two reach outside it on purpose. */
        enum class Bottom
        {
            Fit,                  // the `bottom` list, fitted to the scale like `top`
            Borrowed,             // the `bottom` list, fitted to the parallel key instead
            SecondaryDominants    // a dominant seventh a fifth above each chord of the top row
        };

        struct StyleRows
        {
            const char* name;
            Row top, bottom;
            Bottom kind = Bottom::Fit;
        };

        const Row triads { "", "m", "dim", "aug", "sus2", "sus4", "5" };

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

                // The key's triads over the parallel key's, column by column:
                // in C major, IV over iv (F over Fm), vi over bVI (Am over Ab).
                { "Borrowed", triads, triads, Bottom::Borrowed },

                // The key's triads, each with the dominant seventh that leads
                // to it underneath: in C major A7 under Dm, E7 under Am.
                { "Secondary Dominants", triads, {}, Bottom::SecondaryDominants },
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

    int parallelScale (int scaleIndex)
    {
        const auto parent = scale (scaleIndex).spellAs;
        return scale (parent).steps[2] == 3 ? 0 : 1;   // a minor key borrows from major, anything else from minor
    }

    std::array<PadChord, numPads> suggest (int keyRoot, int scaleIndex, int style)
    {
        const auto& rows = styles()[(size_t) (style >= 0 && style < numStyles() ? style : 0)];

        keyRoot = ((keyRoot % 12) + 12) % 12;

        // The pitch of a column's degree, climbing: pads 1-7 the degrees, pad
        // 8 the root an octave up (a pentatonic wraps after five).
        const auto columnPitch = [keyRoot] (int scaleOfColumn, int column)
        {
            const auto& steps = scale (scaleOfColumn).steps;
            const auto degrees = (int) steps.size();
            return keyRoot + steps[(size_t) (column % degrees)] + 12 * (column / degrees);
        };

        // A pitch above the Chord Octave's C as a pad's root and octave.
        const auto place = [] (PadChord& chord, int pitch)
        {
            chord.root = ((pitch % 12) + 12) % 12;
            chord.octave = (pitch - chord.root) / 12;
            chord.inversion = 0;
        };

        std::array<PadChord, numPads> pads;

        for (int column = 0; column < 8; ++column)
        {
            auto& top = pads[(size_t) column];
            place (top, columnPitch (scaleIndex, column));
            top.type = pick (rows.top, top.root, keyRoot, scaleIndex);

            auto& bottom = pads[(size_t) column + 8];

            switch (rows.kind)
            {
                case Bottom::Fit:
                    place (bottom, columnPitch (scaleIndex, column));
                    bottom.type = pick (rows.bottom, bottom.root, keyRoot, scaleIndex);
                    break;

                case Bottom::Borrowed:
                {
                    // Always seven columns of the parallel key, also under a
                    // pentatonic: it is the key's parent that has a parallel.
                    const auto parallel = parallelScale (scaleIndex);
                    place (bottom, columnPitch (parallel, column));
                    bottom.type = pick (rows.bottom, bottom.root, keyRoot, parallel);
                    break;
                }

                case Bottom::SecondaryDominants:
                    // A fourth below the chord it leads to - a fifth above, an
                    // octave down - so the row climbs with the one above it.
                    place (bottom, columnPitch (scaleIndex, column) - 5);
                    bottom.type = findChordType ("7");
                    break;
            }
        }

        return pads;
    }
}
