//
//  Suggest.h
//  Chordbox
//
//  Fills the sixteen pads from a key, a scale and a style.
//
//  Each row of eight walks up the scale: pads 1-7 are its seven degrees and
//  pad 8 the root an octave up (a pentatonic wraps after five). A style is a
//  list of preferred chord types per row; each pad takes the first one that
//  fits the scale on its degree. So "Triads + 7ths" in C major comes out as
//  C Dm Em F G Am Bdim C over Cmaj7 Dm7 Em7 Fmaj7 G7 Am7 Bm7b5 Cmaj7 without
//  the table knowing anything about diatonic harmony - and the same lists
//  give sensible pads in harmonic minor or a pentatonic.
//
//  If none of a row's types fits a degree, the pad takes the first chord in
//  the dictionary that does.
//
//  Two styles reach outside the scale on purpose, in their second row:
//  Borrowed puts the parallel key's triads under the key's own (C major over
//  C minor: F over Fm, Am over Ab), and Secondary Dominants puts under each
//  triad the dominant seventh that leads to it (A7 under Dm, E7 under Am).
//
//  Styles: Triads + 7ths, Sus, Colour, Sixths, Jazz, Quartal, Power,
//  Borrowed, Secondary Dominants. New ones go at the end - a session stores
//  the style by its position.
//

#pragma once

#include "PadChord.h"

#include <array>

namespace Theory
{
    static constexpr int numPads = 16;

    enum class Style { TriadsSevenths, Sus, Colour };

    int numStyles();
    const char* styleName (int style);

    std::array<PadChord, numPads> suggest (int keyRoot, int scaleIndex, int style);

    /** The key a scale borrows from: major for a minor key, natural minor for
        anything else. */
    int parallelScale (int scaleIndex);
}
