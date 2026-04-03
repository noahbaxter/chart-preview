#ifdef DEBUG

#include "DebugEditorController.h"
#include "../PluginProcessor.h"
#include "../Midi/Processing/MidiInterpreter.h"
#include "../UI/ToolbarComponent.h"
#include "../Visual/Managers/GridlineGenerator.h"
#include "../Midi/Utils/TimeConverter.h"

#ifndef CHARTCHOTIC_MIDI_ASSET_DIR
  #define CHARTCHOTIC_MIDI_ASSET_DIR ""
#endif

DebugEditorController::DebugEditorController() {}

DebugEditorController::~DebugEditorController()
{
    frameProfileLogger.stop();
}

void DebugEditorController::init(juce::Component& parent, ChartchoticAudioProcessor& processor,
                                  juce::ValueTree& state, bool isStandalone)
{
    processorPtr = &processor;
    statePtr = &state;
    standalone = isStandalone;

    scanMidiDirectory();

    if (standalone)
        loadDebugChart(playbackController.getChartIndex());

    consoleOutput.setMultiLine(true);
    consoleOutput.setReadOnly(true);
    consoleOutput.setOpaque(false);
    consoleOutput.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0, 0, 0).withAlpha(0.75f));
    consoleOutput.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    parent.addChildComponent(consoleOutput);

    clearLogsButton.setButtonText("Clear Logs");
    clearLogsButton.onClick = [this]() {
        processorPtr->clearDebugText();
        consoleOutput.clear();
    };
    parent.addChildComponent(clearLogsButton);

    copyLogsButton.setButtonText("Copy");
    copyLogsButton.onClick = [this]() {
        juce::SystemClipboard::copyTextToClipboard(consoleOutput.getText());
    };
    parent.addChildComponent(copyLogsButton);

    frameProfileLogger.start();
}

