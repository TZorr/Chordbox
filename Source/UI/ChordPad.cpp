//
//  ChordPad.cpp
//  Chordbox
//

#include "ChordPad.h"
#include "Palette.h"
#include "PluginProcessor.h"
#include "Theory/ChordTypes.h"
#include "Theory/Scales.h"

namespace
{
    // Menu item ids. Chords: 1 + root * 100 + type, so 1..1200.
    constexpr int inversionBase = 5000;
    constexpr int octaveBase = 6000;
    constexpr int clearId = 7000;

    int chordId (int root, int type) { return 1 + root * 100 + type; }

    juce::String fromUtf8 (const std::string& text)
    {
        return juce::String (juce::CharPointer_UTF8 (text.c_str()));
    }

    const juce::String dragPrefix = "chordbox-slot:";
}

ChordPad::ChordPad (ChordboxProcessor& processorToUse, int slotIndex)
    : processor (processorToUse), index (slotIndex), tile (ChordParams::isTile (slotIndex))
{
    setRepaintsOnMouseActivity (true);
    lastHitCount = processor.getHitCount (index);
}

juce::String ChordPad::dragDescription (int slot)
{
    return dragPrefix + juce::String (slot);
}

bool ChordPad::parseDragDescription (const juce::var& description, int& slot)
{
    const auto text = description.toString();

    if (! text.startsWith (dragPrefix))
        return false;

    slot = text.fromFirstOccurrenceOf (dragPrefix, false, false).getIntValue();
    return slot >= 0 && slot < ChordParams::numSlots;
}

juce::String ChordPad::tileLabel (int slot)
{
    return juce::String::charToString ((juce::juce_wchar) ('A' + slot - ChordParams::numPads));
}

void ChordPad::setSelected (bool shouldBeSelected)
{
    if (selected != shouldBeSelected)
    {
        selected = shouldBeSelected;
        repaint();
    }
}

void ChordPad::setDropHover (bool hover)
{
    dropHover = hover;
    repaint();
}

void ChordPad::updateFlash()
{
    // The name, degree and trigger note change from outside the pad: a new
    // key respells the chord, a loaded session replaces it, the Trigger Note
    // moves every pad's note, a drop fills a tile.
    const auto name = processor.getPadName (index);
    const auto numeral = processor.getPadNumeral (index);
    const auto trigger = tile ? juce::String() : ChordParams::noteName (processor.getTriggerNote() + index);

    if (name != shownName || numeral != shownNumeral || trigger != shownTrigger)
    {
        shownName = name;
        shownNumeral = numeral;
        shownTrigger = trigger;
        repaint();
    }

    const auto held = processor.isPadHeld (index);
    const auto hits = processor.getHitCount (index);

    if (hits != lastHitCount || held != shownHeld)
    {
        if (hits != lastHitCount)
            flash = 0.35f + 0.65f * processor.getLastVelocity (index);

        lastHitCount = hits;
        shownHeld = held;
        repaint();
    }
    else if (! held && flash > 0.0f)
    {
        flash = flash < 0.02f ? 0.0f : flash * 0.78f;
        repaint();
    }
}

