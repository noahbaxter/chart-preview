/*
    ==============================================================================

        NoteRenderer.cpp
        Author:  Noah Baxter

        Note/gem rendering extracted from SceneRenderer.

    ==============================================================================
*/

#include "NoteRenderer.h"
#include "../Art/BarGemArt.h"
#include "../Geometry/RenderTypeConfig.h"
#include "../../Editor/AuthoringTypes.h"
#include "../../Midi/Utils/InstrumentMapper.h"

namespace
{
    // Kick mode splits the bar down the middle: 2x on the left half, 1x on the right. Both
    // the "is this a kick" and "is this the 2x" questions are per-part, since elite puts its
    // 2x on column 9 while column 6 is Tom 3.
    Render::ClipHalf resolveBarKickClip(Part part, bool barModeActive, uint gemColumn)
    {
        if (!isDrumLike(part) || !barModeActive || !isDrumKick(gemColumn, part))
            return Render::ClipHalf::None;
        return isDrum2xKick(gemColumn, part) ? Render::ClipHalf::Left
                                             : Render::ClipHalf::Right;
    }

    // A flam draws two gems inside one lane. Rather than offsetting a copy by hand, split the
    // lane into two sub-lanes and run each half through the normal lane math, so it lands on
    // the arc, foreshortens and warps like a gem that genuinely sat there. `width` is each
    // sub-lane's share of the parent, `spread` the gap between their centres, both fractions
    // of the parent's width; spread = 1 - width puts them flush with the lane's outer edges.
    PositionConstants::NormalizedCoordinates flamSubLane(
        const PositionConstants::NormalizedCoordinates& lane, bool leftHalf,
        float width, float spread)
    {
        float centreFrac = 0.5f + (leftHalf ? -0.5f : 0.5f) * spread;
        auto half = lane;
        half.normWidth1 = lane.normWidth1 * width;
        half.normWidth2 = lane.normWidth2 * width;
        half.normX1 = lane.normX1 + lane.normWidth1 * centreFrac - half.normWidth1 * 0.5f;
        half.normX2 = lane.normX2 + lane.normWidth2 * centreFrac - half.normWidth2 * 0.5f;
        return half;
    }
}

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

    // How far a flam narrows this glyph, as a fraction of the lane. Same gem-type split as
    // gemTypeScale above, so the two stay readable side by side.
    float flamWidthForGem(Gem gem, const FlamTypeWidths& w)
    {
        switch (gem) {
        case Gem::HOPO_GHOST:  return w.ghost;
        case Gem::TAP_ACCENT:  return w.accent;
        case Gem::CYM:         return w.cymbal;
        case Gem::CYM_GHOST:   return w.cymGhost;
        case Gem::CYM_ACCENT:  return w.cymAccent;
        default:               return w.note;
        }
    }
}

NoteRenderer::NoteRenderer(juce::ValueTree& state, AssetManager& assetManager)
    : state(state), assetManager(assetManager)
{
}

const OverlayAdjust& NoteRenderer::getOverlayAdjustForGem(Gem gem, bool isDrums, bool hiHat, bool hiHatOpen) const
{
    if (!isDrums) return overlayAdjusts[OVERLAY_GUITAR_TAP];
    // Open hi-hat art is taller than closed (cone lifted over a separated disc), so it needs its
    // own overlay offsets; closed hi-hats and standard cymbals fall through to their own types.
    switch (gem) {
    case Gem::HOPO_GHOST: return overlayAdjusts[OVERLAY_DRUM_NOTE_GHOST];
    case Gem::TAP_ACCENT: return overlayAdjusts[OVERLAY_DRUM_NOTE_ACCENT];
    case Gem::CYM_GHOST:  return overlayAdjusts[hiHat ? (hiHatOpen ? OVERLAY_DRUM_HIHAT_OPEN_GHOST  : OVERLAY_DRUM_HIHAT_GHOST)
                                                      : OVERLAY_DRUM_CYM_GHOST];
    case Gem::CYM_ACCENT: return overlayAdjusts[hiHat ? (hiHatOpen ? OVERLAY_DRUM_HIHAT_OPEN_ACCENT : OVERLAY_DRUM_HIHAT_ACCENT)
                                                      : OVERLAY_DRUM_CYM_ACCENT];
    default: { static const OverlayAdjust none; return none; }
    }
}

