//
//  Scales.h
//  Chordbox
//
//  The scales the pads can be suggested from, whether a chord fits one, and
//  how to spell a note name in a key.
//
//  A chord "fits" a scale when every one of its pitch classes is in the scale.
//  That is the whole test, deliberately: it needs no notion of function or
//  degree, so it works the same for a major scale, a mode or a pentatonic.
//
//  Spelling matters to anyone reading a chord name: in F major the fourth is
//  Bb, never A#. For a seven-note scale every degree gets its own letter,
//  counted from the key's root, and the accidental follows from the pitch. A
//  pentatonic is spelt as the seven-note scale it is cut from. Notes outside
//  the scale are spelt as the parallel key spells them (Ab and Bb in C major,
//  from C minor; G# in A minor, from A major), since that is where borrowed
//  chords come from; what is left takes the key's direction - flats in a flat
//  key, sharps in a sharp one - and the usual mix (C# Eb F# Ab Bb) in a key
//  with neither.
//
//  A chord's own notes are spelt from its root instead (above()): the third of
//  B7 is D#, not Eb, whatever the key.
//

#pragma once

#include <string>
#include <vector>

namespace Theory
{
    struct Scale
    {
        std::string name;
        std::vector<int> steps;   // semitones above the root, ascending, starting at 0
        int spellAs;              // the seven-note scale whose letters it borrows (itself if heptatonic)
    };

    const std::vector<Scale>& scales();
    int numScales();
    const Scale& scale (int index);

    /** The scale's pitch classes on a root, as a 12-bit mask (bit 0 = C). */
    int scaleMask (int root, int scaleIndex);

    /** Every pitch class of the chord is in the scale. */
    bool fitsScale (int chordRoot, int chordTypeIndex, int scaleRoot, int scaleIndex);

    /** Which step of the scale a pitch class is, or -1 if it is not in it. */
    int degreeOf (int pitchClass, int scaleRoot, int scaleIndex);

    /** Note names as the key spells them. */
    class Spelling
    {
    public:
        Spelling (int keyRoot, int scaleIndex);

        std::string name (int pitchClass) const;

        /** The note `interval` semitones above `rootPitch`, `letterSteps`
            letters above the root's letter: a major third (4, 2) above B is
            D#. Falls back to name() past a double accidental. */
        std::string above (int rootPitch, int interval, int letterSteps) const;

        /** The key's own name, e.g. "Bb" or "C#". */
        std::string rootName() const { return name (root); }

    private:
        int root;
        std::string names[12];
    };

    /** The twelve key names the Key selector offers: C Db D Eb E F F# G Ab A Bb B. */
    std::string keyName (int pitchClass);

    /** +1 per sharp, -1 per flat: "Bb" is -1, "F##" is 2. */
    int accidentalOf (const std::string& noteName);
}
