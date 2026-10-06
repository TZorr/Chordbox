//
//  PluginProcessor.h
//  Chordbox
//
//  A MIDI effect: notes in, chords out. No audio.
//
//  Sixteen consecutive notes, from Trigger Note up, play the sixteen pads;
//  each pad sends the notes of its chord, on the channel and at the sample
//  position of the note that played it. Everything else passes through, and
//  notes outside the pad range do too while Thru is on.
//
//  Three rules keep notes from hanging, which is the one thing a chord
//  generator must never do:
//
//  - A pad remembers the notes it actually sent. Its note-off releases those,
//    even if the pad was given another chord, or the Chord Octave moved, while
//    it was held.
//  - The key that played a pad is remembered too. Moving Trigger Note while a
//    key is down still releases the pad that key started.
//  - Every outgoing note is counted. Two pads sharing a note (C and Am share C
//    and E) retrigger it, and the note stops only when the last pad holding it
//    lets go.
//
//  Transport stop and All Notes Off release every pad.
//
//  Above the sixteen pads sits a buffer of sixteen tiles. Suggest fills the
//  buffer, never the pads; chords reach the pads by being dragged there, so
//  the pads hold only what was chosen for them. A tile plays when clicked, so
//  it can be auditioned, but never from a key. Pads and tiles share one
//  numbering, "slots": 0-15 the pads, 16-31 the tiles. Either may be empty; an
//  empty pad's key plays nothing.
//
//  The pads themselves live on the message thread (setPad, suggestTiles) and
//  reach the audio thread as one packed atomic word per pad - see PadChord.h.
//  Clicks on the panel come in through a lock-free FIFO and play at the start
//  of the next block.
//

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterIds.h"
#include "Theory/PadChord.h"
#include "Theory/Suggest.h"

class ChordboxProcessor : public juce::AudioProcessor,
                          public juce::ChangeBroadcaster
{
public:
    ChordboxProcessor();
    ~ChordboxProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }

    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    //  Message thread
    juce::AudioProcessorValueTreeState parameters;

    Theory::PadChord getPad (int slot) const;
    void setPad (int slot, const Theory::PadChord&);

    bool isFilled (int slot) const;
    void clearSlot (int slot);

    /** Drag and drop: a pad dropped on a pad, or a tile on a tile, swaps the
        two; across the two kinds it copies. */
    void dropSlot (int source, int target);

    /** Refills the sixteen buffer tiles from the current key, scale and
        style. The pads are left alone. */
    void suggestTiles();

    int getStyle() const noexcept { return style; }
    void setStyle (int newStyle);

    int getSelectedPad() const noexcept { return selectedPad; }
    void setSelectedPad (int pad) { selectedPad = juce::jlimit (0, ChordParams::numPads - 1, pad); }

    int getKey() const;
    int getScale() const;
    int getTriggerNote() const;
    int getOctaveBase() const;

    /** "Fmaj7sus2", spelt in the current key; empty for an empty slot. */
    juce::String getPadName (int slot) const;
    /** "IV" - empty if the root is not in the key. */
    juce::String getPadNumeral (int slot) const;
    /** The MIDI notes the slot sends now. */
    juce::Array<int> getPadNotes (int slot) const;
    /** "F3 G3 C4 E4", spelt in the current key. */
    juce::String getNotesText (const juce::Array<int>& notes) const;

    /** A click on a pad or a tile. Velocity 0..1. */
    void padDown (int slot, float velocity);
    void padUp (int slot);

    //==============================================================================
    //  Any thread: what the panel draws
    bool isPadHeld (int pad) const noexcept     { return held[(size_t) pad].load(); }
    int getHitCount (int pad) const noexcept    { return hits[(size_t) pad].load(); }
    float getLastVelocity (int pad) const noexcept { return lastVelocity[(size_t) pad].load(); }
    int getLastPlayedPad() const noexcept       { return lastPlayedPad.load(); }
    int getPlayCount() const noexcept           { return playCount.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameterLayout();

    void publish (int pad);
    juce::ValueTree slotsToTree (const juce::Identifier& type, const juce::Identifier& childType,
                                 int first, int count) const;
    void slotsFromTree (const juce::ValueTree&, int first, int count);

    // Audio thread
    void padOn (int pad, int velocity, int channel, juce::MidiBuffer& out, int position);
    void padOff (int pad, juce::MidiBuffer& out, int position);
    void releaseAll (juce::MidiBuffer& out, int position);

    juce::CriticalSection padLock;
    std::array<Theory::PadChord, ChordParams::numSlots> pads;
    std::array<bool, ChordParams::numSlots> filled {};
    int style = 0;
    int selectedPad = 0;

    std::array<std::atomic<uint64_t>, ChordParams::numSlots> packedPads {};

    struct PanelEvent { int pad; float velocity; bool on; };
    juce::AbstractFifo panelFifo { 64 };
    std::array<PanelEvent, 64> panelEvents {};

    struct Sounding
    {
        bool on = false;
        int channel = 1;
        int count = 0;
        int notes[Theory::maxPackedNotes] {};
    };

    std::array<Sounding, ChordParams::numSlots> sounding {};
    int noteRefs[16][128] {};      // per channel and note: how many pads hold it
    int8_t padFromKey[16][128] {};  // per incoming channel and note: pad + 1, or 0
    bool keyPassed[16][128] {};     // the incoming note-on went through as Thru
    bool wasPlaying = false;
    std::atomic<bool> releasePending { false };

    std::array<std::atomic<bool>, ChordParams::numSlots> held {};
    std::array<std::atomic<int>, ChordParams::numSlots> hits {};
    std::array<std::atomic<float>, ChordParams::numSlots> lastVelocity {};
    std::atomic<int> lastPlayedPad { 0 }, playCount { 0 };

    std::atomic<float>* keyParam = nullptr;
    std::atomic<float>* scaleParam = nullptr;
    std::atomic<float>* triggerParam = nullptr;
    std::atomic<float>* octaveParam = nullptr;
    std::atomic<float>* velocityParam = nullptr;
    std::atomic<float>* thruParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordboxProcessor)
};
