/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Host/ReaperTrackDetector.h"
#include "Midi/Processing/NoteProcessor.h"
#include "Visual/Utils/PositionMath.h"

// CI injects CHARTCHOTIC_VERSION_STRING with full version (e.g. 0.9.5-dev.20260226.abc1234)
// Falls back to JucePlugin_VersionString from .jucer (base semver), then "dev" for unset builds
#ifdef CHARTCHOTIC_VERSION_STRING
  #define CHARTCHOTIC_VERSION CHARTCHOTIC_VERSION_STRING
#elif defined(JucePlugin_VersionString)
  #define CHARTCHOTIC_VERSION JucePlugin_VersionString
#else
  #define CHARTCHOTIC_VERSION "dev"
#endif

//==============================================================================
ChartchoticAudioProcessorEditor::ChartchoticAudioProcessorEditor(ChartchoticAudioProcessor &p, juce::ValueTree &state)
    : AudioProcessorEditor(&p),
      state(state),
      audioProcessor(p),
      assetManager(),
      writeController(state),
      toolbar(state, writeController)
{
    // Create scratch renderer for shared track image cache
    cacheRenderer = std::make_unique<TrackRenderer>(state);

    // Pre-allocate all 4 highway slots (pooled, never destroyed)
    for (int i = 0; i < MAX_HIGHWAY_SLOTS; i++)
    {
        slots[i].highway = std::make_unique<HighwayComponent>(state, assetManager);
        slots[i].highway->setTrackImageCache(&trackImageCache);
        slots[i].highway->setVisible(false);

        // Wire mouse dispatch into the write controller (M1.3). Callbacks are
        // no-ops in M1; this just verifies events reach the controller.
        auto& hw = *slots[i].highway;
        hw.setOnPointerMove  ([this](const AuthoringPoint& p, const AuthoringContext& c) { writeController.onPointerMove (p, c); });
        hw.setOnPointerDown  ([this](const AuthoringPoint& p, const AuthoringContext& c) { writeController.onPointerDown (p, c); });
        hw.setOnPointerDrag  ([this](const AuthoringPoint& p, const AuthoringContext& c) { writeController.onPointerDrag (p, c); });
        hw.setOnPointerUp    ([this](const AuthoringPoint& p, const AuthoringContext& c) { writeController.onPointerUp   (p, c); });
        hw.setOnPointerExit  ([this]() { writeController.onPointerExit  (); });
        hw.setOnPointerCancel([this]() { writeController.onPointerCancel(); });

        // Coordinate-domain conversion: HitTestMapper returns "seconds offset
        // from cursor"; the controller wants project QN. Convert at the
        // dispatch boundary (M0-G design rule).
        hw.setSecondsToProjectQN([this](double secondsFromCursor) -> double {
            double cursorQN = lastKnownPosition.toDouble();
            auto& reaperProvider = audioProcessor.getReaperMidiProvider();
            if (reaperProvider.isReaperApiAvailable())
            {
                double cursorTime = reaperProvider.ppqToTime(cursorQN);
                return reaperProvider.timeToPpq(cursorTime + secondsFromCursor);
            }
            // Standard fallback: use instantaneous BPM.
            double bpm = defaultBPM;
            if (auto* playHead = audioProcessor.getPlayHead())
            {
                auto positionInfo = playHead->getPosition();
                if (positionInfo.hasValue())
                    bpm = positionInfo->getBpm().orFallback(defaultBPM);
            }
            return cursorQN + secondsFromCursor * (bpm / 60.0);
        });

        // Inverse of the above — used by the hover-ghost renderer to convert a
        // snapped project QN back into a seconds-from-cursor offset (then into a
        // normalized highway position).
        hw.setProjectQNToSeconds([this](double projectQN) -> double {
            double cursorQN = lastKnownPosition.toDouble();
            auto& reaperProvider = audioProcessor.getReaperMidiProvider();
            if (reaperProvider.isReaperApiAvailable())
            {
                double cursorTime = reaperProvider.ppqToTime(cursorQN);
                double targetTime = reaperProvider.ppqToTime(projectQN);
                return targetTime - cursorTime;
            }
            double bpm = defaultBPM;
            if (auto* playHead = audioProcessor.getPlayHead())
            {
                auto positionInfo = playHead->getPosition();
                if (positionInfo.hasValue())
                    bpm = positionInfo->getBpm().orFallback(defaultBPM);
            }
            return (projectQN - cursorQN) * (60.0 / bpm);
        });

        // Read access to the WriteController's overlay state (hover ghost lives there).
        hw.setOverlayStateGetter([this]() -> const OverlayState& {
            return writeController.getOverlayState();
        });
    }

    // Activate slot 0 as default
    {
        auto& slot = slots[0];
        slot.part = getPartFromState(state);
        slot.active = true;
        slot.interpreter = std::make_unique<MidiInterpreter>(
            state, audioProcessor.getNoteStateMapArray(), audioProcessor.getNoteStateMapLock());
        slot.highway->setActivePart(slot.part);
        slot.highway->setVisible(true);
        activeSlotCount = 1;
        writeController.setActivePart(slot.part);
    }
    setLookAndFeel(&chartPreviewLnF);

    // Set up resize constraints
    constrainer.setMinimumSize(minWidth, minHeight);
    setConstrainer(&constrainer);
    setResizable(true, true);

    // Restore saved window size
    int savedWidth = state.hasProperty("editorWidth") ? (int)state["editorWidth"] : defaultWidth;
    int savedHeight = state.hasProperty("editorHeight") ? (int)state["editorHeight"] : defaultHeight;
    savedWidth = juce::jlimit(minWidth, 4096, savedWidth);
    savedHeight = juce::jlimit(minHeight, 4096, savedHeight);
    setSize(savedWidth, savedHeight);

    setWantsKeyboardFocus(true);

    latencyInSeconds = audioProcessor.latencyInSeconds;

#ifdef DEBUG
    bool isStandalone = juce::PluginHostType::getPluginLoadedAs() == AudioProcessor::wrapperType_Standalone;
    debug.onChartLoaded = [this](const DebugMidiFilePlayer::LoadedChart& chart) {
        rebuildSlots(chart);
    };
    debug.init(*this, audioProcessor, state, isStandalone);
#endif

    assets.init(toolbar, [this](auto fn) { forAllHighways(fn); });
    session.init(state, toolbar, audioProcessor,
                 slots.data(), MAX_HIGHWAY_SLOTS, activeSlotCount,
                 {
                     [this]() { resized(); },
                     [this]() { loadState(); },
                     [this](float v) { return computeFarFadeEnd(v); },
                     [this](auto fn) { forEachHighway(fn); },
                     [this](auto fn) { forAllHighways(fn); }
                 });
    initToolbarCallbacks();
    toolbar.setLatencyOffsetRange(CALIBRATION_MIN_MS, CALIBRATION_MAX_MS);

#ifdef DEBUG
    toolbar.getTuningPanel().setAssetManager(assetManager);
#endif

    // Wire and add all pooled highway slots to component tree (hidden by default).
    // In standalone debug mode, rebuildSlots may have already replaced
    // the default slot during init — just re-wire to be safe.
    for (int i = 0; i < MAX_HIGHWAY_SLOTS; i++)
    {
        slots[i].highway->onOverflowChanged = [this]() { resized(); };
        addChildComponent(*slots[i].highway);
    }
    // Make active slots visible
    for (int i = 0; i < activeSlotCount; i++)
        slots[i].highway->setVisible(true);
    addAndMakeVisible(toolbar);

    // Floating write-mode chip — sits over the highway (top-left). Click
    // toggles write mode (same effect as W); state is pushed via the
    // WriteController::onStateChanged callback wired below.
    writeModeIcon.setState(writeController.writeModeActive(), writeController.subMode());
    writeModeIcon.onClick = [this]() {
        writeController.setWriteModeActive(!writeController.writeModeActive());
    };
    addAndMakeVisible(writeModeIcon);
    writeModeIcon.toFront(false);

    initBottomBar();

#ifdef DEBUG
    // Ensure debug console renders on top of highways
    debug.getConsole().toFront(false);
    debug.getClearButton().toFront(false);
    debug.getCopyButton().toFront(false);
#endif

    // If REAPER session already exists (editor recreated after track move), rebuild from it
    if (audioProcessor.isReaperHost)
    {
        toolbar.setReaperMode(true);
        toolbar.updateVisibility();
        if (auto* instrSession = audioProcessor.getInstrumentSession())
            session.rebuildFromSession(*instrSession);
    }

    loadState();
    resized();

    vblankAttachment = juce::VBlankAttachment(this, [this]() { onFrame(); });
}

