/*
    ==============================================================================

        HighwayComponent.h
        Author: Noah Baxter

        Encapsulates one complete chart highway view as a JUCE Component.
        PluginEditor positions it with setBounds().

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Utils/ChartTypes.h"
#include "../Midi/Utils/TimeConverter.h"
#include "Renderers/SceneRenderer.h"
#include "Renderers/TrackRenderer.h"
#include "Managers/AssetManager.h"
#include "Utils/DrawingConstants.h"
#include "Utils/HitTestMapper.h"
#include "Painters/NotePainter.h"

class TrackImageCache;

struct HighwayFrameData {
    TimeBasedTrackWindow trackWindow;
    TimeBasedSustainWindow sustainWindow;
    TimeBasedGridlineMap gridlines;
    TimeBasedFlipRegions flipRegions;
    TimeBasedEventMarkers eventMarkers;
    double windowStartTime = 0.0;
    double windowEndTime = 1.0;
    float scrollOffset = 0.0f;
    double deltaSeconds = 0.0;
    bool isPlaying = false;
    bool discoFlipActive = false;
    Part builtForPart = Part::GUITAR;
};

class HighwayComponent : public juce::Component, private juce::Timer
{
public:
    HighwayComponent(juce::ValueTree& state, AssetManager& assetManager);

    void setActivePart(Part part);
    Part getActivePart() const { return activePart; }

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void setFrameData(const HighwayFrameData& data);
    const HighwayFrameData& getFrameData() const { return frameData; }
    void rebuildTrack();

    // Visibility flags
    void setShowGems(bool on)           { sceneRenderer.showGems = on; repaint(); }
    void setShowBars(bool on)           { sceneRenderer.showBars = on; repaint(); }
    void setShowSustains(bool on)       { sceneRenderer.showSustains = on; repaint(); }
    void setShowLanes(bool on)          { sceneRenderer.showLanes = on; repaint(); }
    void setShowGridlines(bool on)      { sceneRenderer.showGridlines = on; repaint(); }
    void setShowTrack(bool on)          { sceneRenderer.showTrack = on; repaint(); }
    void setShowLaneSeparators(bool on) { sceneRenderer.showLaneSeparators = on; repaint(); }
    void setShowStrikeline(bool on)     { sceneRenderer.showStrikeline = on; repaint(); }
    void setShowHighway(bool on)        { showHighway = on; repaint(); }

    void setHighwayLength(float length) { sceneRenderer.farFadeEnd = length; PositionMath::bemaniHwyScale = length; rebuildTrack(); repaint(); }
    void setTexture(const juce::Image& img) { trackRenderer.setTexture(img); }
    void clearTexture()                 { trackRenderer.clearTexture(); }
    void setTextureScale(float s)       { trackRenderer.textureScale = s; repaint(); }
    void setTextureOpacity(float o)     { trackRenderer.textureOpacity = o; repaint(); }
    void setGemScale(float)             { repaint(); }
    void setBarScale(float)             { repaint(); }

    static constexpr float labelIconSize = 40.0f;

    bool showPartLabel = false;
    bool showDifficultyLabel = false;
    SkillLevel displaySkillLevel = SkillLevel::EXPERT;

    /** When cache is active, instrument change only swaps overlay pointers — no rebuild. */
    void onInstrumentChanged();

    void setTrackImageCache(TrackImageCache* cache) { trackImageCache = cache; }

    // Accessors for debug wiring
    SceneRenderer& getSceneRenderer()   { return sceneRenderer; }
    TrackRenderer& getTrackRenderer()   { return trackRenderer; }

    int getTopOverflow() const { return topOverflow; }
    void updateOverflow();

    bool showHighway = true;
    bool stretchToFill = false;

    int renderWidth = 0, renderHeight = 0;
    std::function<void()> onOverflowChanged;

    /** Defer an expensive rebuild (e.g. window resize).
        Starts the debounce timer; paint() uses old track images until rebuild fires. */
    void deferRebuild() { startTimer(rebuildDebounceMs); repaint(); }

    // Write mode
    void setWriteMode(bool on);
    bool isWriteMode() const { return writeMode; }

    // Mouse overrides (active in write mode only)
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    bool keyPressed(const juce::KeyPress& key) override;

    // Key actions for WriteController
    enum KeyAction { KA_DELETE, KA_MOVE_UP, KA_MOVE_DOWN, KA_LANE_LEFT, KA_LANE_RIGHT };

    // Callbacks — generic input events, WriteController interprets based on mode
    std::function<void(double timeFromCursor, int lane, bool noteExists)> onLeftClick;
    std::function<void(double timeFromCursor, int lane, bool noteExists)> onRightClick;
    std::function<void(double timeFromCursor, int lane, bool noteExists)> onDoubleClick;
    std::function<void(double startTime, int startLane, double endTime, int endLane)> onDragComplete;
    std::function<void(double sustainStartTime, int lane)> onSustainRightClick;
    std::function<void(int action)> onKeyAction;

    // Selection — set externally by PluginEditor each frame (PPQ-based, scroll-stable)
    void setSelection(double timeFromCursor, int lane);
    void clearSelection();

    // Write mode visual hints (set by WriteController each frame)
    struct WriteHints {
        bool snapEnabled = false;
        bool drawMode = false;
        float minSustainNormalized = 0.0f;
        bool freeCursor = false;
    };
    WriteHints writeHints;

