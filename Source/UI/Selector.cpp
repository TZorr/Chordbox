//
//  Selector.cpp
//  Chordbox
//

#include "Selector.h"
#include "Palette.h"

Selector::Selector (const juce::String& labelText)
    : label (labelText)
{
    setRepaintsOnMouseActivity (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

Selector::~Selector() = default;

void Selector::attach (juce::RangedAudioParameter& parameter)
{
    attachment = std::make_unique<juce::ParameterAttachment> (parameter, [this] (float) { refresh(); });

    const auto& range = parameter.getNormalisableRange();
    const auto first = (int) range.start;
    const auto last  = (int) range.end;

    text = [&parameter] { return parameter.getCurrentValueAsText(); };

    // Item ids are value - first + 1: zero is the menu's "nothing chosen".
    fillMenu = [&parameter, first, last] (juce::PopupMenu& menu)
    {
        const auto current = juce::roundToInt (parameter.convertFrom0to1 (parameter.getValue()));
        const auto itemText = [&parameter] (int value)
        {
            return parameter.getText (parameter.convertTo0to1 ((float) value), 32);
        };

        if (last - first < 24)
        {
            for (int value = first; value <= last; ++value)
                menu.addItem (value - first + 1, itemText (value), true, value == current);
            return;
        }

        for (int start = first; start <= last; start += 12)
        {
            const auto end = juce::jmin (last, start + 11);
            juce::PopupMenu group;

            for (int value = start; value <= end; ++value)
                group.addItem (value - first + 1, itemText (value), true, value == current);

            menu.addSubMenu (itemText (start) + " - " + itemText (end), group, true, nullptr,
                             current >= start && current <= end);
        }
    };

    onPick = [this, first] (int itemId)
    {
        if (attachment != nullptr)
            attachment->setValueAsCompleteGesture ((float) (itemId - 1 + first));
    };

    attachment->sendInitialUpdate();
}

void Selector::refresh()
{
    const auto now = text ? text() : juce::String();

    if (now != shown)
    {
        shown = now;
        repaint();
    }
}

void Selector::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour (isMouseOver() ? Palette::buttonHover : Palette::button);
    g.fillRoundedRectangle (bounds, 5.0f);

    auto inner = bounds.reduced (9.0f, 0.0f);

    g.setColour (Palette::inkDim);
    g.setFont (Palette::label (9.5f));
    g.drawText (label.toUpperCase(), inner, juce::Justification::centredLeft);

    // A small down-pointing triangle: this opens a list.
    auto arrow = inner.removeFromRight (8.0f).withSizeKeepingCentre (8.0f, 5.0f);
    juce::Path triangle;
    triangle.addTriangle (arrow.getTopLeft(), arrow.getTopRight(), { arrow.getCentreX(), arrow.getBottom() });
    g.fillPath (triangle);
    inner.removeFromRight (6.0f);

    g.setColour (Palette::ink);
    g.setFont (Palette::label (11.5f));
    g.drawFittedText (shown, inner.toNearestInt(), juce::Justification::centredRight, 1, 0.8f);
}

void Selector::mouseDown (const juce::MouseEvent&)
{
    if (! fillMenu)
        return;

    juce::PopupMenu menu;
    fillMenu (menu);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [safe = juce::Component::SafePointer<Selector> (this)] (int result)
    {
        if (safe == nullptr || result == 0)
            return;

        if (safe->onPick)
            safe->onPick (result);

        safe->refresh();
    });
}
