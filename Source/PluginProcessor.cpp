//
//  PluginProcessor.cpp
//  Chordbox
//

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Theory/ChordTypes.h"
#include "Theory/Scales.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout ChordboxProcessor::makeParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    juce::StringArray keys, scales;
    for (int pc = 0; pc < 12; ++pc)
        keys.add (Theory::keyName (pc));
    for (const auto& s : Theory::scales())
        scales.add (s.name);

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ChordParams::key, 1 },
                                                              "Key", keys, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ChordParams::scale, 1 },
                                                              "Scale", scales, 0));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ChordParams::triggerNote, 1 }, "Trigger Note", 0, ChordParams::maxTrigger,
        ChordParams::defaultTrigger,
        juce::AudioParameterIntAttributes().withStringFromValueFunction ([] (int note, int)
        {
            return ChordParams::noteName (note);
        })));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ChordParams::chordOctave, 1 }, "Chord Octave", 0, 6, ChordParams::defaultOctave,
        juce::AudioParameterIntAttributes().withStringFromValueFunction ([] (int octave, int)
        {
            return "C" + juce::String (octave);
        })));

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ChordParams::velocity, 1 },
                                                              "Velocity",
                                                              juce::StringArray { "As Played", "Fixed 100" }, 0));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ChordParams::thru, 1 },
                                                            "Thru", true));

    return layout;
}

ChordboxProcessor::ChordboxProcessor()
    : AudioProcessor (BusesProperties()),
      parameters (*this, nullptr, ChordParams::stateTreeType, makeParameterLayout())
{
    keyParam      = parameters.getRawParameterValue (ChordParams::key);
    scaleParam    = parameters.getRawParameterValue (ChordParams::scale);
    triggerParam  = parameters.getRawParameterValue (ChordParams::triggerNote);
    octaveParam   = parameters.getRawParameterValue (ChordParams::chordOctave);
    velocityParam = parameters.getRawParameterValue (ChordParams::velocity);
    thruParam     = parameters.getRawParameterValue (ChordParams::thru);

    // A fresh instance starts with chords to choose from - C major, triads
    // over sevenths - in the buffer, and empty pads to put them on.
    const auto suggested = Theory::suggest (0, 0, 0);
    std::copy (suggested.begin(), suggested.end(), pads.begin() + ChordParams::numPads);
    std::fill (filled.begin() + ChordParams::numPads, filled.end(), true);

    for (int slot = 0; slot < ChordParams::numSlots; ++slot)
        publish (slot);
}

//==============================================================================
int ChordboxProcessor::getKey() const          { return (int) keyParam->load(); }
int ChordboxProcessor::getScale() const        { return (int) scaleParam->load(); }
int ChordboxProcessor::getTriggerNote() const  { return (int) triggerParam->load(); }
int ChordboxProcessor::getOctaveBase() const   { return ChordParams::octaveBase ((int) octaveParam->load()); }

Theory::PadChord ChordboxProcessor::getPad (int slot) const
{
    const juce::ScopedLock lock (padLock);
    return pads[(size_t) juce::jlimit (0, ChordParams::numSlots - 1, slot)];
}

bool ChordboxProcessor::isFilled (int slot) const
{
    const juce::ScopedLock lock (padLock);
    return slot >= 0 && slot < ChordParams::numSlots && filled[(size_t) slot];
}

void ChordboxProcessor::setPad (int slot, const Theory::PadChord& chord)
{
    if (slot < 0 || slot >= ChordParams::numSlots)
        return;

    {
        const juce::ScopedLock lock (padLock);
        pads[(size_t) slot] = chord;
        filled[(size_t) slot] = true;
    }

    publish (slot);
    sendChangeMessage();
}

void ChordboxProcessor::clearSlot (int slot)
{
    if (slot < 0 || slot >= ChordParams::numSlots)
        return;

    {
        const juce::ScopedLock lock (padLock);
        filled[(size_t) slot] = false;
    }

    publish (slot);
    sendChangeMessage();
}

