/*
    ==============================================================================

        NoteRenderer.cpp
        Author:  Noah Baxter

        Note/gem rendering extracted from SceneRenderer.

    ==============================================================================
*/

#include "NoteRenderer.h"
#include "../Utils/RenderTypeConfig.h"

using namespace PositionConstants;
using namespace Render;

namespace {
    float gemTypeScale(Gem gem, bool isDrums, const GemTypeScales& s)
    {
        if (!isDrums)
        {
            switch (gem) {
            case Gem::NOTE:        return s.normal;
            case Gem::HOPO_GHOST:  return s.hopo;
            case Gem::TAP_ACCENT:  return s.gTap;
            default: return 1.0f;
            }
        }
        switch (gem) {
        case Gem::NOTE:        return s.normal;
        case Gem::HOPO_GHOST:  return s.dGhost;
        case Gem::TAP_ACCENT:  return s.dAccent;
        case Gem::CYM:         return s.cymbal;
        case Gem::CYM_GHOST:   return s.cymbal * s.cGhost;
        case Gem::CYM_ACCENT:  return s.cymbal * s.cAccent;
        default: return 1.0f;
        }
    }
}

NoteRenderer::NoteRenderer(juce::ValueTree& state, AssetManager& assetManager)
    : state(state), assetManager(assetManager)
{
}

const OverlayAdjust& NoteRenderer::getOverlayAdjustForGem(Gem gem, bool isDrums) const
{
    if (!isDrums) return overlayAdjusts[OVERLAY_GUITAR_TAP];
    switch (gem) {
    case Gem::HOPO_GHOST: return overlayAdjusts[OVERLAY_DRUM_NOTE_GHOST];
    case Gem::TAP_ACCENT: return overlayAdjusts[OVERLAY_DRUM_NOTE_ACCENT];
    case Gem::CYM_GHOST:  return overlayAdjusts[OVERLAY_DRUM_CYM_GHOST];
    case Gem::CYM_ACCENT: return overlayAdjusts[OVERLAY_DRUM_CYM_ACCENT];
    default: { static const OverlayAdjust none; return none; }
    }
}

void NoteRenderer::applyCurvedImageSwap(Frame& frame, int gemIdx, int ovlIdx,
                                         const CurvedSwapArgs& args)
{
    const auto& gemEntry = getCurvedImage(args.glyphImage, args.gemColumn, args.isDrums);
    float gemCurvedAspect = (float)gemEntry.image.getWidth()
                          / (float)gemEntry.image.getHeight();
    auto& gemSprite = frame.sprites[gemIdx];
    gemSprite.image  = const_cast<juce::Image*>(&gemEntry.image);
    gemSprite.height = (args.gemBaseW / gemCurvedAspect) * args.hScale;
    gemSprite.offsetY += gemEntry.yOffsetFraction * args.gemBaseH;

    if (args.overlayImage && args.overlayAdj && ovlIdx >= 0)
    {
        const auto& ovlEntry = getCurvedImage(args.overlayImage, args.gemColumn, args.isDrums);
        float ovlCurvedAspect = (float)ovlEntry.image.getWidth()
                              / (float)ovlEntry.image.getHeight();
        auto& ovlSprite = frame.sprites[ovlIdx];
        float ovlRectSX = args.overlayAdj->scaleX * args.overlayAdj->scale;
        float ovlRectSY = args.overlayAdj->scaleY * args.overlayAdj->scale;
        float ovlCurvedH = (args.gemBaseW * ovlRectSX) / ovlCurvedAspect * args.hScale;
        float ovlContentYBase = args.gemBaseH * ovlRectSY;

        ovlSprite.image  = const_cast<juce::Image*>(&ovlEntry.image);
        ovlSprite.height = ovlCurvedH;
        ovlSprite.offsetX = args.overlayAnchorX + args.overlayAdj->offsetX * ovlSprite.width;
        ovlSprite.offsetY = args.overlayAnchorY
                          + ovlEntry.yOffsetFraction * ovlContentYBase
                          + args.overlayAdj->offsetY * ovlSprite.height;
    }
}

