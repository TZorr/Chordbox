//
//  Scales.cpp
//  Chordbox
//

#include "Scales.h"
#include "ChordTypes.h"

#include <cstdlib>

namespace Theory
{
    namespace
    {
        constexpr int major = 0, naturalMinor = 1;

        constexpr int letterPitch[7] = { 0, 2, 4, 5, 7, 9, 11 };   // C D E F G A B
        constexpr char letterName[7] = { 'C', 'D', 'E', 'F', 'G', 'A', 'B' };

        int wrap (int pitch)
        {
            return ((pitch % 12) + 12) % 12;
        }

        std::string withAccidental (int letter, int accidental)
        {
            std::string text (1, letterName[letter]);
            text.append ((size_t) std::abs (accidental), accidental > 0 ? '#' : 'b');
            return text;
        }

        /** The key's root as a letter and an accidental. Flat keys are named
            flat, sharp keys sharp; on a black key that depends on whether the
            scale has a minor third (C# minor, but Db major). */
        void rootLetter (int root, bool minorThird, int& letter, int& accidental)
        {
            static const char* const majorNames[12] = { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
            static const char* const minorNames[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B" };

            const std::string name = (minorThird ? minorNames : majorNames)[wrap (root)];

            letter = (int) std::string ("CDEFGAB").find (name[0]);
            accidental = name.size() < 2 ? 0 : (name[1] == '#' ? 1 : -1);
        }
    }

    const std::vector<Scale>& scales()
    {
        // The pentatonics are spelt as their parents: major pentatonic as the
        // major scale on the same root, minor pentatonic as the natural minor.
        static const std::vector<Scale> table {
            { "Major",            { 0, 2, 4, 5, 7, 9, 11 }, 0 },
            { "Natural Minor",    { 0, 2, 3, 5, 7, 8, 10 }, 1 },
            { "Dorian",           { 0, 2, 3, 5, 7, 9, 10 }, 2 },
            { "Phrygian",         { 0, 1, 3, 5, 7, 8, 10 }, 3 },
            { "Lydian",           { 0, 2, 4, 6, 7, 9, 11 }, 4 },
            { "Mixolydian",       { 0, 2, 4, 5, 7, 9, 10 }, 5 },
            { "Locrian",          { 0, 1, 3, 5, 6, 8, 10 }, 6 },
            { "Harmonic Minor",   { 0, 2, 3, 5, 7, 8, 11 }, 7 },
            { "Melodic Minor",    { 0, 2, 3, 5, 7, 9, 11 }, 8 },
            { "Major Pentatonic", { 0, 2, 4, 7, 9 },        major },
            { "Minor Pentatonic", { 0, 3, 5, 7, 10 },       naturalMinor },
        };

        return table;
    }

    int numScales()
    {
        return (int) scales().size();
    }

    const Scale& scale (int index)
    {
        const auto& table = scales();
        return table[(size_t) (index >= 0 && index < (int) table.size() ? index : 0)];
    }

    int scaleMask (int root, int scaleIndex)
    {
        int mask = 0;

        for (auto step : scale (scaleIndex).steps)
            mask |= 1 << wrap (root + step);

        return mask;
    }

    bool fitsScale (int chordRoot, int chordTypeIndex, int scaleRoot, int scaleIndex)
    {
        return (chordMask (chordRoot, chordTypeIndex) & ~scaleMask (scaleRoot, scaleIndex)) == 0;
    }

    int degreeOf (int pitchClass, int scaleRoot, int scaleIndex)
    {
        const auto& steps = scale (scaleIndex).steps;

        for (size_t i = 0; i < steps.size(); ++i)
            if (wrap (scaleRoot + steps[i]) == wrap (pitchClass))
                return (int) i;

        return -1;
    }

    //==============================================================================
    Spelling::Spelling (int keyRoot, int scaleIndex)
        : root (wrap (keyRoot))
    {
        const auto parentIndex = scale (scaleIndex).spellAs;
        const auto minorThird = scale (parentIndex).steps[2] == 3;

        int letter = 0, accidental = 0;
        rootLetter (root, minorThird, letter, accidental);

        bool flats = accidental < 0, sharps = accidental > 0;
        bool spelt[12] = {};

        // One letter per degree, counted from the key's root letter. Notes
        // already named keep their name; an accidental beyond maxAccidental is
        // left to the passes after.
        const auto spellFrom = [&] (int from, int maxAccidental, bool decidesDirection)
        {
            const auto& steps = scale (from).steps;

            for (size_t degree = 0; degree < steps.size(); ++degree)
            {
                const auto pitch = wrap (root + steps[degree]);
                const auto degreeLetter = (letter + (int) degree) % 7;
                auto offset = wrap (pitch - letterPitch[degreeLetter]);
                if (offset > 6)
                    offset -= 12;

                if (spelt[pitch] || std::abs (offset) > maxAccidental)
                    continue;

                names[pitch] = withAccidental (degreeLetter, offset);
                spelt[pitch] = true;

                if (decidesDirection)
                {
                    flats  = flats  || offset < 0;
                    sharps = sharps || offset > 0;
                }
            }
        };

        // The key's own notes first. Then the notes of its parallel key - C
        // minor for C major, A major for A minor - which is where the chords a
        // key borrows come from: Ab and Bb in C major, G# and C# in A minor.
        spellFrom (parentIndex, 2, true);
        spellFrom (minorThird ? major : naturalMinor, 1, false);

        static const char* const flatNames[12]  = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
        static const char* const sharpNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        static const char* const mixedNames[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

        const auto* outside = flats && ! sharps ? flatNames : (sharps && ! flats ? sharpNames : mixedNames);

        for (int pitch = 0; pitch < 12; ++pitch)
            if (! spelt[pitch])
                names[pitch] = outside[pitch];
    }

    std::string Spelling::name (int pitchClass) const
    {
        return names[wrap (pitchClass)];
    }

    std::string Spelling::above (int rootPitch, int interval, int letterSteps) const
    {
        const auto rootName = name (rootPitch);
        const auto rootLetterIndex = (int) std::string ("CDEFGAB").find (rootName[0]);
        const auto noteLetter = (rootLetterIndex + letterSteps) % 7;
        const auto pitch = wrap (rootPitch + interval);

        auto offset = wrap (pitch - letterPitch[noteLetter]);
        if (offset > 6)
            offset -= 12;

        return std::abs (offset) > 2 ? name (pitch) : withAccidental (noteLetter, offset);
    }

    int accidentalOf (const std::string& noteName)
    {
        int count = 0;
        for (size_t i = 1; i < noteName.size(); ++i)
            count += noteName[i] == '#' ? 1 : (noteName[i] == 'b' ? -1 : 0);
        return count;
    }

    std::string keyName (int pitchClass)
    {
        static const char* const names[12] = { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
        return names[wrap (pitchClass)];
    }
}