void ChordboxProcessor::dropSlot (int source, int target)
{
    if (source == target || ! isFilled (source) || target < 0 || target >= ChordParams::numSlots)
        return;

    if (ChordParams::isTile (source) == ChordParams::isTile (target))
    {
        // Same kind: swap. Dropped on an empty one, it simply moves there.
        {
            const juce::ScopedLock lock (padLock);
            std::swap (pads[(size_t) source], pads[(size_t) target]);
            std::swap (filled[(size_t) source], filled[(size_t) target]);
        }

        publish (source);
        publish (target);
        sendChangeMessage();
        return;
    }

    setPad (target, getPad (source));
}

void ChordboxProcessor::publish (int slot)
{
    // An empty slot publishes a chord of no notes, which the audio thread
    // plays as nothing.
    packedPads[(size_t) slot].store (isFilled (slot) ? Theory::packNotes (getPad (slot).offsets()) : 0);
}

void ChordboxProcessor::setStyle (int newStyle)
{
    style = juce::jlimit (0, Theory::numStyles() - 1, newStyle);
}

void ChordboxProcessor::suggestTiles()
{
    const auto suggested = Theory::suggest (getKey(), getScale(), style);

    {
        const juce::ScopedLock lock (padLock);
        std::copy (suggested.begin(), suggested.end(), pads.begin() + ChordParams::numPads);
        std::fill (filled.begin() + ChordParams::numPads, filled.end(), true);
    }

    for (int tile = ChordParams::numPads; tile < ChordParams::numSlots; ++tile)
        publish (tile);

    sendChangeMessage();
}

juce::String ChordboxProcessor::getPadName (int pad) const
{
    if (! isFilled (pad))
        return {};

    const Theory::Spelling spelling (getKey(), getScale());
    return juce::String (juce::CharPointer_UTF8 (getPad (pad).name (spelling).c_str()));
}

juce::String ChordboxProcessor::getPadNumeral (int pad) const
{
    if (! isFilled (pad))
        return {};

    return juce::String (juce::CharPointer_UTF8 (getPad (pad).numeral (getKey(), getScale()).c_str()));
}

juce::Array<int> ChordboxProcessor::getPadNotes (int pad) const
{
    juce::Array<int> notes;
    const auto base = getOctaveBase();

    if (! isFilled (pad))
        return notes;

    for (auto offset : getPad (pad).offsets())
        if (base + offset >= 0 && base + offset < 128)
            notes.add (base + offset);

    return notes;
}

juce::String ChordboxProcessor::getNotesText (const juce::Array<int>& notes) const
{
    const Theory::Spelling spelling (getKey(), getScale());
    juce::StringArray names;

    for (auto note : notes)
        names.add (juce::String (spelling.name (note).c_str()) + juce::String (note / 12 - 2));

    return names.joinIntoString (" ");
}

//==============================================================================
void ChordboxProcessor::padDown (int pad, float velocity)
{
    const auto scope = panelFifo.write (1);
    if (scope.blockSize1 > 0)
        panelEvents[(size_t) scope.startIndex1] = { pad, velocity, true };
}

void ChordboxProcessor::padUp (int pad)
{
    const auto scope = panelFifo.write (1);
    if (scope.blockSize1 > 0)
        panelEvents[(size_t) scope.startIndex1] = { pad, 0.0f, false };
}

//==============================================================================
void ChordboxProcessor::prepareToPlay (double, int)
{
    // A host may stop calling processBlock with chords still sounding, and
    // prepare again later. Whatever was held then is released on the first
    // block after.
    releasePending = true;
}