void ChordPad::paint (juce::Graphics& g)
{
    // Pads and tiles share one compact layout, so the four rows line up: a
    // small top line (number or letter, trigger note), then the chord's name
    // with its degree beside it. What tells them apart is colour and size -
    // the pads dark with their group colour and large names, the buffer grey
    // and quieter.
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const auto filled = shownName.isNotEmpty();
    const auto lit = flash > 0.45f;
    const auto groupColour = Palette::padColour (index);

    auto text = bounds.reduced (9.0f, 0.0f);

    if (! filled)
    {
        juce::Path outline;
        outline.addRoundedRectangle (bounds.reduced (0.5f), 5.0f);
        const float dashes[] = { 4.0f, 3.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (outline, outline, dashes, 2);

        g.setColour (dropHover ? Palette::ink : (tile ? Palette::sectionLine : Palette::inkDim.withAlpha (0.6f)));
        g.fillPath (outline);
    }
    else
    {
        auto fill = tile ? (isMouseOver() ? Palette::buttonHover : Palette::button)
                         : (isMouseOver() ? Palette::padHover : Palette::pad);
        fill = fill.interpolatedWith (tile ? Palette::accent : groupColour, flash * 0.85f);

        g.setColour (fill);
        g.fillRoundedRectangle (bounds, 5.0f);

        if (! tile)
        {
            juce::Graphics::ScopedSaveState state (g);
            juce::Path clip;
            clip.addRoundedRectangle (bounds, 5.0f);
            g.reduceClipRegion (clip);
            g.setColour (groupColour);
            g.fillRect (bounds.withHeight (3.0f));
        }
    }

    if (selected || (dropHover && filled))
    {
        g.setColour (dropHover ? (tile ? Palette::ink : Palette::padText) : Palette::accent);
        g.drawRoundedRectangle (bounds.reduced (1.0f), 5.0f, 2.0f);
    }

    // Top line.
    text.removeFromTop (6.0f);
    auto topRow = text.removeFromTop (12.0f);

    const auto dim = ! filled ? Palette::inkDim
                              : (lit ? Palette::pad : (tile ? Palette::inkDim : Palette::padTextDim));

    g.setColour (dim);
    g.setFont (Palette::mono (9.5f, true));
    g.drawText (tile ? tileLabel (index) : juce::String (index + 1).paddedLeft ('0', 2), topRow,
                juce::Justification::centredLeft);

    if (! tile)
    {
        g.setFont (Palette::mono (9.5f));
        g.drawText (shownTrigger, topRow, juce::Justification::centredRight);
    }

    // Name and degree.
    auto nameRow = text.withTrimmedBottom (3.0f);

    if (! filled)
    {
        g.setFont (Palette::label (9.0f));
        g.setColour (Palette::inkDim);
        g.drawFittedText (tile ? "EMPTY" : "DROP A CHORD", nameRow.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
        return;
    }

    g.setFont (Palette::label (tile ? 9.5f : 10.5f));
    g.setColour (lit ? Palette::pad : (tile ? Palette::inkDim : groupColour.withAlpha (0.9f)));
    g.drawText (shownNumeral, nameRow, juce::Justification::centredRight);
    nameRow.removeFromRight (juce::GlyphArrangement::getStringWidth (Palette::label (10.5f), shownNumeral) + 6.0f);

    g.setFont (juce::Font (juce::FontOptions (tile ? 13.0f : 17.0f, juce::Font::bold)));
    g.setColour (lit ? Palette::pad : (tile ? Palette::ink : Palette::padText));
    g.drawFittedText (shownName, nameRow.toNearestInt(), juce::Justification::centredLeft, 1, 0.6f);
}

void ChordPad::mouseDown (const juce::MouseEvent& event)
{
    dragStarted = false;

    if (! tile && onSelect)
        onSelect (index);

    if (event.mods.isPopupMenu())
    {
        if (tile)
            showTileMenu();
        else
            showMenu();
        return;
    }

    if (! processor.isFilled (index))
        return;

    const auto height = (float) juce::jmax (1, getHeight());
    const auto velocity = tile ? 0.8f
                               : juce::jmap (juce::jlimit (0.0f, 1.0f, event.position.y / height), 1.0f, 0.35f);

    processor.padDown (index, velocity);
    pressed = true;
}

void ChordPad::mouseDrag (const juce::MouseEvent& event)
{
    if (dragStarted || event.mods.isPopupMenu() || event.getDistanceFromDragStart() < 6)
        return;

    if (! processor.isFilled (index))
        return;

    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
    {
        dragStarted = true;

        // A drag is a move, not a performance: the chord the click started
        // stops as soon as the drag begins.
        if (pressed)
        {
            processor.padUp (index);
            pressed = false;
        }

        const auto image = createComponentSnapshot (getLocalBounds()).convertedToFormat (juce::Image::ARGB);
        container->startDragging (dragDescription (index), this, juce::ScaledImage (image), false);
    }
}

void ChordPad::mouseUp (const juce::MouseEvent&)
{
    if (pressed)
    {
        processor.padUp (index);
        pressed = false;
    }
}

bool ChordPad::isInterestedInDragSource (const SourceDetails& details)
{
    int source = -1;
    return parseDragDescription (details.description, source) && source != index;
}

void ChordPad::itemDropped (const SourceDetails& details)
{
    setDropHover (false);

    int source = -1;

    if (! parseDragDescription (details.description, source))
        return;

    processor.dropSlot (source, index);

    if (! tile && onSelect)
        onSelect (index);
}

void ChordPad::showTileMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Clear Tile", processor.isFilled (index));

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [safe = juce::Component::SafePointer<ChordPad> (this)] (int result)
    {
        if (safe != nullptr && result == 1)
            safe->processor.clearSlot (safe->index);
    });
}

