# ==============================================================================
#
#  Chordbox
#
#  A 16-pad chord generator: a MIDI effect that turns one note into a chord.
#  Pick a key and a scale, let it suggest chords for the pads - triads and
#  sevenths, sus chords, colour chords - and change any pad by hand, down to a
#  Fmaj7sus2 in second inversion.
#
#  Built from Kitbox: the same panel, palette and pads, with the sampler taken
#  out and a small music-theory engine (Source/Theory) put in.
#
#  Two build products come out of this file:
#
#    AU          the classic v2 MIDI processor (aumi), for Logic's MIDI FX slot.
#    Standalone  the development build, with a MIDI output to pick.
#
#  No VST3: Logic does not load VST3, and VST3 has no real MIDI-effect type -
#  other hosts would list it as an instrument that makes no sound.
#
#  No AUv3, deliberately, for the same reason as Kitbox: FX Analyzer shipped
#  one and dragging it to another slot in Logic deadlocked the host through
#  JUCE's callSync in the AUv3 wrapper.
#
#  Two console targets do the verifying. ChordboxCheck tests the theory engine
#  against answers known on paper - Fmaj7sus2 is F G C E, the triads of C major
#  are C Dm Em F G Am Bdim. EditorShot drives the processor with MIDI, checks
#  that nothing ever hangs, round-trips the state and renders the panel to a PNG.
#
# ==============================================================================

cmake_minimum_required(VERSION 3.22)

# Bump with every change to the parameter list: Logic does not re-validate an
# AU whose version is unchanged. 0.2.0: eight tiles above the pads (saved in
# the session), drag and drop between pads and tiles, the scale octave in the
# display. 0.3.0: the tiles became a buffer of sixteen that Suggest fills;
# pads start empty and are filled by dragging; four rows of one height.
# 0.4.0: RND, the humanize switch (a new parameter).
# 0.5.0: four more Suggest styles - Sixths, Jazz, Quartal, Power.
# 0.6.0: Borrowed and Secondary Dominants; chord tones spelt from the root.
# 0.6.1: RND on by default (a parameter default changed).
project(Chordbox VERSION 0.6.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Apple Silicon only, matching the other plugin bundles on this machine.
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "26.5" CACHE STRING "" FORCE)

# JUCE lives outside the project tree rather than as a submodule, because it is
# shared with every other plugin on this machine.
set(JUCE_PATH "$ENV{HOME}/JUCE" CACHE PATH "Path to the JUCE framework")

if(NOT EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found at ${JUCE_PATH}. Pass -DJUCE_PATH=/path/to/JUCE.")
endif()

add_subdirectory("${JUCE_PATH}" JUCE)

# ------------------------------------------------------------------------------
# The plugin
# ------------------------------------------------------------------------------

juce_add_plugin(Chordbox
    # The identity is aumi/Chbx/Tzor. Changing a code after a session has been
    # saved makes a different plugin, and that session loses it.
    COMPANY_NAME                "T'Zorr"
    BUNDLE_ID                   TZorr.Chordbox
    PLUGIN_MANUFACTURER_CODE    Tzor
    PLUGIN_CODE                 Chbx
    FORMATS                     AU Standalone
    PRODUCT_NAME                "Chordbox"
    DESCRIPTION                 "16-pad chord generator"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE    # Scripts/install.sh does this, cache flush and all
    AU_MAIN_TYPE                kAudioUnitType_MIDIProcessor
    HARDENED_RUNTIME_ENABLED    TRUE
    NEEDS_WEB_BROWSER           FALSE
    NEEDS_CURL                  FALSE)

# The theory engine is listed separately so the check target links exactly the
# objects the plugin runs.
set(CHORDBOX_CORE_SOURCES
    Source/Theory/ChordTypes.cpp
    Source/Theory/Scales.cpp
    Source/Theory/PadChord.cpp
    Source/Theory/Suggest.cpp)

set(CHORDBOX_UI_SOURCES
    Source/UI/Selector.cpp
    Source/UI/ChordPad.cpp
    Source/UI/ChordDisplay.cpp)

target_sources(Chordbox PRIVATE
    ${CHORDBOX_CORE_SOURCES}
    ${CHORDBOX_UI_SOURCES}
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(Chordbox PRIVATE Source)

target_compile_definitions(Chordbox PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(Chordbox PRIVATE
    juce::juce_audio_utils
    juce::juce_gui_extra
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# ------------------------------------------------------------------------------
# ChordboxCheck - the theory harness
# ------------------------------------------------------------------------------

juce_add_console_app(ChordboxCheck PRODUCT_NAME "ChordboxCheck")

target_sources(ChordboxCheck PRIVATE
    Verification/ChordboxCheck.cpp
    ${CHORDBOX_CORE_SOURCES})

target_include_directories(ChordboxCheck PRIVATE Source)

target_compile_definitions(ChordboxCheck PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(ChordboxCheck PRIVATE
    juce::juce_core
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# ------------------------------------------------------------------------------
# EditorShot - drives the processor with MIDI and renders the panel to a PNG
# ------------------------------------------------------------------------------

juce_add_console_app(EditorShot PRODUCT_NAME "EditorShot")

target_sources(EditorShot PRIVATE Verification/EditorShot.cpp)

target_compile_definitions(EditorShot PUBLIC
    JUCE_MODAL_LOOPS_PERMITTED=1)

# Linking the plugin's own shared-code library rather than recompiling its
# sources: that library already carries the JucePlugin_* definitions that
# juce_add_plugin generates.
target_link_libraries(EditorShot PRIVATE
    Chordbox
    juce::juce_gui_basics
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

target_include_directories(EditorShot PRIVATE Source)

# Both sides define JUCE_STANDALONE_APPLICATION and the console app's is the
# one that should win.
target_compile_options(EditorShot PRIVATE -Wno-macro-redefined)
