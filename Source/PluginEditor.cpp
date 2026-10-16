//
//  PluginEditor.cpp
//  Chordbox
//

#include "PluginEditor.h"
#include "UI/Palette.h"
#include "Theory/Suggest.h"

namespace
{
    /** The panel's theme is the user's, not the session's: it lives in
        ~/Library/Application Support/Chordbox/Chordbox.settings, shared by
        every instance and every project, and never travels with a session. */
    std::unique_ptr<juce::PropertiesFile> openSettings()
    {
        juce::PropertiesFile::Options options;
        options.applicationName     = JucePlugin_Name;
        options.filenameSuffix      = ".settings";
        options.folderName          = JucePlugin_Name;
        options.osxLibrarySubFolder = "Application Support";
        options.getDefaultFile().getParentDirectory().createDirectory();
        return std::make_unique<juce::PropertiesFile> (options);
    }

    constexpr const char* darkModeKey = "darkMode";

    constexpr int margin = 16;
    constexpr int sectionGap = 10;
    constexpr int sectionPadding = 8;
    constexpr int titleHeight = 20;
}

//==============================================================================
void ChordboxEditor::FlatButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour (down ? Palette::accent : (highlighted ? Palette::buttonHover : Palette::button));
    g.fillRoundedRectangle (bounds, 5.0f);

    g.setColour (down ? Palette::pad : Palette::ink);
    g.setFont (Palette::label (10.5f));
    g.drawText (getButtonText().toUpperCase(), bounds, juce::Justification::centred);
}

//==============================================================================
ChordboxEditor::ChordboxEditor (ChordboxProcessor& p)
    : AudioProcessorEditor (&p), chordbox (p), display (p)
{
    const auto parameter = [this] (const char* id) -> juce::RangedAudioParameter&
    {
        return *chordbox.parameters.getParameter (id);
    };

    key.attach (parameter (ChordParams::key));
    scale.attach (parameter (ChordParams::scale));
    trigger.attach (parameter (ChordParams::triggerNote));
    octave.attach (parameter (ChordParams::chordOctave));
    velocity.attach (parameter (ChordParams::velocity));
    thru.attach (parameter (ChordParams::thru));

    // Style is not a parameter: it only matters at the moment Suggest is pressed.
    style.text = [this] { return juce::String (Theory::styleName (chordbox.getStyle())); };
    style.fillMenu = [this] (juce::PopupMenu& menu)
    {
        for (int s = 0; s < Theory::numStyles(); ++s)
            menu.addItem (s + 1, Theory::styleName (s), true, s == chordbox.getStyle());
    };
    style.onPick = [this] (int itemId) { chordbox.setStyle (itemId - 1); };

    suggestButton.onClick = [this] { chordbox.suggestTiles(); };

    for (auto* selector : { &key, &scale, &style, &trigger, &octave, &velocity, &thru })
    {
        selector->refresh();
        addAndMakeVisible (*selector);
    }

    addAndMakeVisible (suggestButton);
    addAndMakeVisible (display);

    // The saved theme first, so the first paint is already the right one.
    Palette::setDark (openSettings()->getBoolValue (darkModeKey, false));
    shownDark = Palette::dark;
    darkButton.setToggleState (Palette::dark, juce::dontSendNotification);
    darkButton.setTooltip ("Dark panel on / off");
    darkButton.onClick = [this] { setDarkMode (darkButton.getToggleState()); };
    addAndMakeVisible (darkButton);
    addAndMakeVisible (presetButton);

    for (int pad = 0; pad < ChordParams::numPads; ++pad)
    {
        auto button = std::make_unique<ChordPad> (chordbox, pad);
        button->onSelect = [this] (int index) { selectPad (index); };
        button->updateFlash();
        addAndMakeVisible (*button);
        pads.push_back (std::move (button));
    }

    for (int slot = ChordParams::numPads; slot < ChordParams::numSlots; ++slot)
    {
        auto tile = std::make_unique<ChordPad> (chordbox, slot);
        tile->updateFlash();
        addAndMakeVisible (*tile);
        tiles.push_back (std::move (tile));
    }

    chordbox.addChangeListener (this);
    shownPlayCount = chordbox.getPlayCount();

    setSize (editorWidth, editorHeight);
    selectPad (chordbox.getSelectedPad());

    startTimerHz (30);
}

ChordboxEditor::~ChordboxEditor()
{
    stopTimer();
    chordbox.removeChangeListener (this);
}

//==============================================================================
void ChordboxEditor::selectPad (int pad)
{
    selectedPad = juce::jlimit (0, ChordParams::numPads - 1, pad);
    chordbox.setSelectedPad (selectedPad);

    for (int i = 0; i < (int) pads.size(); ++i)
        pads[(size_t) i]->setSelected (i == selectedPad);

    display.setPad (selectedPad);
}

void ChordboxEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // A restored session may bring its own selected pad and style.
    if (chordbox.getSelectedPad() != selectedPad)
        selectPad (chordbox.getSelectedPad());

    style.refresh();
}

void ChordboxEditor::setDarkMode (bool shouldBeDark)
{
    Palette::setDark (shouldBeDark);

    if (auto settings = openSettings())
    {
        settings->setValue (darkModeKey, shouldBeDark);
        settings->saveIfNeeded();
    }

    shownDark = shouldBeDark;
    darkButton.setToggleState (shouldBeDark, juce::dontSendNotification);
    repaint();
}