void ChordboxProcessor::padOn (int pad, int velocity, int channel, juce::MidiBuffer& out, int position)
{
    if (sounding[(size_t) pad].on)
        padOff (pad, out, position);

    int offsets[Theory::maxPackedNotes];
    const auto count = Theory::unpackNotes (packedPads[(size_t) pad].load(), offsets);

    if (count == 0)
        return;   // an empty pad or tile
    const auto base = ChordParams::octaveBase ((int) octaveParam->load());

    auto& voice = sounding[(size_t) pad];
    voice.on = true;
    voice.channel = channel;
    voice.count = 0;

    const auto ch = juce::jlimit (1, 16, channel);
    const auto vel = (juce::uint8) juce::jlimit (1, 127, velocity);

    for (int i = 0; i < count; ++i)
    {
        const auto note = base + offsets[i];

        if (note < 0 || note > 127)
            continue;

        // A note another pad already holds is struck again rather than left
        // to ring: playing C and then Am should sound the C of Am.
        auto& refs = noteRefs[ch - 1][note];
        if (refs > 0)
            out.addEvent (juce::MidiMessage::noteOff (ch, note), position);

        out.addEvent (juce::MidiMessage::noteOn (ch, note, vel), position);
        ++refs;
        voice.notes[voice.count++] = note;
    }

    held[(size_t) pad] = true;
    lastVelocity[(size_t) pad] = (float) velocity / 127.0f;
    hits[(size_t) pad].fetch_add (1);
    lastPlayedPad = pad;
    playCount.fetch_add (1);
}

void ChordboxProcessor::padOff (int pad, juce::MidiBuffer& out, int position)
{
    auto& voice = sounding[(size_t) pad];

    if (! voice.on)
        return;

    const auto ch = juce::jlimit (1, 16, voice.channel);

    for (int i = 0; i < voice.count; ++i)
    {
        auto& refs = noteRefs[ch - 1][voice.notes[i]];

        if (refs > 0 && --refs == 0)
            out.addEvent (juce::MidiMessage::noteOff (ch, voice.notes[i]), position);
    }

    voice.on = false;
    voice.count = 0;
    held[(size_t) pad] = false;
}

void ChordboxProcessor::releaseAll (juce::MidiBuffer& out, int position)
{
    for (int slot = 0; slot < ChordParams::numSlots; ++slot)
        padOff (slot, out, position);

    std::memset (padFromKey, 0, sizeof (padFromKey));
}

void ChordboxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();

    juce::MidiBuffer out;

    if (releasePending.exchange (false))
        releaseAll (out, 0);

    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            const auto playing = position->getIsPlaying();

            if (wasPlaying && ! playing)
                releaseAll (out, 0);

            wasPlaying = playing;
        }
    }

    // Clicks on the panel, at the start of the block. They play on channel 1.
    {
        const auto scope = panelFifo.read (panelFifo.getNumReady());

        const auto handle = [&] (int start, int size)
        {
            for (int i = start; i < start + size; ++i)
            {
                const auto& event = panelEvents[(size_t) i];

                if (event.pad < 0 || event.pad >= ChordParams::numSlots)
                    continue;

                if (event.on)
                    padOn (event.pad, juce::roundToInt (event.velocity * 127.0f), 1, out, 0);
                else
                    padOff (event.pad, out, 0);
            }
        };

        handle (scope.startIndex1, scope.blockSize1);
        handle (scope.startIndex2, scope.blockSize2);
    }

    const auto trigger = (int) triggerParam->load();
    const auto fixedVelocity = velocityParam->load() > 0.5f;
    const auto thru = thruParam->load() > 0.5f;

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        const auto position = metadata.samplePosition;

        if (message.isNoteOn())
        {
            const auto ch = message.getChannel() - 1;
            const auto note = message.getNoteNumber();
            const auto pad = note - trigger;

            if (pad >= 0 && pad < ChordParams::numPads)
            {
                padFromKey[ch][note] = (int8_t) (pad + 1);
                padOn (pad, fixedVelocity ? ChordParams::fixedVelocity : message.getVelocity(),
                       message.getChannel(), out, position);
            }
            else if (thru)
            {
                keyPassed[ch][note] = true;
                out.addEvent (message, position);
            }

            continue;
        }

        if (message.isNoteOff())
        {
            const auto ch = message.getChannel() - 1;
            const auto note = message.getNoteNumber();

            if (const auto pad = padFromKey[ch][note] - 1; pad >= 0)
            {
                padFromKey[ch][note] = 0;
                padOff (pad, out, position);
            }
            else if (keyPassed[ch][note])
            {
                keyPassed[ch][note] = false;
                out.addEvent (message, position);
            }

            continue;
        }

        if (message.isAllNotesOff() || message.isAllSoundOff())
            releaseAll (out, position);

        out.addEvent (message, position);
    }

    midi.swapWith (out);
}