bool NoteRenderer::isEliteHiHatGlyph(const GemWrapper& gemWrapper, uint gemColumn, bool starPowerActive) const
{
    if (activePart != Part::ELITE_DRUMS || gemColumn < 1 || gemColumn > 8) return false;
    const auto& style = PositionConstants::ELITE_LANE_STYLES[gemColumn];
    if (!style.cymbal || style.tint != PositionConstants::DrumLaneTint::Yellow) return false;
    if (starPowerActive && gemWrapper.starPower) return false;   // SP draws the white cymbal
    return gemWrapper.hihat != HiHatState::Indifferent;          // Indifferent = ordinary cymbal
}

void NoteRenderer::applyCurvedImageSwap(Frame& frame, int gemIdx, int ovlIdx,
                                         const CurvedSwapArgs& args)
{
    auto& gemSprite = frame.sprites[gemIdx];
    const auto& gemEntry = getCurvedImage(args.glyphImage, args.gemColumn, args.isDrums,
                                          gemSprite.width * args.pixelScale);
    // Size the glyph content, then grow the sprite to cover the arc padding around it. Going
    // via the padded image's aspect instead lets the bake's integer padding leak into the
    // gem's height.
    float srcAspect = (float)args.glyphImage->getWidth() / (float)args.glyphImage->getHeight();
    float gemContentH = (args.gemBaseW / srcAspect) * args.hScale;
    gemSprite.image  = const_cast<juce::Image*>(&gemEntry.image);
    gemSprite.height = gemContentH / gemEntry.contentFraction;
    gemSprite.offsetY += gemEntry.yOffsetFraction * args.gemBaseH;

    if (args.overlayImage && args.overlayAdj && ovlIdx >= 0)
    {
        auto& ovlSprite = frame.sprites[ovlIdx];
        const auto& ovlEntry = getCurvedImage(args.overlayImage, args.gemColumn, args.isDrums,
                                              ovlSprite.width * args.pixelScale);
        float ovlSrcAspect = (float)args.overlayImage->getWidth()
                           / (float)args.overlayImage->getHeight();
        float ovlRectSX = args.overlayAdj->scaleX * args.overlayAdj->scale;
        float ovlRectSY = args.overlayAdj->scaleY * args.overlayAdj->scale;
        float ovlContentH = (args.overlayBaseW * ovlRectSX) / ovlSrcAspect * args.hScale;
        float ovlCurvedH = ovlContentH / ovlEntry.contentFraction;
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
    setFrame(activePart, width, height, posEnd, farFadeEnd, farFadeLen, farFadeCurve);
    currentVpDepth = currentConfig->getPerspectiveParams().vanishingPointDepth;
    currentNoteCurvature = isDrumLike(activePart) ? noteCurvatureDrums : noteCurvatureGuitar;

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
    auto fbStrike = PositionMath::getFretboardEdge(getRenderType(activePart), 0.0f, width, height, HIGHWAY_POS_START, posEnd);
    auto fbCur    = PositionMath::getFretboardEdge(getRenderType(activePart), position, width, height, HIGHWAY_POS_START, posEnd);
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
                                juce::Image* image, float opacity, Gem gem,
                                bool selected)
{
    auto ctx = buildFrameContext(position);
    GemWrapper dummy;
    dummy.gem = gem;

    Render::Frame frame;
    appendGemSprites(lane, dummy, position, 0.0, ctx, frame, image, opacity);

    for (auto& s : frame.sprites)
    {
        s.drawOrder = (int)DrawOrder::OVERLAY;
        if (selected) s.tint = AuthoringColours::selectTint;
    }

    if (!frame.sprites.empty())
        Render::drawFrame(frame, ctx.anchor, ctx.frameScale, drawCallMap);
}

void NoteRenderer::drawNoteRow(const TimeBasedTrackFrame& gems, float position, double frameTime)
{
    auto ctx = buildFrameContext(position);

    Render::Frame composite;

    // Draw order: bar columns first (behind), then hand/fret lanes. 4-lane/guitar
    // use kick(0)+2xkick(6)+pads. Elite uses kick(0)+2xkick(9)+stomp(10)+splash(11)
    // as bars, then the 8 hand lanes 1..8.
    static const uint drumSeq[]  = {0, 6, 1, 2, 3, 4, 5};
    static const uint eliteSeq[] = {0, 9, 10, 11, 1, 2, 3, 4, 5, 6, 7, 8};
    const bool elite = (activePart == Part::ELITE_DRUMS);
    const uint* drawSequence = elite ? eliteSeq : drumSeq;
    const int   seqLen       = elite ? (int)std::size(eliteSeq) : (int)std::size(drumSeq);
    for (int i = 0; i < seqLen; i++)
    {
        int gemColumn = drawSequence[i];
        if (gems[gemColumn].gem != Gem::NONE)
        {
            int spriteStart = (int)composite.sprites.size();
            if (gems[gemColumn].flam)
            {
                // Grace note first so the main hit draws over it.
                appendGemSprites(gemColumn, gems[gemColumn], position, frameTime, ctx, composite,
                                 nullptr, -1.0f, FlamHalf::Left);
                appendGemSprites(gemColumn, gems[gemColumn], position, frameTime, ctx, composite,
                                 nullptr, -1.0f, FlamHalf::Right);
            }
            else
            {
                appendGemSprites(gemColumn, gems[gemColumn], position, frameTime, ctx, composite);
            }

            bool selected = false;
            for (const auto& sg : selectedGems)
                if (sg.lane == gemColumn && std::abs(sg.time - frameTime) < 0.002)
                { selected = true; break; }

            if (selected)
            {
                for (int s = spriteStart; s < (int)composite.sprites.size(); ++s)
                    composite.sprites[s].tint = AuthoringColours::selectTint;
            }
            else
            {
                bool erasing = false;
                for (const auto& et : eraseTargets)
                    if (et.lane == gemColumn && std::abs(et.time - frameTime) < 0.002)
                    { erasing = true; break; }
                if (erasing)
                    for (int s = spriteStart; s < (int)composite.sprites.size(); ++s)
                        composite.sprites[s].tint = AuthoringColours::eraseTint;
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
                                     float opacityOverride,
                                     FlamHalf flamHalf)
{
    juce::Image* glyphImage;
    bool barNote;

    bool starPowerActive = state.getProperty("starPower");
    bool isDrums = isDrumLike(activePart);
    bool elite = (activePart == Part::ELITE_DRUMS);

    if (imageOverride)
    {
        glyphImage = imageOverride;
        barNote = isBarNote(gemColumn, isDrums ? activePart : Part::GUITAR);
    }
    else if (isGuitarLike(activePart))
    {
        barNote = isBarNote(gemColumn, Part::GUITAR);
        glyphImage = assetManager.getGuitarGlyphImage(gemWrapper, gemColumn, starPowerActive);
    }
    else
    {
        barNote = isBarNote(gemColumn, activePart);
        glyphImage = assetManager.getDrumGlyphImage(gemWrapper, gemColumn, starPowerActive, elite);
    }

    // Elite hi-hat pedal bars (Stomp / Splash) render like a bar (draw order, height, bar
    // scales) but are NOT full width: the geometry block below spans ~3 lanes centered on
    // the hi-hat lane instead of the whole fretboard.
    bool pedalBar = elite && (gemWrapper.gem == Gem::STOMP || gemWrapper.gem == Gem::SPLASH);
    if (pedalBar) barNote = true;

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
    // Ghost kicks read as a fainter bar. Placeholder for elite kick dynamics until there is
    // real art; the accent half is a separate thicker bake, so only the ghost needs this.
    if (barNote && gemWrapper.gem == Gem::HOPO_GHOST)
        opacity *= GemArt::kBarGhostOpacity;

    if (PositionMath::bemaniMode)
    {
        // Bemani has no flam treatment yet, so the two halves would stack as one gem drawn
        // twice. Draw the left call only and skip its twin.
        if (flamHalf == FlamHalf::Right) return;
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
    Render::ClipHalf barClipHalf = Render::ClipHalf::None;
    if (pedalBar)
    {
        // 3-lane span centered on the hi-hat: full-lane left edge of Snare (col 1) to the
        // full-lane right edge of Left Crash (col 3). Same shared strike anchor as the gems.
        auto eL = getColumnEdge(0.0f, laneCoords[resolveLaneIndex(1)], 1.0f, PositionConstants::FRETBOARD_SCALE);
        auto eR = getColumnEdge(0.0f, laneCoords[resolveLaneIndex(3)], 1.0f, PositionConstants::FRETBOARD_SCALE);
        strikeColWidth = eR.rightX - eL.leftX;
        strikeOffsetX  = (eL.leftX + eR.rightX) * 0.5f - ctx.fbStrikeCenterX;
    }
    else if (barNote)
    {
        strikeColWidth = ctx.fbStrikeWidth
                       * PositionConstants::BAR_FRETBOARD_FIT * PositionConstants::BAR_SIZE;
        strikeOffsetX = 0.0f;
        barClipHalf = resolveBarKickClip(activePart, barModeDim < 1.0f, gemColumn);
    }
    else
    {
        const auto& colCoordsRef = laneCoords[resolveLaneIndex(gemColumn)];
        auto strikeEdge = getColumnEdge(0.0f, colCoordsRef, PositionConstants::GEM_SIZE,
                                         PositionConstants::FRETBOARD_SCALE);
        strikeColWidth = strikeEdge.rightX - strikeEdge.leftX;
        strikeOffsetX = (strikeEdge.leftX + strikeEdge.rightX) * 0.5f - ctx.fbStrikeCenterX;
    }
    float strikeColHeight = strikeColWidth / imageAspect;

    // Elite renders into a wider canvas (currentConfig->boardWidthScale), so a full-width
    // bar comes out proportionally taller too. Bars should stretch WIDER to span the extra
    // lanes, not get thicker, so divide the height back down by that factor (1.0 for the
    // non-widened guitar / 4-lane types, so this is a no-op there).
    if (barNote)
        strikeColHeight /= currentConfig->boardWidthScale;

    // Flam: re-place this copy in its own half of the lane. Width and centre come from the
    // sub-lane's own edges, so the copy is squished AND shifted by the same projection that
    // positions a real gem. strikeColHeight above keeps the full-lane value, so the pair is
    // squished, not shrunk. laneOffsetX / laneStrikeWidth remember the parent lane for the
    // hit box, which stays full width.
    float laneOffsetX = strikeOffsetX;
    float laneStrikeWidth = strikeColWidth;
    PositionConstants::NormalizedCoordinates flamCoords;
    if (flamHalf != FlamHalf::None)
    {
        flamCoords = flamSubLane(laneCoords[resolveLaneIndex(gemColumn)],
                                 flamHalf == FlamHalf::Left,
                                 flamWidthForGem(gemWrapper.gem, flamTypeWidths),
                                 flamSubLaneSpread);
        auto flamEdge = getColumnEdge(0.0f, flamCoords, PositionConstants::GEM_SIZE,
                                      PositionConstants::FRETBOARD_SCALE);
        strikeColWidth = flamEdge.rightX - flamEdge.leftX;
        strikeOffsetX = (flamEdge.leftX + flamEdge.rightX) * 0.5f - ctx.fbStrikeCenterX;
    }

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
    } else if (isDrums && !pedalBar) {
        // Pedal bars live at virtual columns 10/11 (no entry in drumColAdjust) and are
        // positioned by their own 3-lane span above, so they skip the per-column adjust.
        uint drumIdx = drumColumnIndex(gemColumn, activePart);
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
    // Kick/open bars stay flat. Pedal bars (Stomp/Splash) instead curve like gems so they sit
    // along the tilted gridline over their off-centre span; their arc offset uses the hi-hat
    // lane's distance (the pedal zone is centred on the hi-hat). A flam half uses its own
    // sub-lane's distance, which is what lifts the two copies onto the arc separately instead
    // of parking both at the parent lane's height.
    constexpr int ELITE_HIHAT_COLUMN = 2;
    float curvature = (barNote && !pedalBar) ? PositionConstants::BAR_CURVATURE : currentNoteCurvature;
    float arcOffsetStrike = 0.0f;
    if (curvature != 0.0f && (!barNote || pedalBar))
    {
        float dist = (flamHalf != FlamHalf::None)
                   ? getColumnDistFromCenter(flamCoords, isDrums)
                   : getColumnDistFromCenter(pedalBar ? ELITE_HIHAT_COLUMN : (int)gemColumn, isDrums);
        // fbStrikeWidth is inflated on elite's wider canvas; the arc is a VERTICAL lift, and
        // elite is wider not taller, so divide the width factor back down (1.0 for non-wide
        // types) or the centre lanes bow up too far and read as sitting behind their row.
        arcOffsetStrike = ctx.fbStrikeWidth * PositionConstants::FRETBOARD_SCALE
                        * curvature * (1.0f - dist * dist)
                        / currentConfig->boardWidthScale;
    }
    // Pedal bar: drop it slightly so it sits ON the gridline (the lift over-shoots on its
    // off-centre span). REFERENCE_HEIGHT px -> current px via resScale.
    if (pedalBar)
        arcOffsetStrike += PositionConstants::ELITE_PEDAL_Z_NUDGE * resScale;

    // --- Append gem sprite ---
    int gemIdx = (int)outFrame.sprites.size();
    {
        Render::FrameSprite s;
        s.image     = glyphImage;
        s.offsetX   = strikeOffsetX;
        s.offsetY   = zOff + arcOffsetStrike;
        s.width     = strikeColWidth  * wScale;
        s.height    = strikeColHeight * hScale;
        // Pedal (Stomp/Splash) bar is a highway marking that sits ON the gridline, so it draws
        // at GRID like the gridline -> the side rails / lane lines / strikeline render OVER it
        // and clip its ends, instead of it floating on top of the rails.
        s.drawOrder = pedalBar ? (int)DrawOrder::GRID
                               : (barNote ? (int)DrawOrder::BAR : (int)DrawOrder::NOTE);
        s.drawColumn = (int)gemColumn;
        s.opacity   = opacity;
        s.clipHalf  = barClipHalf;
        outFrame.sprites.push_back(s);
    }

    // --- Append overlay sprite if present ---
    juce::Image* overlayImage = assetManager.getOverlayImage(
        gemWrapper.gem, isGuitarLike(activePart) ? Part::GUITAR : Part::DRUMS);
    const OverlayAdjust* overlayAdjPtr = nullptr;
    int ovlIdx = -1;
    // Single-overlay flams: the left copy draws no overlay, and the right one draws it centred
    // on the whole lane at full-lane width, so a flam shows one ghost ring / accent chevron
    // instead of two crossing in the overlap.
    bool flamOneOverlay = (flamHalf != FlamHalf::None) && flamSingleOverlay;
    if (flamOneOverlay && flamHalf == FlamHalf::Left)
        overlayImage = nullptr;
    if (overlayImage != nullptr)
    {
        bool hiHat = isEliteHiHatGlyph(gemWrapper, gemColumn, starPowerActive);
        const auto& overlayAdj = getOverlayAdjustForGem(gemWrapper.gem, isDrums, hiHat,
                                     hiHat && gemWrapper.hihat == HiHatState::Open);
        overlayAdjPtr = &overlayAdj;

        float ovlRectSX = overlayAdj.scaleX * overlayAdj.scale;
        float ovlRectSY = overlayAdj.scaleY * overlayAdj.scale;
        float ovlBaseW  = flamOneOverlay ? laneStrikeWidth : strikeColWidth;
        float ovlAnchorX = flamOneOverlay ? laneOffsetX : strikeOffsetX;

        Render::FrameSprite s;
        s.image     = overlayImage;
        s.width     = ovlBaseW        * wScale * ovlRectSX;
        s.height    = strikeColHeight * hScale * ovlRectSY;
        s.offsetX   = ovlAnchorX + overlayAdj.offsetX * s.width;
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
        // Pedal bar: warp across the whole pedal zone (Snare..L-Crash, cols 1-3) so the tilt +
        // curve of the gridline over that off-centre span both fall out of the same warp.
        PositionConstants::NormalizedCoordinates pedalZone;
        if (pedalBar)
        {
            const auto& l = laneCoords[resolveLaneIndex(1)];
            const auto& r = laneCoords[resolveLaneIndex(3)];
            pedalZone = l;
            pedalZone.normWidth1 = (r.normX1 + r.normWidth1) - l.normX1;
            curveCoordsOverride = &pedalZone;
            curveScaleOverride  = PositionConstants::ELITE_PEDAL_CURVE_GAIN;
        }
        // Flam half: warp across the PARENT lane. The warp is a shear and a sheared sprite
        // paints taller, so baking over the squished sub-lane let the width sliders drive
        // height. flamTilt scales the lean, 0 = level.
        else if (flamHalf != FlamHalf::None)
            curveScaleOverride = flamTilt;
        CurvedSwapArgs args{
            glyphImage, overlayImage, overlayAdjPtr,
            pedalBar ? PEDAL_CURVE_COLUMN : (int)gemColumn, isDrums,
            // gemBaseW drives the curved sprite's HEIGHT, so a flam half passes the full lane
            // width here, not its own squished width. Passing the squished width made the
            // curved path scale the gem down instead of squishing it, which is what turned
            // flams into two small gems rather than one gem split in two.
            laneStrikeWidth, strikeColHeight, hScale,
            flamOneOverlay ? laneStrikeWidth : strikeColWidth,
            flamOneOverlay ? laneOffsetX     : strikeOffsetX,
            zOff + arcOffsetStrike,
            ctx.frameScale.x,
        };
        applyCurvedImageSwap(outFrame, gemIdx, ovlIdx, args);
        curveCoordsOverride = nullptr;
        curveScaleOverride  = 1.0f;
    }

    // Capture hit box from final gem sprite (uses same transform as drawFrame).
    // A flam is two sprites but one note: the box comes off the left copy only, widened back
    // out to the full lane so the seam between the halves isn't a dead zone.
    if (imageOverride == nullptr && flamHalf != FlamHalf::Right)
    {
        const auto& gs = outFrame.sprites[gemIdx];
        bool flam = (flamHalf != FlamHalf::None);
        float cx = ctx.anchor.x + (flam ? laneOffsetX : gs.offsetX) * ctx.frameScale.x;
        float cy = ctx.anchor.y + gs.offsetY * ctx.frameScale.y;
        float sw = (flam ? laneStrikeWidth * (gs.width / strikeColWidth) : gs.width)
                 * ctx.frameScale.x;
        float sh = gs.height * ctx.frameScale.y;
        auto hbRect = juce::Rectangle<float>(cx - sw * 0.5f, cy - sh * 0.5f, sw, sh);
        if (barClipHalf == Render::ClipHalf::Left)
            hbRect = hbRect.withWidth(hbRect.getWidth() * 0.5f);
        else if (barClipHalf == Render::ClipHalf::Right)
            hbRect = hbRect.withX(hbRect.getCentreX()).withWidth(hbRect.getWidth() * 0.5f);
        hitBoxes.push_back({ (int)gemColumn, frameTime, hbRect });
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
        uint laneIdx = resolveLaneIndex(gemColumn);
        const NormalizedCoordinates* colCoordsPtr = &laneCoords[laneIdx];
        int bemaniIdx = (int)laneIdx - 1;
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
    Render::ClipHalf bemaniClipHalf = barNote ? resolveBarKickClip(activePart, barModeDim < 1.0f, gemColumn)
                                              : Render::ClipHalf::None;
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
        s.clipHalf = bemaniClipHalf;
        frame.sprites.push_back(s);
    }

    juce::Image* overlayImage = assetManager.getOverlayImage(
        gemWrapper.gem, isGuitarLike(activePart) ? Part::GUITAR : Part::DRUMS);
    const OverlayAdjust* overlayAdjPtr = nullptr;
    int ovlIdx = -1;
    if (overlayImage != nullptr)
    {
        bool starPowerActive = state.getProperty("starPower");
        bool hiHat = isEliteHiHatGlyph(gemWrapper, gemColumn, starPowerActive);
        const auto& overlayAdj = getOverlayAdjustForGem(gemWrapper.gem, isDrums, hiHat,
                                     hiHat && gemWrapper.hihat == HiHatState::Open);
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
            glyphRect.getWidth(), 0.0f, 0.0f,
            1.0f,
        };
        applyCurvedImageSwap(frame, gemIdx, ovlIdx, args);
    }

    juce::Point<float> bemaniAnchor(glyphRect.getCentreX(), glyphRect.getCentreY());
    juce::Point<float> bemaniScale(1.0f, 1.0f);
    Render::drawFrame(frame, bemaniAnchor, bemaniScale, *currentDrawCallMap);

    const auto& gs = frame.sprites[gemIdx];
    float sw = gs.width;
    float sh = gs.height;
    auto hbRect = juce::Rectangle<float>(bemaniAnchor.x - sw * 0.5f,
                                          bemaniAnchor.y - sh * 0.5f, sw, sh);
    if (bemaniClipHalf == Render::ClipHalf::Left)
        hbRect = hbRect.withWidth(hbRect.getWidth() * 0.5f);
    else if (bemaniClipHalf == Render::ClipHalf::Right)
        hbRect = hbRect.withX(hbRect.getCentreX()).withWidth(hbRect.getWidth() * 0.5f);
    hitBoxes.push_back({ (int)gemColumn, frameTime, hbRect });
}

float NoteRenderer::getColumnDistFromCenter(int column, bool isDrums)
{
    return getColumnDistFromCenter(laneCoords[resolveLaneIndex((uint)column)], isDrums);
}

// Coords overload: for spans that aren't a whole lane, like a flam's half-lane.
float NoteRenderer::getColumnDistFromCenter(
    const PositionConstants::NormalizedCoordinates& colCoords, bool isDrums)
{
    const auto& fbCoords = isDrums ? drumFretboardCoords : guitarFretboardCoords;
    return PositionMath::columnDistFromCenter(fbCoords, colCoords);
}

// Snap a drawn width up to the next power of two, bounded by the source (never bake bigger
// than the art) and by a floor (a 4px bake would alias into mush at the far end).
int NoteRenderer::curveSizeBucket(float drawnWidthPx, int sourceWidth)
{
    int want = juce::jmax(CURVE_BAKE_MIN_WIDTH, (int)std::ceil(drawnWidthPx));
    int bucket = CURVE_BAKE_MIN_WIDTH;
    while (bucket < want && bucket < sourceWidth)
        bucket *= 2;
    return juce::jmin(bucket, juce::jmax(1, sourceWidth));
}

const NoteRenderer::CurvedImageEntry& NoteRenderer::getCurvedImage(
    juce::Image* src, int column, bool isDrums, float drawnWidthPx)
{
    float curv = (isDrums ? noteCurvatureDrums : noteCurvatureGuitar) * curveScaleOverride;
    int bucket = curveSizeBucket(drawnWidthPx, src->getWidth());
    CurveKey key{src, column, isDrums, (int)std::lround(curv * 10000.0f), bucket};
    auto it = curvedCache.find(key);
    if (it != curvedCache.end())
        return it->second;

    // Bound cache so dragging the curvature slider can't grow unbounded. The working set is
    // now (src x column x curvature x size bucket): flams add one warp per elite lane at their
    // own tilt, and each gem holds a few size buckets as it travels down the highway. A cache that
    // clears mid-frame re-bakes everything on screen, so the bound has to sit well above it.
    if (curvedCache.size() >= 1200)
        curvedCache.clear();

    // Bake at the size bucket, i.e. roughly what this gem is drawn at, rather than a fixed
    // fraction of the art.
    int srcW = bucket;
    int srcH = (int)std::lround((double)src->getHeight() * bucket / (double)src->getWidth());
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

    // Pedal bar spans the whole pedal zone (off-centre); everything else warps across one lane.
    const auto& colCoords = curveCoordsOverride ? *curveCoordsOverride
                                                : laneCoords[resolveLaneIndex((uint)column)];

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

    float contentFraction = (float)srcH / (float)destH;

    auto [insertIt, _] = curvedCache.emplace(
        key, CurvedImageEntry{std::move(dest), yOffsetFraction, contentFraction});
    return insertIt->second;
}