private:
    static constexpr int rebuildDebounceMs = 500;

    Part activePart = Part::GUITAR;
    Part pendingPart = Part::GUITAR;
    void commitPendingPart();
    juce::ValueTree& state;
    AssetManager& assetManager;
    SceneRenderer sceneRenderer;
    TrackRenderer trackRenderer;
    TrackImageCache* trackImageCache = nullptr;

    HighwayFrameData frameData;

    int topOverflow = 0;

    // Write mode state
    bool writeMode = false;
    HitTestMapper hitTestMapper;

    // Hover state
    HitTestResult hoverResult;
    bool hoverValid = false;
    bool hoverOnExistingNote = false;

    // Drag state
    struct DragState {
        bool active = false;
        bool isLeftButton = false;
        HitTestResult startResult;
        juce::Point<float> mouseDownScreenPos;
        static constexpr float distanceThreshold = 3.0f;
    };
    DragState drag;

    // Selection state
    bool hasSelection = false;
    double selectedTime = 0.0;
    int selectedLane = -1;

    // Coordinate conversion helpers
    float timeToNormalized(double time) const;
    double normalizedToTime(float pos) const;
    double windowTimeSpan() const;

    // Lane visual resolution (deduplicates isDrums/guitar branching for lane→gemCol+coords)
    struct LaneVisuals { uint gemCol; PositionConstants::NormalizedCoordinates laneCoords; };
    LaneVisuals resolveLaneVisuals(int lane) const;

    // Find a note head in the current trackWindow near the given position+lane.
    bool findNoteAtPosition(float normalizedPosition, int laneIndex,
                            double& outTime, int& outLane) const;

    // Find a sustain body at the given position+lane. Returns the sustain's start time.
    bool findSustainAtPosition(float normalizedPosition, int laneIndex,
                               double& outStartTime) const;

    // Render-space transform: maps render coords to component coords (or inverse).
    // Returns the forward transform (render → screen). Use inverted() for screen → render.
    juce::AffineTransform getRenderTransform() const;

    juce::Point<float> screenToRenderCoords(juce::Point<float> screen) const;
    HitTestResult performHitTest(juce::Point<float> screenPos) const;

    NotePainter::NoteRect computeNoteOverlay(float position, int lane) const;
    juce::Path buildCurvedNotePath(const NotePainter::NoteRect& nr, float expand = 0.0f) const;

    float snapToNearestGridline(float normalizedPos) const;
    float snapToNearestGridlineOrNote(float normalizedPos, int laneIndex) const;
    float findNextNotePosition(float afterNormalizedPos, int laneIndex) const;
    std::vector<float> findNotePositionsInRange(float fromPos, float toPos, int laneIndex) const;

    // Extracted paint helpers (write mode)
    void paintSustainDragPreview(juce::Graphics& g);
    void paintDragGemHead(juce::Graphics& g);
    void paintHoverCursor(juce::Graphics& g);
    void paintSelectionHighlight(juce::Graphics& g);

    // Dimensions of the last full rebuild (track bake + asset rescale)
    int bakedRenderW = 0, bakedRenderH = 0, bakedOverflow = 0;

#ifdef DEBUG
public:
    bool showDebugColour = false;
    double debugTrackRender_us = 0.0;
    double debugHighwayPaint_us = 0.0;
private:
    juce::Colour debugColour;
#endif
};
