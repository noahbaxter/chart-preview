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

    // Build per-frame render context
    renderCtx.viewportW = width;
    renderCtx.viewportH = height;
    renderCtx.posEnd = posEnd;
    renderCtx.isDrums = isDrumLike(activePart);
    renderCtx.activePart = activePart;
    renderCtx.depthForeshorten = depthForeshorten;
    renderCtx.gemZOffset = gemZOffset;
    renderCtx.barZOffset = barZOffset;
    renderCtx.noteCurvatureGuitar = noteCurvatureGuitar;
    renderCtx.noteCurvatureDrums = noteCurvatureDrums;
    renderCtx.gemTypeScales = gemTypeScales;
    renderCtx.gemScale = gemScale;
    renderCtx.barScale = barScale;
    renderCtx.guitarColAdjust = guitarColAdjust;
    renderCtx.drumColAdjust = drumColAdjust;
    renderCtx.laneCoordsGuitar = laneCoordsGuitar;
    renderCtx.laneCoordsDrums = laneCoordsDrums;

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
    // === Image selection, clip, visibility ===

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

    // === Build GemParams via shared render context ===

    float userScale = barNote
        ? (state.hasProperty("barScale") ? (float)state["barScale"] : 1.0f)
        : (state.hasProperty("gemScale") ? (float)state["gemScale"] : 1.0f);

    float imageAspect = (float)glyphImage->getWidth() / (float)glyphImage->getHeight();
    auto gp = renderCtx.buildGemParams(position, gemColumn, imageAspect,
                                       gemWrapper.gem, gemWrapper.starPower, userScale);

    // Overlay (applied on top of context-built params)
    juce::Image* overlayImage = assetManager.getOverlayImage(gemWrapper.gem, isGuitarLike(activePart) ? Part::GUITAR : Part::DRUMS);
    if (overlayImage != nullptr)
    {
        gp.hasOverlay = true;
        if (!isDrums) {
            gp.overlayAdj = overlayAdjusts[OVERLAY_GUITAR_TAP];
        } else {
            switch (gemWrapper.gem) {
            case Gem::HOPO_GHOST: gp.overlayAdj = overlayAdjusts[OVERLAY_DRUM_NOTE_GHOST]; break;
            case Gem::TAP_ACCENT: gp.overlayAdj = overlayAdjusts[OVERLAY_DRUM_NOTE_ACCENT]; break;
            case Gem::CYM_GHOST:  gp.overlayAdj = overlayAdjusts[OVERLAY_DRUM_CYM_GHOST]; break;
            case Gem::CYM_ACCENT: gp.overlayAdj = overlayAdjusts[OVERLAY_DRUM_CYM_ACCENT]; break;
            default: break;
            }
        }
    }

    float opacity = calculateOpacity(position);
    float wScale = gp.wScale;
    float hScale = gp.hScale;
    float curvature = gp.curvature;

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
            curvedOverlayRect.translate(gp.overlayAdj.offsetX * curvedOverlayRect.getWidth(),
                                        gp.overlayAdj.offsetY * curvedOverlayRect.getHeight());

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


