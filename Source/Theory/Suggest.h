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
//  Styles: Triads + 7ths, Sus, Colour, Sixths, Jazz, Quartal, Power. New ones
//  go at the end - a session stores the style by its position.
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
}