void ChordboxEditor::timerCallback()
{
    // Another Chordbox window may have switched the theme: it is process-wide.
    if (shownDark != Palette::dark)
    {
        shownDark = Palette::dark;
        darkButton.setToggleState (shownDark, juce::dontSendNotification);
        repaint();
    }

    for (auto& pad : pads)
        pad->updateFlash();

    for (auto& tile : tiles)
        tile->updateFlash();

    for (auto* selector : { &key, &scale, &style, &trigger, &octave, &velocity, &thru })
        selector->refresh();

    // The display follows the last chord played, from a key or a click.
    if (const auto count = chordbox.getPlayCount(); count != shownPlayCount)
    {
        shownPlayCount = count;
        display.setPad (chordbox.getLastPlayedPad());
    }

    display.refresh();
}

//==============================================================================
void ChordboxEditor::resized()
{
    auto area = getLocalBounds().reduced (margin, 0);
    area.removeFromTop (margin);

    auto header = area.removeFromTop (86);
    wordmarkArea = header.removeFromLeft (230);

    // Under the wordmark and its subtitle, which end 66 points down.
    darkButton.setBounds (wordmarkArea.getX(), wordmarkArea.getY() + 68, 52, 17);
    presetButton.setBounds (darkButton.getRight() + 6, darkButton.getY(), 64, 17);

    header.removeFromLeft (10);
    display.setBounds (header);

    area.removeFromTop (12);
    auto row = area.removeFromTop (58);

    scaleSection = row.removeFromLeft ((row.getWidth() - sectionGap) * 55 / 100);
    row.removeFromLeft (sectionGap);
    playSection = row;

    // Selectors under each section title, sized by how much they have to say:
    // a scale name is long, a trigger note is two characters.
    const auto layout = [] (juce::Rectangle<int> section, std::vector<std::pair<juce::Component*, int>> items)
    {
        auto inner = section.reduced (sectionPadding, 0);
        inner.removeFromTop (titleHeight + 2);
        inner = inner.removeFromTop (28);

        int weights = 0;
        for (auto& item : items)
            weights += item.second;

        const auto available = inner.getWidth() - 6 * ((int) items.size() - 1);
        for (size_t i = 0; i < items.size(); ++i)
        {
            const auto width = i + 1 == items.size() ? inner.getWidth() : available * items[i].second / weights;
            items[i].first->setBounds (inner.removeFromLeft (width));
            inner.removeFromLeft (6);
        }
    };

    layout (scaleSection, { { &key, 10 }, { &scale, 20 }, { &style, 16 }, { &suggestButton, 9 } });
    layout (playSection,  { { &trigger, 10 }, { &octave, 10 }, { &velocity, 14 }, { &thru, 8 } });

    area.removeFromTop (16);

    // Four rows of eight, one height: the buffer (A-H, I-P), a wider gap,
    // then the pads (1-8, 9-16).
    constexpr int padGap = 6;
    constexpr int groupGap = 12;
    constexpr int padHeight = 54;
    const auto padWidth = (area.getWidth() - 7 * padGap) / 8;

    for (int r = 0; r < 2; ++r)
    {
        auto line = area.removeFromTop (padHeight);

        for (int column = 0; column < 8; ++column)
        {
            const auto width = column == 7 ? line.getWidth() : padWidth;
            tiles[(size_t) (r * 8 + column)]->setBounds (line.removeFromLeft (width));
            line.removeFromLeft (padGap);
        }

        area.removeFromTop (r == 0 ? padGap : groupGap);
    }

    // Pads: two rows of eight, 1-8 on top.
    for (int r = 0; r < 2; ++r)
    {
        auto line = area.removeFromTop (padHeight);

        for (int column = 0; column < 8; ++column)
        {
            const auto width = column == 7 ? line.getWidth() : padWidth;
            pads[(size_t) (r * 8 + column)]->setBounds (line.removeFromLeft (width));
            line.removeFromLeft (padGap);
        }

        area.removeFromTop (padGap);
    }
}

void ChordboxEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::body);

    // Wordmark, two-tone as Kitbox's.
    {
        auto area = wordmarkArea.toFloat();
        const auto font = juce::Font (juce::FontOptions (30.0f, juce::Font::bold)).withExtraKerningFactor (0.16f);
        g.setFont (font);

        const auto chordWidth = juce::GlyphArrangement::getStringWidth (font, "CHORD");
        auto nameRow = area.removeFromTop (52.0f).withTrimmedTop (10.0f);

        g.setColour (Palette::accent);
        g.drawText ("CHORD", nameRow, juce::Justification::centredLeft);
        g.setColour (Palette::ink);
        g.drawText ("BOX", nameRow.withTrimmedLeft (chordWidth + 1.0f), juce::Justification::centredLeft);

        g.setColour (Palette::inkDim);
        g.setFont (Palette::label (9.5f));
        g.drawText ("16-PAD CHORD GENERATOR", area.removeFromTop (14.0f), juce::Justification::centredLeft);
    }

    for (const auto& [bounds, title] : { std::pair { scaleSection, "Scale" }, std::pair { playSection, "Play" } })
    {
        g.setColour (Palette::section);
        g.fillRoundedRectangle (bounds.toFloat(), 6.0f);

        const auto titleRow = bounds.reduced (sectionPadding + 2, 0).removeFromTop (titleHeight).toFloat().withTrimmedTop (3.0f);
        g.setColour (Palette::ink);
        g.setFont (Palette::label (10.0f));
        g.drawText (juce::String (title).toUpperCase(), titleRow, juce::Justification::centredLeft);
    }
}