ChartchoticAudioProcessorEditor::~ChartchoticAudioProcessorEditor()
{
    // Shut down async bake thread before destroying members it references
    trackImageCache.shutdown();

    setLookAndFeel(nullptr);

    // Clear static typeface refs so JUCE leak detector doesn't fire on shutdown.
    // Safe in plugin mode too — typefaces are lazily recreated on next use.
    Theme::clearTypefaces();
    ChartchoticLogo::clearTypefaces();
}

void ChartchoticAudioProcessorEditor::onFrame()
{
    if (targetFrameInterval > 0.0)
    {
        juce::int64 now = juce::Time::getHighResolutionTicks();
        double ticksPerSec = static_cast<double>(juce::Time::getHighResolutionTicksPerSecond());
        double elapsed = (now - lastFrameTicks) / ticksPerSec;
        if (elapsed < targetFrameInterval)
            return;
        // Advance by target interval to stay on a stable grid.
        // If we overshot by more than a full interval (e.g. app was suspended), snap to now.
        juce::int64 intervalTicks = static_cast<juce::int64>(targetFrameInterval * ticksPerSec);
        lastFrameTicks += intervalTicks;
        if ((now - lastFrameTicks) / ticksPerSec > targetFrameInterval)
            lastFrameTicks = now;
    }

    printCallback();

    // Frame delta tracking (used by FPS overlay and debug profiler)
    double frameDelta_us_local = 0.0;
    {
        double now = juce::Time::getHighResolutionTicks()
            / static_cast<double>(juce::Time::getHighResolutionTicksPerSecond());
        if (lastFrameTimestamp > 0.0)
        {
            frameDelta_us_local = (now - lastFrameTimestamp) * 1000000.0;
            fpsRing[fpsRingIndex] = frameDelta_us_local;
            fpsRingIndex = (fpsRingIndex + 1) % FPS_RING_SIZE;
        }
        lastFrameTimestamp = now;
    }

#ifdef DEBUG
    debug.beginFrame(frameDelta_us_local);
    debug.onFrame(lastKnownPosition, lastPlayingState);
#endif

    bool isReaperMode = audioProcessor.isReaperHost && audioProcessor.getReaperMidiProvider().isReaperApiAvailable();
    if (isReaperMode && !toolbar.isReaperModeActive())
    {
        toolbar.setReaperMode(true);
        toolbar.updateVisibility();
    }

    // Keep write controller in sync with the primary slot's part/skill. Cheap
    // (just stores enums) and idempotent — covers SessionController paths that
    // don't have a direct hook into the editor. Skill comes from state because
    // single-slot mode doesn't keep slot.skillLevel up to date with the toolbar.
    if (activeSlotCount > 0)
    {
        writeController.setActivePart(slots[0].part);
        const int skillId = state.hasProperty("skillLevel")
            ? (int)state.getProperty("skillLevel")
            : (int)SkillLevel::EXPERT;
        writeController.setActiveSkill((SkillLevel)skillId);
    }

    // MidiWriter and InstrumentSession only become non-null once REAPER has
    // connected. Re-wire each frame so M3.1 onPointerDown picks them up the
    // moment they appear (and clears them on tear-down).
    writeController.setMidiWriter(audioProcessor.getReaperMidiProvider().getWriter());
    writeController.setInstrumentSession(audioProcessor.getInstrumentSession());

    // Per-frame tick — controller uses this for hover refresh under stationary
    // cursor and to enforce playback-gated authoring (no-op in M1, real in M3).
    writeController.onFrameTick(lastKnownPosition.toDouble(), lastPlayingState);

    updateTrackInfoDisplay();

    // Create InstrumentSession on first REAPER detection (if multi-highway enabled)
    bool trackDiscovery = !state.hasProperty("trackDiscovery") || (bool)state["trackDiscovery"];
    if (isReaperMode && trackDiscovery && !audioProcessor.getInstrumentSession())
    {
        audioProcessor.createInstrumentSession();
        if (auto* instrSession = audioProcessor.getInstrumentSession())
            session.rebuildFromSession(*instrSession);
    }

    // Track position changes for render logic
    if (auto* playHead = audioProcessor.getPlayHead()) {
        auto positionInfo = playHead->getPosition();
        if (positionInfo.hasValue()) {
            PPQ currentPosition = PPQ(positionInfo->getPpqPosition().orFallback(0.0));
            bool isCurrentlyPlaying = positionInfo->getIsPlaying();

            lastKnownPosition = currentPosition;
            lastPlayingState = isCurrentlyPlaying;

            // REAPER mode: single owner of REAPER API traffic. Must run whether
            // playing or paused — the audio thread no longer calls REAPER API
            // (would deadlock during track duplication).
            // Hash-gated: pollReaperMidiHash only refetches when the track hash
            // actually changes, so the hot path is cheap during playback.
            if (isReaperMode)
            {
                juce::int64 now = juce::Time::getHighResolutionTicks();
                double elapsed = (now - lastCacheInvalidationTicks)
                    / static_cast<double>(juce::Time::getHighResolutionTicksPerSecond());
                if (elapsed >= 0.05)
                {
                    lastCacheInvalidationTicks = now;
                    audioProcessor.pollReaperMidiHash();

                    // Poll InstrumentSession for changes (re-fetch affected tracks)
                    if (auto* instrSession = audioProcessor.getInstrumentSession())
                    {
                        if (instrSession->pollForChanges())
                            session.rebuildFromSession(*instrSession);
                    }
                }
            }
            else
            {
                lastCacheInvalidationTicks = 0;
            }
        }
    }

    // Build frame data and push to each active highway slot
#ifdef DEBUG
    auto dataBuildStart = std::chrono::high_resolution_clock::now();
#endif
    auto ctx = buildFrameContext();
    HighwayFrameData primaryFrameData;

#ifdef DEBUG
    // Standalone debug mode: bypass normal pipeline
    if (!isReaperMode && debug.isStandalone() && debug.isNotesActive())
    {
        HighwayFrameData frameData;
        debug.buildStandaloneFrameData(frameData, primaryHighway().getSceneRenderer(),
                                       primaryInterpreter(), displaySizeInPPQ.toDouble(),
                                       displayWindowTimeSeconds);
        frameData.scrollOffset = computeScrollOffset();
        frameData.builtForPart = primaryInterpreter().instrumentPart;
        primaryFrameData = frameData;
        slots[0].highway->setFrameData(frameData);
        slots[0].highway->repaint();
    }
    else
#endif
    if (isReaperMode && activeSlotCount > 1)
    {
        FrameDataBuilder::buildReaperBatched(primaryFrameData, ctx);
    }
    else
    {
        for (int i = 0; i < activeSlotCount; i++)
        {
            auto& slot = slots[i];
            HighwayFrameData frameData;
            if (isReaperMode)
                FrameDataBuilder::buildReaper(frameData, ctx, *slot.interpreter);
            else
                FrameDataBuilder::buildStandard(frameData, ctx, *slot.interpreter);
            frameData.builtForPart = slot.interpreter->instrumentPart;

            if (i == 0)
                primaryFrameData = frameData;
            slot.highway->setFrameData(frameData);
            slot.highway->repaint();
        }
    }
#ifdef DEBUG
    double dataBuild_us = std::chrono::duration<double, std::micro>(
        std::chrono::high_resolution_clock::now() - dataBuildStart).count();
    {
        SkillLevel skill = (SkillLevel)(int)state.getProperty("skillLevel");
        Part part = activeSlotCount == 0 ? getPartFromState(state) : primaryHighway().getActivePart();
        int vpW = activeSlotCount == 0 ? 0 : primaryHighway().renderWidth;
        int vpH = activeSlotCount == 0 ? 0 : primaryHighway().renderHeight;
        debug.recordFrameData(primaryFrameData, dataBuild_us,
            activeSlotCount, part, skill, vpW, vpH, lastPlayingState);
    }
#endif

    repaint();
}

