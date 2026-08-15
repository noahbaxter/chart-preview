cmake_minimum_required(VERSION 3.22)
project(ChartchoticRenderHarness VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE (submodule in repo)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/JUCE ${CMAKE_BINARY_DIR}/JUCE)

set(REPO_ROOT   ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(SOURCE_ROOT ${REPO_ROOT}/Chartchotic/Source)
set(ASSET_GEN    ${REPO_ROOT}/assets/generated)
set(ASSET_STATIC ${REPO_ROOT}/assets/static)

juce_add_console_app(render_harness PRODUCT_NAME "Chartchotic Render Harness")

# Renderer stack only — no plugin processor, no Midi processing chain, no REAPER.
target_sources(render_harness PRIVATE
    render_main.cpp
    ${SOURCE_ROOT}/Visual/Renderers/SceneRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/NoteRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/SustainRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/GridlineRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/AnimationRenderer.cpp
    ${SOURCE_ROOT}/Visual/Renderers/TextEventRenderer.cpp
    ${SOURCE_ROOT}/Visual/Art/TrackRenderer.cpp
    ${SOURCE_ROOT}/Visual/Art/ProceduralTrackArt.cpp
    ${SOURCE_ROOT}/Visual/Art/BarGemArt.cpp
    ${SOURCE_ROOT}/Visual/Managers/AssetManager.cpp
    ${SOURCE_ROOT}/Visual/Geometry/PositionMath.cpp
    ${SOURCE_ROOT}/Visual/Geometry/RenderTypeConfig.cpp
    ${SOURCE_ROOT}/Visual/Geometry/HitTestMapper.cpp
    ${SOURCE_ROOT}/Visual/Utils/FrameRenderer.cpp
)

target_include_directories(render_harness PRIVATE
    ${SOURCE_ROOT}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

juce_generate_juce_header(render_harness)

# Binary data — mirrors the main plugin's ChartchoticData list so every
# BinaryData symbol AssetManager / TrackRenderer reference resolves.
juce_add_binary_data(RenderHarnessData
    NAMESPACE BinaryData
    SOURCES
        # Fonts
        ${ASSET_STATIC}/fonts/TurretRoad-Bold.ttf
        ${ASSET_STATIC}/fonts/TurretRoad-Medium.ttf
        ${ASSET_STATIC}/fonts/RussoOne-Regular.ttf
        ${ASSET_STATIC}/fonts/ChakraPetch-Medium.ttf
        # UI / branding
        ${ASSET_STATIC}/backgrounds/background.png
        ${ASSET_STATIC}/backgrounds/background_red.jpg
        ${ASSET_STATIC}/icons/icon_guitar.png
        ${ASSET_STATIC}/icons/icon_bass.png
        ${ASSET_STATIC}/icons/icon_keys.png
        ${ASSET_STATIC}/icons/icon_vocals.png
        ${ASSET_STATIC}/icons/icon_drums.png
        ${ASSET_STATIC}/icons/icon_snap_magnet.svg
        ${ASSET_STATIC}/icons/icon_cursor.svg
        ${ASSET_STATIC}/icons/discoball.jpg
        ${ASSET_STATIC}/logo-reaper.svg
        # Default highway textures
        ${ASSET_STATIC}/highways/kanaizo_amber.png
        ${ASSET_STATIC}/highways/kanaizo_darkwood.png
        ${ASSET_STATIC}/highways/gothic_default.png
        ${ASSET_STATIC}/highways/kanaizo_ornate.png
        ${ASSET_STATIC}/highways/kanaizo_BlueCaustic.png
        ${ASSET_STATIC}/highways/kanaizo_FretWood.png
        ${ASSET_STATIC}/highways/kanaizo_BlueMagentaHex.png
        ${ASSET_STATIC}/highways/kanaizo_IcyMetal.png
        ${ASSET_STATIC}/highways/kanaizo_BrightRadiated.png
        ${ASSET_STATIC}/highways/kanaizo_ModernHueistic.png
        ${ASSET_STATIC}/highways/kanaizo_RedCaustic.png
        ${ASSET_STATIC}/highways/kanaizo_ScratchedRegular.png
        # Notes (generated from PDFs)
        ${ASSET_GEN}/notes/note_blue.png
        ${ASSET_GEN}/notes/note_green.png
        ${ASSET_GEN}/notes/note_orange.png
        ${ASSET_GEN}/notes/note_red.png
        ${ASSET_GEN}/notes/note_white.png
        ${ASSET_GEN}/notes/note_yellow.png
        ${ASSET_GEN}/notes/hopo_blue.png
        ${ASSET_GEN}/notes/hopo_green.png
        ${ASSET_GEN}/notes/hopo_orange.png
        ${ASSET_GEN}/notes/hopo_red.png
        ${ASSET_GEN}/notes/hopo_white.png
        ${ASSET_GEN}/notes/hopo_yellow.png
        ${ASSET_GEN}/notes/cym_blue.png
        ${ASSET_GEN}/notes/cym_green.png
        ${ASSET_GEN}/notes/cym_red.png
        ${ASSET_GEN}/notes/cym_white.png
        ${ASSET_GEN}/notes/cym_yellow.png
        # Elite hi-hat cymbals (extracted gold art, used as-is)
        ${ASSET_GEN}/cymbals/cym_hihat_closed.png
        ${ASSET_GEN}/cymbals/cym_hihat_open.png
        # Ghost cursor templates
        ${ASSET_GEN}/templates/note_blank.png
        ${ASSET_GEN}/templates/hopo_blank.png
        ${ASSET_GEN}/templates/cym_blank.png
        ${ASSET_GEN}/templates/bar_blank.png
        # Bars
        ${ASSET_GEN}/bars/bar_kick.png
        ${ASSET_GEN}/bars/bar_kick_2x.png
        ${ASSET_GEN}/bars/bar_open.png
        ${ASSET_GEN}/bars/bar_white.png
        # Overlays
        ${ASSET_GEN}/overlays/overlay_note_ghost.png
        ${ASSET_GEN}/overlays/overlay_cym_ghost.png
        ${ASSET_GEN}/overlays/overlay_note_tap.png
        ${ASSET_GEN}/overlays/overlay_cym_accent.png
        ${ASSET_GEN}/overlays/overlay_note_accent.png
        # Sustains
        ${ASSET_GEN}/sustains/sustain_open.png
        ${ASSET_GEN}/sustains/sustain_open_white.png
        # Lanes
        ${ASSET_GEN}/lanes/lane_end.png
        ${ASSET_GEN}/lanes/lane_mid.png
        ${ASSET_GEN}/lanes/lane_start.png
        # Track layers
        ${ASSET_GEN}/ui/sidebars.png
        ${ASSET_GEN}/ui/lane_lines_guitar.png
        ${ASSET_GEN}/ui/lane_lines_drums.png
        ${ASSET_GEN}/ui/strikeline_guitar.png
        ${ASSET_GEN}/ui/strikeline_drums.png
        ${ASSET_GEN}/ui/strikeline_connectors.png
        ${ASSET_GEN}/ui/kick_smashers.png
        # Hit animations — flares
        ${ASSET_GEN}/hits/hit_flare_blue.png
        ${ASSET_GEN}/hits/hit_flare_green.png
        ${ASSET_GEN}/hits/hit_flare_orange.png
        ${ASSET_GEN}/hits/hit_flare_red.png
        ${ASSET_GEN}/hits/hit_flare_yellow.png
        ${ASSET_GEN}/hits/hit_flare_white.png
        # Hit animations — flash frames
        ${ASSET_GEN}/hits/hit_flash_1.png
        ${ASSET_GEN}/hits/hit_flash_2.png
        ${ASSET_GEN}/hits/hit_flash_3.png
        ${ASSET_GEN}/hits/hit_flash_4.png
        ${ASSET_GEN}/hits/hit_flash_5.png
        # Hit animations — kick frames (static)
        ${ASSET_STATIC}/hits/hit_kick_1.png
        ${ASSET_STATIC}/hits/hit_kick_2.png
        ${ASSET_STATIC}/hits/hit_kick_3.png
        ${ASSET_STATIC}/hits/hit_kick_4.png
        ${ASSET_STATIC}/hits/hit_kick_5.png
        ${ASSET_STATIC}/hits/hit_kick_6.png
        ${ASSET_STATIC}/hits/hit_kick_7.png
        # Hit animations — open frames (static)
        ${ASSET_STATIC}/hits/hit_open_1.png
        ${ASSET_STATIC}/hits/hit_open_2.png
        ${ASSET_STATIC}/hits/hit_open_3.png
        ${ASSET_STATIC}/hits/hit_open_4.png
        ${ASSET_STATIC}/hits/hit_open_5.png
        ${ASSET_STATIC}/hits/hit_open_6.png
        ${ASSET_STATIC}/hits/hit_open_7.png
)

target_compile_definitions(render_harness PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JucePlugin_Name="Chartchotic"
    JucePlugin_Build_VST3=0
    JucePlugin_Build_AU=0
)

target_link_libraries(render_harness PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    RenderHarnessData
)

if(APPLE)
    target_link_libraries(render_harness PRIVATE
        "-framework Foundation"
        "-framework Cocoa"
    )
endif()

# Export dialog snapshot. The dialog only collects and validates, so it needs
# no live project, but it does drag in the exporter for ChartName::folderName
# and Sink::formatCode, and the exporter drags in the REAPER provider.
juce_add_console_app(dialog_harness PRODUCT_NAME "Chartchotic Dialog Harness")

target_sources(dialog_harness PRIVATE
    dialog_main.cpp
    ${SOURCE_ROOT}/Export/ExportDialogComponent.cpp
    ${SOURCE_ROOT}/Export/BackgroundGenerator.cpp
    ${SOURCE_ROOT}/Export/IconPreview.cpp
    ${SOURCE_ROOT}/Export/ChartExporter.cpp
    ${SOURCE_ROOT}/Export/ChartMidiWriter.cpp
    ${SOURCE_ROOT}/Export/SngPacker.cpp
    ${SOURCE_ROOT}/Host/ReaperTrackDetector.cpp
    ${SOURCE_ROOT}/Midi/Providers/REAPER/ReaperMidiProvider.cpp
    ${SOURCE_ROOT}/Midi/Providers/REAPER/ReaperNoteFetcher.cpp
    ${SOURCE_ROOT}/Midi/Providers/REAPER/ReaperMidiWriter.cpp
    ${SOURCE_ROOT}/Midi/Providers/REAPER/ReaperItemManager.cpp
    ${SOURCE_ROOT}/Midi/Providers/REAPER/MidiCache.cpp
)

target_include_directories(dialog_harness PRIVATE
    ${SOURCE_ROOT}
    ${REPO_ROOT}/third_party/reaper-sdk/sdk
    ${CMAKE_CURRENT_SOURCE_DIR}
)

juce_generate_juce_header(dialog_harness)

target_compile_definitions(dialog_harness PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    # The snapshot has to pump the message loop to let the icon lookup land.
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JucePlugin_Name="Chartchotic"
    JucePlugin_Build_VST3=0
    JucePlugin_Build_AU=0
)

target_link_libraries(dialog_harness PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    RenderHarnessData
)

if(APPLE)
    target_link_libraries(dialog_harness PRIVATE
        "-framework Foundation"
        "-framework Cocoa"
    )
endif()
