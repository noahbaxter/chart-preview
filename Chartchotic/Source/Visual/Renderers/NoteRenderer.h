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
#include "../Geometry/PositionConstants.h"
#include "../Geometry/PositionMath.h"
#include "../Utils/DrawingConstants.h"
#include "../Utils/Frame.h"
#include "../Utils/FrameRenderer.h"
#include "HighwayRenderer.h"

namespace PositionConstants { struct RenderTypeConfig; }

class NoteRenderer : public HighwayRenderer
{
public:
    NoteRenderer(juce::ValueTree& state, AssetManager& assetManager);

    bool showGems = true;
    bool showBars = true;
    float barModeDim = 1.0f;
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
    // Active highway's lane coords are held by HighwayRenderer (set per-part by
    // SceneRenderer); resScale (ColumnAdjust::z reads) too.
    float gemZOffset = 0.0f;
    float cymZOffset = 0.0f;  // Drums only — cymbals tuned separately from toms
    float barZOffset = 0.0f;
    float strikePosGem = 0.0f;
    float strikePosBar = 0.0f;

    void clearCurvedCache() { curvedCache.clear(); }

    // Flam sub-lane shape (see FLAM_SUBLANE_WIDTH / _SPREAD). Both feed the curvature warp,
    // whose bakes are keyed on curvature alone, so a change has to drop the cache or the
    // panel's slider moves the gems while their warp stays where it was.
    PositionConstants::FlamTypeWidths flamTypeWidths = PositionConstants::FLAM_TYPE_WIDTHS;
    float flamSubLaneSpread = PositionConstants::FLAM_SUBLANE_SPREAD;
    // Ghost rings and accent chevrons are per-gem, so a flam draws two of them and they cross
    // in the overlap. On: draw one overlay centred on the whole lane instead.
    bool flamSingleOverlay = PositionConstants::FLAM_SINGLE_OVERLAY;
    float flamTilt = PositionConstants::FLAM_TILT;
    void setFlamShape(const PositionConstants::FlamTypeWidths& widths, float spread,
                      float tilt, bool singleOverlay)
    {
        flamSingleOverlay = singleOverlay;
        if (std::memcmp(&widths, &flamTypeWidths, sizeof(widths)) == 0
            && spread == flamSubLaneSpread && tilt == flamTilt)
            return;
        flamTypeWidths = widths;
        flamSubLaneSpread = spread;
        flamTilt = tilt;
        clearCurvedCache();
    }

    void populate(DrawCallMap& drawCallMap, const TimeBasedTrackWindow& trackWindow,
                  double windowStartTime, double windowEndTime,
                  uint width, uint height,
                  float posEnd,
                  float farFadeEnd, float farFadeLen, float farFadeCurve);

    // Render a single ghost sprite through the same pipeline as real notes.
    // Call AFTER populate() so internal state (curvature, scales, etc.) is configured.
    void renderGhost(DrawCallMap& drawCallMap, int lane, float position,
                     juce::Image* image, float opacity, Gem gem = Gem::NOTE,
                     bool selected = false);

    struct SelectedGem { int lane; double time; };
    std::vector<SelectedGem> selectedGems;
    std::vector<SelectedGem> eraseTargets;

    struct NoteHitBox
    {
        int   lane = -1;
        double timeSec = 0.0;
        juce::Rectangle<float> rect;
    };
    const std::vector<NoteHitBox>& getHitBoxes() const { return hitBoxes; }

private:
    juce::ValueTree& state;
    AssetManager& assetManager;

    // Cached per-populate call (frame geometry state lives in HighwayRenderer)
    DrawCallMap* currentDrawCallMap = nullptr;
    float currentVpDepth = 1.0f;
    float currentNoteCurvature = PositionConstants::NOTE_CURVATURE;
    double cachedNoteClipTime = 0, cachedBarClipTime = 0;

    // Per-time-slice composite context: one anchor + one scale shared by every
    // sprite in the row, so the bar and its stacked gems can't drift apart.
    struct SharedFrameContext
    {
        juce::Point<float> anchor;        // projected screen-space center of the lane plane at this depth
        juce::Point<float> frameScale;    // uniform (x == y); applied to all offsets and sprite sizes
        float fbStrikeWidth = 0.0f;        // fretboard width at strike (pixels)
        float fbStrikeCenterX = 0.0f;      // fretboard center X at strike (pixels)
    };

    // A flam draws the gem twice inside its own lane, each copy squished horizontally and
    // pushed off the lane centre. None = an ordinary single gem filling the lane.
    enum class FlamHalf { None, Left, Right };

