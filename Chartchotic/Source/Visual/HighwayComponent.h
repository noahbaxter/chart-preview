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
#include "../Editor/AuthoringTypes.h"
#include "Renderers/SceneRenderer.h"
#include "Renderers/TrackRenderer.h"
#include "Managers/AssetManager.h"
#include "Utils/DrawingConstants.h"
#include "Utils/HitTestMapper.h"

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

    // Mouse input — dispatches into write-mode callbacks (M1.3).
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp   (const juce::MouseEvent& e) override;

    // Authoring dispatch hooks. Wired by PluginEditor; null when unset.
    using PointerCallback = std::function<void(const AuthoringPoint&, const AuthoringContext&)>;
    void setOnPointerMove  (PointerCallback cb) { onPointerMove   = std::move(cb); }
    void setOnPointerDown  (PointerCallback cb) { onPointerDown   = std::move(cb); }
    void setOnPointerDrag  (PointerCallback cb) { onPointerDrag   = std::move(cb); }
    void setOnPointerUp    (PointerCallback cb) { onPointerUp     = std::move(cb); }
    void setOnPointerExit  (std::function<void()> cb) { onPointerExit   = std::move(cb); }
    void setOnPointerCancel(std::function<void()> cb) { onPointerCancel = std::move(cb); }

    // Coordinate-domain conversion at the dispatch boundary (M0-G design rule).
    // Takes a "seconds offset from cursor" (the timeFromCursor returned by HitTestMapper)
    // and returns project QN. If unset, mouse handlers fall back to rawProjectQN = 0.
    void setSecondsToProjectQN(std::function<double(double)> fn) { secondsToProjectQN = std::move(fn); }

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
    void setWriteMode(bool on)          { sceneRenderer.setWriteMode(on); repaint(); }

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

    // M1.3 mouse dispatch
    HitTestMapper hitTestMapper;
    PointerCallback onPointerMove;
    PointerCallback onPointerDown;
    PointerCallback onPointerDrag;
    PointerCallback onPointerUp;
    std::function<void()> onPointerExit;
    std::function<void()> onPointerCancel;
    std::function<double(double)> secondsToProjectQN;

    // Build authoring payloads from a JUCE mouse event.
    void buildAuthoringPayload(const juce::MouseEvent& e,
                               AuthoringPoint& outPoint,
                               AuthoringContext& outContext) const;

    int topOverflow = 0;

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
