//
//  ParameterIds.h
//  Chordbox
//
//  Parameter IDs and the names of the non-parameter state.
//
//  The parameters are the settings a host may automate: key, scale, where the
//  sixteen trigger notes start, which octave the chords sound in, velocity and
//  thru. What each pad holds is deliberately not a parameter. Sixteen pads of
//  four values would be 64 automation lanes nobody wants, and a chord is one
//  choice, not four knobs - so the pads live in the state as a child tree.
//

#pragma once

#include <juce_core/juce_core.h>

namespace ChordParams
{
    inline constexpr int numPads = 16;

    /** The buffer above the pads: sixteen tiles that Suggest fills and that
        chords are dragged from onto the pads. They are played by clicking
        only, never by a key, and come after the pads wherever both are
        counted ("slots"). */
    inline constexpr int numTiles = 16;
    inline constexpr int numSlots = numPads + numTiles;

    inline bool isTile (int slot) { return slot >= numPads; }

    inline constexpr const char* key         = "key";
    inline constexpr const char* scale       = "scale";
    inline constexpr const char* triggerNote = "triggerNote";
    inline constexpr const char* chordOctave = "chordOctave";
    inline constexpr const char* velocity    = "velocity";
    inline constexpr const char* thru        = "thru";
    inline constexpr const char* humanize    = "humanize";

    inline constexpr int defaultTrigger = 36;   // C1: pads 1-16 on C1-D#2, as in Kitbox
    inline constexpr int maxTrigger = 127 - (numPads - 1);
    inline constexpr int defaultOctave = 3;     // chords start at C3 (MIDI 60)
    inline constexpr int fixedVelocity = 100;

    /** RND: each chord note's velocity moves by up to this fraction either way,
        and the note is delayed by 0 to humanizeMaxDelayMs - a spread of
        +-8 ms around 8 ms, since a MIDI effect cannot play a note before it
        has received it. Both follow a triangular distribution: bounded, and
        most notes land near the middle. */
    inline constexpr float humanizeVelocity = 0.12f;
    inline constexpr double humanizeMaxDelayMs = 16.0;

    /** Logic's numbering: MIDI 60 is C3, MIDI 36 is C1. */
    inline juce::String noteName (int note)
    {
        static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (names[note % 12]) + juce::String (note / 12 - 2);
    }

    /** The MIDI note of the C a Chord Octave value names: 3 -> 60. */
    inline int octaveBase (int octave)
    {
        return 12 * (octave + 2);
    }

    //==============================================================================
    //  Non-parameter state, kept on the parameter tree
    inline const juce::Identifier stateTreeType   { "CHORDBOX" };
    inline const juce::Identifier padsType        { "PADS" };
    inline const juce::Identifier padType         { "PAD" };
    inline const juce::Identifier tilesType       { "TILES" };
    inline const juce::Identifier tileType        { "TILE" };
    inline const juce::Identifier propEmpty       { "empty" };
    inline const juce::Identifier propRoot        { "root" };
    inline const juce::Identifier propChord       { "chord" };   // the symbol, not an index: survives a reordered table
    inline const juce::Identifier propInversion   { "inversion" };
    inline const juce::Identifier propOctave      { "octave" };
    inline const juce::Identifier propStyle       { "style" };
    inline const juce::Identifier propSelectedPad { "selectedPad" };
}