void ChartchoticAudioProcessorEditor::initToolbarCallbacks()
{
    toolbar.onSkillChanged = [this](int id) {
        state.setProperty("skillLevel", id, nullptr);
        propagateToSlots("skillLevel", id);
        if (session.isActive())
        {
            session.getEnabledDifficulties() = { (SkillLevel)id };
            toolbar.setEnabledDifficulties(session.getEnabledDifficulties());
            session.rebuildVisibleSlots();
        }
    };

    toolbar.onDifficultyClicked = [this](SkillLevel skill, bool modifierHeld) {
        if (!session.isActive()) return;

        auto& diffs = session.getEnabledDifficulties();
        if (modifierHeld)
        {
            if (diffs.count(skill))
                diffs.erase(skill);
            else
                diffs.insert(skill);
        }
        else
        {
            diffs.clear();
            diffs.insert(skill);
        }

        if (diffs.size() > 1)
            session.forceSingleInstrument();

        if (!diffs.empty())
            state.setProperty("skillLevel", (int)*diffs.begin(), nullptr);

        toolbar.setEnabledDifficulties(diffs);
        session.rebuildVisibleSlots();
    };

    toolbar.onAllDifficultiesClicked = [this]() {
        if (!session.isActive()) return;
        session.getEnabledDifficulties() = { SkillLevel::EASY, SkillLevel::MEDIUM,
                                SkillLevel::HARD, SkillLevel::EXPERT };

        session.forceSingleInstrument();

        toolbar.setEnabledDifficulties(session.getEnabledDifficulties());
        session.rebuildVisibleSlots();
    };

    toolbar.onInstrumentClicked = [this](Part part, bool modifierHeld) {
        if (!session.isActive()) return;

        auto& parts = session.getEnabledParts();
        if (modifierHeld)
        {
            if (parts.count(part))
                parts.erase(part);
            else
                parts.insert(part);
        }
        else
        {
            parts.clear();
            parts.insert(part);
        }

        if (parts.size() > 1)
            session.forceSingleDifficulty();

        toolbar.setEnabledParts(parts);
        session.rebuildVisibleSlots();
    };

    toolbar.onAllInstrumentsClicked = [this]() {
        if (!session.isActive()) return;
        auto& parts = session.getEnabledParts();
        parts.clear();
        for (auto p : session.getDiscoveredParts())
            parts.insert(p);

        session.forceSingleDifficulty();

        toolbar.setEnabledParts(parts);
        session.rebuildVisibleSlots();
    };

    toolbar.onPartChanged = [this](int id) {
        if (activeSlotCount == 0) return;
        state.setProperty("part", id, nullptr);
        Part newPart = getPartFromState(state);
        primaryInterpreter().instrumentPart = newPart;
        slots[0].part = newPart;
        writeController.setActivePart(newPart);

        if (!PositionMath::bemaniMode)
        {
            float userLength = state.hasProperty("highwayLength")
                ? (float)state["highwayLength"] : FAR_FADE_DEFAULT;
            float scaledLength = computeFarFadeEnd(userLength);
            forEachHighway([scaledLength](auto& hw) {
                hw.getSceneRenderer().farFadeEnd = scaledLength;
                PositionMath::bemaniHwyScale = scaledLength;
            });
            trackImageCache.invalidate();
            requestAsyncRebake();
        }
        updateDisplaySizeFromSpeedSlider();

        forEachHighway([](auto& hw) { hw.onInstrumentChanged(); });
#ifdef DEBUG
        toolbar.getTuningPanel().setDrums(isDrumLike(getPartFromState(state)));
#endif
        propagateToSlots("part", id);
    };

    toolbar.onDrumTypeChanged = [this](int id) {
        state.setProperty("drumType", id, nullptr);
        propagateToSlots("drumType", id);
    };

    toolbar.onAutoHopoChanged = [this](bool enabled) {
        state.setProperty("autoHopo", enabled, nullptr);
        propagateToSlots("autoHopo", enabled);
    };
    toolbar.onHopoThresholdChanged = [this](int thresholdIndex) {
        state.setProperty("hopoThresh", thresholdIndex, nullptr);
        propagateToSlots("hopoThresh", thresholdIndex);
    };

    toolbar.onGemsChanged = [this](bool on) { state.setProperty("showGems", on, nullptr); forEachHighway([on](auto& hw) { hw.setShowGems(on); }); };
    toolbar.onBarsChanged = [this](bool on) { state.setProperty("showBars", on, nullptr); forEachHighway([on](auto& hw) { hw.setShowBars(on); }); };
    toolbar.onSustainsChanged = [this](bool on) { state.setProperty("showSustains", on, nullptr); forEachHighway([on](auto& hw) { hw.setShowSustains(on); }); };
    toolbar.onLanesChanged = [this](bool on) { state.setProperty("showLanes", on, nullptr); forEachHighway([on](auto& hw) { hw.setShowLanes(on); }); };
    toolbar.onGridlinesChanged = [this](bool on) { state.setProperty("showGridlines", on, nullptr); forEachHighway([on](auto& hw) { hw.setShowGridlines(on); }); };

    toolbar.onHitIndicatorsChanged = [this](bool on) { state.setProperty("hitIndicators", on, nullptr); propagateToSlots("hitIndicators", on); };
    toolbar.onStarPowerChanged = [this](bool on) { state.setProperty("starPower", on, nullptr); propagateToSlots("starPower", on); };
    toolbar.onKick2xChanged = [this](bool on) { state.setProperty("kick2x", on, nullptr); propagateToSlots("kick2x", on); };
    toolbar.onDiscoFlipChanged = [this](bool on) { state.setProperty("discoFlip", on, nullptr); propagateToSlots("discoFlip", on); };
    toolbar.onDynamicsChanged = [this](bool on) { state.setProperty("dynamics", on, nullptr); propagateToSlots("dynamics", on); };

    toolbar.onTrackChanged = [this](bool on) { state.setProperty("showTrack", on, nullptr); propagateToSlots("showTrack", on); forEachHighway([on](auto& hw) { hw.setShowTrack(on); }); };
    toolbar.onLaneSeparatorsChanged = [this](bool on) { state.setProperty("showLaneSeparators", on, nullptr); propagateToSlots("showLaneSeparators", on); forEachHighway([on](auto& hw) { hw.setShowLaneSeparators(on); }); };
    toolbar.onStrikelineChanged = [this](bool on) { state.setProperty("showStrikeline", on, nullptr); propagateToSlots("showStrikeline", on); forEachHighway([on](auto& hw) { hw.setShowStrikeline(on); }); };
    toolbar.onHighwayChanged = [this](bool on) { state.setProperty("showHighway", on, nullptr); propagateToSlots("showHighway", on); forEachHighway([on](auto& hw) { hw.setShowHighway(on); }); };

    toolbar.onNoteSpeedChanged = [this](int speed) {
        state.setProperty("noteSpeed", speed, nullptr);
        updateDisplaySizeFromSpeedSlider();
        if (audioProcessor.isReaperHost && audioProcessor.getReaperMidiProvider().isReaperApiAvailable())
            audioProcessor.invalidateReaperCache();
    };

    toolbar.onFramerateChanged = [this](int id) {
        state.setProperty("framerate", id, nullptr);
        switch (id) {
        case 1: targetFrameInterval = 1.0 / 15.0; break;
        case 2: targetFrameInterval = 1.0 / 30.0; break;
        case 3: targetFrameInterval = 1.0 / 60.0; break;
        default: targetFrameInterval = 0.0; break;
        }
    };

    toolbar.onShowFpsChanged = [this](bool on) { showFps = on; };
    toolbar.onShowBackgroundChanged = [this](bool on) { assets.setBackgroundVisible(on); };

    toolbar.onTrackDiscoveryChanged = [this](bool on) {
        lastDisplayedTrackNumber = -1; // force status bar refresh
        lastDisplayedTrackName = {};
        if (on)
        {
            // Re-create InstrumentSession and rebuild from discovery
            if (!audioProcessor.getInstrumentSession())
            {
                audioProcessor.createInstrumentSession();
                if (auto* instrSession = audioProcessor.getInstrumentSession())
                    session.rebuildFromSession(*instrSession);
            }
        }
        else
        {
            // Tear down discovery, fall back to single-instrument mode.
            // Uses the processor's own noteStateMapArray (fed by ReaperMidiPipeline
            // reading from the plugin's track). Multi-difficulty still works.
            audioProcessor.destroyInstrumentSession();

            for (int i = 0; i < MAX_HIGHWAY_SLOTS; i++)
            {
                slots[i].active = false;
                slots[i].highway->setVisible(false);
            }

            auto& slot = slots[0];
            Part currentPart = getPartFromState(state);
            Part fallbackPart = isDrumLike(currentPart) ? Part::DRUMS : Part::GUITAR;
            state.setProperty("part", (int)fallbackPart, nullptr);
            slot.part = fallbackPart;
            slot.active = true;
            slot.ownedState.reset();
            slot.interpreter = std::make_unique<MidiInterpreter>(
                state, audioProcessor.getNoteStateMapArray(), audioProcessor.getNoteStateMapLock());
            slot.highway->setActivePart(slot.part);
            slot.highway->showPartLabel = false;
            slot.highway->showDifficultyLabel = false;
            slot.highway->setVisible(true);
            activeSlotCount = 1;
            writeController.setActivePart(slot.part);

            toolbar.setMultiInstrumentMode(false);
            toolbar.resetToManualMode();
            resized();
            loadState();
        }
    };

    toolbar.onLatencyChanged = [this](int id) { state.setProperty("latency", id, nullptr); applyLatencySetting(id); };
    toolbar.onLatencyOffsetChanged = [this](int offsetMs) {
        if (audioProcessor.isReaperHost && audioProcessor.getReaperMidiProvider().isReaperApiAvailable())
            audioProcessor.invalidateReaperCache();
    };

    toolbar.onBackgroundChanged = [this](const juce::String& bgName) { state.setProperty("background", bgName, nullptr); assets.loadBackground(bgName); };

    toolbar.onGemScaleChanged = [this](float scale) { state.setProperty("gemScale", scale, nullptr); propagateToSlots("gemScale", scale); forEachHighway([scale](auto& hw) { hw.setGemScale(scale); }); };
    toolbar.onBarScaleChanged = [this](float scale) { state.setProperty("barScale", scale, nullptr); propagateToSlots("barScale", scale); forEachHighway([scale](auto& hw) { hw.setBarScale(scale); }); };

    toolbar.onHighwayLengthChanged = [this](float length) {
        state.setProperty("highwayLength", length, nullptr);
        if (!PositionMath::bemaniMode)
        {
            float scaledLength = computeFarFadeEnd(length);
            forEachHighway([scaledLength](auto& hw) {
                hw.getSceneRenderer().farFadeEnd = scaledLength;
                PositionMath::bemaniHwyScale = scaledLength;
            });
            trackImageCache.invalidate();
            requestAsyncRebake();
        }
        else
        {
            forEachHighway([this, length](auto& hw) { hw.setHighwayLength(computeFarFadeEnd(length)); });
        }
        updateDisplaySizeFromSpeedSlider();
    };

    toolbar.onStretchChanged = [this](bool on) { forEachHighway([on](auto& hw) { hw.stretchToFill = on; }); state.setProperty("stretchToFill", on, nullptr); resized(); };

    toolbar.onBemaniModeChanged = [this](bool on) {
        PositionMath::bemaniMode = on;
        PositionMath::bemaniHwyScale = 1.0f;
        state.setProperty("bemaniMode", on, nullptr);
        updateDisplaySizeFromSpeedSlider();
        trackImageCache.invalidate();
        resized();
        toolbar.resized();
    };

    toolbar.onHighwayTextureChanged = [this](const juce::String& textureName) {
        state.setProperty("highwayTexture", textureName, nullptr);
        if (textureName.isEmpty()) forAllHighways([](auto& hw) { hw.clearTexture(); });
        else assets.loadHighwayTexture(textureName);
    };

    toolbar.onTextureScaleChanged = [this](float scale) { state.setProperty("textureScale", scale, nullptr); forAllHighways([scale](auto& hw) { hw.setTextureScale(scale); }); };
    toolbar.onTextureOpacityChanged = [this](float opacity) { state.setProperty("textureOpacity", opacity, nullptr); forAllHighways([opacity](auto& hw) { hw.setTextureOpacity(opacity); }); };

    toolbar.onOpenBackgroundFolder = [this]() { assets.getBackgroundDirectory().revealToUser(); };
    toolbar.onOpenTextureFolder = [this]() { assets.getHighwayTextureDirectory().revealToUser(); };

    // Refresh the floating mode chip + sub-toolbar whenever write-mode state
    // changes (W/Q toggles, [/]/S/T grid changes). When the sub-toolbar's
    // visibility flips, the toolbar reports a new height — trigger our own
    // resized() so the highway reclaims/yields the row's space.
    writeController.onStateChanged = [this]() {
        writeModeIcon.setState(writeController.writeModeActive(), writeController.subMode());
        bool wm = writeController.writeModeActive();
        forAllHighways([wm](auto& hw) { hw.setWriteMode(wm); });
        if (toolbar.refreshFromWriteController())
            resized();
    };

#ifdef DEBUG
    debug.wireCallbacks(toolbar, primaryHighway(), [this]() { repaint(); });
#endif
}