void NoteRenderer::populate(DrawCallMap& drawCallMap, const TimeBasedTrackWindow& trackWindow,
                            double windowStartTime, double windowEndTime,
                            uint width, uint height,
                            float posEnd,
                            float farFadeEnd, float farFadeLen, float farFadeCurve)
{
    hitBoxes.clear();
    currentDrawCallMap = &drawCallMap;
    currentConfig = getRenderTypeConfig(getRenderType(activePart));
    currentVpDepth = currentConfig->getPerspectiveParams().vanishingPointDepth;
    currentNoteCurvature = isDrumLike(activePart) ? noteCurvatureDrums : noteCurvatureGuitar;
    this->width = width;
    this->height = height;
    this->posEnd = posEnd;
    this->farFadeEnd = farFadeEnd;
    this->farFadeLen = farFadeLen;
    this->farFadeCurve = farFadeCurve;

    double windowTimeSpan = windowEndTime - windowStartTime;
    bool hitAnimationsOn = state.getProperty("hitIndicators");

    // When hit animations are on, notes clip at the strike position.
    // strikePosGem/strikePosBar shift the clip point (negative = past strikeline = lower on screen).
    // When off, notes flow past the strikeline to the bottom of the highway.
    double noteClipTime = hitAnimationsOn ? (strikePosGem * windowTimeSpan) : (HIGHWAY_POS_START * windowTimeSpan);
    double barClipTime = hitAnimationsOn ? (strikePosBar * windowTimeSpan) : (HIGHWAY_POS_START * windowTimeSpan);
    // Use the more permissive clip for frame-level skip; per-gem clip happens in drawGem
    double frameClipTime = std::min(noteClipTime, barClipTime);
    cachedNoteClipTime = noteClipTime;
    cachedBarClipTime = barClipTime;

    for (const auto& frameItem : trackWindow)
    {
        double frameTime = frameItem.first;

        if (frameTime < frameClipTime) continue;

        float normalizedPosition = (float)((frameTime - windowStartTime) / windowTimeSpan);

        drawNoteRow(frameItem.second, normalizedPosition, frameTime);
    }
}

NoteRenderer::SharedFrameContext NoteRenderer::buildFrameContext(float position)
{
    bool isDrums = isDrumLike(activePart);
    auto fbStrike = PositionMath::getFretboardEdge(isDrums, 0.0f, width, height, HIGHWAY_POS_START, posEnd);
    auto fbCur    = PositionMath::getFretboardEdge(isDrums, position, width, height, HIGHWAY_POS_START, posEnd);
    float fbSW = fbStrike.rightX - fbStrike.leftX;
    float wRatio = (fbSW > 0.0f) ? ((fbCur.rightX - fbCur.leftX) / fbSW) : 1.0f;

    return {
        {(fbCur.leftX + fbCur.rightX) * 0.5f, fbCur.centerY},
        {wRatio, wRatio},
        fbSW,
        (fbStrike.leftX + fbStrike.rightX) * 0.5f
    };
}

void NoteRenderer::renderGhost(DrawCallMap& drawCallMap, int lane, float position,
                                juce::Image* image, float opacity, Gem gem)
{
    auto ctx = buildFrameContext(position);
    GemWrapper dummy;
    dummy.gem = gem;

    Render::Frame frame;
    // image=nullptr → appendGemSprites falls through to the real colored asset
    appendGemSprites(lane, dummy, position, 0.0, ctx, frame, image, opacity);

    for (auto& s : frame.sprites)
        s.drawOrder = (int)DrawOrder::OVERLAY;

    if (!frame.sprites.empty())
        Render::drawFrame(frame, ctx.anchor, ctx.frameScale, drawCallMap);
}

