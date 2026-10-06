//
//  PluginEditor.h
//  Chordbox
//
//  The panel, top to bottom:
//
//      CHORDBOX   [ display: chord name, notes, keyboard                    ]
//      SCALE  Key | Scale | Style | Suggest     PLAY  Trigger | Octave | Velocity | Thru
//      [A ][B ][C ][D ][E ][F ][G ][H ]            <- tiles: a shelf for chords
//      [ 1][ 2][ 3][ 4][ 5][ 6][ 7][ 8]
//      [ 9][10][11][12][13][14][15][16]
//
//  Kitbox's layout, with the knob rows replaced by one row of selectors: every
//  setting here is a choice from a list. Suggest refills all sixteen pads from
//  Key, Scale and Style; changing Key or Scale alone does not, so a set of
//  hand-picked chords is never thrown away by a stray click - and a chord
//  worth keeping across a Suggest can be parked on a tile first. The editor is
//  the drag container for moving chords between pads and tiles.
//
//  The pads sit a clear 30 points above the bottom edge, as in Kitbox: Logic
//  swallows clicks in roughly the last 20 points of a plugin window.
//

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "UI/ChordDisplay.h"
#include "UI/ChordPad.h"
#include "UI/Selector.h"

class ChordboxEditor : public juce::AudioProcessorEditor,
                       public juce::DragAndDropContainer,
                       private juce::ChangeListener,
                       private juce::Timer
{
public:
    explicit ChordboxEditor (ChordboxProcessor&);
    ~ChordboxEditor() override;

    static constexpr int editorWidth  = 1080;
    static constexpr int editorHeight = 460;

    void paint (juce::Graphics&) override;
    void resized() override;

    void selectPad (int pad);

private:
    /** Kitbox's flat button. */
    class FlatButton : public juce::Button
    {
    public:
        explicit FlatButton (const juce::String& text) : juce::Button (text) {}
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    ChordboxProcessor& chordbox;
    int selectedPad = 0;
    int shownPlayCount = 0;

    ChordDisplay display;

    Selector key { "Key" }, scale { "Scale" }, style { "Style" };
    FlatButton suggestButton { "Suggest" };
    Selector trigger { "Trigger" }, octave { "Octave" }, velocity { "Velocity" }, thru { "Thru" };

    juce::Rectangle<int> wordmarkArea, scaleSection, playSection;

    std::vector<std::unique_ptr<ChordPad>> pads, tiles;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordboxEditor)
};
