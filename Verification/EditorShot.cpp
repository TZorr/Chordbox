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
//  - the buffer: a new instance has it filled and the pads empty; Suggest
//    fills it and never touches the pads; its tiles play on a click but never
//    from a key, copy onto pads, swap and move among themselves;
//  - an empty pad plays nothing, and its key does not pass through;
//  - RND: velocities within +-12 %, delays within 0-16 ms and centred on 8,
//    each note its own - and still no note-off before its note-on, no note
//    left hanging, shared notes, transport stop and All Notes Off included;
//  - the pads, buffer, key, style and selection survive a session round trip,
//    and sessions from 0.1 and 0.2 load.
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

    /** A new instance. Most checks want something on the pads: with
        loadPads, the buffer's C major is copied onto them, tile by tile, so
        pad 1 is C, pad 9 Cmaj7 and so on. */
    std::unique_ptr<ChordboxProcessor> fresh (bool loadPads = true)
    {
        auto processor = std::make_unique<ChordboxProcessor>();
        processor->prepareToPlay (48000.0, blockSize);
        run (*processor, {});   // the release that prepareToPlay schedules

        if (loadPads)
            for (int pad = 0; pad < ChordParams::numPads; ++pad)
                processor->dropSlot (ChordParams::numPads + pad, pad);

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

    void checkBuffer()
    {
        constexpr int tileA = ChordParams::numPads, tileB = ChordParams::numPads + 1;
        Listener listener;

        // A new instance: C major in the buffer, nothing on the pads.
        {
            auto blank = fresh (false);

            bool padsEmpty = true;
            for (int pad = 0; pad < ChordParams::numPads; ++pad)
                padsEmpty = padsEmpty && ! blank->isFilled (pad);

            check (padsEmpty, "a new instance starts with empty pads");
            check (blank->getPadName (tileA) == "C" && blank->getPadName (tileA + 8) == "Cmaj7"
                       && blank->getPadName (tileA + 15) == "Cmaj7",
                   "and C major in the buffer");

            auto out = run (*blank, { { 0, on (36) }, { 10, off (36) } }, &listener);
            check (out.messages.isEmpty(), "an empty pad's key plays nothing and does not pass through", out.describe());

            blank->padDown (0, 1.0f);
            blank->padUp (0);
            out = run (*blank, {}, &listener);
            check (out.messages.isEmpty(), "a click on an empty pad plays nothing", out.describe());

            setParam (*blank, ChordParams::key, 2.0f);   // D major
            blank->suggestTiles();
            check (blank->getPadName (tileA) == "D" && ! blank->isFilled (0), "Suggest fills the buffer, not the pads");

            blank->dropSlot (tileA, 0);
            out = run (*blank, { { 0, on (36) } }, &listener);
            check (blank->getPadName (0) == "D" && list (out.notesOn()) == "62 66 69",
                   "a tile dropped on an empty pad fills it, and the key plays it", out.describe());
            run (*blank, { { 0, off (36) } }, &listener);
        }

        auto processor = fresh();

        processor->clearSlot (tileA);
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

        // Tile onto tile: swapped; onto an empty tile: moved. An empty tile drops nothing.
        processor->dropSlot (tileA, tileB);
        check (processor->getPadName (tileA) == "Dm" && processor->getPadName (tileB) == "F", "a tile dropped on a tile swaps");
        processor->clearSlot (tileA);
        processor->dropSlot (tileB, tileA);
        check (processor->getPadName (tileA) == "F" && ! processor->isFilled (tileB), "a tile dropped on an empty tile moves");
        processor->dropSlot (tileB, 5);
        check (processor->getPadName (5) == "Am", "an empty tile drops nothing");

        // Suggest refills the buffer and leaves the pads.
        processor->suggestTiles();
        check (processor->getPadName (1) == "F" && processor->getPadName (tileA) == "C"
                   && processor->getPadName (tileB) == "Dm",
               "Suggest refills the buffer and keeps the pads");

        // Clear Pad.
        processor->clearSlot (3);
        out = run (*processor, { { 0, on (39) }, { 5, off (39) } }, &listener);
        check (! processor->isFilled (3) && out.messages.isEmpty(), "a cleared pad is silent", out.describe());

        check (listener.stillSounding() == 0 && listener.strayOffs == 0, "nothing left sounding after the buffer");
    }

    void checkHumanize()
    {
        auto processor = fresh();   // 48 kHz: 16 ms is 768 samples
        setParam (*processor, ChordParams::humanize, 1.0f);
        processor->setPad (1, chord (9, "m", 0, -1));   // Am below C: shares C and E with pad 1

        Listener listener;
        juce::int64 clock = 0;
        std::map<int, juce::int64> onAt;
        juce::Array<int> velocities;
        juce::Array<double> delays;
        int spreadChords = 0;
        bool offBeforeOn = false;

        // One block, with every output time made absolute.
        const auto step = [&] (std::initializer_list<std::pair<int, juce::MidiMessage>> events, juce::int64 played,
                               juce::Array<juce::int64>* chordDelays)
        {
            const auto out = run (*processor, events, &listener);

            for (int i = 0; i < out.messages.size(); ++i)
            {
                const auto& m = out.messages[i];
                const auto at = clock + out.positions[i];

                if (m.isNoteOn())
                {
                    velocities.add (m.getVelocity());
                    if (played >= 0)
                    {
                        delays.add ((double) (at - played) * 1000.0 / 48000.0);
                        if (chordDelays != nullptr)
                            chordDelays->addIfNotAlreadyThere (at - played);
                    }
                    onAt[m.getNoteNumber()] = at;
                }
                else if (m.isNoteOff() && onAt.count (m.getNoteNumber()) > 0 && at < onAt[m.getNoteNumber()])
                {
                    offBeforeOn = true;
                }
            }

            clock += blockSize;
        };

        for (int hit = 0; hit < 200; ++hit)
        {
            juce::Array<juce::int64> chordDelays;
            const auto played = clock + 100;
            step ({ { 100, on (36, 100) } }, played, &chordDelays);
            step ({ { 0, off (36) } }, played, &chordDelays);
            step ({}, played, &chordDelays);
            step ({}, played, &chordDelays);

            if (chordDelays.size() > 1)
                ++spreadChords;
        }

        int low = 127, high = 0;
        bool velocitiesInRange = true;
        for (auto v : velocities)
        {
            low = juce::jmin (low, v);
            high = juce::jmax (high, v);
            velocitiesInRange = velocitiesInRange && v >= 88 && v <= 112;
        }

        double sum = 0.0, earliest = 1.0e9, latest = -1.0e9;
        for (auto d : delays)
        {
            sum += d;
            earliest = juce::jmin (earliest, d);
            latest = juce::jmax (latest, d);
        }
        const auto mean = delays.isEmpty() ? 0.0 : sum / delays.size();

        check (velocities.size() == 600, "RND: every note of every chord plays", juce::String (velocities.size()));
        check (velocitiesInRange, "RND: velocity stays within 100 +-12 %",
               juce::String (low) + ".." + juce::String (high));
        check (low <= 94 && high >= 106, "RND: and uses that range", juce::String (low) + ".." + juce::String (high));
        check (earliest >= 0.0 && latest <= 16.0 + 0.05, "RND: delays stay within 0-16 ms",
               juce::String (earliest, 2) + ".." + juce::String (latest, 2));
        check (std::abs (mean - 8.0) < 1.5, "RND: delays centre on 8 ms", juce::String (mean, 2));
        check (spreadChords > 150, "RND: each note of a chord gets its own delay", juce::String (spreadChords));
        check (! offBeforeOn, "RND: no note-off before its note-on");
        check (listener.stillSounding() == 0 && listener.strayOffs == 0, "RND: nothing left sounding");

        // Shared notes, quickly overlapping, many times.
        for (int round = 0; round < 50; ++round)
        {
            step ({ { 0, on (36) }, { 10, on (37) }, { 20, off (36) }, { 30, off (37) } }, -1, nullptr);
            step ({}, -1, nullptr);
            step ({}, -1, nullptr);
        }
        check (listener.stillSounding() == 0 && ! offBeforeOn, "RND: shared notes leave nothing hanging");

        // All Notes Off and transport stop while chords are still being delayed.
        step ({ { 0, on (36) }, { 1, on (38) }, { 2, juce::MidiMessage::allNotesOff (1) } }, -1, nullptr);
        step ({}, -1, nullptr);
        step ({}, -1, nullptr);
        check (listener.stillSounding() == 0, "RND: All Notes Off releases delayed chords");
        step ({ { 0, off (36) }, { 0, off (38) } }, -1, nullptr);

        FakePlayHead playHead;
        processor->setPlayHead (&playHead);
        playHead.playing = true;
        step ({ { 500, on (40) } }, -1, nullptr);
        playHead.playing = false;
        step ({}, -1, nullptr);
        step ({}, -1, nullptr);
        step ({}, -1, nullptr);
        check (listener.stillSounding() == 0, "RND: transport stop releases delayed chords");
        step ({ { 0, off (40) } }, -1, nullptr);
        processor->setPlayHead (nullptr);

        // Off again: exact, as before.
        setParam (*processor, ChordParams::humanize, 0.0f);
        const auto out = run (*processor, { { 100, on (36, 100) } }, &listener);
        check (list (out.notesOn (100)) == "60 64 67" && out.messages[0].getVelocity() == 100,
               "RND off: exact again", out.describe());
        run (*processor, { { 0, off (36) } }, &listener);
        check (listener.stillSounding() == 0 && listener.strayOffs == 0, "nothing left sounding after RND");
    }

    void checkStateRoundTrip()
    {
        auto original = fresh();
        setParam (*original, ChordParams::key, 5.0f);       // F
        setParam (*original, ChordParams::scale, 2.0f);     // Dorian
        setParam (*original, ChordParams::chordOctave, 2.0f);
        setParam (*original, ChordParams::triggerNote, 48.0f);
        original->setStyle (2);
        setParam (*original, ChordParams::humanize, 1.0f);
        original->setPad (3, chord (5, "maj7sus2", 2, -1));
        original->setPad (15, chord (11, "13", 3, 2));
        original->setSelectedPad (9);
        original->clearSlot (7);                                               // an empty pad
        original->setPad (ChordParams::numPads + 2, chord (2, "m9", 1, 1));   // tile C changed
        original->clearSlot (ChordParams::numPads + 5);                        // tile F empty

        juce::MemoryBlock state;
        original->getStateInformation (state);

        auto restored = fresh();
        restored->setStateInformation (state.getData(), (int) state.getSize());

        const auto same = [&] (int first, int last)
        {
            for (int slot = first; slot < last; ++slot)
                if (restored->isFilled (slot) != original->isFilled (slot)
                    || (original->isFilled (slot) && ! (restored->getPad (slot) == original->getPad (slot))))
                    return false;
            return true;
        };

        check (same (0, ChordParams::numPads) && ! restored->isFilled (7), "every pad survives the round trip, empty ones too");
        check (same (ChordParams::numPads, ChordParams::numSlots) && ! restored->isFilled (ChordParams::numPads + 5),
               "the buffer survives, empty tiles too");

        // A session from 0.2 had eight tiles: they load into A-H, I-P stay empty.
        {
            auto tree = juce::ValueTree::fromXml (*juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize()));
            auto tiles = tree.getChildWithName (ChordParams::tilesType);
            while (tiles.getNumChildren() > 8)
                tiles.removeChild (8, nullptr);

            juce::MemoryBlock old;
            juce::AudioProcessor::copyXmlToBinary (*tree.createXml(), old);

            auto older = fresh();
            older->setStateInformation (old.getData(), (int) old.getSize());
            check (older->getPad (ChordParams::numPads + 2) == original->getPad (ChordParams::numPads + 2)
                       && ! older->isFilled (ChordParams::numPads + 8) && ! older->isFilled (ChordParams::numSlots - 1),
                   "a 0.2 session's eight tiles load into A-H");
        }

        // A session from 0.1, before tiles: the buffer comes back empty, not
        // holding this instance's suggestion.
        {
            auto tree = juce::ValueTree::fromXml (*juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize()));
            tree.removeChild (tree.getChildWithName (ChordParams::tilesType), nullptr);
            juce::MemoryBlock old;
            juce::AudioProcessor::copyXmlToBinary (*tree.createXml(), old);

            auto older = fresh();
            older->setStateInformation (old.getData(), (int) old.getSize());
            check (! older->isFilled (ChordParams::numPads), "an old session's tiles come back empty");
        }
        check (restored->getKey() == 5 && restored->getScale() == 2 && restored->getOctaveBase() == 48
                   && restored->getTriggerNote() == 48,
               "key, scale, octave and trigger survive");
        check (restored->getStyle() == 2 && restored->getSelectedPad() == 9, "style and selected pad survive");
        check (restored->parameters.getRawParameterValue (ChordParams::humanize)->load() > 0.5f, "RND survives");
        check (restored->getPadName (3) == "Fmaj7sus2/C", "and the pad reads Fmaj7sus2/C", restored->getPadName (3));

        // And the restored pads play what they say (exactly: RND off for this).
        setParam (*restored, ChordParams::humanize, 0.0f);
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
    std::printf ("Buffer...\n");
    checkBuffer();
    std::printf ("RND...\n");
    checkHumanize();
    std::printf ("Session round trip...\n");
    checkStateRoundTrip();

    // The panel: C major (Borrowed) suggested into the buffer, a progression built on
    // the pads from it - with the Fmaj7sus2 from the request, held, so the
    // pad and the display's keyboards are lit - and the rest of the pads
    // still empty.
    {
        auto processor = fresh (false);
        const auto tile = [] (int letter) { return ChordParams::numPads + letter; };

        // Borrowed: C major's triads over C minor's.
        for (int style = 0; style < Theory::numStyles(); ++style)
            if (juce::String (Theory::styleName (style)) == "Borrowed")
                processor->setStyle (style);
        processor->suggestTiles();

        processor->dropSlot (tile (0), 0);    // C
        processor->dropSlot (tile (5), 1);    // Am
        processor->dropSlot (tile (3), 2);    // F
        processor->dropSlot (tile (4), 3);    // G
        processor->setPad (4, chord (5, "maj7sus2"));
        processor->dropSlot (tile (13), 8);   // Ab, borrowed
        processor->dropSlot (tile (14), 9);   // Bb, borrowed
        processor->dropSlot (tile (11), 10);  // Fm, borrowed
        processor->setSelectedPad (4);
        setParam (*processor, ChordParams::humanize, 1.0f);

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditorAndMakeActive());
        pump (50);

        run (*processor, { { 0, on (36 + 4, 110) } });
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