void NoteRenderer::drawNoteRow(const TimeBasedTrackFrame& gems, float position, double frameTime)
{
    auto ctx = buildFrameContext(position);

    Render::Frame composite;

    uint drawSequence[] = {0, 6, 1, 2, 3, 4, 5};
    for (int i = 0; i < gems.size(); i++)
    {
        int gemColumn = drawSequence[i];
        if (gems[gemColumn].gem != Gem::NONE)
        {
            int spriteStart = (int)composite.sprites.size();
            appendGemSprites(gemColumn, gems[gemColumn], position, frameTime, ctx, composite);

            bool selected = false;
            for (const auto& sg : selectedGems)
                if (sg.lane == gemColumn && std::abs(sg.time - frameTime) < 0.002)
                { selected = true; break; }

            if (selected)
            {
                static const juce::Colour kSelTint = juce::Colour(180, 220, 255).withAlpha((uint8)140);
                for (int s = spriteStart; s < (int)composite.sprites.size(); ++s)
                    composite.sprites[s].tint = kSelTint;
            }
        }
    }

    if (!composite.sprites.empty())
        Render::drawFrame(composite, ctx.anchor, ctx.frameScale, *currentDrawCallMap);
}

void NoteRenderer::appendGemSprites(uint gemColumn, const GemWrapper& gemWrapper, float position,
                                     double frameTime, const SharedFrameContext& ctx,
                                     Render::Frame& outFrame,
                                     juce::Image* imageOverride,
                                     float opacityOverride)
{
    juce::Image* glyphImage;
    bool barNote;

    bool starPowerActive = state.getProperty("starPower");
    bool isDrums = isDrumLike(activePart);

    if (imageOverride)
    {
        glyphImage = imageOverride;
        barNote = isBarNote(gemColumn, isDrums ? Part::DRUMS : Part::GUITAR);
    }
    else if (isGuitarLike(activePart))
    {
        barNote = isBarNote(gemColumn, Part::GUITAR);
        glyphImage = assetManager.getGuitarGlyphImage(gemWrapper, gemColumn, starPowerActive);
    }
    else
    {
        barNote = isBarNote(gemColumn, Part::DRUMS);
        glyphImage = assetManager.getDrumGlyphImage(gemWrapper, gemColumn, starPowerActive);
    }

    if (!imageOverride)
    {
        double clipTime = barNote ? cachedBarClipTime : cachedNoteClipTime;
        if (frameTime < clipTime) return;
        if (barNote && !showBars) return;
        if (!barNote && !showGems) return;
    }

    if (glyphImage == nullptr)
        return;

    float imageAspect = (float)glyphImage->getWidth() / (float)glyphImage->getHeight();
    float opacity = (opacityOverride >= 0.0f) ? opacityOverride : calculateOpacity(position);
    if (!barNote && barModeDim < 1.0f) opacity *= barModeDim;

    if (PositionMath::bemaniMode)
    {
        drawGemBemani(gemColumn, gemWrapper, position, frameTime, glyphImage, barNote, opacity);
        return;
    }

    // ============================================================================
    // PERSPECTIVE PATH: append sprites to the shared composite frame.
    // The shared anchor + frameScale come from ctx (one projection of the lane
    // plane at this musical position). All sprite offsets and sizes below are
    // expressed in strike-reference pixels relative to ctx.anchor; drawFrame
    // applies ctx.frameScale uniformly so the bar and its stacked gems can't
    // drift apart.
    // ============================================================================

    // Strike-reference width + horizontal offset from shared anchor
    float strikeColWidth, strikeOffsetX;
    if (barNote)
    {
        strikeColWidth = ctx.fbStrikeWidth
                       * PositionConstants::BAR_FRETBOARD_FIT * PositionConstants::BAR_SIZE;
        strikeOffsetX = 0.0f;  // bar is centered on fretboard
    }
    else
    {
        const auto& colCoordsRef = isGuitarLike(activePart)
            ? laneCoordsGuitar[(gemColumn < GUITAR_LANE_COUNT) ? gemColumn : 1]
            : laneCoordsDrums[drumColumnIndex(gemColumn)];
        auto strikeEdge = getColumnEdge(0.0f, colCoordsRef, PositionConstants::GEM_SIZE,
                                         PositionConstants::FRETBOARD_SCALE);
        strikeColWidth = strikeEdge.rightX - strikeEdge.leftX;
        strikeOffsetX = (strikeEdge.leftX + strikeEdge.rightX) * 0.5f - ctx.fbStrikeCenterX;
    }
    float strikeColHeight = strikeColWidth / imageAspect;

    // userScale (settings popup) — sprite-size multiplier; center stays at lane
    float userScale = barNote
        ? (state.hasProperty("barScale") ? (float)state["barScale"] : 1.0f)
        : (state.hasProperty("gemScale") ? (float)state["gemScale"] : 1.0f);

    // Per-gem-type scale + star-power multiplier
    float typeScale = barNote ? 1.0f : gemTypeScale(gemWrapper.gem, isDrums, gemTypeScales);
    float spMul = 1.0f;
    if (!barNote && gemWrapper.starPower)
    {
        float spScale = gemTypeScales.spGem;
        if (std::abs(spScale - 1.0f) > 0.001f) spMul = spScale;
    }

    const auto& baseScale = barNote ? barScale : gemScale;
    float wScale = baseScale.width  * typeScale * spMul * userScale;
    float hScale = baseScale.height * typeScale * spMul * userScale;

    // zOff (Z lift in strike-reference pixels) + per-column adjustment
    bool isCymbalGem = !barNote && isDrums
        && (gemWrapper.gem == Gem::CYM
            || gemWrapper.gem == Gem::CYM_GHOST
            || gemWrapper.gem == Gem::CYM_ACCENT);
    float zOff = barNote ? barZOffset : (isCymbalGem ? cymZOffset : gemZOffset);

    float colSNear = 1.0f, colSFar = 1.0f, colW = 1.0f, colH = 1.0f;
    if (!isDrums && gemColumn < (int)GUITAR_LANE_COUNT) {
        const auto& ca = guitarColAdjust[gemColumn];
        colSNear = ca.sNear; colSFar = ca.sFar; colW = ca.w; colH = ca.h;
    } else if (isDrums) {
        uint drumIdx = drumColumnIndex(gemColumn);
        const auto& ca = drumColAdjust[drumIdx];
        colSNear = ca.sNear; colSFar = ca.sFar; colW = ca.w; colH = ca.h;
        if (!barNote) zOff += ca.z * resScale;   // ca.z is at REFERENCE_HEIGHT
    }

    float t = juce::jlimit(0.0f, 1.0f, position / currentVpDepth);
    float colScale = colSNear + (colSFar - colSNear) * t;
    wScale *= colScale * colW;
    hScale *= colScale * colH;

    // Curvature arc, in strike-reference pixels (ctx.frameScale carries it to
    // current pixels at draw time, matching legacy current-pixel-space arc).
    float curvature = barNote ? PositionConstants::BAR_CURVATURE : currentNoteCurvature;
    float arcOffsetStrike = 0.0f;
    if (curvature != 0.0f && !barNote)
    {
        float dist = getColumnDistFromCenter(gemColumn, isDrums);
        arcOffsetStrike = ctx.fbStrikeWidth * PositionConstants::FRETBOARD_SCALE
                        * curvature * (1.0f - dist * dist);
    }

    // --- Append gem sprite ---
    int gemIdx = (int)outFrame.sprites.size();
    {
        Render::FrameSprite s;
        s.image     = glyphImage;
        s.offsetX   = strikeOffsetX;
        s.offsetY   = zOff + arcOffsetStrike;
        s.width     = strikeColWidth  * wScale;
        s.height    = strikeColHeight * hScale;
        s.drawOrder = barNote ? (int)DrawOrder::BAR : (int)DrawOrder::NOTE;
        s.drawColumn = (int)gemColumn;
        s.opacity   = opacity;
        outFrame.sprites.push_back(s);
    }

    // --- Append overlay sprite if present ---
    juce::Image* overlayImage = assetManager.getOverlayImage(
        gemWrapper.gem, isGuitarLike(activePart) ? Part::GUITAR : Part::DRUMS);
    const OverlayAdjust* overlayAdjPtr = nullptr;
    int ovlIdx = -1;
    if (overlayImage != nullptr)
    {
        const auto& overlayAdj = getOverlayAdjustForGem(gemWrapper.gem, isDrums);
        overlayAdjPtr = &overlayAdj;

        float ovlRectSX = overlayAdj.scaleX * overlayAdj.scale;
        float ovlRectSY = overlayAdj.scaleY * overlayAdj.scale;

        Render::FrameSprite s;
        s.image     = overlayImage;
        s.width     = strikeColWidth  * wScale * ovlRectSX;
        s.height    = strikeColHeight * hScale * ovlRectSY;
        s.offsetX   = strikeOffsetX + overlayAdj.offsetX * s.width;
        s.offsetY   = zOff + arcOffsetStrike + overlayAdj.offsetY * s.height;
        s.drawOrder = (int)DrawOrder::OVERLAY;
        s.drawColumn = (int)gemColumn;
        s.opacity   = opacity;
        ovlIdx = (int)outFrame.sprites.size();
        outFrame.sprites.push_back(s);
    }

    // --- Curvature swap: replace gem (and overlay) sprite images with cached
    // curved variants; adjust height to curved aspect and re-add content Y
    // offset so the opaque cone sits where the straight asset's cone would. ---
    if (curvature != 0.0f)
    {
        CurvedSwapArgs args{
            glyphImage, overlayImage, overlayAdjPtr,
            (int)gemColumn, isDrums,
            strikeColWidth, strikeColHeight, hScale,
            strikeOffsetX, zOff + arcOffsetStrike,
        };
        applyCurvedImageSwap(outFrame, gemIdx, ovlIdx, args);
    }

    // Capture hit box from final gem sprite (uses same transform as drawFrame)
    if (imageOverride == nullptr)
    {
        const auto& gs = outFrame.sprites[gemIdx];
        float cx = ctx.anchor.x + gs.offsetX * ctx.frameScale.x;
        float cy = ctx.anchor.y + gs.offsetY * ctx.frameScale.y;
        float sw = gs.width  * ctx.frameScale.x;
        float sh = gs.height * ctx.frameScale.y;
        hitBoxes.push_back({ (int)gemColumn, frameTime,
            juce::Rectangle<float>(cx - sw * 0.5f, cy - sh * 0.5f, sw, sh) });
    }
}

