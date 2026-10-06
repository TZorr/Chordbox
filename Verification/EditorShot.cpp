//
//  EditorShot.cpp
//  Chordbox
//
//  Builds the plugin headlessly, plays it with MIDI, checks what comes out,
//  and paints the panel into a PNG.
//
//  The MIDI checks are about the one failure a chord generator must never
//  have - a note left hanging:
//
//  - a pad's chord comes out on the sample, channel and velocity of the key
//    that played it, and its note-offs on the sample of the key's note-off;
//  - a pad re-chorded, re-octaved, or moved to another trigger note while held
//    still releases exactly what it played;
//  - two pads sharing notes release each note only when the last of them lets go;
//  - transport stop and All Notes Off release everything;
//  - Thru passes other notes, and still passes a note-off whose note-on it
//    passed after Thru has been switched off;
//  - clicks on the panel play and release;
//  - tiles: filled by dropping a pad, played by a click but never by a key,
//    copied onto pads, swapped and moved among themselves, kept by Suggest;
//  - the pads, tiles, key, style and selection survive a session round trip.
//
//  The render is the other half: look at the PNG after every layout change.
//
//  Usage:  EditorShot <output-directory>
//

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Theory/ChordTypes.h"

#include <cstdio>
#include <map>

namespace
{
    int failures = 0, checks = 0;

    void check (bool condition, const juce::String& what, const juce::String& detail = {})
    {
        ++checks;

        if (! condition)
        {
            ++failures;
            std::printf ("FAIL: %s  %s\n", what.toRawUTF8(), detail.toRawUTF8());
        }
    }

    constexpr int blockSize = 512;