    SharedFrameContext buildFrameContext(float position);
    void drawNoteRow(const TimeBasedTrackFrame& gems, float position, double frameTime);
    void appendGemSprites(uint gemColumn, const GemWrapper& gemWrapper, float position,
                          double frameTime, const SharedFrameContext& ctx,
                          Render::Frame& outFrame,
                          juce::Image* imageOverride = nullptr,
                          float opacityOverride = -1.0f,
                          FlamHalf flamHalf = FlamHalf::None);
    // Bemani path: flat / no perspective. Builds and draws its own single-gem
    // Frame directly (anchor at gem's screen position, scale 1.0). Doesn't
    // contribute to the shared composite — bemani has no chord-stack drift.
    void drawGemBemani(uint gemColumn, const GemWrapper& gemWrapper, float position,
                       double frameTime, juce::Image* glyphImage, bool barNote, float opacity);

    const PositionConstants::OverlayAdjust& getOverlayAdjustForGem(Gem gem, bool isDrums, bool hiHat, bool hiHatOpen) const;

    // True when this gem renders the elite Hi-Hat gem art (col 2, not star-power, not Indifferent)
    // -- mirrors getDrumGlyphImage's hi-hat branch so the overlay adjust matches the drawn glyph.
    bool isEliteHiHatGlyph(const GemWrapper& gemWrapper, uint gemColumn, bool starPowerActive) const;

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
        float overlayBaseW;        // overlay's base width: the gem's, or the whole lane's
                                   // when a flam draws one centred overlay for the pair
        float overlayAnchorX;      // overlay offsetX baseline
        float overlayAnchorY;      // overlay offsetY baseline
        float pixelScale;          // strike-reference px -> on-screen px, for bake sizing
    };
    void applyCurvedImageSwap(Render::Frame& frame, int gemIdx, int ovlIdx,
                              const CurvedSwapArgs& args);

    // Curved note image cache (per (image, column, isDrums))
    struct CurvedImageEntry
    {
        juce::Image image;
        float yOffsetFraction;  // baseline shift as fraction of dest height
        // Rows of the baked image that are glyph rather than arc padding. Sprite height is
        // derived from the CONTENT (contentHeight / contentFraction), never from the padded
        // image's aspect: the padding is `ceil(maxShift) + 2` whole pixels, which is a bigger
        // slice of a small bake than a large one, so sizing off the image aspect made a gem's
        // height drift with its bake bucket — visible as a flam changing height as its width
        // slider moves.
        float contentFraction = 1.0f;
    };

    // Cache key includes curvature (quantized to 1e-4) so dragging the curvature
    // slider between two values doesn't thrash the cache on every change.
    struct CurveKey
    {
        juce::Image* src;
        int column;
        bool isDrums;
        int curvatureQ;   // curvature * 10000, rounded
        int sizeBucket;   // baked width in px, snapped to a power of two
        bool operator<(const CurveKey& o) const {
            return std::tie(src, column, isDrums, curvatureQ, sizeBucket)
                 < std::tie(o.src, o.column, o.isDrums, o.curvatureQ, o.sizeBucket);
        }
    };
    std::map<CurveKey, CurvedImageEntry> curvedCache;

    // A cached curved gem is baked at roughly the size it will be DRAWN at, not at a fixed
    // fraction of the source art. JUCE's image blit cost tracks the source size (a big source
    // blows the cache line budget per destination pixel), so blitting a strikeline-sized
    // master into a far-away 40px gem was costing an order of magnitude more than the pixels
    // warranted: on elite that was ~230us a sprite against ~45us right-sized. Sizes snap to
    // powers of two so a gem sliding down the highway reuses a handful of bakes instead of
    // baking a new one every frame.
    static constexpr int CURVE_BAKE_MIN_WIDTH = 32;
    static int curveSizeBucket(float drawnWidthPx, int sourceWidth);

    // When set, getCurvedImage warps across THESE coords instead of a single lane's. Only the
    // elite Stomp/Splash bar uses it, which spans the ~3-lane pedal zone (off-centre), so its
    // warp picks up the gridline's tilt AND curve over that span. Paired with a synthetic
    // cache-key column (PEDAL_CURVE_COLUMN). Reset to nullptr after each swap.
    const PositionConstants::NormalizedCoordinates* curveCoordsOverride = nullptr;
    // Multiplies the warp curvature for the current swap. 1.0 for everything except the pedal
    // bar, which steepens its tilt (ELITE_PEDAL_CURVE_GAIN) so its warp matches the FRETBOARD_SCALE
    // its whole-bar lift already carries, and a flam half, which scales it by FLAM_TILT.
    // Reset to 1.0 after each swap. It is part of the bake's cache key, so the variants
    // don't collide.
    float curveScaleOverride = 1.0f;
    static constexpr int PEDAL_CURVE_COLUMN = -100;   // synthetic cache-key column for the pedal bar

    const CurvedImageEntry& getCurvedImage(juce::Image* src, int column, bool isDrums,
                                           float drawnWidthPx);
    float getColumnDistFromCenter(int column, bool isDrums);
    float getColumnDistFromCenter(const PositionConstants::NormalizedCoordinates& colCoords,
                                  bool isDrums);

    std::vector<NoteHitBox> hitBoxes;
};
