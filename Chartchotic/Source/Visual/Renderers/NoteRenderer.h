/*
    ==============================================================================

        NoteRenderer.h
        Author:  Noah Baxter

        Note/gem rendering: drawNotesFromMap, drawFrame, drawGem,
        plus overlay positioning (absorbed from GlyphRenderer).

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <map>
#include <tuple>
#include "../../Utils/ChartTypes.h"
#include "../../Midi/Utils/TimeConverter.h"
#include "../Managers/AssetManager.h"
#include "../Utils/PositionConstants.h"
#include "../Utils/PositionMath.h"
#include "../Utils/DrawingConstants.h"
#include "../Painters/NotePainter.h"

class NoteRenderer
{
public:
    NoteRenderer(juce::ValueTree& state, AssetManager& assetManager);

    Part activePart = Part::GUITAR;

    bool showGems = true;
    bool showBars = true;
    float noteCurvatureGuitar = PositionConstants::NOTE_CURVATURE;
    float noteCurvatureDrums = PositionConstants::NOTE_CURVATURE;
    PositionConstants::ElementScale gemScale = PositionConstants::GEM_SCALE;
    PositionConstants::ElementScale barScale = PositionConstants::BAR_SCALE;
    float depthForeshorten = PositionConstants::NOTE_DEPTH_FORESHORTEN;
    PositionConstants::GemTypeScales gemTypeScales;
    PositionConstants::OverlayAdjust overlayAdjusts[PositionConstants::NUM_OVERLAY_TYPES];
    PositionConstants::ColumnAdjust guitarColAdjust[6] = {};
    PositionConstants::ColumnAdjust drumColAdjust[5] = {};
    const PositionConstants::NormalizedCoordinates* laneCoordsGuitar = nullptr;
    const PositionConstants::NormalizedCoordinates* laneCoordsDrums = nullptr;
    float gemZOffset = 0.0f;
    float barZOffset = 0.0f;
    float strikePosGem = 0.0f;
    float strikePosBar = 0.0f;
    bool isPlaying = false;

    void clearCurvedCache() { curvedCache.clear(); }

    void populate(DrawCallMap& drawCallMap, const TimeBasedTrackWindow& trackWindow,
                  double windowStartTime, double windowEndTime,
                  uint width, uint height,
                  float posEnd,
                  float farFadeEnd, float farFadeLen, float farFadeCurve);

private:
    juce::ValueTree& state;
    AssetManager& assetManager;

    // Cached per-populate call
    DrawCallMap* currentDrawCallMap = nullptr;
    uint width = 0, height = 0;
    float posEnd = 0;
    float farFadeEnd = 0, farFadeLen = 0, farFadeCurve = 0;
    double cachedNoteClipTime = 0, cachedBarClipTime = 0;

    using LaneCorners = PositionConstants::LaneCorners;
    using NormalizedCoordinates = PositionConstants::NormalizedCoordinates;

    LaneCorners getColumnEdge(float position, const NormalizedCoordinates& colCoords,
                              float sizeScale, float fretboardScale = 1.0f,
                              int bemaniLaneIdx = -1)
    {
        bool isDrums = isDrumLike(activePart);
        return PositionMath::getColumnPosition(isDrums, position, width, height,
                                               PositionConstants::HIGHWAY_POS_START, posEnd,
                                               colCoords, sizeScale, fretboardScale, bemaniLaneIdx);
    }

    float calculateOpacity(float position)
    {
        if (PositionMath::bemaniMode) return 1.0f;
        return calculateFarFade(position, farFadeEnd, farFadeLen, farFadeCurve);
    }

    void drawFrame(const TimeBasedTrackFrame& gems, float position, double frameTime);
    void drawGem(uint gemColumn, const GemWrapper& gemWrapper, float position, double frameTime);


    // Curved note image cache
    struct CurvedImageEntry
    {
        juce::Image image;
        float yOffsetFraction;  // baseline shift as fraction of dest height
    };

    using CurveKey = std::tuple<juce::Image*, int, bool>;  // sourcePtr, column, isDrums
    std::map<CurveKey, CurvedImageEntry> curvedCache;
    float lastCachedCurvatureGuitar = PositionConstants::NOTE_CURVATURE;
    float lastCachedCurvatureDrums = PositionConstants::NOTE_CURVATURE;

    const CurvedImageEntry& getCurvedImage(juce::Image* src, int column, bool isDrums);
    float getColumnDistFromCenter(int column, bool isDrums);
};