void DebugEditorController::wireCallbacks(ToolbarComponent& toolbar,
                                           HighwayComponent& highway,
                                           std::function<void()> repaintEditor)
{
    auto& dbg = toolbar.getDebugPanel();

    // Pass discovered chart names to the toolbar panel
    {
        std::vector<juce::String> names;
        for (auto& entry : chartEntries)
            names.push_back(entry.name);
        dbg.setChartNames(names);
    }

    dbg.onDebugPlayChanged = [this](bool playing) {
        playbackController.setPlaying(playing);
    };
    dbg.onDebugChartChanged = [this](int index) {
        playbackController.setChartIndex(index);
        loadDebugChart(index);
    };
    dbg.onDebugConsoleChanged = [this](bool show) {
        consoleOutput.setVisible(show);
        clearLogsButton.setVisible(show);
        copyLogsButton.setVisible(show);
    };
    dbg.onProfilerChanged = [this](bool on) {
        showProfilerOverlay = on;
    };

    auto& tune = toolbar.getTuningPanel();
    tune.initDefaults(highway.getTrackRenderer());

    tune.onLayerChanged = [this, &highway](int layer, float scale, float x, float y) {
        bool isDrums = isDrumLike(getPartFromState(*statePtr));
        auto* layers = isDrums ? highway.getTrackRenderer().layersDrums : highway.getTrackRenderer().layersGuitar;
        layers[layer] = {scale, x, y};
        highway.getTrackRenderer().invalidate();
        highway.rebuildTrack();
    };

    tune.onTileStepChanged = [&highway](float step) {
        highway.getTrackRenderer().tileStep = step;
        highway.rebuildTrack();
    };

    tune.onTileScaleStepChanged = [&highway](float step) {
        highway.getTrackRenderer().tileScaleStep = step;
        highway.rebuildTrack();
    };

    tune.onTextureScaleChanged = [&highway, repaintEditor](float val) {
        highway.getTrackRenderer().textureScale = val;
        repaintEditor();
    };

    tune.onTextureOpacityChanged = [&highway, repaintEditor](float val) {
        highway.getTrackRenderer().textureOpacity = val;
        repaintEditor();
    };

    tune.onPolyShadeChanged = [&highway]() {
        highway.getTrackRenderer().invalidate();
        highway.rebuildTrack();
    };

    tune.onDebugColourChanged = [&highway](bool on) {
        highway.showDebugColour = on;
    };

    tune.onStretchChanged = [&highway](bool on) {
        highway.stretchToFill = on;
        if (auto* parent = highway.getParentComponent())
            parent->resized();
    };

    tune.onBemaniToggled = [&highway, repaintEditor](bool on) {
        PositionMath::bemaniMode = on;
        PositionMath::bemaniHwyScale = 1.0f;
        // resized() sets correct dimensions for the new mode
        if (auto* parent = highway.getParentComponent())
            parent->resized();
        // Force immediate rebuild with correct dimensions
        highway.getTrackRenderer().invalidate();
        highway.rebuildTrack();
        repaintEditor();
    };

    tune.onHwyScaleChanged = [&highway, repaintEditor](float gtr, float drm) {
        highway.getTrackRenderer().invalidate();
        highway.rebuildTrack();
        if (auto* parent = highway.getParentComponent())
            parent->resized();
        repaintEditor();
    };

    tune.onBemaniTuningChanged = [repaintEditor]() {
        repaintEditor();
    };

    tune.onLogoPadChanged = [&toolbar](float gap, float nudge) {
        toolbar.getLogo().logoGapRatio = gap;
        toolbar.getLogo().dotNudge = nudge;
        toolbar.resized();
    };

    tune.onPerspectiveChanged = [&highway]() {
        highway.getTrackRenderer().invalidate();
        highway.rebuildTrack();
    };

    tune.onTuningChanged = [&tune, &highway, repaintEditor]() {
        tune.applyTo(highway.getSceneRenderer());
        repaintEditor();
    };

    tune.onLaneCoordsChanged = [&tune, &highway, repaintEditor]() {
        tune.applyTo(highway.getSceneRenderer());
        highway.rebuildTrack();
        repaintEditor();
    };
}

void DebugEditorController::beginFrame(double delta_us)
{
    frameDelta_us = delta_us;
    frameProfileLogger.beginFrame();
}

void DebugEditorController::onFrame(PPQ& lastKnownPosition, bool& lastPlayingState)
{
    if (!standalone) return;

    playbackController.advancePlayhead();
    if (debugChartLengthInBeats > 0.0 && playbackController.getCurrentPPQ().toDouble() > debugChartLengthInBeats)
        playbackController.nudgePlayhead(-debugChartLengthInBeats);
    lastKnownPosition = playbackController.getCurrentPPQ();
    if (playbackController.isPlaying())
        lastPlayingState = true;
}

void DebugEditorController::recordFrameData(const HighwayFrameData& primaryFrameData, double dataBuild_us,
                                             int slotCount, Part activePart, SkillLevel skill,
                                             int viewportW, int viewportH, bool isPlaying)
{
    pendingDataBuild_us = dataBuild_us;
    frameProfileLogger.recordFrameData(
        frameDelta_us, dataBuild_us, lockWait_us,
        (int)primaryFrameData.trackWindow.size(),
        (int)primaryFrameData.sustainWindow.size(),
        (int)primaryFrameData.gridlines.size(),
        slotCount, activePart, skill, viewportW, viewportH, isPlaying);
}