    struct FakePlayHead : juce::AudioPlayHead
    {
        bool playing = false;

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setIsPlaying (playing);
            return info;
        }
    };

    /** Everything that came out, and which notes are sounding afterwards. */
    struct Output
    {
        juce::Array<juce::MidiMessage> messages;
        juce::Array<int> positions;

        juce::Array<int> notesOn (int at = -1) const
        {
            juce::Array<int> notes;
            for (int i = 0; i < messages.size(); ++i)
                if (messages[i].isNoteOn() && (at < 0 || positions[i] == at))
                    notes.add (messages[i].getNoteNumber());
            return notes;
        }

        juce::Array<int> notesOff (int at = -1) const
        {
            juce::Array<int> notes;
            for (int i = 0; i < messages.size(); ++i)
                if (messages[i].isNoteOff() && (at < 0 || positions[i] == at))
                    notes.add (messages[i].getNoteNumber());
            return notes;
        }

        juce::String describe() const
        {
            juce::StringArray parts;
            for (int i = 0; i < messages.size(); ++i)
                parts.add (juce::String (positions[i]) + ":" + messages[i].getDescription());
            return parts.joinIntoString (", ");
        }
    };

    /** Tracks every note across blocks, the way a synth would hear them. */
    struct Listener
    {
        std::map<std::pair<int, int>, int> sounding;   // (channel, note) -> 1 if on
        int strayOffs = 0;

        void hear (const Output& out)
        {
            for (const auto& message : out.messages)
            {
                const auto id = std::make_pair (message.getChannel(), message.getNoteNumber());

                if (message.isNoteOn())
                    sounding[id] = 1;
                else if (message.isNoteOff())
                {
                    if (sounding[id] == 0)
                        ++strayOffs;
                    sounding[id] = 0;
                }
            }
        }

        int stillSounding() const
        {
            int count = 0;
            for (const auto& entry : sounding)
                count += entry.second;
            return count;
        }
    };

    Output run (ChordboxProcessor& processor, std::initializer_list<std::pair<int, juce::MidiMessage>> events,
                Listener* listener = nullptr)
    {
        juce::AudioBuffer<float> buffer (0, blockSize);
        juce::MidiBuffer midi;

        for (const auto& [position, message] : events)
            midi.addEvent (message, position);

        processor.processBlock (buffer, midi);

        Output out;
        for (const auto metadata : midi)
        {
            out.messages.add (metadata.getMessage());
            out.positions.add (metadata.samplePosition);
        }

        if (listener != nullptr)
            listener->hear (out);

        return out;
    }

    juce::MidiMessage on (int note, int velocity = 100, int channel = 1)
    {
        return juce::MidiMessage::noteOn (channel, note, (juce::uint8) velocity);
    }

    juce::MidiMessage off (int note, int channel = 1)
    {
        return juce::MidiMessage::noteOff (channel, note);
    }

    void setParam (ChordboxProcessor& processor, const char* id, float value)
    {
        auto* parameter = processor.parameters.getParameter (id);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    }

    juce::String list (const juce::Array<int>& notes)
    {
        juce::StringArray parts;
        for (auto n : notes)
            parts.add (juce::String (n));
        return parts.joinIntoString (" ");
    }

    Theory::PadChord chord (int root, const char* symbol, int inversion = 0, int octave = 0)
    {
        return { root, Theory::findChordType (symbol), inversion, octave };
    }

    std::unique_ptr<ChordboxProcessor> fresh()
    {
        auto processor = std::make_unique<ChordboxProcessor>();
        processor->prepareToPlay (48000.0, blockSize);
        run (*processor, {});   // the release that prepareToPlay schedules
        return processor;
    }

    //==============================================================================
    void checkChordsOut()
    {
        auto processor = fresh();
        Listener listener;

        // Pad 1 in C major is C: C3 E3 G3, at the key's sample and velocity.
        auto out = run (*processor, { { 100, on (36, 90, 3) } }, &listener);
        check (list (out.notesOn (100)) == "60 64 67", "pad 1 plays C3 E3 G3 at the key's sample", out.describe());
        check (out.messages.size() == 3 && out.messages[0].getChannel() == 3 && out.messages[0].getVelocity() == 90,
               "on the key's channel and velocity", out.describe());

        out = run (*processor, { { 300, off (36, 3) } }, &listener);
        check (list (out.notesOff (300)) == "60 64 67", "its note-off releases them at the key's sample", out.describe());
        check (listener.stillSounding() == 0 && listener.strayOffs == 0, "nothing left sounding");

        // Fixed velocity.
        setParam (*processor, ChordParams::velocity, 1.0f);
        out = run (*processor, { { 0, on (37, 20) }, { 10, off (37) } }, &listener);
        check (out.messages.size() == 6 && out.messages[0].getVelocity() == ChordParams::fixedVelocity,
               "Fixed velocity plays at 100", out.describe());
    }

    void checkNothingHangs()
    {
        auto processor = fresh();
        Listener listener;

        // Re-chorded while held: the note-off releases the C it played, not the Dm.
        run (*processor, { { 0, on (36) } }, &listener);
        processor->setPad (0, chord (2, "m"));
        auto out = run (*processor, { { 0, off (36) } }, &listener);
        check (list (out.notesOff()) == "60 64 67", "a pad re-chorded while held releases its old chord", out.describe());

        // Chord Octave moved while held.
        run (*processor, { { 0, on (37) } }, &listener);   // Dm: 62 65 69
        setParam (*processor, ChordParams::chordOctave, 4.0f);
        out = run (*processor, { { 0, off (37) } }, &listener);
        check (list (out.notesOff()) == "62 65 69", "a pad whose octave moved while held releases the old notes", out.describe());
        setParam (*processor, ChordParams::chordOctave, 3.0f);

        // Trigger Note moved while held: the key still releases its pad.
        run (*processor, { { 0, on (36) } }, &listener);
        setParam (*processor, ChordParams::triggerNote, 48.0f);
        out = run (*processor, { { 0, off (36) } }, &listener);
        check (list (out.notesOff()) == "62 65 69", "a key releases its pad after Trigger Note moved", out.describe());
        setParam (*processor, ChordParams::triggerNote, 36.0f);

        // Two pads sharing C and E: C3 E3 G3 and A2 C3 E3.
        processor->setPad (0, chord (0, ""));
        processor->setPad (1, chord (9, "m", 0, -1));
        run (*processor, { { 0, on (36) } }, &listener);
        out = run (*processor, { { 0, on (37) } }, &listener);
        check (list (out.notesOff()) == "60 64" && list (out.notesOn()) == "57 60 64",
               "a shared note is struck again by the second pad", out.describe());

        out = run (*processor, { { 0, off (36) } }, &listener);
        check (list (out.notesOff()) == "67", "letting go of the first pad keeps the shared notes", out.describe());

        out = run (*processor, { { 0, off (37) } }, &listener);
        check (list (out.notesOff()) == "57 60 64", "the last pad releases them", out.describe());
        check (listener.stillSounding() == 0, "nothing left sounding after shared notes");

        // Transport stop.
        FakePlayHead playHead;
        processor->setPlayHead (&playHead);
        playHead.playing = true;
        run (*processor, { { 0, on (36) }, { 0, on (40) } }, &listener);
        playHead.playing = false;
        out = run (*processor, {}, &listener);
        check (listener.stillSounding() == 0, "transport stop releases every pad", out.describe());
        run (*processor, { { 0, off (36) }, { 0, off (40) } }, &listener);
        processor->setPlayHead (nullptr);

        // All Notes Off.
        run (*processor, { { 0, on (38) } }, &listener);
        out = run (*processor, { { 5, juce::MidiMessage::allNotesOff (1) } }, &listener);
        check (listener.stillSounding() == 0, "All Notes Off releases every pad", out.describe());
        check (out.messages.getLast().isAllNotesOff(), "and is passed on");

        check (listener.strayOffs == 0, "no note-off for a note that was not on");
    }

    void checkThruAndPanel()
    {
        auto processor = fresh();
        Listener listener;

        auto out = run (*processor, { { 7, on (80) } }, &listener);
        check (list (out.notesOn (7)) == "80" && out.messages.size() == 1, "Thru passes a note outside the pads", out.describe());

        setParam (*processor, ChordParams::thru, 0.0f);
        out = run (*processor, { { 0, off (80) }, { 1, on (81) } }, &listener);
        check (list (out.notesOff()) == "80" && out.notesOn().isEmpty(),
               "with Thru off, a passed note still gets its note-off, a new one is held back", out.describe());

        out = run (*processor, { { 0, off (81) } }, &listener);
        check (out.messages.isEmpty(), "and the held-back note's note-off is too", out.describe());

        // A CC goes through whatever Thru says.
        out = run (*processor, { { 0, juce::MidiMessage::controllerEvent (1, 1, 64) } });
        check (out.messages.size() == 1 && out.messages[0].isController(), "controllers pass through");

        // The panel: a click plays at the start of the next block, on channel 1.
        processor->padDown (4, 1.0f);   // G: G3 B3 D4
        out = run (*processor, {}, &listener);
        check (list (out.notesOn (0)) == "67 71 74" && out.messages[0].getVelocity() == 127,
               "a click plays the pad", out.describe());
        check (processor->isPadHeld (4) && processor->getLastPlayedPad() == 4, "and shows it held");

        processor->padUp (4);
        out = run (*processor, {}, &listener);
        check (list (out.notesOff (0)) == "67 71 74" && ! processor->isPadHeld (4), "releasing the click releases it",
               out.describe());

        // Down and up within one block: played and released, in that order.
        processor->padDown (0, 0.5f);
        processor->padUp (0);
        out = run (*processor, {}, &listener);
        check (out.notesOn().size() == 3 && out.notesOff().size() == 3 && out.messages[0].isNoteOn(),
               "a click shorter than a block still plays and releases", out.describe());

        check (listener.stillSounding() == 0, "nothing left sounding after the panel");
    }

    void checkTiles()
    {
        auto processor = fresh();
        Listener listener;
        constexpr int tileA = ChordParams::numPads, tileB = ChordParams::numPads + 1;

        check (! processor->isFilled (tileA) && processor->getPadName (tileA).isEmpty(), "a tile starts empty");
        processor->padDown (tileA, 1.0f);
        auto out = run (*processor, {}, &listener);
        check (out.messages.isEmpty(), "an empty tile plays nothing", out.describe());
        processor->padUp (tileA);
        run (*processor, {}, &listener);

        // Pad 4 (F) dropped on tile A: copied, the pad keeps it.
        processor->dropSlot (3, tileA);
        check (processor->getPadName (tileA) == "F" && processor->getPadName (3) == "F", "a pad dropped on a tile copies");

        processor->padDown (tileA, 0.8f);
        out = run (*processor, {}, &listener);
        check (list (out.notesOn (0)) == "65 69 72", "a click on a tile plays its chord", out.describe());
        processor->padUp (tileA);
        out = run (*processor, {}, &listener);
        check (list (out.notesOff (0)) == "65 69 72", "and releases it", out.describe());

        // Keys play pads only: the note after pad 16 is not tile A.
        out = run (*processor, { { 0, on (ChordParams::defaultTrigger + ChordParams::numPads) } }, &listener);
        check (list (out.notesOn()) == juce::String (ChordParams::defaultTrigger + ChordParams::numPads),
               "a key never plays a tile", out.describe());
        run (*processor, { { 0, off (ChordParams::defaultTrigger + ChordParams::numPads) } }, &listener);

        // Tile onto pads: copied, and can be copied again.
        processor->dropSlot (tileA, 0);
        processor->dropSlot (tileA, 9);
        check (processor->getPadName (0) == "F" && processor->getPadName (9) == "F" && processor->isFilled (tileA),
               "a tile dropped on pads copies, and stays filled");

        // Pad onto pad: swapped.
        processor->dropSlot (0, 1);
        check (processor->getPadName (0) == "Dm" && processor->getPadName (1) == "F", "a pad dropped on a pad swaps");

        // Tile onto an empty tile: moved. An empty tile dragged nowhere does nothing.
        processor->dropSlot (tileA, tileB);
        check (! processor->isFilled (tileA) && processor->getPadName (tileB) == "F", "a tile dropped on an empty tile moves");
        processor->dropSlot (tileA, 5);
        check (processor->getPadName (5) == "Am", "an empty tile drops nothing");

        // Suggest refills pads, not tiles.
        processor->suggestPads();
        check (processor->getPadName (1) == "Dm" && processor->getPadName (tileB) == "F", "Suggest keeps the tiles");

        processor->clearTile (tileB);
        check (! processor->isFilled (tileB), "Clear Tile empties it");
        processor->clearTile (3);
        check (processor->isFilled (3), "a pad cannot be cleared");

        check (listener.stillSounding() == 0 && listener.strayOffs == 0, "nothing left sounding after the tiles");
    }

    void checkStateRoundTrip()
    {
        auto original = fresh();
        setParam (*original, ChordParams::key, 5.0f);       // F
        setParam (*original, ChordParams::scale, 2.0f);     // Dorian
        setParam (*original, ChordParams::chordOctave, 2.0f);
        setParam (*original, ChordParams::triggerNote, 48.0f);
        original->setStyle (2);
        original->setPad (3, chord (5, "maj7sus2", 2, -1));
        original->setPad (15, chord (11, "13", 3, 2));
        original->setSelectedPad (9);
        original->setPad (ChordParams::numPads + 2, chord (2, "m9", 1, 1));   // tile C filled, the others empty

        juce::MemoryBlock state;
        original->getStateInformation (state);

        auto restored = fresh();
        restored->setStateInformation (state.getData(), (int) state.getSize());

        bool samePads = true;
        for (int pad = 0; pad < ChordParams::numPads; ++pad)
            samePads = samePads && restored->getPad (pad) == original->getPad (pad);

        check (samePads, "every pad survives the round trip");

        bool sameTiles = true;
        for (int slot = ChordParams::numPads; slot < ChordParams::numSlots; ++slot)
            sameTiles = sameTiles && restored->isFilled (slot) == original->isFilled (slot)
                     && (! original->isFilled (slot) || restored->getPad (slot) == original->getPad (slot));
        check (sameTiles && restored->isFilled (ChordParams::numPads + 2), "the tiles survive, empty ones too");

        // A session saved before tiles existed restores with them empty.
        {
            auto tree = juce::ValueTree::fromXml (*juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize()));
            tree.removeChild (tree.getChildWithName (ChordParams::tilesType), nullptr);
            juce::MemoryBlock old;
            juce::AudioProcessor::copyXmlToBinary (*tree.createXml(), old);

            auto older = fresh();
            older->dropSlot (0, ChordParams::numPads);
            older->setStateInformation (old.getData(), (int) old.getSize());
            check (! older->isFilled (ChordParams::numPads), "an old session's tiles come back empty");
        }
        check (restored->getKey() == 5 && restored->getScale() == 2 && restored->getOctaveBase() == 48
                   && restored->getTriggerNote() == 48,
               "key, scale, octave and trigger survive");
        check (restored->getStyle() == 2 && restored->getSelectedPad() == 9, "style and selected pad survive");
        check (restored->getPadName (3) == "Fmaj7sus2/C", "and the pad reads Fmaj7sus2/C", restored->getPadName (3));

        // And the restored pads play what they say.
        auto out = run (*restored, { { 0, on (48 + 15) } });
        check (list (out.notesOn()) == list (restored->getPadNotes (15)), "a restored pad plays its notes", out.describe());
    }

    //==============================================================================
    bool writePng (const juce::Image& image, const juce::File& file)
    {
        // Deleted first: createOutputStream appends to an existing file, and a
        // PNG decoder stops at the first image - the old one.
        file.deleteFile();
        std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
        juce::PNGImageFormat png;
        return stream != nullptr && png.writeImageToStream (image, *stream);
    }

    void pump (int ms)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (ms);
    }
}

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const juce::File outputDirectory = argc > 1 ? juce::File (juce::String (argv[1]))
                                                : juce::File::getCurrentWorkingDirectory();

    if (! outputDirectory.createDirectory())
    {
        std::printf ("FAIL: cannot create %s\n", outputDirectory.getFullPathName().toRawUTF8());
        return 1;
    }

    std::printf ("Chords out...\n");
    checkChordsOut();
    std::printf ("Nothing hangs...\n");
    checkNothingHangs();
    std::printf ("Thru and the panel...\n");
    checkThruAndPanel();
    std::printf ("Tiles...\n");
    checkTiles();
    std::printf ("Session round trip...\n");
    checkStateRoundTrip();

    // The panel: C major, the IV turned into the Fmaj7sus2 from the request,
    // held, so the pad and the display's keyboards are lit.
    {
        auto processor = fresh();
        processor->setPad (11, chord (5, "maj7sus2"));
        processor->setSelectedPad (3);

        // Three tiles filled, the rest showing what they are for.
        processor->setPad (ChordParams::numPads + 0, chord (5, "maj7sus2"));
        processor->setPad (ChordParams::numPads + 1, chord (9, "m9"));
        processor->setPad (ChordParams::numPads + 3, chord (7, "7sus4", 1));

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditorAndMakeActive());
        pump (50);

        run (*processor, { { 0, on (36 + 11, 110) } });
        pump (60);

        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        const auto file = outputDirectory.getChildFile ("chordbox.png");
        check (writePng (image, file), "the panel renders to " + file.getFullPathName());

        processor->editorBeingDeleted (editor.get());
        editor.reset();
    }

    std::printf ("\n%d checks, %d failure(s)\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