//==============================================================================
juce::ValueTree ChordboxProcessor::slotsToTree (const juce::Identifier& type, const juce::Identifier& childType,
                                                int first, int count) const
{
    juce::ValueTree tree (type);

    for (int slot = first; slot < first + count; ++slot)
    {
        juce::ValueTree child (childType);

        if (isFilled (slot))
        {
            const auto chord = getPad (slot);
            child.setProperty (ChordParams::propRoot, chord.root, nullptr);
            child.setProperty (ChordParams::propChord, juce::String (Theory::chordType (chord.type).symbol.c_str()), nullptr);
            child.setProperty (ChordParams::propInversion, chord.inversion, nullptr);
            child.setProperty (ChordParams::propOctave, chord.octave, nullptr);
        }
        else
        {
            child.setProperty (ChordParams::propEmpty, true, nullptr);
        }

        tree.appendChild (child, nullptr);
    }

    return tree;
}

void ChordboxProcessor::slotsFromTree (const juce::ValueTree& tree, int first, int count)
{
    for (int i = 0; i < juce::jmin (count, tree.getNumChildren()); ++i)
    {
        const auto child = tree.getChild (i);
        const auto slot = first + i;
        const auto empty = (bool) child.getProperty (ChordParams::propEmpty, false);

        Theory::PadChord chord;
        chord.root = juce::jlimit (0, 11, (int) child.getProperty (ChordParams::propRoot, 0));
        chord.type = juce::jmax (0, Theory::findChordType (child.getProperty (ChordParams::propChord).toString().toStdString()));
        chord.inversion = juce::jlimit (0, chord.numInversions() - 1, (int) child.getProperty (ChordParams::propInversion, 0));
        chord.octave = juce::jlimit (Theory::PadChord::minOctave, Theory::PadChord::maxOctave,
                                     (int) child.getProperty (ChordParams::propOctave, 0));

        {
            const juce::ScopedLock lock (padLock);
            pads[(size_t) slot] = chord;
            filled[(size_t) slot] = ! empty;
        }

        publish (slot);
    }
}

void ChordboxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // copyState() rather than the tree itself: the tree can still hold a value
    // the host has just changed until the APVTS flushes it.
    auto state = parameters.copyState();

    state.removeChild (state.getChildWithName (ChordParams::padsType), nullptr);
    state.removeChild (state.getChildWithName (ChordParams::tilesType), nullptr);
    state.appendChild (slotsToTree (ChordParams::padsType, ChordParams::padType, 0, ChordParams::numPads), nullptr);
    state.appendChild (slotsToTree (ChordParams::tilesType, ChordParams::tileType,
                                    ChordParams::numPads, ChordParams::numTiles), nullptr);
    state.setProperty (ChordParams::propStyle, style, nullptr);
    state.setProperty (ChordParams::propSelectedPad, selectedPad, nullptr);

    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ChordboxProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (ChordParams::stateTreeType))
        return;

    const auto state = juce::ValueTree::fromXml (*xml);
    parameters.replaceState (state);

    setStyle (state.getProperty (ChordParams::propStyle, 0));
    setSelectedPad (state.getProperty (ChordParams::propSelectedPad, 0));

    if (const auto padsTree = state.getChildWithName (ChordParams::padsType); padsTree.isValid())
        slotsFromTree (padsTree, 0, ChordParams::numPads);

    // A session from before the tiles has none, and one from 0.2 has eight:
    // the tiles it does not mention are emptied, not left holding whatever
    // this instance had.
    for (int tile = ChordParams::numPads; tile < ChordParams::numSlots; ++tile)
        clearSlot (tile);

    if (const auto tilesTree = state.getChildWithName (ChordParams::tilesType); tilesTree.isValid())
        slotsFromTree (tilesTree, ChordParams::numPads, ChordParams::numTiles);

    sendChangeMessage();
}

//==============================================================================
juce::AudioProcessorEditor* ChordboxProcessor::createEditor()
{
    return new ChordboxEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChordboxProcessor();
}