void ChartchoticAudioProcessorEditor::initBottomBar()
{
    footer.init(juce::String("v") + CHARTCHOTIC_VERSION);
    addAndMakeVisible(footer);

    updateBanner.onPromptDismissed = [this]() { audioProcessor.updatePromptDismissed = true; };
    updateChecker.onUpdateCheckComplete = [this](const UpdateChecker::UpdateInfo& info)
    {
        if (info.available)
        {
            bool autoPrompt = !audioProcessor.updatePromptDismissed;
            updateBanner.setUpdateInfo(info.version, info.downloadUrl, autoPrompt);
            footer.setUpdateAvailable();
            resized();
        }
    };
#ifndef DEBUG
    updateChecker.checkForUpdates();
#endif
}

void ChartchoticAudioProcessorEditor::updateTrackInfoDisplay()
{
    // Prefer generic track info (works in any DAW via JUCE/VST3)
    int trackNum = audioProcessor.detectedTrackNumber.load();
    juce::String trackName = audioProcessor.getDetectedTrackName();

    // REAPER fallback: use reaperTrack state property for track number
    if (trackNum < 0 && audioProcessor.isReaperHost)
        trackNum = (int)state.getProperty("reaperTrack"); // 1-based

    if (trackNum < 0)
    {
        if (lastDisplayedTrackNumber != -1)
        {
            footer.getStatusBar().clearTrackInfo();
            lastDisplayedTrackNumber = -1;
            lastDisplayedTrackName = {};
        }
        return;
    }

    // REAPER fallback: use REAPER API for track name if JUCE/VST3 didn't provide one
    if (trackName.isEmpty() && audioProcessor.isReaperHost
        && audioProcessor.getReaperMidiProvider().isReaperApiAvailable())
    {
        int trackIndex = (int)state.getProperty("reaperTrack") - 1;
        std::string reaperName = ReaperTrackDetector::getTrackName(
            audioProcessor.reaperMidiProvider.getReaperGetFunc(), trackIndex);
        trackName = juce::String(reaperName);
    }

    if (trackName.isEmpty())
        trackName = "Track " + juce::String(trackNum);

    // Only update UI when something changed
    if (trackNum == lastDisplayedTrackNumber && trackName == lastDisplayedTrackName)
        return;

    lastDisplayedTrackNumber = trackNum;
    lastDisplayedTrackName = trackName;
    footer.getStatusBar().setTrackInfo(trackNum, trackName);
}

