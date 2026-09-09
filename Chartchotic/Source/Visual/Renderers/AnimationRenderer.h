/*
    ==============================================================================

        AnimationRenderer.h
        Created by Claude Code (refactoring animation logic)
        Author: Noah Baxter

        Encapsulates animation detection, state management, and rendering.
        Combines detection (note crossing), state updates (sustains), and rendering.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include "../../Utils/ChartTypes.h"
#include "../../Midi/Utils/TimeConverter.h"
#include "../Managers/AnimationManager.h"
#include "../Managers/AssetManager.h"
#include "../Utils/PositionConstants.h"
#include "../Utils/PositionMath.h"
#include "../Utils/DrawingConstants.h"
#include "../../UI/ControlConstants.h"

class AnimationRenderer
{
public:
    AnimationRenderer(juce::ValueTree &state, AssetManager &assetManager);

    Part activePart = Part::GUITAR;
    ~AnimationRenderer();

    /**
     * Detect notes that have crossed the strikeline and trigger animations.
     * Should be called once per frame when playback is active.
     */
    void detectAndTriggerAnimations(const TimeBasedTrackWindow& trackWindow, double strikeTimeOffset = 0.0);

    /**
     * Update sustain states based on current sustain window.
     * Animations hold at frame 1 during sustains.
     * If isPlaying and sustain is active but no animation exists, force-trigger it.
     */
    void updateSustainStates(const TimeBasedSustainWindow& sustainWindow, bool isPlaying);

    // Tuning params — set by SceneRenderer before calling renderToDrawCallMap
    const PositionConstants::NormalizedCoordinates* laneCoordsGuitar = nullptr;
    const PositionConstants::NormalizedCoordinates* laneCoordsDrums = nullptr;
    float hitGemZOffset = 0.0f;
    float hitBarZOffset = 0.0f;
    float noteCurvature = 0.0f;
    PositionConstants::HitScale hitGemScale = PositionConstants::HIT_GEM_SCALE;
    PositionConstants::HitScale hitBarScale = PositionConstants::HIT_BAR_SCALE;
    PositionConstants::HitTypeConfig hitTypeConfig;
    // Pointer into scene-side drumColAdjust + resScale — multiply ca.z * resScale
    // at use site so we don't pre-bake the full array per frame.
    const PositionConstants::ColumnAdjust* drumColAdjust = PositionConstants::DRUM_COL_ADJUST;
    float resScale = 1.0f;

    /**
     * Populate drawCallMap with animation render calls.
     * Animations are added to BAR_ANIMATION and NOTE_ANIMATION layers for proper Z-ordering.
     * Bezier params (posEnd) come from SceneRenderer so debug sliders apply.
     */
    void renderToDrawCallMap(DrawCallMap& drawCallMap, uint width, uint height,
                             float posEnd,
                             float strikePos = 0.0f);

    /**
     * Advance all active animations by delta time.
     * Call this once per render frame after rendering.
     */
    void advanceFrames(double deltaSeconds);

    /**
     * Clear all animations (e.g., when transport stops).
     */
    void reset();

private:
    juce::ValueTree &state;
    AnimationManager animationManager;
    AssetManager& assetManager;

    // Track the last note time per column to ensure every note triggers an animation
    std::array<double, 7> lastNoteTimePerColumn = {-999.0, -999.0, -999.0, -999.0, -999.0, -999.0, -999.0};

    // Helper: Trigger animation for a specific gem column
    void triggerAnimationForColumn(uint gemColumn, Gem gemType = Gem::NOTE, bool starPower = false);

    // Bezier column edge helper
    PositionConstants::LaneCorners getColumnEdge(float position, const PositionConstants::NormalizedCoordinates& colCoords,
                                                  float sizeScale, float posEnd,
                                                  float fretboardScale = 1.0f,
                                                  int bemaniLaneIdx = -1)
    {
        bool isDrums = isDrumLike(activePart);
        return PositionMath::getColumnPosition(getRenderType(activePart), position, cachedWidth, cachedHeight,
                                               PositionConstants::HIGHWAY_POS_START, posEnd,
                                               colCoords, sizeScale, fretboardScale, bemaniLaneIdx);
    }

    uint cachedWidth = 0, cachedHeight = 0;

    // Rendering helpers
    void renderKickAnimation(juce::Graphics &g, const AnimationConstants::HitAnimation& anim, uint width, uint height, const PositionConstants::CoordinateOffset& offset,
                             float posEnd, float strikePos);
    void renderFretAnimation(juce::Graphics &g, const AnimationConstants::HitAnimation& anim, uint width, uint height, const PositionConstants::CoordinateOffset& offset,
                             float posEnd, float strikePos);
};
