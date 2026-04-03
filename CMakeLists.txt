cmake_minimum_required(VERSION 3.22)

# Read version from VERSION file
file(READ "${CMAKE_SOURCE_DIR}/VERSION" CHARTCHOTIC_VERSION)
string(STRIP "${CHARTCHOTIC_VERSION}" CHARTCHOTIC_VERSION)

project(Chartchotic VERSION ${CHARTCHOTIC_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build channel: DEV or RELEASE (passed via -DBUILD_CHANNEL=...)
if(NOT DEFINED BUILD_CHANNEL)
    set(BUILD_CHANNEL "DEV")
endif()

# Version string: full version including prerelease suffix (passed via -DVERSION_STRING=...)
# For local dev builds, generate a timestamped version automatically
if(NOT DEFINED VERSION_STRING)
    if(BUILD_CHANNEL STREQUAL "DEV")
        string(TIMESTAMP _timestamp "%Y%m%d%H%M%S" UTC)
        execute_process(
            COMMAND git rev-parse --short HEAD
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            OUTPUT_VARIABLE _short_sha
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(_short_sha STREQUAL "")
            set(_short_sha "local")
        endif()
        set(VERSION_STRING "${CHARTCHOTIC_VERSION}-dev.${_timestamp}.${_short_sha}")
    else()
        set(VERSION_STRING "${CHARTCHOTIC_VERSION}")
    endif()
endif()

# Suppress warnings from JUCE (third-party code)
set(_saved_cxx_flags "${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
add_subdirectory(third_party/JUCE)
set(CMAKE_CXX_FLAGS "${_saved_cxx_flags}")

# Plugin definition
juce_add_plugin(Chartchotic
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "Dichotic Studios"
    BUNDLE_ID "com.dichoticstudios.chartchotic"
    PLUGIN_MANUFACTURER_CODE Dich
    PLUGIN_CODE ChPv
    FORMATS VST3 AU LV2 Standalone
    PRODUCT_NAME "Chartchotic"
    PLUGIN_DESC "A tool for visualizing midi as guitar/drum game charts"

    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    NEEDS_CURL TRUE

    VST3_CATEGORIES Tools
    LV2URI https://dichoticstudios.com/plugins/chartchotic
)

juce_generate_juce_header(Chartchotic)

# Source files
target_sources(Chartchotic
    PRIVATE
        Chartchotic/Source/PluginProcessor.cpp
        Chartchotic/Source/PluginProcessor.h
        Chartchotic/Source/PluginEditor.cpp
        Chartchotic/Source/PluginEditor.h
        # Editor (PluginEditor delegates)
        Chartchotic/Source/Editor/AssetController.cpp
        Chartchotic/Source/Editor/AssetController.h
        Chartchotic/Source/Editor/FrameDataBuilder.cpp
        Chartchotic/Source/Editor/FrameDataBuilder.h
        Chartchotic/Source/Editor/SessionController.cpp
        Chartchotic/Source/Editor/SessionController.h
        Chartchotic/Source/Host/ReaperVST3.h
        # DebugTools
        Chartchotic/Source/DebugTools/DebugEditorController.cpp
        Chartchotic/Source/DebugTools/DebugEditorController.h
        Chartchotic/Source/DebugTools/FrameProfileLogger.h
        Chartchotic/Source/DebugTools/DebugMidiFilePlayer.h
        Chartchotic/Source/DebugTools/DebugPlaybackController.h
        Chartchotic/Source/DebugTools/DebugToolbarPanel.cpp
        Chartchotic/Source/DebugTools/DebugToolbarPanel.h
        Chartchotic/Source/DebugTools/DebugTuningPanel.cpp
        Chartchotic/Source/DebugTools/DebugTuningPanel.h
        Chartchotic/Source/DebugTools/Logger.h
        Chartchotic/Source/DebugTools/DataGenerators.h
        # Midi
        Chartchotic/Source/Midi/ChartTextEvent.h
        Chartchotic/Source/Midi/InstrumentSlot.h
        Chartchotic/Source/Midi/InstrumentSession.cpp
        Chartchotic/Source/Midi/InstrumentSession.h
        # Midi / Discovery
        Chartchotic/Source/Midi/Discovery/TrackDiscovery.h
        Chartchotic/Source/Midi/Discovery/ReaperGlobalDiscovery.h
        Chartchotic/Source/Midi/Discovery/ReaperLocalDiscovery.h
        Chartchotic/Source/Midi/Discovery/ManualDiscovery.h
        # Midi / Providers (DAW-agnostic)
        Chartchotic/Source/Midi/Providers/TrackNoteProvider.h
        Chartchotic/Source/Midi/Providers/ReaperTrackNoteProvider.h
        Chartchotic/Source/Midi/Providers/MidiFileNoteProvider.h
        # Midi / Processing
        Chartchotic/Source/Midi/Processing/MidiInterpreter.cpp
        Chartchotic/Source/Midi/Processing/MidiInterpreter.h
        Chartchotic/Source/Midi/Processing/MidiProcessor.cpp
        Chartchotic/Source/Midi/Processing/MidiProcessor.h
        Chartchotic/Source/Midi/Processing/NoteProcessor.cpp
        Chartchotic/Source/Midi/Processing/NoteProcessor.h
        Chartchotic/Source/Midi/Processing/SharedTrackData.h
        Chartchotic/Source/Midi/Processing/TrackResolver.cpp
        Chartchotic/Source/Midi/Processing/TrackResolver.h
        # Midi / Providers / REAPER
        Chartchotic/Source/Midi/Providers/REAPER/MidiCache.cpp
        Chartchotic/Source/Midi/Providers/REAPER/MidiCache.h
        Chartchotic/Source/Midi/Providers/REAPER/ReaperApiHelpers.h
        Chartchotic/Source/Midi/Providers/REAPER/ReaperMidiProvider.cpp
        Chartchotic/Source/Midi/Providers/REAPER/ReaperMidiProvider.h
        Chartchotic/Source/Midi/Providers/REAPER/ReaperMidiWriter.cpp
        Chartchotic/Source/Midi/Providers/REAPER/ReaperMidiWriter.h
        Chartchotic/Source/Midi/Providers/REAPER/ReaperNoteFetcher.cpp
        Chartchotic/Source/Midi/Providers/REAPER/ReaperNoteFetcher.h
        # Midi / Providers
        Chartchotic/Source/Midi/Providers/MidiWriter.h
        # Midi / Pipelines
        Chartchotic/Source/Midi/Pipelines/MidiPipeline.h
        Chartchotic/Source/Midi/Pipelines/MidiPipelineFactory.cpp
        Chartchotic/Source/Midi/Pipelines/MidiPipelineFactory.h
        Chartchotic/Source/Midi/Pipelines/ReaperMidiPipeline.cpp
        Chartchotic/Source/Midi/Pipelines/ReaperMidiPipeline.h
        Chartchotic/Source/Midi/Pipelines/StandardMidiPipeline.cpp
        Chartchotic/Source/Midi/Pipelines/StandardMidiPipeline.h
        # Midi / Utils
        Chartchotic/Source/Midi/Utils/GemCalculator.cpp
        Chartchotic/Source/Midi/Utils/GemCalculator.h
        Chartchotic/Source/Midi/Utils/InstrumentMapper.h
        Chartchotic/Source/Midi/Utils/MidiConstants.h
        Chartchotic/Source/Midi/Utils/MidiTypes.h
        Chartchotic/Source/Midi/Utils/PPQ.h
        Chartchotic/Source/Midi/Utils/TempoTimeSignatureEventHelper.h
        Chartchotic/Source/Midi/Utils/TimeConverter.h
        # Host (REAPER integration)
        Chartchotic/Source/Host/ReaperIntegration.cpp
        Chartchotic/Source/Host/ReaperIntegration.h
        Chartchotic/Source/Host/ReaperTrackDetector.cpp
        Chartchotic/Source/Host/ReaperTrackDetector.h
        Chartchotic/Source/Host/ReaperVST2Extensions.cpp
        Chartchotic/Source/Host/ReaperVST2Extensions.h
        Chartchotic/Source/Host/ReaperVST3Extensions.cpp
        Chartchotic/Source/Host/ReaperVST3Extensions.h
        Chartchotic/Source/Host/TrackInfoListener.cpp
        Chartchotic/Source/Host/TrackInfoListener.h
        # Visual / Renderers
        Chartchotic/Source/Visual/Renderers/AnimationRenderer.cpp
        Chartchotic/Source/Visual/Renderers/AnimationRenderer.h
        Chartchotic/Source/Visual/Renderers/SceneRenderer.cpp
        Chartchotic/Source/Visual/Renderers/SceneRenderer.h
        Chartchotic/Source/Visual/Renderers/GridlineRenderer.cpp
        Chartchotic/Source/Visual/Renderers/GridlineRenderer.h
        Chartchotic/Source/Visual/Renderers/TextEventRenderer.cpp
        Chartchotic/Source/Visual/Renderers/TextEventRenderer.h
        Chartchotic/Source/Visual/Renderers/TrackRenderer.cpp
        Chartchotic/Source/Visual/Renderers/TrackRenderer.h
        Chartchotic/Source/Visual/HighwayComponent.cpp
        Chartchotic/Source/Visual/HighwayComponent.h
        Chartchotic/Source/Visual/HighwaySlot.h
        Chartchotic/Source/Visual/TrackImageCache.h
        Chartchotic/Source/Visual/Renderers/NoteRenderer.cpp
        Chartchotic/Source/Visual/Renderers/NoteRenderer.h
        Chartchotic/Source/Visual/Renderers/SustainRenderer.cpp
        Chartchotic/Source/Visual/Renderers/SustainRenderer.h
        # Visual / Utils
        Chartchotic/Source/Visual/Utils/DrawingConstants.h
        Chartchotic/Source/Visual/Utils/Frame.h
        Chartchotic/Source/Visual/Utils/FrameRenderer.cpp
        Chartchotic/Source/Visual/Utils/FrameRenderer.h
        Chartchotic/Source/Visual/Utils/PositionConstants.h
        Chartchotic/Source/Visual/Utils/HitTestMapper.cpp
        Chartchotic/Source/Visual/Utils/HitTestMapper.h
        Chartchotic/Source/Visual/Utils/PositionMath.cpp
        Chartchotic/Source/Visual/Utils/PositionMath.h
        Chartchotic/Source/Visual/Utils/RenderTypeConfig.cpp
        Chartchotic/Source/Visual/Utils/RenderTypeConfig.h
        Chartchotic/Source/Visual/Utils/RenderTiming.h
        Chartchotic/Source/Visual/Renderers/TrackFade.h
        # Visual / Managers
        Chartchotic/Source/Visual/Managers/AnimationManager.h
        Chartchotic/Source/Visual/Managers/AssetManager.cpp
        Chartchotic/Source/Visual/Managers/AssetManager.h
        Chartchotic/Source/Visual/Managers/GridlineGenerator.h
        # UI - Controls
        Chartchotic/Source/UI/Controls/CheckboxToggle.h
        Chartchotic/Source/UI/Controls/PanelSectionHeader.h
        Chartchotic/Source/UI/Controls/PillToggle.h
        Chartchotic/Source/UI/Controls/PopupMenuButton.h
        Chartchotic/Source/UI/Controls/CircleIconSelector.h
        Chartchotic/Source/UI/Controls/SegmentedButtons.h
        Chartchotic/Source/UI/Controls/ValueStepper.h
        # UI - LookAndFeel
        Chartchotic/Source/UI/LookAndFeel/ChartchoticLookAndFeel.cpp
        Chartchotic/Source/UI/LookAndFeel/ChartchoticLookAndFeel.h
        # UI - Core
        Chartchotic/Source/UI/ChartchoticLogo.h
        Chartchotic/Source/UI/Theme.h
        Chartchotic/Source/UI/MenuGroup.h
        Chartchotic/Source/UI/SectionHeader.h
        Chartchotic/Source/UI/ControlConstants.h
        Chartchotic/Source/UI/FooterComponent.h
        Chartchotic/Source/UI/StatusBarComponent.h
        Chartchotic/Source/UI/TooltipStrings.h
        Chartchotic/Source/UI/ToolbarComponent.cpp
        Chartchotic/Source/UI/ToolbarComponent.h
        Chartchotic/Source/UI/UpdateChecker.cpp
        Chartchotic/Source/UI/UpdateChecker.h
        # Utils
        Chartchotic/Source/Visual/Utils/LatencySmoother.h
        Chartchotic/Source/Utils/ChartTypes.h
)

# Binary data (images + SVG)
# Generated assets come from assets/generated/<category>/ (via convert_assets.sh)
# Static assets (no PDF source) come from assets/static/
# MIDI test files come from assets/midi/
juce_add_binary_data(ChartchoticData
    NAMESPACE BinaryData
    SOURCES
        # Fonts
        assets/static/fonts/TurretRoad-Bold.ttf
        assets/static/fonts/TurretRoad-Medium.ttf
        assets/static/fonts/RussoOne-Regular.ttf
        assets/static/fonts/ChakraPetch-Medium.ttf
        # UI / branding
        assets/static/backgrounds/background.png
        assets/static/backgrounds/background_red.jpg
        assets/static/icons/icon_guitar.png
        assets/static/icons/icon_bass.png
        assets/static/icons/icon_keys.png
        assets/static/icons/icon_vocals.png
        assets/static/icons/icon_drums.png
        assets/static/icons/discoball.jpg
        assets/static/logo-reaper.svg
        # Default highway textures
        assets/static/highways/kanaizo_amber.png
        assets/static/highways/kanaizo_darkwood.png
        assets/static/highways/kanaizo_gothic.png
        assets/static/highways/kanaizo_ornate.png
        assets/static/highways/kanaizo_BlueCaustic.png
        assets/static/highways/kanaizo_FretWood.png
        assets/static/highways/kanaizo_BlueMagentaHex.png


        assets/static/highways/kanaizo_IcyMetal.png
        assets/static/highways/kanaizo_BrightRadiated.png
        assets/static/highways/kanaizo_ModernHueistic.png
        assets/static/highways/kanaizo_RedCaustic.png
        assets/static/highways/kanaizo_ScratchedRegular.png
        # Notes (generated from PDFs)
        assets/generated/notes/note_blue.png
        assets/generated/notes/note_green.png
        assets/generated/notes/note_orange.png
        assets/generated/notes/note_red.png
        assets/generated/notes/note_white.png
        assets/generated/notes/note_yellow.png
        assets/generated/notes/hopo_blue.png
        assets/generated/notes/hopo_green.png
        assets/generated/notes/hopo_orange.png
        assets/generated/notes/hopo_red.png
        assets/generated/notes/hopo_white.png
        assets/generated/notes/hopo_yellow.png
        assets/generated/notes/cym_blue.png
        assets/generated/notes/cym_green.png
        assets/generated/notes/cym_red.png
        assets/generated/notes/cym_white.png
        assets/generated/notes/cym_yellow.png
        # Bars (generated from PDFs)
        assets/generated/bars/bar_kick.png
        assets/generated/bars/bar_kick_2x.png
        assets/generated/bars/bar_open.png
        assets/generated/bars/bar_white.png
        # Overlays (generated from PDFs)
        assets/generated/overlays/overlay_note_ghost.png
        assets/generated/overlays/overlay_cym_ghost.png
        assets/generated/overlays/overlay_note_tap.png
        assets/generated/overlays/overlay_cym_accent.png
        assets/generated/overlays/overlay_note_accent.png
        # Sustains (generated from PDFs)
        assets/generated/sustains/sustain_open.png
        assets/generated/sustains/sustain_open_white.png
        # Markers (generated from PDFs)
        assets/generated/markers/marker_beat.png
        assets/generated/markers/marker_half_beat.png
        assets/generated/markers/marker_measure.png
        # Lanes (generated from PDFs)
        assets/generated/lanes/lane_end.png
        assets/generated/lanes/lane_mid.png
        assets/generated/lanes/lane_start.png
        # Track layers (generated from PDFs)
        assets/generated/ui/sidebars.png
        assets/generated/ui/lane_lines_guitar.png
        assets/generated/ui/lane_lines_drums.png
        assets/generated/ui/strikeline_guitar.png
        assets/generated/ui/strikeline_drums.png
        assets/generated/ui/strikeline_connectors.png
        assets/generated/ui/kick_smashers.png
        # Hit animations — flares (generated from PDFs)
        assets/generated/hits/hit_flare_blue.png
        assets/generated/hits/hit_flare_green.png
        assets/generated/hits/hit_flare_orange.png
        assets/generated/hits/hit_flare_red.png
        assets/generated/hits/hit_flare_yellow.png
        assets/generated/hits/hit_flare_white.png
        # Hit animations — flash frames (generated from PDFs)
        assets/generated/hits/hit_flash_1.png
        assets/generated/hits/hit_flash_2.png
        assets/generated/hits/hit_flash_3.png
        assets/generated/hits/hit_flash_4.png
        assets/generated/hits/hit_flash_5.png
        # Hit animations — kick frames (static)
        assets/static/hits/hit_kick_1.png
        assets/static/hits/hit_kick_2.png
        assets/static/hits/hit_kick_3.png
        assets/static/hits/hit_kick_4.png
        assets/static/hits/hit_kick_5.png
        assets/static/hits/hit_kick_6.png
        assets/static/hits/hit_kick_7.png
        # Hit animations — open frames (static)
        assets/static/hits/hit_open_1.png
        assets/static/hits/hit_open_2.png
        assets/static/hits/hit_open_3.png
        assets/static/hits/hit_open_4.png
        assets/static/hits/hit_open_5.png
        assets/static/hits/hit_open_6.png
        assets/static/hits/hit_open_7.png
        # MIDI test files loaded from disk at runtime (see CHARTCHOTIC_MIDI_ASSET_DIR)
)

# REAPER SDK header path
target_include_directories(Chartchotic
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/reaper-sdk/sdk
)

# Compile definitions
target_compile_definitions(Chartchotic
    PUBLIC
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=1
    PRIVATE
        CHARTCHOTIC_BUILD_CHANNEL="${BUILD_CHANNEL}"
        CHARTCHOTIC_VERSION_STRING="${VERSION_STRING}"
        CHARTCHOTIC_MIDI_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets/midi"
)

# Link libraries
target_link_libraries(Chartchotic
    PRIVATE
        ChartchoticData
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
)

# Platform-specific flags
if(APPLE)
    target_compile_options(Chartchotic PRIVATE
        -Wno-deprecated-declarations
    )
endif()

# --- Standard (non-REAPER) variant for local dev testing only ---
if(BUILD_STD)
juce_add_plugin(ChartchoticStd
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "Dichotic Studios"
    BUNDLE_ID "com.dichoticstudios.chartchotic-std"
    PLUGIN_MANUFACTURER_CODE Dich
    PLUGIN_CODE ChSt
    FORMATS VST3 AU
    PRODUCT_NAME "Chartchotic Std"
    PLUGIN_DESC "Chartchotic — standard DAW mode (no REAPER integration)"

    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    NEEDS_CURL TRUE

    VST3_CATEGORIES Tools
)

juce_generate_juce_header(ChartchoticStd)

# Reuse the same source files
get_target_property(_chartchotic_sources Chartchotic SOURCES)
target_sources(ChartchoticStd PRIVATE ${_chartchotic_sources})

target_include_directories(ChartchoticStd
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/reaper-sdk/sdk
)

target_compile_definitions(ChartchoticStd
    PUBLIC
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=1
    PRIVATE
        CHARTCHOTIC_BUILD_CHANNEL="${BUILD_CHANNEL}"
        CHARTCHOTIC_VERSION_STRING="${VERSION_STRING}"
        CHARTCHOTIC_MIDI_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets/midi"
        CHARTCHOTIC_DISABLE_REAPER=1
)

target_link_libraries(ChartchoticStd
    PRIVATE
        ChartchoticData
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
)

if(APPLE)
    target_compile_options(ChartchoticStd PRIVATE
        -Wno-deprecated-declarations
    )
endif()
endif() # BUILD_STD