//==============================================================================
void ChartchoticAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);

    if (assets.isBackgroundVisible())
        g.drawImage(assets.getCurrentBackground(), getLocalBounds().toFloat());

    // DAW icon in footer
    if (audioProcessor.isReaperHost && audioProcessor.attemptReaperConnection())
        footer.setDawIcon(assets.getReaperLogo());
    else
        footer.setDawIcon(nullptr);
}

void ChartchoticAudioProcessorEditor::paintOverChildren(juce::Graphics& g)
{
    // "Nothing selected" or "no parts detected" placeholder
    if (activeSlotCount == 0)
    {
        int tbHeight = toolbar.getHeight();
        auto area = getLocalBounds().withTrimmedTop(tbHeight);

        if (audioProcessor.isReaperHost && session.getDiscoveredParts().empty())
        {
            // No instrument tracks found — show naming guide
            float s = (float)getHeight() / (float)defaultHeight;
            float headerSize = 20.0f * s;
            float listSize = 14.0f * s;
            int lineH = juce::roundToInt(listSize * 1.6f);

            juce::StringArray trackNames;
            for (auto& entry : getImplementedTrackNames())
                trackNames.addIfNotAlreadyThere(entry.name);

            int blockH = juce::roundToInt(headerSize * 2.0f) + lineH * trackNames.size();
            int y = area.getCentreY() - blockH / 2;

            g.setColour(juce::Colours::white.withAlpha(0.6f));
            g.setFont(Theme::getUIFont(headerSize));
            g.drawText("No instrument tracks detected", area.getX(), y, area.getWidth(),
                       juce::roundToInt(headerSize * 1.5f), juce::Justification::centred);
            y += juce::roundToInt(headerSize * 2.0f);

            g.setColour(juce::Colours::white.withAlpha(0.35f));
            g.setFont(Theme::getUIFont(listSize));
            for (auto& name : trackNames)
            {
                g.drawText(name, area.getX(), y, area.getWidth(), lineH, juce::Justification::centred);
                y += lineH;
            }
        }
        else if (session.isActive())
        {
            g.setColour(juce::Colours::white.withAlpha(0.4f));
            g.setFont(Theme::getUIFont(16.0f));
            juce::String msg = session.getEnabledParts().empty() ? "Select an instrument"
                             : session.getEnabledDifficulties().empty() ? "Select a difficulty"
                             : "No data";
            g.drawText(msg, area, juce::Justification::centred);
        }
    }

    if (showFps)
        drawFpsOverlay(g);

#ifdef DEBUG
    {
        HighwayComponent* hwPtrs[MAX_PROFILED_HIGHWAYS] = {};
        int hwCount = std::min(activeSlotCount, (int)MAX_PROFILED_HIGHWAYS);
        for (int i = 0; i < hwCount; ++i)
            hwPtrs[i] = slots[i].highway.get();
        debug.paintOverChildren(g,
            activeSlotCount == 0 ? nullptr : &primaryHighway(),
            hwPtrs, hwCount, activeSlotCount > 0);
    }
#endif
}

