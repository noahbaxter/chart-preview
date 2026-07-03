cmake_minimum_required(VERSION 3.22)
project(ChartchoticBenchmark VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE (submodule in repo)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/JUCE ${CMAKE_BINARY_DIR}/JUCE)

# Source root for project headers
set(SOURCE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../Chartchotic/Source)
set(ASSET_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../Chartchotic/Assets)

juce_add_console_app(bench_rendering PRODUCT_NAME "Chartchotic Benchmark")

target_sources(bench_rendering PRIVATE
    bench_main.cpp
    ${SOURCE_ROOT}/Visual/Renderers/SceneRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/AnimationRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/NoteRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/SustainRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/GridlineRenderer.cpp
    ${SOURCE_ROOT}/Visual/Art/TrackRenderer.cpp
    ${SOURCE_ROOT}/Visual/Managers/AssetManager.cpp
    ${SOURCE_ROOT}/Visual/Geometry/PositionMath.cpp
    ${SOURCE_ROOT}/Midi/Processing/MidiInterpreter.cpp
    ${SOURCE_ROOT}/Midi/Utils/ChordAnalyzer.cpp
    ${SOURCE_ROOT}/Midi/Utils/GemCalculator.cpp
    ${SOURCE_ROOT}/Midi/Utils/LaneDetector.cpp
)