void DebugEditorController::paintOverChildren(juce::Graphics& g, HighwayComponent* primaryHighway,
                                                HighwayComponent* const* allHighways, int highwayCount,
                                                bool hasSlotsVisible)
{
    if (hasSlotsVisible && primaryHighway)
    {
        if (showProfilerOverlay)
            drawProfilerOverlay(g, primaryHighway->getSceneRenderer());

        // Collect per-highway timing
        HighwayProfileRecord hwRecords[MAX_PROFILED_HIGHWAYS] = {};
        int hwCount = std::min(highwayCount, (int)MAX_PROFILED_HIGHWAYS);
        for (int i = 0; i < hwCount; ++i)
        {
            auto& hw = *allHighways[i];
            auto& t = hw.getSceneRenderer().lastPhaseTiming;
            hwRecords[i].paint_us = (int)hw.debugHighwayPaint_us;
            hwRecords[i].scene_us = (int)t.total_us;
            hwRecords[i].notes_us = (int)t.notes_us;
            hwRecords[i].sustains_us = (int)t.sustains_us;
            hwRecords[i].execute_us = (int)t.execute_us;
            hwRecords[i].track_us = (int)hw.debugTrackRender_us;
        }

        frameProfileLogger.recordPaintData(
            primaryHighway->getSceneRenderer().lastPhaseTiming,
            primaryHighway->debugTrackRender_us,
            textureRender_us,
            primaryHighway->debugHighwayPaint_us,
            hwRecords, hwCount);
    }
    else
    {
        PhaseTiming empty;
        frameProfileLogger.recordPaintData(empty, 0.0, 0.0, 0.0);
    }
}

void DebugEditorController::drawProfilerOverlay(juce::Graphics& g, const SceneRenderer& sceneRenderer)
{
    auto& t = sceneRenderer.lastPhaseTiming;

    profilerRing[profilerRingIndex] = t.total_us;
    profilerRingIndex = (profilerRingIndex + 1) % PROFILER_RING_SIZE;

    frameDeltaRing[frameDeltaRingIndex] = frameDelta_us;
    frameDeltaRingIndex = (frameDeltaRingIndex + 1) % PROFILER_RING_SIZE;

    lockWaitRing[lockWaitRingIndex] = lockWait_us;
    lockWaitRingIndex = (lockWaitRingIndex + 1) % PROFILER_RING_SIZE;

    auto ringAvg = [](const double* ring, int size) {
        double sum = 0.0;
        int count = 0;
        for (int i = 0; i < size; ++i)
            if (ring[i] > 0.0) { sum += ring[i]; ++count; }
        return count > 0 ? sum / count : 0.0;
    };

    double avgTotal = ringAvg(profilerRing, PROFILER_RING_SIZE);
    double avgFrameDelta = ringAvg(frameDeltaRing, PROFILER_RING_SIZE);
    double avgLockWait = ringAvg(lockWaitRing, PROFILER_RING_SIZE);
    double fps = avgFrameDelta > 0.0 ? 1000000.0 / avgFrameDelta : 0.0;

    juce::String text;
    text << "FPS: " << juce::String(fps, 1) << "\n"
         << "Delta:   " << juce::String((int)avgFrameDelta) << " us\n"
         << "Lock:    " << juce::String((int)avgLockWait) << " us\n"
         << "Notes:   " << juce::String((int)t.notes_us) << " us\n"
         << "Sustain: " << juce::String((int)t.sustains_us) << " us\n"
         << "Grid:    " << juce::String((int)t.gridlines_us) << " us\n"
         << "Anim:    " << juce::String((int)t.animation_us) << " us\n"
         << "Texture: " << juce::String((int)textureRender_us) << " us\n"
         << "Execute: " << juce::String((int)t.execute_us) << " us\n"
         << "Total:   " << juce::String((int)t.total_us) << " us";

    auto layerUs = [&](DrawOrder d) -> int {
        return (int)t.layer_us[static_cast<int>(d)];
    };

    text << "\n---\n"
         << "Grid:    " << layerUs(DrawOrder::GRID) << " us\n"
         << "Lane:    " << layerUs(DrawOrder::LANE) << " us\n"
         << "Bar:     " << layerUs(DrawOrder::BAR) << " us\n"
         << "Sustain: " << layerUs(DrawOrder::SUSTAIN) << " us\n"
         << "Note:    " << layerUs(DrawOrder::NOTE) << " us\n"
         << "Overlay: " << layerUs(DrawOrder::OVERLAY) << " us";

    g.setFont(juce::Font(12.0f));
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillRect(4, 40, 140, 252);
    g.setColour(juce::Colours::white);
    g.drawFittedText(text, 8, 42, 132, 248, juce::Justification::topLeft, 18);
}

