//
//  ChordDisplay.h
//  Chordbox
//
//  The dark screen in the header, after Kitbox's SampleDisplay: the chord of
//  the pad last played or selected, its notes by name, and a keyboard with
//  those notes marked - lit while the chord sounds, dim while it does not.
//
//  To the left of the keyboard, set apart, one octave shows the key: the
//  scale's notes marked and named as the key spells them, the root brightest.
//
//  Top left beside that octave sits RND, the humanize switch: a small toggle
//  bound to the parameter, lit in the accent colour while on.
//
//  It follows whichever happened last, a click on a pad or a chord played from
//  a key, so it reads as "what am I hearing" while playing and "what is on this
//  pad" while editing.
//

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <optional>

class ChordboxProcessor;

class ChordDisplay : public juce::Component
{
public:
    explicit ChordDisplay (ChordboxProcessor&);

    void setPad (int pad);

    /** 30 Hz, from the editor. */
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class RndButton : public juce::Button
    {
    public:
        RndButton() : juce::Button ("RND") { setClickingTogglesState (false); }
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    };

    /** The info column on the left, and the keyboards' area on the right. */
    void layout (juce::Rectangle<float>& info, juce::Rectangle<float>& keysArea) const;

    void paintScaleOctave (juce::Graphics&, juce::Rectangle<float> keys);

    /** Octaves of keys from firstC. marked() gives a key's colour if it is
        marked; label() its name, or nothing. */
    static void paintKeys (juce::Graphics&, juce::Rectangle<float> keys, int firstC, int octaves,
                           const std::function<std::optional<juce::Colour> (int)>& marked,
                           const std::function<juce::String (int)>& label);

    ChordboxProcessor& processor;
    int pad = 0, shownKey = -1, shownScale = -1;
    bool shownHeld = false;
    juce::String shownName, shownNumeral;
    juce::Array<int> shownNotes;

    RndButton rnd;
    std::unique_ptr<juce::ParameterAttachment> rndAttachment;
};
