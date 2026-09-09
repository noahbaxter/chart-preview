cmake_minimum_required(VERSION 3.22)
project(ChartchoticUnitTests VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Catch2
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.0
)
FetchContent_MakeAvailable(Catch2)

# JUCE (submodule in repo)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/JUCE ${CMAKE_BINARY_DIR}/JUCE)

# Source root for project headers
set(SOURCE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../Chartchotic/Source)

juce_add_console_app(unit_tests PRODUCT_NAME "Chartchotic Unit Tests")

target_sources(unit_tests PRIVATE
    test_smoke.cpp
    test_gem_calculator.cpp
    test_midi_interpreter.cpp
    test_gridline_generator.cpp
    test_instrument_mapper.cpp
    test_frame.cpp
    ${SOURCE_ROOT}/Midi/Utils/GemCalculator.cpp
    ${SOURCE_ROOT}/Midi/Processing/MidiInterpreter.cpp
    ${SOURCE_ROOT}/Midi/Processing/TrackResolver.cpp
    ${SOURCE_ROOT}/Visual/Utils/PositionMath.cpp
    ${SOURCE_ROOT}/Visual/Utils/RenderTypeConfig.cpp
    ${SOURCE_ROOT}/Visual/Utils/FrameRenderer.cpp
)

target_include_directories(unit_tests PRIVATE
    ${SOURCE_ROOT}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

juce_generate_juce_header(unit_tests)

target_compile_definitions(unit_tests PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JucePlugin_Name="Chartchotic"
    JucePlugin_Build_VST3=0
    JucePlugin_Build_AU=0
)

target_link_libraries(unit_tests PRIVATE
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    Catch2::Catch2WithMain
)

if(APPLE)
    target_link_libraries(unit_tests PRIVATE
        "-framework Foundation"
        "-framework Cocoa"
    )
endif()
