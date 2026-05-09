/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <array>
#include <map>
#include <set>
#include <unordered_map>
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Visual/HighwaySlot.h"
#include "Visual/TrackImageCache.h"
#include "Visual/Managers/GridlineGenerator.h"
#include "Utils/ChartTypes.h"
#include "Midi/Utils/TimeConverter.h"
#include "Visual/Utils/LatencySmoother.h"
#include "UI/UpdateChecker.h"
#include "UI/LookAndFeel/ChartchoticLookAndFeel.h"
#include "UI/ToolbarComponent.h"
#include "UI/Controls/WriteModeIcon.h"
#include "UI/UpdateBannerComponent.h"
#include "UI/FooterComponent.h"
#include "Editor/AssetController.h"
#include "Editor/SessionController.h"
#include "Editor/FrameDataBuilder.h"
#include "Editor/WriteController.h"
#include "Editor/EditController.h"
#ifdef DEBUG
#include "DebugTools/DebugEditorController.h"
#endif

//==============================================================================

class ChartchoticAudioProcessorEditor  :
    public juce::AudioProcessorEditor
{
public:
    ChartchoticAudioProcessorEditor (ChartchoticAudioProcessor&, juce::ValueTree &state);
    ~ChartchoticAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;

    // Resizable constraints
    juce::ComponentBoundsConstrainer* getConstrainer();
    void parentSizeChanged() override;

    //==============================================================================
    // UI Callbacks

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (writeController.writeModeActive()
            && writeController.subMode() == SubMode::Edit)
        {
            if (editController.onKeyPress(key))
                return true;
        }
        if (writeController.onKeyPress(key))
            return true;

#ifdef DEBUG
        if (debug.keyPressed(key, toolbar))
            return true;
#endif

        return false;
    }

    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {

#ifdef DEBUG
        if (debug.mouseWheelMove(wheel, event.mods.isShiftDown(),
                                            SCROLL_NORMAL_BEATS, SCROLL_SHIFT_BEATS))
            return;
#endif

        // Get the current playhead position
        if (auto* playHead = audioProcessor.getPlayHead())
        {
            auto positionInfo = playHead->getPosition();
            if (positionInfo.hasValue())
            {
                double currentPPQ = positionInfo->getPpqPosition().orFallback(0.0);
                double jumpBeats = event.mods.isShiftDown() ? SCROLL_SHIFT_BEATS : SCROLL_NORMAL_BEATS;

                // Note: when shift is held, deltaY might be in deltaX instead
                double wheelDelta = wheel.deltaY != 0.0 ? wheel.deltaY : wheel.deltaX;
                double jumpAmount = wheelDelta * jumpBeats;

                double newPPQ = currentPPQ + jumpAmount;
                newPPQ = std::max(0.0, newPPQ);  // Clamp to 0

                audioProcessor.requestTimelinePositionChange(PPQ(newPPQ));
            }
        }
    }

private:
    juce::ValueTree& state;

    ChartchoticAudioProcessor& audioProcessor;
    AssetManager assetManager;

    static constexpr int MAX_HIGHWAY_SLOTS = 4;
    std::array<HighwaySlot, MAX_HIGHWAY_SLOTS> slots;
    int activeSlotCount = 0;

    // Shared track image cache (guitar + drums baked once at full resolution)
    TrackImageCache trackImageCache;
    std::unique_ptr<TrackRenderer> cacheRenderer;
    void requestAsyncRebake();

    HighwayComponent& primaryHighway() { return *slots[0].highway; }
    MidiInterpreter& primaryInterpreter() { return *slots[0].interpreter; }

    template <typename Fn>
    void forEachHighway(Fn&& fn)
    {
        for (int i = 0; i < activeSlotCount; i++)
            fn(*slots[i].highway);
    }

    template <typename Fn>
    void forEachActiveSlot(Fn&& fn)
    {
        for (int i = 0; i < activeSlotCount; i++)
            fn(slots[i]);
    }

    // Iterate ALL pooled highways (including inactive/hidden ones).
    // Use for texture/asset operations that must be set on all slots upfront.
    template <typename Fn>
    void forAllHighways(Fn&& fn)
    {
        for (int i = 0; i < MAX_HIGHWAY_SLOTS; i++)
            fn(*slots[i].highway);
    }

    void propagateToSlots(const juce::Identifier& prop, const juce::var& value)
    {
        for (int i = 0; i < activeSlotCount; i++)
            if (slots[i].ownedState)
                slots[i].ownedState->setProperty(prop, value, nullptr);
    }

    // Custom look and feel
    ChartchoticLookAndFeel chartPreviewLnF;

    // Write-mode controller (must be declared before toolbar — toolbar holds a reference)
    WriteController writeController;
    EditController editController;

    // UI Components
    ToolbarComponent toolbar;

    // Floating mode chip — owned by the editor so it sits over the highway
    // (top-left, just below the toolbar). Click toggles write mode; key
    // shortcuts (W/Q) still work via WriteController.
    WriteModeIcon writeModeIcon;

    //==============================================================================
    // UI Elements
    static constexpr int defaultWidth = 600;
    static constexpr int defaultHeight = 600;
    static constexpr int minWidth = 200;
    static constexpr int minHeight = 200;
    static constexpr double sceneAspectRatio = 4.0 / 3.0;
    static constexpr int highwayGridPadding = 4;        // px between highways and window edges in multi-slot grid
    static constexpr double bemaniMinAspect = 1.0 / 2.0; // minimum width:height ratio for Bemani highways

    // Virtual scene dimensions (maintain internal 4:3 ratio)
    int sceneWidth = defaultWidth;
    int sceneHeight = defaultHeight;

    // Asset management (backgrounds, textures, logos)
    AssetController assets;

    // Session/slot management (multi-highway, instrument/difficulty selection)
    SessionController session;

    UpdateBannerComponent updateBanner;
    FooterComponent footer { updateBanner };
    UpdateChecker updateChecker;

    //==============================================================================

    void initToolbarCallbacks();
    void initBottomBar();
    void loadState();
    void updateTrackInfoDisplay();