target_include_directories(bench_rendering PRIVATE
    ${SOURCE_ROOT}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

juce_generate_juce_header(bench_rendering)

# Binary data — same assets as main plugin
juce_add_binary_data(BenchmarkData
    NAMESPACE BinaryData
    SOURCES
        ${ASSET_ROOT}/background.png
        ${ASSET_ROOT}/background_red.jpg
        ${ASSET_ROOT}/bar_kick.png
        ${ASSET_ROOT}/bar_kick_2x.png
        ${ASSET_ROOT}/bar_open.png
        ${ASSET_ROOT}/bar_white.png
        ${ASSET_ROOT}/cym_blue.png
        ${ASSET_ROOT}/cym_green.png
        ${ASSET_ROOT}/cym_red.png
        ${ASSET_ROOT}/cym_white.png
        ${ASSET_ROOT}/cym_yellow.png
        ${ASSET_ROOT}/hopo_blue.png
        ${ASSET_ROOT}/hopo_green.png
        ${ASSET_ROOT}/hopo_orange.png
        ${ASSET_ROOT}/hopo_red.png
        ${ASSET_ROOT}/hopo_white.png
        ${ASSET_ROOT}/hopo_yellow.png
        ${ASSET_ROOT}/lane_end.png
        ${ASSET_ROOT}/lane_mid.png
        ${ASSET_ROOT}/lane_start.png
        ${ASSET_ROOT}/marker_beat.png
        ${ASSET_ROOT}/marker_half_beat.png
        ${ASSET_ROOT}/marker_measure.png
        ${ASSET_ROOT}/note_blue.png
        ${ASSET_ROOT}/note_green.png
        ${ASSET_ROOT}/note_orange.png
        ${ASSET_ROOT}/note_red.png
        ${ASSET_ROOT}/note_white.png
        ${ASSET_ROOT}/note_yellow.png
        ${ASSET_ROOT}/overlay_cym_accent.png
        ${ASSET_ROOT}/overlay_cym_ghost.png
        ${ASSET_ROOT}/overlay_cym_ghost_80scale.png
        ${ASSET_ROOT}/overlay_note_accent.png
        ${ASSET_ROOT}/overlay_note_ghost.png
        ${ASSET_ROOT}/overlay_note_tap.png
        ${ASSET_ROOT}/sustain_blue.png
        ${ASSET_ROOT}/sustain_green.png
        ${ASSET_ROOT}/sustain_open.png
        ${ASSET_ROOT}/sustain_open_white.png
        ${ASSET_ROOT}/sustain_orange.png
        ${ASSET_ROOT}/sustain_red.png
        ${ASSET_ROOT}/sustain_white.png
        ${ASSET_ROOT}/sustain_yellow.png
        ${ASSET_ROOT}/track_drum.png
        ${ASSET_ROOT}/track_guitar.png
        ${ASSET_ROOT}/logo-reaper.svg
        ${ASSET_ROOT}/hit_1.png
        ${ASSET_ROOT}/hit_2.png
        ${ASSET_ROOT}/hit_3.png
        ${ASSET_ROOT}/hit_4.png
        ${ASSET_ROOT}/hit_5.png
        ${ASSET_ROOT}/hit_flare_blue.png
        ${ASSET_ROOT}/hit_flare_green.png
        ${ASSET_ROOT}/hit_flare_orange.png
        ${ASSET_ROOT}/hit_flare_red.png
        ${ASSET_ROOT}/hit_flare_yellow.png
        ${ASSET_ROOT}/hit_kick_1.png
        ${ASSET_ROOT}/hit_kick_2.png
        ${ASSET_ROOT}/hit_kick_3.png
        ${ASSET_ROOT}/hit_kick_4.png
        ${ASSET_ROOT}/hit_kick_5.png
        ${ASSET_ROOT}/hit_kick_6.png
        ${ASSET_ROOT}/hit_kick_7.png
        ${ASSET_ROOT}/hit_open_1.png
        ${ASSET_ROOT}/hit_open_2.png
        ${ASSET_ROOT}/hit_open_3.png
        ${ASSET_ROOT}/hit_open_4.png
        ${ASSET_ROOT}/hit_open_5.png
        ${ASSET_ROOT}/hit_open_6.png
        ${ASSET_ROOT}/hit_open_7.png
        ${ASSET_ROOT}/midi/test.mid
        ${ASSET_ROOT}/midi/stress.mid
        ${ASSET_ROOT}/midi/sleepy_tea.mid
        ${ASSET_ROOT}/midi/further_side.mid
        ${ASSET_ROOT}/midi/scarlet.mid
        ${ASSET_ROOT}/midi/everything_went_black.mid
        ${ASSET_ROOT}/midi/akroasis.mid
)

target_compile_definitions(bench_rendering PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JucePlugin_Name="Chartchotic"
    JucePlugin_Build_VST3=0
    JucePlugin_Build_AU=0
)

target_link_libraries(bench_rendering PRIVATE
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    BenchmarkData
)

if(APPLE)
    target_link_libraries(bench_rendering PRIVATE
        "-framework Foundation"
        "-framework Cocoa"
    )
endif()

#==============================================================================
# Pipeline benchmark (MIDI processing, NoteProcessor, GemCalculator, MidiInterpreter)

juce_add_console_app(bench_pipeline PRODUCT_NAME "Chartchotic Pipeline Benchmark")

target_sources(bench_pipeline PRIVATE
    bench_pipeline.cpp
    ${SOURCE_ROOT}/Midi/Processing/NoteProcessor.cpp
    ${SOURCE_ROOT}/Midi/Processing/MidiProcessor.cpp
    ${SOURCE_ROOT}/Midi/Processing/MidiInterpreter.cpp
    ${SOURCE_ROOT}/Midi/Utils/GemCalculator.cpp
    ${SOURCE_ROOT}/Midi/Utils/ChordAnalyzer.cpp
    ${SOURCE_ROOT}/Midi/Utils/LaneDetector.cpp
    ${SOURCE_ROOT}/Visual/Geometry/PositionMath.cpp
    ${SOURCE_ROOT}/Visual/Renderers/SceneRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/AnimationRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/NoteRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/SustainRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/GridlineRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/TextEventRenderer.cpp
    ${SOURCE_ROOT}/Visual/Art/TrackRenderer.cpp
    ${SOURCE_ROOT}/Visual/Managers/AssetManager.cpp
    ${SOURCE_ROOT}/Visual/HighwayComponent.cpp
    ${SOURCE_ROOT}/UI/LookAndFeel/ChartchoticLookAndFeel.cpp
)

target_include_directories(bench_pipeline PRIVATE
    ${SOURCE_ROOT}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

juce_generate_juce_header(bench_pipeline)

target_compile_definitions(bench_pipeline PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JucePlugin_Name="Chartchotic"
    JucePlugin_Build_VST3=0
    JucePlugin_Build_AU=0
    CHARTCHOTIC_MIDI_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../assets/midi"
    CHARTCHOTIC_NO_BINARY_DATA=1
)

target_link_libraries(bench_pipeline PRIVATE
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
)

if(APPLE)
    target_link_libraries(bench_pipeline PRIVATE
        "-framework Foundation"
        "-framework Cocoa"
    )
endif()
