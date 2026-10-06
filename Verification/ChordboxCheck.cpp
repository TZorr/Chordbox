//
//  ChordboxCheck.cpp
//  Chordbox
//
//  Tests the theory engine against answers known on paper: what a chord
//  contains, which chords a scale suggests, how a key spells its notes, what
//  an inversion does. None of it needs a plugin, so none of it builds one.
//
//  Usage:  ChordboxCheck
//

#include <juce_core/juce_core.h>

#include "Theory/ChordTypes.h"
#include "Theory/PadChord.h"
#include "Theory/Scales.h"
#include "Theory/Suggest.h"

#include <cstdio>

using namespace Theory;

namespace
{
    int failures = 0, checks = 0;

    void check (bool condition, const std::string& what, const std::string& detail = {})
    {
        ++checks;

        if (! condition)
        {
            ++failures;
            std::printf ("FAIL: %s  %s\n", what.c_str(), detail.c_str());
        }
    }

    int scaleIndex (const std::string& name)
    {
        for (int i = 0; i < numScales(); ++i)
            if (scale (i).name == name)
                return i;

        std::printf ("FAIL: no scale called %s\n", name.c_str());
        ++failures;
        return 0;
    }

    int styleIndex (const std::string& name)
    {
        for (int i = 0; i < numStyles(); ++i)
            if (name == styleName (i))
                return i;

        std::printf ("FAIL: no style called %s\n", name.c_str());
        ++failures;
        return 0;
    }

    std::string join (const std::vector<std::string>& parts)
    {
        std::string text;
        for (const auto& part : parts)
            text += (text.empty() ? "" : " ") + part;
        return text;
    }

    std::string row (const std::array<PadChord, numPads>& pads, int first, const Spelling& spelling)
    {
        std::vector<std::string> names;
        for (int pad = first; pad < first + 8; ++pad)
            names.push_back (pads[(size_t) pad].name (spelling));
        return join (names);
    }

    std::string offsets (const PadChord& chord)
    {
        std::vector<std::string> parts;
        for (auto offset : chord.offsets())
            parts.push_back (std::to_string (offset));
        return join (parts);
    }

    //==============================================================================
    void checkDictionary()
    {
        // Every symbol once, every chord rooted on 0 and ascending.
        for (int i = 0; i < numChordTypes(); ++i)
        {
            const auto& chord = chordType (i);
            check (findChordType (chord.symbol) == i, "chord symbol is unique: '" + chord.symbol + "'");
            check (! chord.intervals.empty() && chord.intervals.front() == 0, "chord starts on its root: " + chord.symbol);
            check (std::is_sorted (chord.intervals.begin(), chord.intervals.end()), "intervals ascend: " + chord.symbol);
            check ((int) chord.intervals.size() <= maxPackedNotes, "chord fits the packed word: " + chord.symbol);
        }

        // The chord from the request: F maj7sus2 = F G C E.
        const auto maj7sus2 = findChordType ("maj7sus2");
        check (maj7sus2 >= 0, "maj7sus2 is in the dictionary");
        const auto f = 5, g = 7, c = 0, e = 4;
        check (chordMask (5, maj7sus2) == ((1 << f) | (1 << g) | (1 << c) | (1 << e)), "Fmaj7sus2 is F G C E");

        const Spelling cMajor (0, scaleIndex ("Major"));
        check (PadChord { 5, maj7sus2, 0, 0 }.name (cMajor) == "Fmaj7sus2", "Fmaj7sus2 is named so");
        check (fitsScale (5, maj7sus2, 0, scaleIndex ("Major")), "Fmaj7sus2 fits C major");
        check (! fitsScale (5, findChordType ("7"), 0, scaleIndex ("Major")), "F7 does not fit C major (Eb)");
    }

