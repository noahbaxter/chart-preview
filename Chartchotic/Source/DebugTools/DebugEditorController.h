#pragma once

#ifdef DEBUG

#include <JuceHeader.h>
#include "DebugPlaybackController.h"
#include "DebugMidiFilePlayer.h"
#include "FrameProfileLogger.h"
#include "../Midi/DiscoFlipState.h"
#include "../Visual/HighwayComponent.h"
#include "../Visual/Utils/RenderTiming.h"
#include "../Visual/Utils/DrawingConstants.h"
#include "../Utils/ChartTypes.h"

class ChartchoticAudioProcessor;
class MidiInterpreter;
class ToolbarComponent;

class DebugEditorController
{
public:
    DebugEditorController();
    ~DebugEditorController();

    void init(juce::Component& parent, ChartchoticAudioProcessor& processor,
              juce::ValueTree& state, bool isStandalone);

    void wireCallbacks(ToolbarComponent& toolbar,
                       HighwayComponent& highway,
                       std::function<void()> repaintEditor);

    // --- Frame lifecycle (called from PluginEditor) ---

    // Call at top of onFrame(), after throttle check
    void beginFrame(double frameDelta_us);

    // Call at top of onFrame() — advances standalone playhead
    void onFrame(PPQ& lastKnownPosition, bool& lastPlayingState);

    // Call after building all slot frame data — logs frame metrics to TSV
    void recordFrameData(const HighwayFrameData& primaryFrameData, double dataBuild_us,
                         int slotCount, Part activePart, SkillLevel skill,
                         int viewportW, int viewportH, bool isPlaying);

    // --- Paint lifecycle ---

    // Call from paintOverChildren() — draws overlay + logs paint metrics to TSV
    void paintOverChildren(juce::Graphics& g, HighwayComponent* primaryHighway,
                           HighwayComponent* const* allHighways, int highwayCount,
                           bool hasSlotsVisible);

    // RAII lock wait measurement — use around interpreter calls
    ScopedPhaseMeasure measureLockWait() { return ScopedPhaseMeasure(lockWait_us, true); }

    // Called from paint() — draws profiler overlay
    void drawProfilerOverlay(juce::Graphics& g, const SceneRenderer& sceneRenderer);

    // Build frame data for standalone debug mode
    void buildStandaloneFrameData(HighwayFrameData& out,
                         SceneRenderer& sceneRenderer,
                         MidiInterpreter& midiInterpreter,
                         double displaySizeInPPQ,
                         double displayWindowTimeSeconds);

    // Scroll offset computation for standalone mode
    // Returns true if handled (caller should use outOffset), false if not standalone
    bool computeScrollOffset(float& outOffset, double displayWindowTimeSeconds) const;

    // Key/wheel handlers — return true if consumed
    bool keyPressed(const juce::KeyPress& key, ToolbarComponent& toolbar);
    bool mouseWheelMove(const juce::MouseWheelDetails& wheel, bool shiftDown,
                        double scrollNormalBeats, double scrollShiftBeats);

    // State queries
    bool isStandalone() const { return standalone; }
    bool isNotesActive() const { return playbackController.isNotesActive(); }
    const DiscoFlipState* getDiscoFlipState() const { return &discoFlipState; }

    // Callback when chart load discovers instrument parts — provides parts + per-track notes
    std::function<void(const DebugMidiFilePlayer::LoadedChart&)> onChartLoaded;

    // Reload current chart (e.g. after instrument switch)
    void reloadCurrentChart() { loadDebugChart(playbackController.getChartIndex()); }

    // Console
    juce::TextEditor& getConsole() { return consoleOutput; }
    juce::TextButton& getClearButton() { return clearLogsButton; }
    juce::TextButton& getCopyButton() { return copyLogsButton; }

    // Profiler timing targets (public for ScopedPhaseMeasure in buildFrameData)
    double textureRender_us = 0.0;
    double lockWait_us = 0.0;

private:
    bool standalone = false;
    bool showProfilerOverlay = false;
    double frameDelta_us = 0.0;
    double pendingDataBuild_us = 0.0;
    juce::ValueTree* statePtr = nullptr;
    ChartchoticAudioProcessor* processorPtr = nullptr;

    DebugPlaybackController playbackController;
    FrameProfileLogger frameProfileLogger;

    // Profiler
    static constexpr int PROFILER_RING_SIZE = 60;
    double profilerRing[PROFILER_RING_SIZE] = {};
    int profilerRingIndex = 0;
    double frameDeltaRing[PROFILER_RING_SIZE] = {};
    int frameDeltaRingIndex = 0;
    double lockWaitRing[PROFILER_RING_SIZE] = {};
    int lockWaitRingIndex = 0;

    // Console
    juce::TextEditor consoleOutput;
    juce::TextButton clearLogsButton;
    juce::TextButton copyLogsButton;

    // Debug chart loading (scans assets/midi/ directory)
    TempoTimeSignatureMap debugMidiTempoMap;
    double debugChartLengthInBeats = 0.0;
    DiscoFlipState discoFlipState;
    void loadDebugChart(int index);
    void scanMidiDirectory();

    struct DebugChartEntry { juce::String name; juce::File file; };
    std::vector<DebugChartEntry> chartEntries; // index 0 = "None" (empty)
};

#endif