void ChordPad::showMenu()
{
    using namespace Theory;

    // An empty pad has no chord to tick, invert or move; picking one fills it.
    const auto filled = processor.isFilled (index);
    const auto current = processor.getPad (index);
    const auto key = processor.getKey();
    const auto scaleIndex = processor.getScale();
    const Spelling spelling (key, scaleIndex);

    const auto addChords = [&] (juce::PopupMenu& menu, int root, bool inScaleOnly)
    {
        for (auto family : families())
        {
            bool headed = false;

            for (int type = 0; type < numChordTypes(); ++type)
            {
                if (chordType (type).family != family)
                    continue;

                if (inScaleOnly && ! fitsScale (root, type, key, scaleIndex))
                    continue;

                if (! headed)
                {
                    menu.addSectionHeader (familyName (family));
                    headed = true;
                }

                PadChord chord { root, type, 0, 0 };
                menu.addItem (chordId (root, type), fromUtf8 (chord.name (spelling)), true,
                              filled && current.root == root && current.type == type);
            }
        }
    };

    juce::PopupMenu menu;

    // In the key: one submenu per degree, the chords that fit on it.
    menu.addSectionHeader (fromUtf8 (spelling.rootName()) + " " + juce::String (scale (scaleIndex).name.c_str()));

    for (auto step : scale (scaleIndex).steps)
    {
        const auto root = (key + step) % 12;
        juce::PopupMenu chords;
        addChords (chords, root, true);

        if (chords.getNumItems() == 0)
            continue;

        // The degree, in capitals whatever the chord: the submenu is the root,
        // not one chord on it.
        const PadChord probe { root, 0, 0, 0 };
        const auto title = fromUtf8 (spelling.name (root)) + "    " + fromUtf8 (probe.numeral (key, scaleIndex));

        menu.addSubMenu (title, chords, true, nullptr, filled && current.root == root);
    }

    // Everything: twelve roots, each split by family.
    juce::PopupMenu all;
    for (int root = 0; root < 12; ++root)
    {
        juce::PopupMenu chords;
        addChords (chords, root, false);
        all.addSubMenu (fromUtf8 (spelling.name (root)), chords, true, nullptr, filled && current.root == root);
    }

    menu.addSeparator();
    menu.addSubMenu ("All Chords", all);

    juce::PopupMenu inversions;
    static const char* const inversionNames[] = { "Root Position", "1st Inversion", "2nd Inversion",
                                                  "3rd Inversion", "4th Inversion", "5th Inversion" };
    for (int i = 0; i < juce::jmin (current.numInversions(), 6); ++i)
    {
        auto inverted = current;
        inverted.inversion = i;
        inversions.addItem (inversionBase + i, juce::String (inversionNames[i]) + "    " + fromUtf8 (inverted.name (spelling)),
                            true, current.inversion == i);
    }

    juce::PopupMenu octaves;
    for (int o = PadChord::minOctave; o <= PadChord::maxOctave; ++o)
        octaves.addItem (octaveBase + o - PadChord::minOctave, o > 0 ? "+" + juce::String (o) : juce::String (o),
                         true, current.octave == o);

    menu.addSeparator();
    menu.addSubMenu ("Inversion", inversions, filled);
    menu.addSubMenu ("Octave", octaves, filled);
    menu.addSeparator();
    menu.addItem (clearId, "Clear Pad", filled);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [safe = juce::Component::SafePointer<ChordPad> (this)] (int result)
    {
        if (safe == nullptr || result == 0)
            return;

        if (result == clearId)
        {
            safe->processor.clearSlot (safe->index);
            return;
        }

        auto chord = safe->processor.isFilled (safe->index) ? safe->processor.getPad (safe->index)
                                                             : PadChord {};

        if (result >= octaveBase)
        {
            chord.octave = result - octaveBase + PadChord::minOctave;
        }
        else if (result >= inversionBase)
        {
            chord.inversion = result - inversionBase;
        }
        else
        {
            chord.root = (result - 1) / 100;
            chord.type = (result - 1) % 100;
            chord.inversion = juce::jlimit (0, chord.numInversions() - 1, chord.inversion);
        }

        safe->processor.setPad (safe->index, chord);
    });
}