void NoteRenderer::drawGemBemani(uint gemColumn, const GemWrapper& gemWrapper, float position,
                                  double frameTime, juce::Image* glyphImage, bool barNote, float opacity)
{
    const auto* config = currentConfig;
    bool isDrums = isDrumLike(activePart);
    float imageAspect = (float)glyphImage->getWidth() / (float)glyphImage->getHeight();
    float sizeScale = barNote ? PositionConstants::BAR_SIZE : PositionConstants::GEM_SIZE;

    // --- Build glyphRect (screen-space) ---
    juce::Rectangle<float> glyphRect;
    if (barNote)
    {
        glyphRect = PositionMath::computeBemaniBarRect(
            isDrums, position, width, height, posEnd,
            sizeScale, imageAspect);
    }
    else
    {
        const NormalizedCoordinates* colCoordsPtr = nullptr;
        int bemaniIdx = -1;
        if (isGuitarLike(activePart))
        {
            int idx = (gemColumn < GUITAR_LANE_COUNT) ? gemColumn : 1;
            colCoordsPtr = &laneCoordsGuitar[idx];
            bemaniIdx = idx - 1;
        }
        else
        {
            uint drumIdx = drumColumnIndex(gemColumn);
            colCoordsPtr = &laneCoordsDrums[drumIdx];
            bemaniIdx = (int)drumIdx - 1;
        }
        auto edge = getColumnEdge(position, *colCoordsPtr, 1.0f,
                                  PositionConstants::FRETBOARD_SCALE, bemaniIdx);
        float laneWidth = edge.rightX - edge.leftX;
        float colWidth = laneWidth * sizeScale;
        float colHeight = colWidth / imageAspect;
        float cx = (edge.leftX + edge.rightX) * 0.5f;
        glyphRect = juce::Rectangle<float>(cx - colWidth * 0.5f, edge.centerY - colHeight * 0.5f, colWidth, colHeight);
    }

    // --- Settings-popup userScale, then bemani Y nudge ---
    float userScale = barNote
        ? (state.hasProperty("barScale") ? (float)state["barScale"] : 1.0f)
        : (state.hasProperty("gemScale") ? (float)state["gemScale"] : 1.0f);
    if (std::abs(userScale - 1.0f) > 0.001f)
    {
        float cx = glyphRect.getCentreX();
        float cy = glyphRect.getCentreY();
        glyphRect = juce::Rectangle<float>(cx - glyphRect.getWidth() * userScale / 2.0f,
                                            cy - glyphRect.getHeight() * userScale / 2.0f,
                                            glyphRect.getWidth() * userScale,
                                            glyphRect.getHeight() * userScale);
    }
    bool isCymbalGem = !barNote && isDrums
        && (gemWrapper.gem == Gem::CYM
            || gemWrapper.gem == Gem::CYM_GHOST
            || gemWrapper.gem == Gem::CYM_ACCENT);
    float nudge = barNote ? bemaniConfig.barNudge
                          : (isCymbalGem ? bemaniConfig.cymNudge : config->bemaniGemNudge());
    float pixelsPerUnit = PositionConstants::REFERENCE_HEIGHT * bemaniConfig.strikelinePos
                        / std::max(0.1f, PositionMath::bemaniHwyScale);
    glyphRect.translate(0.0f, pixelsPerUnit * nudge);

    // --- Per-gem-type scale + star-power multiplier ---
    float typeScale = barNote ? 1.0f : gemTypeScale(gemWrapper.gem, isDrums, gemTypeScales);
    float spMul = 1.0f;
    if (!barNote && gemWrapper.starPower)
    {
        float spScale = gemTypeScales.spGem;
        if (std::abs(spScale - 1.0f) > 0.001f) spMul = spScale;
    }

    float baseW = barNote ? bemaniConfig.barW : bemaniConfig.gemW;
    float baseH = barNote ? bemaniConfig.barH : bemaniConfig.gemH;
    float wScale = baseW * typeScale * spMul;
    float hScale = baseH * typeScale * spMul;

    float baseCurv = barNote ? PositionConstants::BAR_CURVATURE : currentNoteCurvature;
    float curvature = baseCurv * bemaniConfig.curvature;

    // --- Single-gem Frame, scale = 1 (flat). zOff folds into sprite-level offsets. ---
    Render::Frame frame;

    int gemIdx = (int)frame.sprites.size();
    {
        Render::FrameSprite s;
        s.image = glyphImage;
        s.offsetX = 0.0f;
        s.offsetY = 0.0f;
        s.width = glyphRect.getWidth() * wScale;
        s.height = glyphRect.getHeight() * hScale;
        s.drawOrder = barNote ? (int)DrawOrder::BAR : (int)DrawOrder::NOTE;
        s.drawColumn = (int)gemColumn;
        s.opacity = opacity;
        frame.sprites.push_back(s);
    }

    juce::Image* overlayImage = assetManager.getOverlayImage(
        gemWrapper.gem, isGuitarLike(activePart) ? Part::GUITAR : Part::DRUMS);
    const OverlayAdjust* overlayAdjPtr = nullptr;
    int ovlIdx = -1;
    if (overlayImage != nullptr)
    {
        const auto& overlayAdj = getOverlayAdjustForGem(gemWrapper.gem, isDrums);
        overlayAdjPtr = &overlayAdj;

        float ovlRectSX = overlayAdj.scaleX * overlayAdj.scale;
        float ovlRectSY = overlayAdj.scaleY * overlayAdj.scale;

        Render::FrameSprite ov;
        ov.image = overlayImage;
        ov.width = glyphRect.getWidth() * wScale * ovlRectSX;
        ov.height = glyphRect.getHeight() * hScale * ovlRectSY;
        ov.offsetX = overlayAdj.offsetX * ov.width;
        ov.offsetY = overlayAdj.offsetY * ov.height;
        ov.drawOrder = (int)DrawOrder::OVERLAY;
        ov.drawColumn = (int)gemColumn;
        ov.opacity = opacity;
        ovlIdx = (int)frame.sprites.size();
        frame.sprites.push_back(ov);
    }

    if (curvature != 0.0f)
    {
        CurvedSwapArgs args{
            glyphImage, overlayImage, overlayAdjPtr,
            (int)gemColumn, isDrums,
            glyphRect.getWidth(), glyphRect.getHeight(), hScale,
            0.0f, 0.0f,
        };
        applyCurvedImageSwap(frame, gemIdx, ovlIdx, args);
    }

    juce::Point<float> bemaniAnchor(glyphRect.getCentreX(), glyphRect.getCentreY());
    juce::Point<float> bemaniScale(1.0f, 1.0f);
    Render::drawFrame(frame, bemaniAnchor, bemaniScale, *currentDrawCallMap);

    const auto& gs = frame.sprites[gemIdx];
    float sw = gs.width;
    float sh = gs.height;
    hitBoxes.push_back({ (int)gemColumn, frameTime,
        juce::Rectangle<float>(bemaniAnchor.x - sw * 0.5f,
                               bemaniAnchor.y - sh * 0.5f, sw, sh) });
}

