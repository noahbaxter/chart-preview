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

class MidiWriter;

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
    void setWriteMode(bool on, MidiWriter* writer, int trackIndex);
    bool isWriteMode() const { return writeMode; }

    // Mouse overrides (active in write mode only)
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    bool keyPressed(const juce::KeyPress& key) override;

    // Callbacks — editor wires these to REAPER MIDI operations
    std::function<void(double timeFromCursor, int pitch)> onNoteEditRequested;
    std::function<void(double timeFromCursor, int pitch)> onNoteDeleteRequested;
    std::function<void(double timeFromCursor, int pitch, int direction)> onNoteMoveRequested;
    std::function<void(double timeFromCursor, int lane)> onNoteSelected;  // click on existing note
    std::function<void()> onSelectionCleared;  // click on empty space

    // Selection — set externally by PluginEditor each frame (PPQ-based, scroll-stable)
    void setSelection(double timeFromCursor, int lane);
    void clearSelection();

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
    MidiWriter* midiWriter = nullptr;
    int writeTrackIndex = -1;
    HitTestMapper hitTestMapper;
    HitTestResult hoverResult;
    bool hoverValid = false;
    bool hoverOnExistingNote = false;

    // Selection state
    bool hasSelection = false;
    double selectedTime = 0.0;   // time key from trackWindow
    int selectedLane = -1;

    // Find a note in the current trackWindow near the given position+lane.
    // Returns true and sets outTime/outLane if found.
    bool findNoteAtPosition(float normalizedPosition, int laneIndex,
                            double& outTime, int& outLane) const;

    // Convert screen pixel to render-space pixel (inverts the paint() transform)
    juce::Point<float> screenToRenderCoords(juce::Point<float> screen) const;

    // Run hit test at a screen position and return the result
    HitTestResult performHitTest(juce::Point<float> screenPos) const;

    // Shared overlay geometry for ghost cursor and selection highlight
    struct NoteOverlay {
        float screenLeftX, screenRightX, screenCenterY, screenH;
        float renderLeftX, renderRightX;  // for curvature sampling
        float curvature, arcOffset, sy;
        float position;  // normalized highway position (for fretboard edge queries)
        bool isBar;
    };
    NoteOverlay computeNoteOverlay(float position, int lane) const;
    juce::Path buildCurvedNotePath(const NoteOverlay& ov, float expand = 0.0f) const;

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