void DebugEditorController::buildStandaloneFrameData(HighwayFrameData& out,
                                             SceneRenderer& sceneRenderer,
                                             MidiInterpreter& midiInterpreter,
                                             double displaySizeInPPQ,
                                             double displayWindowTimeSeconds)
{
    if (!standalone || !playbackController.isNotesActive()) return;

    // Wire disco flip state to drum interpreters only
    bool isDrumSlot = midiInterpreter.instrumentPart == Part::DRUMS;
    midiInterpreter.setDiscoFlipState(isDrumSlot && discoFlipState.hasRegions() ? &discoFlipState : nullptr);

    PPQ trackWindowStartPPQ = playbackController.getCurrentPPQ();
    PPQ trackWindowEndPPQ = trackWindowStartPPQ + PPQ(displaySizeInPPQ);
    PPQ extendedStart = trackWindowStartPPQ - PPQ(displaySizeInPPQ);

    PartWindow partWindow = midiInterpreter.resolveAllDifficulties(extendedStart, trackWindowEndPPQ, trackWindowStartPPQ);
    SkillLevel activeSkill = (SkillLevel)((int)midiInterpreter.getState().getProperty("skillLevel"));
    auto& diffWindow = partWindow.forSkill(activeSkill);
    TrackWindow& ppqTrackWindow = diffWindow.trackWindow;
    SustainWindow& ppqSustainWindow = diffWindow.sustainWindow;

    double bpm = playbackController.getBPM();
    auto ppqToTime = [bpm](double ppq) { return ppq * (60.0 / bpm); };

    TempoTimeSignatureMap tempoTimeSigMap = debugMidiTempoMap;
    if (tempoTimeSigMap.empty())
        tempoTimeSigMap[PPQ(0.0)] = TempoTimeSignatureEvent(PPQ(0.0), bpm, 4, 4);

    PPQ cursorPPQ = trackWindowStartPPQ;
    TimeBasedTrackWindow timeTrackWindow = TimeConverter::convertTrackWindow(ppqTrackWindow, cursorPPQ, ppqToTime);
    TimeBasedSustainWindow timeSustainWindow = TimeConverter::convertSustainWindow(ppqSustainWindow, cursorPPQ, ppqToTime);
    TimeBasedGridlineMap timeGridlineMap = GridlineGenerator::generateGridlines(
        tempoTimeSigMap, extendedStart, trackWindowEndPPQ, cursorPPQ, ppqToTime);

    out.trackWindow = timeTrackWindow;
    out.sustainWindow = timeSustainWindow;
    out.gridlines = timeGridlineMap;
    out.windowStartTime = 0.0;
    out.windowEndTime = displayWindowTimeSeconds;
    out.isPlaying = playbackController.isPlaying();

    bool isProDrums = (int)statePtr->getProperty("drumType") == 2;
    bool discoEnabled = (bool)statePtr->getProperty("discoFlip");
    int midiDiff = (int)statePtr->getProperty("skillLevel") - 1;
    out.discoFlipActive = isDrumSlot && isProDrums && discoEnabled
                          && discoFlipState.isFlipped(trackWindowStartPPQ, midiDiff);

    // Convert flip region boundaries to time-relative for highway markers (drums only)
    if (isDrumSlot && isProDrums && discoEnabled && discoFlipState.hasRegions())
    {
        double cursorTime = ppqToTime(cursorPPQ.toDouble());
        for (const auto& r : discoFlipState.getRegions(midiDiff))
        {
            double startTime = ppqToTime(r.start.toDouble()) - cursorTime;
            double endTime   = ppqToTime(r.end.toDouble())   - cursorTime;
            if (startTime > 0.0 || endTime > -displayWindowTimeSeconds)
                out.flipRegions.push_back({startTime, endTime});
        }
    }
}