#ifdef DEBUG
    void rebuildSlots(const DebugMidiFilePlayer::LoadedChart& chart);
#endif
    void updateDisplaySizeFromSpeedSlider();
    void applyLatencySetting(int latencyValue);

    FrameContext buildFrameContext();

    float computeScrollOffset();

    float latencyInSeconds = 0.0;

    // Resize constraints
    juce::ComponentBoundsConstrainer constrainer;

    // Position tracking for cursor vs playhead separation
    PPQ lastKnownPosition = 0.0;
    bool lastPlayingState = false;

    PPQ displaySizeInPPQ = 1.5; // Only used for MIDI window fetching
    double displayWindowTimeSeconds = 1.0; // Actual render window time in seconds

    // Compute farFadeEnd from user slider value × per-instrument scale
    float computeFarFadeEnd(float userLength) const
    {
        bool drums = activeSlotCount == 0 ? isDrumLike(getPartFromState(state))
                                          : isDrumLike(slots[0].part);
        return userLength * getHwyScale(drums);
    }

    // VBlank-synced rendering
    juce::VBlankAttachment vblankAttachment;
    double targetFrameInterval = 0.0;  // 0 = native (no throttle), otherwise seconds between frames
    juce::int64 lastFrameTicks = 0;
    void onFrame();

    // FPS counter
    bool showFps = false;
    static constexpr int FPS_RING_SIZE = 60;
    double fpsRing[FPS_RING_SIZE] = {};
    int fpsRingIndex = 0;
    double lastFrameTimestamp = 0.0;
    void drawFpsOverlay(juce::Graphics& g);

    // Cache invalidation throttling (for REAPER MIDI edit detection while paused)
    juce::int64 lastCacheInvalidationTicks = 0;

    // Track info display (footer status bar)
    int lastDisplayedTrackNumber = -1;
    juce::String lastDisplayedTrackName;

    // Scroll wheel timeline control
    static constexpr double SCROLL_NORMAL_BEATS = 2.0;   // Normal scroll: quarter note
    static constexpr double SCROLL_SHIFT_BEATS = 0.5;     // Shift+scroll: full beat

#ifdef DEBUG
    DebugEditorController debug;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChartchoticAudioProcessorEditor)

    // Latency tracking
    PPQ defaultLatencyInPPQ = 0.0;
    double defaultBPM = 120.0;

    PPQ latencyInPPQ()
    {
        auto* playHead = audioProcessor.getPlayHead();
        if (playHead == nullptr) return defaultLatencyInPPQ;

        auto positionInfo = playHead->getPosition();
        if (!positionInfo.hasValue()) return defaultLatencyInPPQ;

        double bpm = positionInfo->getBpm().orFallback(defaultBPM);
        return PPQ(audioProcessor.latencyInSeconds * (bpm / 60.0));
    }

    // Multi-buffer latency smoothing
    LatencySmoother latencySmoother;
    PPQ smoothedLatencyInPPQ()
    {
        return latencySmoother.smooth(latencyInPPQ());
    }

    //==============================================================================
    // Prints

    void printCallback()
    {
        if (!audioProcessor.debugText.isEmpty())
        {
#ifdef DEBUG
            debug.getConsole().moveCaretToEnd();
            debug.getConsole().insertTextAtCaret(audioProcessor.debugText);
#endif
            audioProcessor.debugText.clear();
        }
    }
};