void ChartchoticAudioProcessorEditor::resized()
{
    state.setProperty("editorWidth", getWidth(), nullptr);
    state.setProperty("editorHeight", getHeight(), nullptr);

    // Virtual scene dimensions
    sceneWidth = getWidth();
    // Toolbar scales with width, but also caps by height so panels fit vertically.
    // The 0.09 vertical ratio ensures toolbar + tallest panel + footer all fit.
    int fromWidth = juce::roundToInt(getWidth() * ToolbarComponent::toolbarRatio);
    int fromHeight = juce::roundToInt(getHeight() * 0.09f);
    int tbStripHeight = std::min({ fromWidth, fromHeight, ToolbarComponent::maxToolbarHeight });

    // Total toolbar height — grows when write mode is active (sub-toolbar row).
    int tbHeight = toolbar.getReportedHeight(tbStripHeight);

    if (PositionMath::bemaniMode)
    {
        // Bemani: enforce minimum aspect ratio, but grow taller to fill available space
        int minH = juce::roundToInt(sceneWidth / bemaniMinAspect);
        int available = getHeight() - tbHeight;
        sceneHeight = std::max(minH, available);
    }
    else
    {
        sceneHeight = juce::roundToInt(sceneWidth / sceneAspectRatio);
    }

    const int margin = 10;

    // Toolbar at top — scales with editor width. Height includes sub-toolbar
    // row when write mode is active.
    toolbar.setBounds(0, 0, getWidth(), tbHeight);

    // Floating mode chip — top-left of the highway, just below the toolbar.
    // Sized off the toolbar strip so it scales with the rest of the UI.
    {
        int chipSize = juce::jmax(20, juce::roundToInt(tbStripHeight * 0.95f));
        int chipInset = juce::jmax(6, juce::roundToInt(tbStripHeight * 0.22f));
        writeModeIcon.setBounds(chipInset, tbHeight + chipInset, chipSize, chipSize);
        writeModeIcon.toFront(false);
    }

    // Footer bar height — same min(width, height) logic as toolbar
    int footerFromWidth = juce::roundToInt(getWidth() * FooterComponent::footerRatio);
    int footerFromHeight = juce::roundToInt(getHeight() * 0.06f);
    int footerH = std::min({ footerFromWidth, footerFromHeight, FooterComponent::maxFooterHeight });

    // Tell popup panels where the footer starts so they don't overlap it
    toolbar.setPanelBottomMargin(footerH);

    // Layout highway slots below toolbar, above footer
    if (activeSlotCount > 0)
    {
        int contentW = getWidth();
        int contentH = getHeight() - tbHeight - footerH;
        int numSlots = activeSlotCount;

        if (numSlots == 1)
        {
            auto& hw = *slots[0].highway;
            hw.renderWidth = sceneWidth;
            hw.renderHeight = sceneHeight;
            hw.updateOverflow();
            hw.setBounds(0, tbHeight, contentW, contentH);
        }
        else
        {
            // Determine grid layout based on aspect ratio and slot count
            //   Bemani: always a single horizontal row
            //   wide  (aspect > 1.5): single row
            //   tall  (aspect < 0.7): single column
            //   square-ish:           grid (2x2 for 3-4 slots, row for 2)
            float aspect = (float)contentW / (float)std::max(1, contentH);
            int cols, rows;

            if (PositionMath::bemaniMode)
            {
                cols = numSlots;
                rows = 1;
            }
            else if (numSlots <= 2)
            {
                if (aspect > 1.0f) { cols = numSlots; rows = 1; }
                else               { cols = 1; rows = numSlots; }
            }
            else
            {
                // 3-4 slots
                if (aspect > 1.5f)      { cols = numSlots; rows = 1; }
                else if (aspect < 0.7f) { cols = 1; rows = numSlots; }
                else                    { cols = 2; rows = (numSlots + 1) / 2; }
            }

            int pad = highwayGridPadding;

            // Uniform padding: pad between every slot and at window edges.
            // Total horizontal padding = (cols + 1) gaps × pad pixels.
            int totalPadX = (cols + 1) * pad;
            int totalPadY = (rows + 1) * pad;
            int slotBaseW = (contentW - totalPadX) / cols;
            int slotBaseH = (contentH - totalPadY) / rows;

            for (int i = 0; i < numSlots; ++i)
            {
                auto& hw = *slots[i].highway;
                int col = i % cols;
                int row = i / cols;

                int slotX = pad + col * (slotBaseW + pad);
                int slotY = tbHeight + pad + row * (slotBaseH + pad);
                int slotW = slotBaseW;
                int slotH = slotBaseH;

                // Each slot gets its own scene dimensions based on its bounds
                if (PositionMath::bemaniMode)
                {
                    hw.renderWidth = slotW;
                    int minH = juce::roundToInt(slotW / bemaniMinAspect);
                    hw.renderHeight = std::max(minH, slotH);
                }
                else
                {
                    hw.renderWidth = slotW;
                    hw.renderHeight = juce::roundToInt(slotW / sceneAspectRatio);
                }

                hw.updateOverflow();
                hw.setBounds(slotX, slotY, slotW, slotH);
            }
        }
    }

    footer.label.setFont(Theme::getUIFont(Theme::fontSize));
    footer.setBounds(0, getHeight() - footerH, getWidth(), footerH);

    // Rebake shared track cache when scene dimensions change or cache was externally invalidated
    if (trackImageCache.isDirty() ||
        sceneWidth != trackImageCache.getBakedWidth() ||
        sceneHeight != trackImageCache.getBakedHeight())
    {
        trackImageCache.invalidate();
        requestAsyncRebake();
    }

    #ifdef DEBUG
    // Debug console floats below the entire toolbar (including the
    // sub-toolbar row when write mode is active).
    int debugTop = tbHeight;
    debug.getClearButton().setBounds(margin, debugTop + 4, 100, 20);
    debug.getCopyButton().setBounds(margin + 104, debugTop + 4, 60, 20);
    debug.getConsole().setBounds(margin, debugTop + 28, getWidth() - (2 * margin), getHeight() - debugTop - 38);
    #endif

}

void ChartchoticAudioProcessorEditor::updateDisplaySizeFromSpeedSlider()
{
    // Convert note speed to highway time: 7.87 is the default highway length in world units,
    // so at note speed N, notes take 7.87/N seconds to reach the strikeline.
    int noteSpeed = state.hasProperty("noteSpeed") ? (int)state["noteSpeed"] : NOTE_SPEED_DEFAULT;

    // Same formula for both modes; Bemani applies ratio so same speed value feels equivalent.
    // Flat viewport has no foreshortening, so it needs a longer time window to match
    // the readable note density of perspective mode.
    displayWindowTimeSeconds = 7.87 / (double)noteSpeed;
    if (PositionMath::bemaniMode)
        displayWindowTimeSeconds *= (double)NOTE_SPEED_BEMANI_RATIO;

    // PPQ window must cover the full visible highway (farFadeEnd * window time) at worst-case tempo.
    // At 300 BPM, 1 second = 5 quarter notes. Base window of 30 PPQ scaled by highway length.
    const double BASE_PPQ_WINDOW = 30.0;
    double hwScale = activeSlotCount == 0 ? 1.0 : std::max(1.0, (double)primaryHighway().getSceneRenderer().farFadeEnd);
    displaySizeInPPQ = PPQ(BASE_PPQ_WINDOW * hwScale);

    // Sync the display size to the processor so processBlock can use it
    audioProcessor.setDisplayWindowSize(displaySizeInPPQ);
}

