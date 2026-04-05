/*
    ==============================================================================

        NoteRenderer.cpp
        Author:  Noah Baxter

        Note/gem rendering extracted from SceneRenderer.

    ==============================================================================
*/

#include "NoteRenderer.h"

using namespace PositionConstants;

NoteRenderer::NoteRenderer(juce::ValueTree& state, AssetManager& assetManager)
    : state(state), assetManager(assetManager)
{
    std::copy_n(OVERLAY_DEFAULTS, NUM_OVERLAY_TYPES, overlayAdjusts);
}

void NoteRenderer::populate(DrawCallMap& drawCallMap, const TimeBasedTrackWindow& trackWindow,
                            double windowStartTime, double windowEndTime,
                            uint width, uint height,
                            float posEnd,
                            float farFadeEnd, float farFadeLen, float farFadeCurve)
{
    currentDrawCallMap = &drawCallMap;
    this->width = width;
    this->height = height;
    this->posEnd = posEnd;
    this->farFadeEnd = farFadeEnd;
    this->farFadeLen = farFadeLen;
    this->farFadeCurve = farFadeCurve;

    double windowTimeSpan = windowEndTime - windowStartTime;
    bool hitAnimationsOn = state.getProperty("hitIndicators");

    // When hit animations are on AND playing, notes clip at the strike position.
    // When paused (even with hits enabled), show everything — lets users browse/edit freely.
    bool clipAtStrike = hitAnimationsOn && isPlaying;
    double noteClipTime = clipAtStrike ? (strikePosGem * windowTimeSpan) : (HIGHWAY_POS_START * windowTimeSpan);
    double barClipTime = clipAtStrike ? (strikePosBar * windowTimeSpan) : (HIGHWAY_POS_START * windowTimeSpan);
    // Use the more permissive clip for frame-level skip; per-gem clip happens in drawGem
    double frameClipTime = std::min(noteClipTime, barClipTime);
    cachedNoteClipTime = noteClipTime;
    cachedBarClipTime = barClipTime;

    for (const auto& frameItem : trackWindow)
    {
        double frameTime = frameItem.first;

        if (frameTime < frameClipTime) continue;

        float normalizedPosition = (float)((frameTime - windowStartTime) / windowTimeSpan);

        drawFrame(frameItem.second, normalizedPosition, frameTime);
    }
}

void NoteRenderer::drawFrame(const TimeBasedTrackFrame& gems, float position, double frameTime)
{
    uint drawSequence[] = {0, 6, 1, 2, 3, 4, 5};
    for (int i = 0; i < gems.size(); i++)
    {
        int gemColumn = drawSequence[i];
        if (gems[gemColumn].gem != Gem::NONE)
        {
            drawGem(gemColumn, gems[gemColumn], position, frameTime);
        }
    }
}

