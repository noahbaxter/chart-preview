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
#include "../Utils/Frame.h"
#include "../Utils/FrameRenderer.h"

namespace PositionConstants { struct RenderTypeConfig; }

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
    PositionConstants::GemTypeScales gemTypeScales;
    // Scene-side arrays referenced via const-pointer — no per-frame copy.
    // Initialized to compile-time defaults; SceneRenderer::paint repoints
    // to its (mutable) arrays so debug-panel edits flow through.
    const PositionConstants::OverlayAdjust* overlayAdjusts = PositionConstants::OVERLAY_DEFAULTS;
    const PositionConstants::ColumnAdjust* guitarColAdjust = PositionConstants::GUITAR_COL_ADJUST;
    const PositionConstants::ColumnAdjust* drumColAdjust   = PositionConstants::DRUM_COL_ADJUST;
    const PositionConstants::NormalizedCoordinates* laneCoordsGuitar = nullptr;
    const PositionConstants::NormalizedCoordinates* laneCoordsDrums = nullptr;
    // Z values in ColumnAdjust are tuned at REFERENCE_HEIGHT — multiply by
    // resScale at the read site instead of pre-baking per-frame.
    float resScale = 1.0f;
    float gemZOffset = 0.0f;
    float cymZOffset = 0.0f;  // Drums only — cymbals tuned separately from toms
    float barZOffset = 0.0f;
    float strikePosGem = 0.0f;
    float strikePosBar = 0.0f;

    void clearCurvedCache() { curvedCache.clear(); }

    void populate(DrawCallMap& drawCallMap, const TimeBasedTrackWindow& trackWindow,
                  double windowStartTime, double windowEndTime,
                  uint width, uint height,
                  float posEnd,
                  float farFadeEnd, float farFadeLen, float farFadeCurve);

    // Render a single ghost sprite through the same pipeline as real notes.
    // Call AFTER populate() so internal state (curvature, scales, etc.) is configured.
    void renderGhost(DrawCallMap& drawCallMap, int lane, float position,
                     juce::Image* image, float opacity);

private:
    juce::ValueTree& state;
    AssetManager& assetManager;

    // Cached per-populate call
    DrawCallMap* currentDrawCallMap = nullptr;
    const PositionConstants::RenderTypeConfig* currentConfig = nullptr;
    float currentVpDepth = 1.0f;
    float currentNoteCurvature = PositionConstants::NOTE_CURVATURE;
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

    // Per-time-slice composite context: one anchor + one scale shared by every
    // sprite in the row, so the bar and its stacked gems can't drift apart.
    struct SharedFrameContext
    {
        juce::Point<float> anchor;        // projected screen-space center of the lane plane at this depth
        juce::Point<float> frameScale;    // uniform (x == y); applied to all offsets and sprite sizes
        float fbStrikeWidth = 0.0f;        // fretboard width at strike (pixels)
        float fbStrikeCenterX = 0.0f;      // fretboard center X at strike (pixels)
    };

    SharedFrameContext buildFrameContext(float position);
    void drawNoteRow(const TimeBasedTrackFrame& gems, float position, double frameTime);
    void appendGemSprites(uint gemColumn, const GemWrapper& gemWrapper, float position,
                          double frameTime, const SharedFrameContext& ctx,
                          Render::Frame& outFrame,
                          juce::Image* imageOverride = nullptr,
                          float opacityOverride = -1.0f);
    // Bemani path: flat / no perspective. Builds and draws its own single-gem
    // Frame directly (anchor at gem's screen position, scale 1.0). Doesn't
    // contribute to the shared composite — bemani has no chord-stack drift.
    void drawGemBemani(uint gemColumn, const GemWrapper& gemWrapper, float position,
                       juce::Image* glyphImage, bool barNote, float opacity);

    const PositionConstants::OverlayAdjust& getOverlayAdjustForGem(Gem gem, bool isDrums) const;

    // Replace gem (and optional overlay) sprite images with cached curved variants
    // and adjust their height/offsetY accordingly. Shared between perspective and
    // bemani paths — anchorX/anchorY supply the path-specific overlay baseline
    // (strikeOffsetX / zOff+arc for perspective; 0 / 0 for bemani).
    struct CurvedSwapArgs
    {
        juce::Image* glyphImage;
        juce::Image* overlayImage;       // nullable
        const PositionConstants::OverlayAdjust* overlayAdj;  // nullable
        int   gemColumn;
        bool  isDrums;
        float gemBaseW;            // base width before scale
        float gemBaseH;            // base height before scale
        float hScale;              // height scale multiplier
        float overlayAnchorX;      // overlay offsetX baseline
        float overlayAnchorY;      // overlay offsetY baseline
    };
    void applyCurvedImageSwap(Render::Frame& frame, int gemIdx, int ovlIdx,
                              const CurvedSwapArgs& args);

    // Curved note image cache (per (image, column, isDrums))
    struct CurvedImageEntry
    {
        juce::Image image;
        float yOffsetFraction;  // baseline shift as fraction of dest height
    };

    // Cache key includes curvature (quantized to 1e-4) so dragging the curvature
    // slider between two values doesn't thrash the cache on every change.
    struct CurveKey
    {
        juce::Image* src;
        int column;
        bool isDrums;
        int curvatureQ;   // curvature * 10000, rounded
        bool operator<(const CurveKey& o) const {
            return std::tie(src, column, isDrums, curvatureQ)
                 < std::tie(o.src, o.column, o.isDrums, o.curvatureQ);
        }
    };
    std::map<CurveKey, CurvedImageEntry> curvedCache;

    const CurvedImageEntry& getCurvedImage(juce::Image* src, int column, bool isDrums);
    float getColumnDistFromCenter(int column, bool isDrums);
};
