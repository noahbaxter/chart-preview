cmake_minimum_required(VERSION 3.15)

# Set the project name and version
project(ChartPreview VERSION 1.0.0)

# Set the path to the JUCE directory
set(JUCE_DIR "/path/to/JUCE")

# Add JUCE as a subdirectory
add_subdirectory(${JUCE_DIR} JUCE)

# Create a target for the plugin
juce_add_plugin(ChartPreview
    PLUGIN_MANUFACTURER_CODE Juce
    PLUGIN_CODE ChPr
    FORMATS VST3
    PRODUCT_NAME "Chart Preview"
)

# Add source files
target_sources(ChartPreview PRIVATE
    Source/MidiProcessor.cpp
    Source/MidiProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
)

# Include JUCE modules
target_link_libraries(ChartPreview PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_audio_processors
)
