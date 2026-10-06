//
//  ChordDisplay.cpp
//  Chordbox
//

#include "ChordDisplay.h"
#include "Palette.h"
#include "ChordPad.h"
#include "PluginProcessor.h"
#include "Theory/Scales.h"

ChordDisplay::ChordDisplay (ChordboxProcessor& processorToUse)
    : processor (processorToUse)
{
    auto& parameter = *processor.parameters.getParameter (ChordParams::humanize);

    rndAttachment = std::make_unique<juce::ParameterAttachment> (parameter, [this] (float value)
    {
        rnd.setToggleState (value > 0.5f, juce::dontSendNotification);
        rnd.repaint();
    });

    rnd.onClick = [this] { rndAttachment->setValueAsCompleteGesture (rnd.getToggleState() ? 0.0f : 1.0f); };
    rnd.setTooltip (juce::String (juce::CharPointer_UTF8 ("Humanize: velocity \xc2\xb1" "12 %, timing 0\xe2\x80\x93" "16 ms")));
    rnd.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    addAndMakeVisible (rnd);

    rndAttachment->sendInitialUpdate();
}

void ChordDisplay::RndButton::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto on = getToggleState();

    g.setColour (on ? Palette::accent : (highlighted ? Palette::displayGrid.brighter (0.2f) : Palette::displayGrid));
    g.fillRoundedRectangle (bounds, 3.0f);

    g.setColour (on ? Palette::display : Palette::displayDim);
    g.setFont (Palette::mono (9.5f, true));
    g.drawText ("RND", bounds, juce::Justification::centred);
}

void ChordDisplay::layout (juce::Rectangle<float>& info, juce::Rectangle<float>& keysArea) const
{
    keysArea = getLocalBounds().toFloat().reduced (14.0f, 10.0f);
    info = keysArea.removeFromLeft (juce::jmin (260.0f, keysArea.getWidth() * 0.35f));
    keysArea.removeFromLeft (14.0f);
}

void ChordDisplay::resized()
{
    juce::Rectangle<float> info, keysArea;
    layout (info, keysArea);

    // Top right of the info column: directly left of the scale octave.
    rnd.setBounds (juce::Rectangle<float> (info.getRight() - 34.0f, info.getY(), 34.0f, 15.0f).toNearestInt());
}

void ChordDisplay::setPad (int newPad)
{
    if (pad != newPad)
    {
        pad = newPad;
        refresh();
        repaint();
    }
}

void ChordDisplay::refresh()
{
    const auto name = processor.getPadName (pad);
    const auto numeral = processor.getPadNumeral (pad);
    const auto notes = processor.getPadNotes (pad);
    const auto held = processor.isPadHeld (pad);
    const auto key = processor.getKey();
    const auto scale = processor.getScale();

    if (name != shownName || numeral != shownNumeral || notes != shownNotes || held != shownHeld
        || key != shownKey || scale != shownScale)
    {
        shownName = name;
        shownNumeral = numeral;
        shownNotes = notes;
        shownHeld = held;
        shownKey = key;
        shownScale = scale;
        repaint();
    }
}

void ChordDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour (Palette::display);
    g.fillRoundedRectangle (bounds, 6.0f);

    juce::Rectangle<float> info, area;
    layout (info, area);

    // Left: pad or tile, degree, chord name, notes.
    {
        const auto tile = ChordParams::isTile (pad);
        const auto label = tile ? "BUFFER " + ChordPad::tileLabel (pad)
                                : "PAD " + juce::String (pad + 1).paddedLeft ('0', 2);

        auto top = info.removeFromTop (14.0f);
        g.setFont (Palette::mono (10.5f, true));
        g.setColour (Palette::displayDim);
        g.drawText (label, top, juce::Justification::centredLeft);

        if (shownNumeral.isNotEmpty())
        {
            g.setColour (tile ? Palette::accent : Palette::padColour (pad));
            g.drawText (shownNumeral, top.withTrimmedLeft (72.0f), juce::Justification::centredLeft);
        }

        auto notesRow = info.removeFromBottom (14.0f);
        g.setFont (Palette::mono (11.0f));
        g.setColour (Palette::displayDim);
        g.drawText (processor.getNotesText (shownNotes), notesRow, juce::Justification::centredLeft);

        g.setFont (juce::Font (juce::FontOptions (32.0f, juce::Font::bold)));
        g.setColour (Palette::displayText);
        g.drawFittedText (shownName.isNotEmpty() ? shownName : juce::String ("-"), info.toNearestInt(),
                          juce::Justification::centredLeft, 1, 0.6f);
    }

    // Right: one octave showing the scale, a gap, then the chord's keyboard -
    // three octaves from the C at or below its lowest note, more if it reaches
    // further (a thirteenth in second inversion does), at most five. Both
    // share one key width, so they read as the same instrument.
    auto low = 48, high = 48;
    if (! shownNotes.isEmpty())
    {
        low  = shownNotes.getFirst();
        high = shownNotes.getLast();
    }

    const auto firstC = juce::jmax (0, low / 12 * 12);
    const auto octaves = juce::jlimit (3, 5, (high - firstC) / 12 + 1);

    constexpr float gap = 18.0f;
    const auto keyWidth = (area.getWidth() - gap) / (float) (7 * (octaves + 1));
    const auto keys = area.withSizeKeepingCentre (area.getWidth(), juce::jmin (area.getHeight(), 66.0f));

    auto scaleKeys = keys.withWidth (keyWidth * 7.0f);
    auto chordKeys = keys.withTrimmedLeft (scaleKeys.getWidth() + gap);

    paintScaleOctave (g, scaleKeys);
    paintKeys (g, chordKeys, firstC, octaves, [this] (int note) -> std::optional<juce::Colour>
    {
        if (! shownNotes.contains (note))
            return std::nullopt;

        return shownHeld ? Palette::displayText : Palette::displayDim;
    }, [] (int note) -> juce::String
    {
        return note % 12 == 0 ? "C" + juce::String (note / 12 - 2) : juce::String();
    });
}

