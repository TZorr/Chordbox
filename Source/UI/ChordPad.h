//
//  ChordPad.h
//  Chordbox
//
//  One of the sixteen pads, or one of the sixteen buffer tiles above them.
//
//  A pad is taken from Kitbox's PadButton: the same dark pad, group-colour
//  strip, accent outline when selected, and a flash that lights it when it
//  plays. Unlike a drum hit a chord lasts, so the pad stays lit for as long as
//  its chord is held - from the panel or from a key - and fades only once it
//  is let go. The chord's name is the pad's label, its degree in the key (IV,
//  vi, vii°) beside it, and the note that plays it in the corner.
//
//  A tile is one place in the buffer that Suggest fills: grey, smaller type,
//  lettered A-P. Clicking it plays the chord to audition it; dragging it onto a
//  pad puts the chord there. Pads start empty and play nothing until then.
//
//  Click and hold: play the chord (a pad louder the higher up it is hit, as in
//  Kitbox). Right-click a pad: choose its chord - from the key first, then
//  from every chord on every root - its inversion and octave, or clear it.
//  Right-click a tile: clear it.
//
//  Drag and drop: a pad onto a pad swaps the two; a pad onto a tile, or a tile
//  onto a pad, copies the chord across, so one tile can fill several pads; a
//  tile onto a tile swaps them.
//

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class ChordboxProcessor;

class ChordPad : public juce::Component,
                 public juce::DragAndDropTarget
{
public:
    ChordPad (ChordboxProcessor& processorToUse, int slotIndex);

    std::function<void (int)> onSelect;

    void setSelected (bool shouldBeSelected);

    /** 30 Hz, from the editor: picks up plays, holds and renamed chords. */
    void updateFlash();

    static bool parseDragDescription (const juce::var& description, int& slot);
    static juce::String dragDescription (int slot);

    /** "A" for the first tile. */
    static juce::String tileLabel (int slot);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { setDropHover (true); }
    void itemDragExit (const SourceDetails&) override  { setDropHover (false); }
    void itemDropped (const SourceDetails&) override;

private:
    void setDropHover (bool hover);
    void showMenu();
    void showTileMenu();

    ChordboxProcessor& processor;
    const int index;
    const bool tile;
    bool selected = false, pressed = false, shownHeld = false, dropHover = false, dragStarted = false;
    int lastHitCount = 0;
    float flash = 0.0f;
    juce::String shownName, shownNumeral, shownTrigger;
};