bool DebugEditorController::computeScrollOffset(float& outOffset, double displayWindowTimeSeconds) const
{
    if (!standalone) return false;

    double bpm = playbackController.getBPM();
    double ppq = playbackController.getCurrentPPQ().toDouble();

    int latencyOffsetMs = (int)statePtr->getProperty("latencyOffsetMs");
    double latencyOffsetBeats = (latencyOffsetMs / 1000.0) * (bpm / 60.0);
    ppq -= latencyOffsetBeats;

    double absoluteTime = ppq * (60.0 / bpm);
    double scrollRate = 1.0 / displayWindowTimeSeconds;
    outOffset = (float)(-absoluteTime * scrollRate);
    return true;
}

bool DebugEditorController::keyPressed(const juce::KeyPress& key, ToolbarComponent& toolbar)
{
    if (!standalone) return false;

    if (key == juce::KeyPress::spaceKey)
    {
        playbackController.togglePlay();
        toolbar.setDebugPlay(playbackController.isPlaying());
        return true;
    }
    return false;
}

bool DebugEditorController::mouseWheelMove(const juce::MouseWheelDetails& wheel, bool shiftDown,
                                            double scrollNormalBeats, double scrollShiftBeats)
{
    if (!standalone) return false;

    double wheelDelta = wheel.deltaY != 0.0 ? wheel.deltaY : wheel.deltaX;
    double jumpBeats = shiftDown ? scrollShiftBeats : scrollNormalBeats;
    playbackController.nudgePlayhead(wheelDelta * jumpBeats);
    return true;
}


void DebugEditorController::scanMidiDirectory()
{
    chartEntries.clear();
    chartEntries.push_back({"None", {}});

    juce::File midiDir(CHARTCHOTIC_MIDI_ASSET_DIR);
    if (!midiDir.isDirectory()) return;

    auto files = midiDir.findChildFiles(juce::File::findFiles, false, "*.mid");
    files.sort();

    for (auto& f : files)
    {
        juce::String name = f.getFileNameWithoutExtension().replaceCharacter('_', ' ');
        chartEntries.push_back({name, f});
    }
}

void DebugEditorController::loadDebugChart(int index)
{
    if (index <= 0 || index >= (int)chartEntries.size() || !chartEntries[index].file.existsAsFile())
    {
        debugMidiTempoMap.clear();
        // Notify with empty chart to clear highways
        if (onChartLoaded)
        {
            DebugMidiFilePlayer::LoadedChart empty;
            onChartLoaded(empty);
        }
        return;
    }

    juce::MemoryBlock data;
    chartEntries[index].file.loadFileAsData(data);

    auto result = DebugMidiFilePlayer::loadMidiFile(
        (const char*)data.getData(), (int)data.getSize());

    debugMidiTempoMap = result.tempoMap;
    debugChartLengthInBeats = result.lengthInBeats;
    playbackController.setBPM(result.initialBPM);

    // Update MidiProcessor's tempo map (global, shared)
    {
        const juce::ScopedLock tempoLock(processorPtr->getTempoLock());
        processorPtr->getTempoTimeSignatureMap() = result.tempoMap;
    }

    // Build disco flip state from PART DRUMS text events only
    auto drumTextIt = result.trackTextEvents.find("PART DRUMS");
    if (drumTextIt != result.trackTextEvents.end())
        discoFlipState.buildFromTextEvents(drumTextIt->second);
    else
        discoFlipState = DiscoFlipState();

    // Notify editor with full chart data — each slot processes its own track
    if (onChartLoaded)
        onChartLoaded(result);
}

#endif