void ChordDisplay::paintScaleOctave (juce::Graphics& g, juce::Rectangle<float> keys)
{
    // C to B with the key's notes marked and named as the key spells them -
    // F major shows Bb, not A# - and the root brightest.
    const Theory::Spelling spelling (shownKey, shownScale);
    const auto mask = Theory::scaleMask (shownKey, shownScale);

    paintKeys (g, keys, 0, 1, [this, mask] (int note) -> std::optional<juce::Colour>
    {
        if ((mask & (1 << (note % 12))) == 0)
            return std::nullopt;

        return note % 12 == shownKey ? Palette::displayText : Palette::displayDim;
    }, [spelling, mask] (int note) -> juce::String
    {
        if ((mask & (1 << (note % 12))) == 0)
            return {};

        return juce::String (spelling.name (note).c_str());
    });
}

void ChordDisplay::paintKeys (juce::Graphics& g, juce::Rectangle<float> keys, int firstC, int octaves,
                              const std::function<std::optional<juce::Colour> (int)>& marked,
                              const std::function<juce::String (int)>& label)
{
    static constexpr int whiteSteps[7] = { 0, 2, 4, 5, 7, 9, 11 };
    static constexpr int blackAfter[5] = { 0, 1, 3, 4, 5 };   // white key index a black key follows
    static constexpr int blackSteps[5] = { 1, 3, 6, 8, 10 };

    const auto whiteCount = octaves * 7;
    const auto keyWidth = keys.getWidth() / (float) whiteCount;
    const auto labelFont = Palette::mono (8.5f);

    for (int w = 0; w < whiteCount; ++w)
    {
        const auto note = firstC + 12 * (w / 7) + whiteSteps[w % 7];
        juce::Rectangle<float> key (keys.getX() + (float) w * keyWidth, keys.getY(), keyWidth, keys.getHeight());

        g.setColour (marked (note).value_or (Palette::displayGrid.brighter (0.3f)));
        g.fillRoundedRectangle (key.reduced (1.0f, 0.0f), 2.0f);

        if (const auto text = label (note); text.isNotEmpty())
        {
            g.setColour (Palette::display.withAlpha (0.75f));
            g.setFont (labelFont);
            g.drawFittedText (text, key.removeFromBottom (12.0f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
        }
    }

    for (int o = 0; o < octaves; ++o)
    {
        for (int b = 0; b < 5; ++b)
        {
            const auto note = firstC + 12 * o + blackSteps[b];
            const auto x = keys.getX() + (float) (o * 7 + blackAfter[b] + 1) * keyWidth - keyWidth * 0.3f;
            juce::Rectangle<float> key (x, keys.getY(), keyWidth * 0.6f, keys.getHeight() * 0.6f);

            const auto colour = marked (note);
            g.setColour (Palette::display);
            g.fillRect (key.expanded (1.0f, 0.0f));
            g.setColour (colour.value_or (Palette::display));
            g.fillRoundedRectangle (key, 1.5f);

            // A black key's name sits at its foot, and only when the key is
            // marked: an unmarked black key has nothing to say.
            if (const auto text = label (note); text.isNotEmpty() && colour.has_value())
            {
                g.setColour (Palette::display.withAlpha (0.85f));
                g.setFont (labelFont);
                g.drawFittedText (text, key.removeFromBottom (12.0f).expanded (3.0f, 0.0f).toNearestInt(),
                                  juce::Justification::centred, 1, 0.6f);
            }
        }
    }
}