void NoteRenderer::drawGem(uint gemColumn, const GemWrapper& gemWrapper, float position, double frameTime)
{
    // === ORCHESTRATION: image selection, clip, visibility ===

    juce::Image* glyphImage;
    bool barNote;
    bool starPowerActive = state.getProperty("starPower");
    bool isDrums = isDrumLike(activePart);

    if (isGuitarLike(activePart))
    {
        barNote = isBarNote(gemColumn, Part::GUITAR);
        glyphImage = assetManager.getGuitarGlyphImage(gemWrapper, gemColumn, starPowerActive);
    }
    else
    {
        barNote = isBarNote(gemColumn, Part::DRUMS);
        glyphImage = assetManager.getDrumGlyphImage(gemWrapper, gemColumn, starPowerActive);
    }

    double clipTime = barNote ? cachedBarClipTime : cachedNoteClipTime;
    if (frameTime < clipTime) return;
    if (barNote && !showBars) return;
    if (!barNote && !showGems) return;
    if (glyphImage == nullptr) return;

    // === ORCHESTRATION: resolve all scale/offset/curvature values ===

    float sizeScale = barNote ? PositionConstants::BAR_SIZE : PositionConstants::GEM_SIZE;
    float adjustedPosition = barNote ? position + PositionConstants::BAR_NOTE_POS_OFFSET : position;

    // Foreshortening
    float foreshorten = 1.0f;
    if (depthForeshorten > 0.0f && !PositionMath::bemaniMode)
    {
#ifdef DEBUG
        const auto& pp = PositionMath::perspParams(isDrumLike(activePart));
#else
        auto pp = PositionConstants::getPerspectiveParams(isDrumLike(activePart));
#endif
        float depth = std::max(0.0f, adjustedPosition) / pp.vanishingPointDepth;
        float scaleNear = 1.0f + (pp.highwayDepth / pp.playerDistance) * pp.perspectiveStrength;
        float psCur = scaleNear / (1.0f + depth * (scaleNear - 1.0f));
        float rawRatio = psCur / scaleNear;
        foreshorten = 1.0f - (1.0f - rawRatio) * depthForeshorten;
    }

    // User scale
    float userScale = barNote
        ? (state.hasProperty("barScale") ? (float)state["barScale"] : 1.0f)
        : (state.hasProperty("gemScale") ? (float)state["gemScale"] : 1.0f);

    // Curvature
    float noteCurv = isDrums ? noteCurvatureDrums : noteCurvatureGuitar;
    float baseCurv = barNote ? PositionConstants::BAR_CURVATURE : noteCurv;
    float curvature = PositionMath::bemaniMode ? baseCurv * bemaniConfig.curvature : baseCurv;

    // Base scale factors
    const auto& baseScale = barNote ? barScale : gemScale;
    float baseW, baseH;
    if (PositionMath::bemaniMode)
    {
        baseW = barNote ? bemaniConfig.barW : bemaniConfig.gemW;
        baseH = barNote ? bemaniConfig.barH : bemaniConfig.gemH;
    }
    else
    {
        baseW = baseScale.width;
        baseH = baseScale.height;
    }

    // Per-note-type scale
    float typeScale = 1.0f;
    if (!isDrums)
    {
        switch (gemWrapper.gem) {
        case Gem::NOTE:        typeScale = gemTypeScales.normal; break;
        case Gem::HOPO_GHOST:  typeScale = gemTypeScales.hopo; break;
        case Gem::TAP_ACCENT:  typeScale = gemTypeScales.gTap; break;
        default: break;
        }
    }
    else
    {
        switch (gemWrapper.gem) {
        case Gem::NOTE:        typeScale = gemTypeScales.normal; break;
        case Gem::HOPO_GHOST:  typeScale = gemTypeScales.dGhost; break;
        case Gem::TAP_ACCENT:  typeScale = gemTypeScales.dAccent; break;
        case Gem::CYM:         typeScale = gemTypeScales.cymbal; break;
        case Gem::CYM_GHOST:   typeScale = gemTypeScales.cGhost; break;
        case Gem::CYM_ACCENT:  typeScale = gemTypeScales.cAccent; break;
        default: break;
        }
    }

    // Bar notes maintain constant size regardless of SP or gem-type scaling
    float spMul = 1.0f;
    if (!barNote)
    {
        if (gemWrapper.starPower)
        {
            float spScale = gemTypeScales.spGem;
            if (std::abs(spScale - 1.0f) > 0.001f)
                spMul = spScale;
        }
    }
    else
    {
        typeScale = 1.0f;
    }

    float wScale = baseW * typeScale * spMul;
    float hScale = baseH * typeScale * spMul;

    // Per-column adjustments + Z offset + strikeline width (perspective only)
    float rawZOff = 0.0f;
    float strikeWidth = 1.0f;
    if (!PositionMath::bemaniMode)
    {
        rawZOff = barNote ? barZOffset : gemZOffset;
        float colSNear = 1.0f, colSFar = 1.0f, colW = 1.0f, colH = 1.0f;
        if (!isDrums && gemColumn < (int)GUITAR_LANE_COUNT) {
            const auto& ca = guitarColAdjust[gemColumn];
            colSNear = ca.sNear; colSFar = ca.sFar; colW = ca.w; colH = ca.h;
        } else if (isDrums) {
            uint drumIdx = drumColumnIndex(gemColumn);
            const auto& ca = drumColAdjust[drumIdx];
            colSNear = ca.sNear; colSFar = ca.sFar; colW = ca.w; colH = ca.h;
            if (!barNote)
                rawZOff += ca.z;
        }

#ifdef DEBUG
        float vpDepth = PositionMath::perspParams(isDrumLike(activePart)).vanishingPointDepth;
#else
        float vpDepth = PositionConstants::getPerspectiveParams(isDrumLike(activePart)).vanishingPointDepth;
#endif
        float t = juce::jlimit(0.0f, 1.0f, position / vpDepth);
        float colScale = colSNear + (colSFar - colSNear) * t;
        wScale *= colScale * colW;
        hScale *= colScale * colH;

        // Strikeline width for perspective Z scaling
        if (barNote)
        {
            auto fbStrike = PositionMath::getFretboardEdge(isDrums, 0.0f, width, height,
                                                            PositionConstants::HIGHWAY_POS_START, posEnd);
            strikeWidth = (fbStrike.rightX - fbStrike.leftX) * PositionConstants::BAR_FRETBOARD_FIT * PositionConstants::BAR_SIZE;
        }
        else
        {
            const auto& colCoordsRef = isGuitarLike(activePart)
                ? laneCoordsGuitar[(gemColumn < GUITAR_LANE_COUNT) ? gemColumn : 1]
                : laneCoordsDrums[drumColumnIndex(gemColumn)];
            auto strikeEdge = getColumnEdge(0.0f, colCoordsRef, PositionConstants::GEM_SIZE, PositionConstants::FRETBOARD_SCALE);
            strikeWidth = strikeEdge.rightX - strikeEdge.leftX;
        }
    }

    // Lane coords for this column (zeroed for bar notes — unused by computeGemRects bar path)
    PositionConstants::NormalizedCoordinates laneCoords{};
    int bemaniLaneIdx = -1;
    if (!barNote)
    {
        if (!isDrums) {
            int idx = (gemColumn < GUITAR_LANE_COUNT) ? gemColumn : 1;
            laneCoords = laneCoordsGuitar[idx];
            bemaniLaneIdx = idx - 1;
        } else {
            uint drumIdx = drumColumnIndex(gemColumn);
            laneCoords = laneCoordsDrums[drumIdx];
            bemaniLaneIdx = (int)drumIdx - 1;
        }
    }

    // Bemani nudge
    float bemaniNudgeY = 0.0f;
    if (PositionMath::bemaniMode)
    {
        float nudge = barNote ? bemaniConfig.barNudge : bemaniConfig.gemNudge(isDrums);
        float pixelsPerUnit = PositionConstants::REFERENCE_HEIGHT * bemaniConfig.strikelinePos
                            / std::max(0.1f, PositionMath::bemaniHwyScale);
        bemaniNudgeY = pixelsPerUnit * nudge;
    }

    // Overlay
    juce::Image* overlayImage = assetManager.getOverlayImage(gemWrapper.gem, isGuitarLike(activePart) ? Part::GUITAR : Part::DRUMS);
    PositionConstants::OverlayAdjust overlayAdj{};
    if (overlayImage != nullptr)
    {
        if (!isDrums) {
            overlayAdj = overlayAdjusts[OVERLAY_GUITAR_TAP];
        } else {
            switch (gemWrapper.gem) {
            case Gem::HOPO_GHOST: overlayAdj = overlayAdjusts[OVERLAY_DRUM_NOTE_GHOST]; break;
            case Gem::TAP_ACCENT: overlayAdj = overlayAdjusts[OVERLAY_DRUM_NOTE_ACCENT]; break;
            case Gem::CYM_GHOST:  overlayAdj = overlayAdjusts[OVERLAY_DRUM_CYM_GHOST]; break;
            case Gem::CYM_ACCENT: overlayAdj = overlayAdjusts[OVERLAY_DRUM_CYM_ACCENT]; break;
            default: break;
            }
        }
    }

    float opacity = calculateOpacity(position);

    // === PIXEL MATH: delegated to NotePainter ===

    // TODO: This param packing is ugly. Once drag-to-place is built and we have two
    // consumers of computeGemRects, factor the shared fields (viewport, instrument config,
    // lane coords, active scales) into a per-frame RenderContext struct that gets set once
    // by SceneRenderer and passed to all painters. The overlap between this block and the
    // drag-to-place caller will reveal the right shape for that struct.
    NotePainter::GemParams gp;
    gp.position = position;
    gp.gemColumn = (int)gemColumn;
    gp.isBar = barNote;
    gp.isDrums = isDrums;
    gp.viewportW = width;
    gp.viewportH = height;
    gp.posEnd = posEnd;
    gp.imageAspect = (float)glyphImage->getWidth() / (float)glyphImage->getHeight();
    gp.sizeScale = sizeScale;
    gp.userScale = userScale;
    gp.wScale = wScale;
    gp.hScale = hScale;
    gp.foreshorten = foreshorten;
    gp.rawZOffset = rawZOff;
    gp.strikeWidth = strikeWidth;
    gp.curvature = curvature;
    gp.laneCoords = laneCoords;
    gp.bemaniLaneIdx = bemaniLaneIdx;
    gp.bemaniNudgeY = bemaniNudgeY;
    gp.hasOverlay = (overlayImage != nullptr);
    gp.overlayAdj = overlayAdj;

    auto rects = NotePainter::computeGemRects(gp);

    // === DRAW CALLS: curved cache via NotePainter, drawing via NotePainter ===

    DrawOrder layer = barNote ? DrawOrder::BAR : DrawOrder::NOTE;

    if (curvature != 0.0f)
    {
        const auto& entry = NotePainter::getCurvedImage(
            glyphImage, gemColumn, isDrums,
            noteCurvatureGuitar, noteCurvatureDrums,
            laneCoordsGuitar, laneCoordsDrums);
        const juce::Image* curvedImgPtr = &entry.image;
        float cachedAspect = (float)curvedImgPtr->getWidth() / (float)curvedImgPtr->getHeight();
        auto curvedRect = NotePainter::computeCurvedDrawRect(
            rects.glyphRect, cachedAspect, entry.yOffsetFraction,
            rects.arcOffset, wScale, hScale, rects.perspZOffset);

        (*currentDrawCallMap)[static_cast<int>(layer)][gemColumn].push_back(
            [curvedImgPtr, opacity, curvedRect](juce::Graphics& g) {
                NotePainter::paintGem(g, *curvedImgPtr, curvedRect, opacity);
            });
    }
    else
    {
        (*currentDrawCallMap)[static_cast<int>(layer)][gemColumn].push_back(
            [glyphImage, opacity, drawRect = rects.drawRect](juce::Graphics& g) {
                NotePainter::paintGem(g, *glyphImage, drawRect, opacity);
            });
    }

    if (overlayImage != nullptr)
    {
        if (curvature != 0.0f)
        {
            const auto& entry = NotePainter::getCurvedImage(
                overlayImage, gemColumn, isDrums,
                noteCurvatureGuitar, noteCurvatureDrums,
                laneCoordsGuitar, laneCoordsDrums);
            const juce::Image* curvedOverlayPtr = &entry.image;
            float cachedAspect = (float)curvedOverlayPtr->getWidth() / (float)curvedOverlayPtr->getHeight();
            auto curvedOverlayRect = NotePainter::computeCurvedDrawRect(
                rects.overlayGlyphRect, cachedAspect, entry.yOffsetFraction,
                rects.arcOffset, wScale, hScale, rects.perspZOffset);
            curvedOverlayRect.translate(overlayAdj.offsetX * curvedOverlayRect.getWidth(),
                                        overlayAdj.offsetY * curvedOverlayRect.getHeight());

            (*currentDrawCallMap)[static_cast<int>(DrawOrder::OVERLAY)][gemColumn].push_back(
                [curvedOverlayPtr, opacity, curvedOverlayRect](juce::Graphics& g) {
                    NotePainter::paintGem(g, *curvedOverlayPtr, curvedOverlayRect, opacity);
                });
        }
        else
        {
            (*currentDrawCallMap)[static_cast<int>(DrawOrder::OVERLAY)][gemColumn].push_back(
                [overlayImage, opacity, drawRect = rects.overlayDrawRect](juce::Graphics& g) {
                    NotePainter::paintGem(g, *overlayImage, drawRect, opacity);
                });
        }
    }
}