void ChartchoticAudioProcessorEditor::loadState()
{
    // Migrate old redBackground bool to new background string
    if (state.hasProperty("redBackground") && !state.hasProperty("background"))
    {
        if ((bool)state["redBackground"] && assets.getBackgroundNames().contains("background_red"))
            state.setProperty("background", "background_red", nullptr);
        state.removeProperty("redBackground", nullptr);
    }

    // Background visibility (default off)
    assets.setBackgroundVisible(state.hasProperty("showBackground") && (bool)state["showBackground"]);

    // Load saved background image
    {
        juce::String savedBg = state.getProperty("background").toString();
        assets.loadBackground(savedBg);
    }

    // Default to gothic if no saved preference (first launch only)
    if (!state.hasProperty("highwayTexture") && assets.getHighwayTextureNames().contains("gothic_default"))
        state.setProperty("highwayTexture", "gothic_default", nullptr);

    toolbar.loadState();

    // Restore render toggle flags (default ON if property missing from saved state)
    {
        bool gems = !state.hasProperty("showGems") || (bool)state["showGems"];
        bool bars = !state.hasProperty("showBars") || (bool)state["showBars"];
        bool sustains = !state.hasProperty("showSustains") || (bool)state["showSustains"];
        bool lanes = !state.hasProperty("showLanes") || (bool)state["showLanes"];
        bool gridlines = !state.hasProperty("showGridlines") || (bool)state["showGridlines"];
        bool track = !state.hasProperty("showTrack") || (bool)state["showTrack"];
        bool laneSeps = !state.hasProperty("showLaneSeparators") || (bool)state["showLaneSeparators"];
        bool strikeline = !state.hasProperty("showStrikeline") || (bool)state["showStrikeline"];
        bool hwy = !state.hasProperty("showHighway") || (bool)state["showHighway"];

        forEachHighway([=](auto& hw) {
            hw.getSceneRenderer().showGems = gems;
            hw.getSceneRenderer().showBars = bars;
            hw.getSceneRenderer().showSustains = sustains;
            hw.getSceneRenderer().showLanes = lanes;
            hw.getSceneRenderer().showGridlines = gridlines;
            hw.getSceneRenderer().showTrack = track;
            hw.getSceneRenderer().showLaneSeparators = laneSeps;
            hw.getSceneRenderer().showStrikeline = strikeline;
            hw.showHighway = hwy;
        });
    }

    // Restore highway length (apply per-instrument scale)
    {
        float userLength = state.hasProperty("highwayLength")
            ? juce::jlimit(FAR_FADE_MIN, FAR_FADE_MAX, (float)state["highwayLength"])
            : FAR_FADE_DEFAULT;
        float scaledLength = computeFarFadeEnd(userLength);
        forEachHighway([scaledLength](auto& hw) {
            hw.getSceneRenderer().farFadeEnd = scaledLength;
        });
        PositionMath::bemaniHwyScale = scaledLength;
    }

    // Load highway texture
    juce::String savedTexture = state.getProperty("highwayTexture").toString();
    if (savedTexture.isNotEmpty() && assets.getHighwayTextureNames().contains(savedTexture))
        assets.loadHighwayTexture(savedTexture);

    // Restore texture parameters (all pooled highways, not just active)
    if (state.hasProperty("textureScale"))
    {
        float ts = (float)state["textureScale"];
        forAllHighways([ts](auto& hw) { hw.getTrackRenderer().textureScale = ts; });
    }
    if (state.hasProperty("textureOpacity"))
    {
        float to = juce::jlimit(0.05f, 1.0f, (float)state["textureOpacity"]);
        forAllHighways([to](auto& hw) { hw.getTrackRenderer().textureOpacity = to; });
    }

    // Apply side-effects that listeners would normally do
    applyLatencySetting((int)state["latency"]);
    int frId = (int)state["framerate"];
    // Migrate old 120/144 FPS options (ids 4,5) and unset (0) to Native (id 4)
    if (frId < 1 || frId > 4)
        frId = 4;
    targetFrameInterval = (frId == 1) ? 1.0 / 15.0 :
                          (frId == 2) ? 1.0 / 30.0 :
                          (frId == 3) ? 1.0 / 60.0 : 0.0;

    showFps = (bool)state.getProperty("showFps", false);

    // Restore stretch mode
    {
        bool stretch = state.hasProperty("stretchToFill") && (bool)state["stretchToFill"];
        forEachHighway([stretch](auto& hw) { hw.stretchToFill = stretch; });
    }

    // Restore Bemani mode
    PositionMath::bemaniMode = state.hasProperty("bemaniMode") && (bool)state["bemaniMode"];
    PositionMath::bemaniHwyScale = 1.0f;

    updateDisplaySizeFromSpeedSlider();
    trackImageCache.invalidate();
    forEachHighway([](auto& hw) { hw.rebuildTrack(); });
}


void ChartchoticAudioProcessorEditor::requestAsyncRebake()
{
    if (!trackImageCache.isDirty() || activeSlotCount == 0)
        return;

    // Always bake at full single-highway resolution (sceneWidth × sceneHeight),
    // NOT at per-slot dimensions which are smaller in multi-highway mode.
    int w = sceneWidth;
    int h = sceneHeight;
    if (w <= 0 || h <= 0) return;

    // Compute overflow at full resolution
    auto& sr = slots[0].highway->getSceneRenderer();
    auto farEdgeGuitar = PositionMath::getFretboardEdge(
        false, sr.farFadeEnd, w, h,
        PositionConstants::HIGHWAY_POS_START, sr.highwayPosEnd);
    auto farEdgeDrums = PositionMath::getFretboardEdge(
        true, sr.farFadeEnd, w, h,
        PositionConstants::HIGHWAY_POS_START, sr.highwayPosEnd);
    int overflowGuitar = std::max(0, (int)std::ceil(-farEdgeGuitar.centerY));
    int overflowDrums  = std::max(0, (int)std::ceil(-farEdgeDrums.centerY));

    auto& hw = *slots[0].highway;

    trackImageCache.requestBake(*cacheRenderer, w, h, overflowGuitar, overflowDrums,
        sr.farFadeEnd, sr.farFadeLen, sr.farFadeCurve, sr.highwayPosEnd,
        sr.guitarLaneCoordsLocal, (int)PositionConstants::GUITAR_LANE_COUNT,
        sr.drumLaneCoordsLocal, (int)PositionConstants::DRUM_LANE_COUNT,
        hw.getTrackRenderer().textureScale, hw.getTrackRenderer().textureOpacity,
        [this]() {
            // If more invalidations arrived while we were baking, re-trigger
            if (trackImageCache.isDirty())
            {
                requestAsyncRebake();
                return;
            }

            // Called on message thread after staging committed
            forEachHighway([](auto& hw) { hw.rebuildTrack(); });
        });
}

