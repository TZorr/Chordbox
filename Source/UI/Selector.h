//
//  Selector.h
//  Chordbox
//
//  A flat field that shows one value and opens a menu of the others: Key,
//  Scale, Style, Trigger, Octave, Velocity, Thru.
//
//  Every setting on this panel is a choice from a list, not a continuous
//  amount, so a knob would be the wrong control - turning one to step through
//  twelve keys or eleven scales is slower than picking from a menu and shows
//  less. The field says its name and its value at all times, as Kitbox's
//  knobs do.
//
//  attach() binds it to a parameter (with proper host gestures); a field with
//  no parameter behind it, like Style, sets text/fillMenu/onPick itself.
//

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class Selector : public juce::Component
{
public:
    explicit Selector (const juce::String& labelText);
    ~Selector() override;

    /** Shows and sets a parameter. Long lists (Trigger: 113 notes) come in
        submenus of twelve. */
    void attach (juce::RangedAudioParameter&);

    std::function<juce::String()> text;
    std::function<void (juce::PopupMenu&)> fillMenu;
    std::function<void (int itemId)> onPick;

    /** 30 Hz, from the editor: repaints if the value shown has changed. */
    void refresh();

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

private:
    juce::String label, shown;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};
