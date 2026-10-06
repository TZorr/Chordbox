//
//  PadChord.h
//  Chordbox
//
//  What one pad holds: a root, a chord type, an inversion and an octave, and
//  the notes that come out of them.
//
//  Notes are offsets in semitones above the C of the Chord Octave parameter,
//  not MIDI notes, so turning that knob moves every pad at once without
//  touching the pads themselves. An inversion lifts the lowest notes by an
//  octave, one per step: C E G, then E G C, then G C E.
//
//  The audio thread never sees a PadChord. It gets the offsets packed into one
//  64-bit word per pad (packNotes), which an atomic can hold without a lock:
//  up to eight notes of seven bits each, and a count.
//

#pragma once

#include "Scales.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Theory
{
    struct PadChord
    {
        int root = 0;        // pitch class, 0 = C
        int type = 0;        // index into chordTypes()
        int inversion = 0;   // 0 = root position
        int octave = 0;      // octaves above the Chord Octave's C, -2..+2

        static constexpr int minOctave = -2, maxOctave = 2;

        /** Semitones above the Chord Octave's C, ascending. */
        std::vector<int> offsets() const;

        /** "Fmaj7sus2", "Am/C". */
        std::string name (const Spelling&) const;

        /** "IV", "ii", "vii°" - empty if the root is not in the key. */
        std::string numeral (int keyRoot, int scaleIndex) const;

        int numInversions() const;

        bool operator== (const PadChord&) const = default;
    };

    static constexpr int maxPackedNotes = 8;

    uint64_t packNotes (const std::vector<int>& offsets);

    /** Writes up to maxPackedNotes offsets; returns how many. */
    int unpackNotes (uint64_t packed, int* offsets);
}