#ifdef DEBUG
void ChartchoticAudioProcessorEditor::rebuildSlots(const DebugMidiFilePlayer::LoadedChart& chart)
{
    // Map Part enum to MIDI track name
    auto trackNameForPart = [](Part p) -> juce::String {
        switch (p) {
            case Part::GUITAR:     return "PART GUITAR";
            case Part::GUITAR_COOP: return "PART GUITAR COOP";
            case Part::RHYTHM:     return "PART RHYTHM";
            case Part::BASS:       return "PART BASS";
            case Part::KEYS:       return "PART KEYS";
            case Part::DRUMS:      return "PART DRUMS";
            case Part::VOCALS:     return "PART VOCALS";
            default:               return "PART GUITAR";
        }
    };

    // Hide all pooled slots
    for (int i = 0; i < MAX_HIGHWAY_SLOTS; i++)
    {
        slots[i].active = false;
        slots[i].highway->setVisible(false);
    }

    // If no parts found, configure slot 0 as default
    if (chart.foundParts.empty())
    {
        auto& slot = slots[0];
        slot.part = getPartFromState(state);
        slot.active = true;
        slot.ownedState.reset();
        slot.interpreter = std::make_unique<MidiInterpreter>(
            state, audioProcessor.getNoteStateMapArray(), audioProcessor.getNoteStateMapLock());
        slot.highway->setActivePart(slot.part);
        slot.highway->setVisible(true);
        activeSlotCount = 1;
        writeController.setActivePart(slot.part);

        resized();
        loadState();
        return;
    }

    // Configure one slot per discovered part (up to MAX_HIGHWAY_SLOTS)
    int slotIdx = 0;
    for (Part part : chart.foundParts)
    {
        if (slotIdx >= MAX_HIGHWAY_SLOTS) break;

        auto& slot = slots[slotIdx];
        slot.part = part;
        slot.active = true;
        slot.ownedState.reset();

        slot.noteStateMapArray = std::make_shared<NoteStateMapArray>();
        slot.noteStateMapLock = std::make_shared<juce::CriticalSection>();

        // Process this instrument's notes into the slot's own NoteStateMapArray
        juce::String tName = trackNameForPart(part);
        auto notesIt = chart.trackNotes.find(tName);
        if (notesIt != chart.trackNotes.end())
        {
            const juce::ScopedLock lock(*slot.noteStateMapLock);
            for (auto& map : *slot.noteStateMapArray)
                map.clear();

            NoteProcessor noteProcessor;
            noteProcessor.processAllNotes(notesIt->second, *slot.noteStateMapArray);
        }

        slot.interpreter = std::make_unique<MidiInterpreter>(
            part, state, *slot.noteStateMapArray, *slot.noteStateMapLock);
        slot.highway->setActivePart(part);
        slot.highway->setVisible(true);
        slotIdx++;
    }

    activeSlotCount = slotIdx;

    // Sync write controller to the primary slot's part.
    if (activeSlotCount > 0)
        writeController.setActivePart(slots[0].part);

    // Multi-highway mode: show part labels and all toolbar options
    bool multiSlot = activeSlotCount > 1;
    for (int i = 0; i < activeSlotCount; i++)
        slots[i].highway->showPartLabel = multiSlot;
    toolbar.setMultiInstrumentMode(multiSlot);

    resized();
    loadState();
}
#endif

void ChartchoticAudioProcessorEditor::applyLatencySetting(int latencyValue)
{
    switch (latencyValue) {
    case 1: latencyInSeconds = 0.000; break;
    case 2: latencyInSeconds = 0.250; break;
    case 3: latencyInSeconds = 0.500; break;
    case 4: latencyInSeconds = 0.750; break;
    case 5: latencyInSeconds = 1.000; break;
    case 6: latencyInSeconds = 1.500; break;
    default: latencyInSeconds = 0.500; break;
    }
    audioProcessor.setLatencyInSeconds(latencyInSeconds);
}

juce::ComponentBoundsConstrainer* ChartchoticAudioProcessorEditor::getConstrainer()
{
    return &constrainer;
}

void ChartchoticAudioProcessorEditor::parentSizeChanged()
{
    AudioProcessorEditor::parentSizeChanged();
}


float ChartchoticAudioProcessorEditor::computeScrollOffset()
{
#ifdef DEBUG
    {
        float debugOffset;
        if (debug.computeScrollOffset(debugOffset, displayWindowTimeSeconds))
            return debugOffset;
    }
#endif

    // Use absolute time from the playhead — immune to BPM/time sig changes
    double absoluteTime = 0.0;
    if (auto* playHead = audioProcessor.getPlayHead())
    {
        auto positionInfo = playHead->getPosition();
        if (positionInfo.hasValue())
            absoluteTime = positionInfo->getTimeInSeconds().orFallback(0.0);
    }

    // Apply latency offset to match note rendering position
    int latencyOffsetMs = (int)state.getProperty("latencyOffsetMs");
    absoluteTime -= latencyOffsetMs / 1000.0;

    // Raw continuous scroll value — consumers do their own modulo against tile size.
    // No wrapping here avoids double-modulo precision snaps.
    double scrollRate = 1.0 / displayWindowTimeSeconds;
    return (float)(-absoluteTime * scrollRate);
}

FrameContext ChartchoticAudioProcessorEditor::buildFrameContext()
{
    WriteGridConfig wgc;
    wgc.active        = writeController.writeModeActive() && writeController.snapEnabled();
    wgc.stepDivision  = writeController.stepDivision();
    wgc.tuplet        = writeController.tuplet();

    return FrameContext {
        audioProcessor,
        state,
        lastKnownPosition,
        displaySizeInPPQ,
        displayWindowTimeSeconds,
        computeScrollOffset(),
        smoothedLatencyInPPQ(),
        slots.data(),
        activeSlotCount,
        wgc
    };
}

void ChartchoticAudioProcessorEditor::drawFpsOverlay(juce::Graphics& g)
{
    double sum = 0.0;
    int count = 0;
    for (int i = 0; i < FPS_RING_SIZE; ++i)
        if (fpsRing[i] > 0.0) { sum += fpsRing[i]; ++count; }
    double avgDelta = count > 0 ? sum / count : 0.0;
    double fps = avgDelta > 0.0 ? 1000000.0 / avgDelta : 0.0;

    float s = (float)getHeight() / (float)defaultHeight;
    int tbHeight = std::min(juce::roundToInt(getHeight() * ToolbarComponent::toolbarRatio),
                            ToolbarComponent::maxToolbarHeight);
    int pad = juce::roundToInt(8.0f * s);
    int textH = juce::roundToInt(16.0f * s);
    auto area = juce::Rectangle<int>(pad, tbHeight + pad, juce::roundToInt(80.0f * s), textH);

    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillRoundedRectangle(area.toFloat().expanded(4.0f * s, 2.0f * s), 3.0f);
    g.setColour(juce::Colours::white);
    g.setFont(Theme::getUIFont(12.0f * s));
    g.drawText(juce::String(fps, 1) + " FPS", area, juce::Justification::centredLeft);
}