    void checkSuggestions()
    {
        const auto major = scaleIndex ("Major");
        const Spelling cMajor (0, major);

        {
            const auto pads = suggest (0, major, styleIndex ("Triads + 7ths"));
            check (row (pads, 0, cMajor) == "C Dm Em F G Am Bdim C", "C major triads", row (pads, 0, cMajor));
            check (row (pads, 8, cMajor) == "Cmaj7 Dm7 Em7 Fmaj7 G7 Am7 Bm7b5 Cmaj7", "C major sevenths", row (pads, 8, cMajor));

            // The pads climb: pad 8 is the C above pad 1.
            check (pads[7].offsets().front() == pads[0].offsets().front() + 12, "pad 8 is the root an octave up");
            check (pads[0].numeral (0, major) == "I" && pads[1].numeral (0, major) == "ii"
                       && pads[6].numeral (0, major) == "vii\xc2\xb0",
                   "numerals I ii vii°");
        }

        {
            const auto pads = suggest (0, major, styleIndex ("Sus"));
            check (pads[11].name (cMajor) == "Fmaj7sus2", "Sus style puts Fmaj7sus2 on the IV", pads[11].name (cMajor));
        }

        {
            // The four styles added in 0.5.0, in C major, row by row.
            const auto rows = [&] (const char* style)
            {
                const auto pads = suggest (0, major, styleIndex (style));
                return row (pads, 0, cMajor) + " / " + row (pads, 8, cMajor);
            };

            check (rows ("Sixths") == "C6 Dm6 Em F6 G6 Am Bdim C6 / C6/9 Dmadd9 Em7 F6/9 G6/9 Amadd9 Bm7b5 C6/9",
                   "Sixths in C major", rows ("Sixths"));
            check (rows ("Jazz") == "Cmaj9 Dm9 Em7 Fmaj9 G9 Am9 Bm7b5 Cmaj9 / C6/9 Dm11 E7sus4 Fmaj7#11 G13 Am11 Bm7b5 C6/9",
                   "Jazz in C major", rows ("Jazz"));
            check (rows ("Quartal") == "Csus4 Dquartal Equartal Fsus2 Gsus4 Aquartal Bquartal Csus4"
                                       " / Cmaj7sus2 D7sus4 E7sus4 Fmaj7sus2 G7sus4 A7sus4 Bquartal Cmaj7sus2",
                   "Quartal in C major", rows ("Quartal"));
            check (rows ("Power") == "C5 D5 E5 F5 G5 A5 Bdim C5 / Csus2 Dsus2 Esus4 Fsus2 Gsus2 Asus2 Bdim Csus2",
                   "Power in C major", rows ("Power"));
        }

        {
            const auto minor = scaleIndex ("Harmonic Minor");
            const Spelling aMinor (9, minor);
            const auto pads = suggest (9, minor, styleIndex ("Triads + 7ths"));
            check (row (pads, 0, aMinor) == "Am Bdim Caug Dm E F G#dim Am", "A harmonic minor triads", row (pads, 0, aMinor));
            check (row (pads, 8, aMinor) == "AmMaj7 Bm7b5 Cmaj7#5 Dm7 E7 Fmaj7 G#dim7 AmMaj7",
                   "A harmonic minor sevenths", row (pads, 8, aMinor));
        }

        {
            // A pentatonic has no B, so it can have no Bdim; and every
            // suggested chord, whatever the style, fits the scale.
            const auto pentatonic = scaleIndex ("Minor Pentatonic");
            for (int s = 0; s < numStyles(); ++s)
            {
                const auto pads = suggest (9, pentatonic, s);
                for (const auto& pad : pads)
                    check (fitsScale (pad.root, pad.type, 9, pentatonic),
                           std::string ("A minor pentatonic, style ") + styleName (s) + ": every pad fits",
                           pad.name (Spelling (9, pentatonic)));
            }
        }

        // Every scale, every key, every style: sixteen pads that fit, or at
        // worst a chord from the dictionary - never an out-of-range root.
        for (int sc = 0; sc < numScales(); ++sc)
            for (int key = 0; key < 12; ++key)
                for (int s = 0; s < numStyles(); ++s)
                    for (const auto& pad : suggest (key, sc, s))
                    {
                        check (pad.root >= 0 && pad.root < 12 && pad.type >= 0 && pad.type < numChordTypes(),
                               "suggested pad is valid");
                        check (pad.octave >= PadChord::minOctave && pad.octave <= PadChord::maxOctave,
                               "suggested octave is in range");
                    }
    }

    void checkSpelling()
    {
        const auto major = scaleIndex ("Major");

        check (Spelling (5, major).name (10) == "Bb", "F major spells its fourth Bb");
        check (Spelling (2, major).name (6) == "F#", "D major spells its third F#");
        check (Spelling (1, major).rootName() == "Db", "the major key on 1 is Db");
        check (Spelling (1, scaleIndex ("Natural Minor")).rootName() == "C#", "the minor key on 1 is C#");
        check (Spelling (6, major).name (5) == "E#", "F# major spells its seventh E#");
        check (Spelling (2, scaleIndex ("Harmonic Minor")).name (1) == "C#", "D harmonic minor: C#");
        check (Spelling (2, scaleIndex ("Harmonic Minor")).name (10) == "Bb", "D harmonic minor: Bb");
        check (Spelling (3, scaleIndex ("Major Pentatonic")).name (10) == "Bb", "Eb pentatonic spells as Eb major");
        check (Spelling (0, major).name (10) == "Bb" && Spelling (0, major).name (1) == "C#",
               "C major: Bb and C# outside the scale");
    }

    void checkVoicing()
    {
        const auto majorTriad = findChordType ("");

        check (offsets ({ 0, majorTriad, 0, 0 }) == "0 4 7", "C root position");
        check (offsets ({ 0, majorTriad, 1, 0 }) == "4 7 12", "C first inversion: E G C");
        check (offsets ({ 0, majorTriad, 2, 0 }) == "7 12 16", "C second inversion: G C E");
        check (offsets ({ 9, findChordType ("m"), 0, -1 }) == "-3 0 4", "Am an octave down");

        const Spelling cMajor (0, scaleIndex ("Major"));
        check (PadChord { 9, findChordType ("m"), 1, 0 }.name (cMajor) == "Am/C", "Am first inversion is Am/C");

        // An inversion beyond the chord's size is clamped, never out of range.
        check (offsets ({ 0, majorTriad, 7, 0 }) == "7 12 16", "an inversion too far is clamped");

        // The packed word the audio thread reads gives back what went in,
        // negative offsets and six-note chords included.
        for (int type = 0; type < numChordTypes(); ++type)
            for (int inversion = 0; inversion < 6; ++inversion)
                for (int octave = PadChord::minOctave; octave <= PadChord::maxOctave; ++octave)
                {
                    const PadChord chord { 11, type, inversion, octave };
                    const auto notes = chord.offsets();
                    int unpacked[maxPackedNotes];
                    const auto count = unpackNotes (packNotes (notes), unpacked);

                    bool same = count == (int) notes.size();
                    for (int i = 0; same && i < count; ++i)
                        same = unpacked[i] == notes[(size_t) i];

                    check (same, "pack/unpack round trip: " + chordType (type).symbol);
                }
    }
}

int main()
{
    std::printf ("Dictionary...\n");
    checkDictionary();
    std::printf ("Suggestions...\n");
    checkSuggestions();
    std::printf ("Spelling...\n");
    checkSpelling();
    std::printf ("Voicing...\n");
    checkVoicing();

    std::printf ("\n%d checks, %d failure(s)\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
