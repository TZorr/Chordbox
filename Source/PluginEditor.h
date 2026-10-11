//
//  PluginEditor.h
//  Chordbox
//
//  The panel, top to bottom:
//
//      CHORDBOX   [ display: chord name, notes, keyboard                    ]
//      SCALE  Key | Scale | Style | Suggest     PLAY  Trigger | Octave | Velocity | Thru
//      [A ][B ][C ][D ][E ][F ][G ][H ]            <- buffer: what Suggest fills,
//      [I ][J ][K ][L ][M ][N ][O ][P ]               grey, smaller type
//      [ 1][ 2][ 3][ 4][ 5][ 6][ 7][ 8]            <- pads: what the keys play
//      [ 9][10][11][12][13][14][15][16]
//
//  Kitbox's layout, with the knob rows replaced by one row of selectors: every
//  setting here is a choice from a list. Suggest fills the sixteen buffer
//  tiles from Key, Scale and Style, and chords are dragged from there onto the
//  pads - so the pads hold only what was chosen for them, and no Suggest ever
//  overwrites them. The editor is the drag container for that.
//
//  All four rows are one height: the pads gave up height to make room for the
//  buffer, and the window kept its size.
//
//  The pads sit a clear 30 points above the bottom edge, as in Kitbox: Logic
//  swallows clicks in roughly the last 20 points of a plugin window.
//

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "UI/ChordDisplay.h"
#include "UI/ChordPad.h"
#include "UI/FlatToggle.h"
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
    void setDarkMode (bool shouldBeDark);

    ChordboxProcessor& chordbox;
    int selectedPad = 0;
    int shownPlayCount = 0;

    ChordDisplay display;

    Selector key { "Key" }, scale { "Scale" }, style { "Style" };
    FlatButton suggestButton { "Suggest" };
    FlatToggle darkButton { "Dark" };
    bool shownDark = false;
    Selector trigger { "Trigger" }, octave { "Octave" }, velocity { "Velocity" }, thru { "Thru" };

    juce::Rectangle<int> wordmarkArea, scaleSection, playSection;

    std::vector<std::unique_ptr<ChordPad>> pads, tiles;

    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordboxEditor)
};