float NoteRenderer::getColumnDistFromCenter(int column, bool isDrums)
{
    const auto& fbCoords = isDrums ? drumFretboardCoords : guitarFretboardCoords;
    const auto& colCoords = isDrums
        ? laneCoordsDrums[(column == 6) ? 0 : ((column < (int)DRUM_LANE_COUNT) ? column : 1)]
        : laneCoordsGuitar[(column < (int)GUITAR_LANE_COUNT) ? column : 1];
    return PositionMath::columnDistFromCenter(fbCoords, colCoords);
}

const NoteRenderer::CurvedImageEntry& NoteRenderer::getCurvedImage(
    juce::Image* src, int column, bool isDrums)
{
    float curv = isDrums ? noteCurvatureDrums : noteCurvatureGuitar;
    CurveKey key{src, column, isDrums, (int)std::lround(curv * 10000.0f)};
    auto it = curvedCache.find(key);
    if (it != curvedCache.end())
        return it->second;

    // Bound cache so dragging the curvature slider can't grow unbounded.
    // 100 entries ≈ 3 curvatures × all (src,column,isDrums) combos.
    if (curvedCache.size() >= 100)
        curvedCache.clear();

    int srcW = src->getWidth() / NOTE_CACHE_DOWNSAMPLE;
    int srcH = src->getHeight() / NOTE_CACHE_DOWNSAMPLE;
    if (srcW < 1) srcW = 1;
    if (srcH < 1) srcH = 1;

    // Downsampled source
    juce::Image downSrc(juce::Image::ARGB, srcW, srcH, true);
    {
        juce::Graphics gDown(downSrc);
        gDown.drawImage(*src, juce::Rectangle<float>(0.0f, 0.0f, (float)srcW, (float)srcH));
    }

    // Compute per-column Y offsets: arcHeight * (1 - dist²) matching snapshot math
    const auto& fbCoords = isDrums ? drumFretboardCoords : guitarFretboardCoords;
    float fbCenterNorm = fbCoords.normX1 + fbCoords.normWidth1 * 0.5f;
    float fbHalfWNorm = fbCoords.normWidth1 * 0.5f;

    const auto& colCoords = isDrums
        ? laneCoordsDrums[(column == 6) ? 0 : ((column < (int)DRUM_LANE_COUNT) ? column : 1)]
        : laneCoordsGuitar[(column < (int)GUITAR_LANE_COUNT) ? column : 1];

    float fbWidthInCache = (float)srcW * (fbCoords.normWidth1 / colCoords.normWidth1);
    float arcHeight = fbWidthInCache * curv;

    float noteLeftNorm = colCoords.normX1;
    float noteRightNorm = colCoords.normX1 + colCoords.normWidth1;

    // Compute Y offset for every pixel column
    std::vector<float> colOffsets(srcW);

    for (int x = 0; x < srcW; x++)
    {
        float t = ((float)x + 0.5f) / (float)srcW;
        float xNorm = noteLeftNorm + t * (noteRightNorm - noteLeftNorm);
        float dist = (xNorm - fbCenterNorm) / fbHalfWNorm;
        colOffsets[x] = arcHeight * (1.0f - dist * dist);
    }

    // Global reference: the arc value at fretboard edge (dist=1), where yOff=0.
    // colOffsets[x] - globalRef gives the displacement from the edge baseline.
    // Positive curvature: center has most displacement (pushed down on screen = convex).
    // Negative curvature: center has most negative offset, edges at 0.
    float globalRef = std::min(0.0f, arcHeight);
    float maxShift = 0.0f;
    for (int x = 0; x < srcW; x++)
    {
        float shift = colOffsets[x] - globalRef;
        if (shift > maxShift) maxShift = shift;
    }

    int extraPx = (int)std::ceil(maxShift) + 2;
    int destH = srcH + extraPx;

    juce::Image dest(juce::Image::ARGB, srcW, destH, true);

    // Per-pixel warp via BitmapData — inverse mapping: for each dest pixel, sample source
    {
        juce::Image::BitmapData srcData(downSrc, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData dstData(dest, juce::Image::BitmapData::writeOnly);

        for (int x = 0; x < srcW; x++)
        {
            // Absolute Y shift: all columns share the same global reference
            float yShift = colOffsets[x] - globalRef;

            for (int dy = 0; dy < destH; dy++)
            {
                float sy = (float)dy - yShift;

                int sy0 = (int)std::floor(sy);
                int sy1 = sy0 + 1;
                float frac = sy - (float)sy0;

                if (sy0 < 0 || sy1 >= srcH) {
                    if (sy0 >= 0 && sy0 < srcH) {
                        dstData.setPixelColour(x, dy, srcData.getPixelColour(x, sy0));
                    } else if (sy1 >= 0 && sy1 < srcH) {
                        dstData.setPixelColour(x, dy, srcData.getPixelColour(x, sy1));
                    }
                    continue;
                }

                auto c0 = srcData.getPixelColour(x, sy0);
                auto c1 = srcData.getPixelColour(x, sy1);
                dstData.setPixelColour(x, dy, c0.interpolatedWith(c1, frac));
            }
        }
    }

    // yOffsetFraction: where this column's content center sits relative to dest image center
    float centerColShift = colOffsets[srcW / 2] - globalRef;
    float srcCenterInDest = centerColShift + (float)srcH * 0.5f;
    float destCenter = (float)destH * 0.5f;
    float yOffsetFraction = (srcCenterInDest - destCenter) / (float)srcH;

    auto [insertIt, _] = curvedCache.emplace(key, CurvedImageEntry{std::move(dest), yOffsetFraction});
    return insertIt->second;
}
