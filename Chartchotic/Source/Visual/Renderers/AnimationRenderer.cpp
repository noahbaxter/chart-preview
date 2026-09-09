/*
    ==============================================================================

        AnimationRenderer.cpp
        Created by Claude Code (refactoring animation logic)
        Author: Noah Baxter

        Encapsulates animation detection, state management, and rendering.

    ==============================================================================
*/

#include "AnimationRenderer.h"
#include "../Utils/RenderTypeConfig.h"

using namespace AnimationConstants;
using namespace PositionConstants;

//==============================================================================

AnimationRenderer::AnimationRenderer(juce::ValueTree &state, AssetManager &assetManager)
    : state(state), assetManager(assetManager)
{
}

AnimationRenderer::~AnimationRenderer()
{
}

//==============================================================================
// Helper: Trigger animation for a specific gem column

void AnimationRenderer::triggerAnimationForColumn(uint gemColumn, Gem gemType, bool starPower)
{
    bool isDrums = isDrumLike(activePart);
    bool is2xKick = isDrums && gemColumn == DRUM_KICK_2X_COLUMN;
    animationManager.triggerHit(gemColumn, isDrums, is2xKick, gemType, starPower);
}

//==============================================================================
// Animation Detection

void AnimationRenderer::detectAndTriggerAnimations(const TimeBasedTrackWindow& trackWindow, double strikeTimeOffset)
{
    // Strike point is at strikeTimeOffset seconds from cursor (0 = strikeline, negative = past it)
    // For each column, find the closest note that has passed the strike point
    // If it's a new note (different from last frame), trigger the animation

    std::array<double, 7> closestPastNotePerColumn = {999.0, 999.0, 999.0, 999.0, 999.0, 999.0, 999.0};
    std::array<GemWrapper, 7> closestGemPerColumn;

    // Find the closest note that has just crossed (or is at) the strike point for each column
    for (const auto &frameItem : trackWindow)
    {
        double frameTime = frameItem.first;  // Time in seconds from cursor
        const auto& gems = frameItem.second;

        // Notes that have crossed the strike point and are within 50ms past it
        if (frameTime <= strikeTimeOffset && frameTime >= strikeTimeOffset - 0.05)
        {
            for (uint gemColumn = 0; gemColumn < gems.size(); ++gemColumn)
            {
                if (gems[gemColumn].gem != Gem::NONE)
                {
                    // This note is past the strike point - check if it's the closest one
                    double distFromStrike = std::abs(frameTime - strikeTimeOffset);
                    double closestDist = std::abs(closestPastNotePerColumn[gemColumn] - strikeTimeOffset);
                    if (distFromStrike < closestDist)
                    {
                        closestPastNotePerColumn[gemColumn] = frameTime;
                        closestGemPerColumn[gemColumn] = gems[gemColumn];
                    }
                }
            }
        }
    }

    // Now trigger animations for any column where we found a new note
    for (uint gemColumn = 0; gemColumn < closestPastNotePerColumn.size(); ++gemColumn)
    {
        // If we found a note (not 999.0) and it's different from the last one we processed
        if (closestPastNotePerColumn[gemColumn] < 999.0 &&
            closestPastNotePerColumn[gemColumn] != lastNoteTimePerColumn[gemColumn])
        {
            // This is a new note! Trigger the animation
            lastNoteTimePerColumn[gemColumn] = closestPastNotePerColumn[gemColumn];
            triggerAnimationForColumn(gemColumn, closestGemPerColumn[gemColumn].gem, closestGemPerColumn[gemColumn].starPower);
        }
    }
}

//==============================================================================
// Sustain State Management

void AnimationRenderer::updateSustainStates(const TimeBasedSustainWindow& sustainWindow, bool isPlaying)
{
    // Strikeline is at time 0 (current playback position)
    // Check if each lane is currently in a sustain (sustain crosses the strikeline)
    std::array<bool, 6> lanesSustaining = {false, false, false, false, false, false};
    const auto& animations = animationManager.getActiveAnimations();

    for (const auto& sustain : sustainWindow)
    {
        // Sustain is active at the strikeline if startTime <= 0 <= endTime
        if (sustain.startTime <= 0.0 && sustain.endTime >= 0.0 &&
            sustain.sustainType == SustainType::SUSTAIN && sustain.gemColumn < lanesSustaining.size())
        {
            lanesSustaining[sustain.gemColumn] = true;

            // Force-trigger: If playing and sustain is active but no animation exists yet
            // (e.g., when seeking into middle of sustain), trigger it now
            if (isPlaying && sustain.gemColumn < animations.size() && !animations[sustain.gemColumn].isActive())
            {
                triggerAnimationForColumn(sustain.gemColumn, Gem::NOTE, sustain.gemType.starPower);
            }
        }
    }

    // Update sustain state for each lane
    for (size_t lane = 0; lane < lanesSustaining.size(); ++lane)
    {
        animationManager.setSustainState(static_cast<int>(lane), lanesSustaining[lane]);
    }
}

//==============================================================================
// Animation Rendering

void AnimationRenderer::renderToDrawCallMap(DrawCallMap& drawCallMap, uint width, uint height,
                                             float posEnd,
                                             float strikePos)
{
    cachedWidth = width;
    cachedHeight = height;

    const auto& animations = animationManager.getActiveAnimations();
    const auto* config = getRenderTypeConfig(getRenderType(activePart));
    float resScale = (float)height / PositionConstants::REFERENCE_HEIGHT;

    for (const auto& anim : animations)
    {
        if (!anim.isActive()) continue;

        if (anim.isBar)
        {
            uint column = anim.is2xKick ? 6 : 0;
            CoordinateOffset offset = config->animOffsets[0];
            offset.xOffset *= resScale;
            offset.yOffset *= resScale;

            drawCallMap[static_cast<int>(DrawOrder::BAR_ANIMATION)][column].push_back([this, anim, width, height, offset, posEnd, strikePos](juce::Graphics &g) {
                this->renderKickAnimation(g, anim, width, height, offset, posEnd, strikePos);
            });
        }
        else
        {
            CoordinateOffset offset = config->animOffsets[anim.lane];
            offset.xOffset *= resScale;
            offset.yOffset *= resScale;

            drawCallMap[static_cast<int>(DrawOrder::NOTE_ANIMATION)][anim.lane].push_back([this, anim, width, height, offset, posEnd, strikePos](juce::Graphics &g) {
                this->renderFretAnimation(g, anim, width, height, offset, posEnd, strikePos);
            });
        }
    }
}

void AnimationRenderer::renderKickAnimation(juce::Graphics &g, const AnimationConstants::HitAnimation& anim, uint width, uint height, const PositionConstants::CoordinateOffset& offset,
                                             float posEnd, float strikePos)
{
    float strikelinePosition = strikePos;
    bool isGuitar = isGuitarLike(activePart);
    bool isDrums = !isGuitar;
    const auto* config = getRenderTypeConfig(getRenderType(activePart));

    bool useWhiteSP = anim.starPower && hitTypeConfig.spWhiteFlare;

    juce::Image* animFrame = nullptr;
    if (useWhiteSP) {
        animFrame = assetManager.getHitAnimationFrame(anim.currentFrame);
    } else if (isGuitar && anim.isOpen) {
        animFrame = assetManager.getOpenAnimationFrame(anim.currentFrame);
    } else {
        animFrame = assetManager.getKickAnimationFrame(anim.currentFrame);
    }

    if (animFrame)
    {
        uint colIdx = 0; // Both guitar open and drum kick use index 0
        const auto& colCoords = isDrums
            ? laneCoordsDrums[colIdx]
            : laneCoordsGuitar[colIdx];

        // Kick/open = bar note — match NoteRenderer sizing exactly
        PositionConstants::LaneCorners edge;
        float imageAspect = (float)animFrame->getWidth() / (float)animFrame->getHeight();

        if (PositionMath::bemaniMode)
        {
            edge = PositionMath::getFretboardEdge(isDrums, strikelinePosition, cachedWidth, cachedHeight,
                       PositionConstants::HIGHWAY_POS_START, posEnd);
            // Use the bar note glyph aspect ratio (not the animation frame aspect)
            // to match NoteRenderer sizing exactly
            juce::Image* noteGlyph = isDrums
                ? assetManager.getDrumGlyphImage(GemWrapper(Gem::NOTE, false), 0, false)
                : assetManager.getGuitarGlyphImage(GemWrapper(Gem::NOTE, false), 0, false);
            float noteAspect = (noteGlyph && noteGlyph->getHeight() > 0)
                ? (float)noteGlyph->getWidth() / (float)noteGlyph->getHeight()
                : imageAspect;
            auto kickRect = PositionMath::computeBemaniBarRect(
                isDrums, strikelinePosition, cachedWidth, cachedHeight,
                posEnd, PositionConstants::BAR_SIZE, noteAspect);

            // Match NoteRenderer: user barScale
            float userScale = state.hasProperty("barScale") ? (float)state["barScale"] : 1.0f;
            float bw = bemaniConfig.barW;
            float bh = bemaniConfig.barH;
            float nudge = bemaniConfig.barNudge;
            kickRect = kickRect.withSizeKeepingCentre(
                kickRect.getWidth() * bw * userScale * offset.widthScale,
                kickRect.getHeight() * bh * userScale * offset.heightScale
            );
            // Center on the strikeline pad, then apply hit bar nudge
            float padY = (float)cachedHeight * bemaniConfig.strikelinePos;
            kickRect.translate(offset.xOffset, offset.yOffset + (padY - kickRect.getCentreY()) + kickRect.getHeight() * config->bemaniHitBarNudge());

            g.setOpacity(1.0f);
            g.drawImage(*animFrame, kickRect);

            if (useWhiteSP)
            {
                auto* flareImage = assetManager.getHitFlareWhiteImage();
                if (flareImage && anim.currentFrame <= HIT_FLARE_MAX_FRAME)
                {
                    g.setOpacity(HIT_FLARE_OPACITY);
                    g.drawImage(*flareImage, kickRect);
                }
            }
        }
        else
        {
            edge = getColumnEdge(strikelinePosition, colCoords, PositionConstants::BAR_SIZE,
                                 posEnd, PositionConstants::FRETBOARD_SCALE, -1);
            auto perspParams = config->getPerspectiveParams();
            float colWidth = edge.rightX - edge.leftX;
            float colHeight = colWidth / perspParams.barNoteHeightRatio;
            juce::Rectangle<float> kickRect(edge.leftX, edge.centerY - colHeight * 0.5f + hitBarZOffset, colWidth, colHeight);

            // Apply hit bar scale + animation offset
            kickRect = kickRect.withSizeKeepingCentre(
                kickRect.getWidth() * hitBarScale.scale * hitBarScale.width * offset.widthScale,
                kickRect.getHeight() * hitBarScale.scale * hitBarScale.height * offset.heightScale
            ).translated(offset.xOffset, offset.yOffset);

            g.setOpacity(1.0f);
            g.drawImage(*animFrame, kickRect);

            // White SP flare for bar hits
            if (useWhiteSP)
            {
                auto* flareImage = assetManager.getHitFlareWhiteImage();
                if (flareImage && anim.currentFrame <= HIT_FLARE_MAX_FRAME)
                {
                    g.setOpacity(HIT_FLARE_OPACITY);
                    g.drawImage(*flareImage, kickRect);
                }
            }
        }
    }
}

void AnimationRenderer::renderFretAnimation(juce::Graphics &g, const AnimationConstants::HitAnimation& anim, uint width, uint height, const PositionConstants::CoordinateOffset& offset,
                                             float posEnd, float strikePos)
{
    float strikelinePosition = strikePos;
    bool isGuitar = isGuitarLike(activePart);
    bool isDrums = !isGuitar;
    Part currentPart = isGuitar ? Part::GUITAR : Part::DRUMS;
    const auto* config = getRenderTypeConfig(getRenderType(activePart));

    auto hitFrame = assetManager.getHitAnimationFrame(anim.currentFrame);

    // Use white flare for SP, purple for tap, otherwise colored
    bool useWhite = (anim.starPower && hitTypeConfig.spWhiteFlare);
    bool usePurple = (isGuitar && anim.gemType == Gem::TAP_ACCENT && hitTypeConfig.tapPurpleFlare);
    auto flareImage = useWhite
        ? assetManager.getHitFlareWhiteImage()
        : usePurple
            ? assetManager.getHitFlarePurpleImage()
            : assetManager.getHitFlareImage(anim.lane, currentPart);

    bool barNote = isBarNote(anim.lane, currentPart);
    uint colIdx = anim.lane;
    if (isDrums) {
        colIdx = drumColumnIndex(anim.lane) < PositionConstants::DRUM_LANE_COUNT ? drumColumnIndex(anim.lane) : 1;
    } else {
        colIdx = (anim.lane < PositionConstants::GUITAR_LANE_COUNT) ? anim.lane : 1;
    }
    const auto& colCoords = isDrums
        ? laneCoordsDrums[colIdx]
        : laneCoordsGuitar[colIdx];

    float sizeScale = barNote ? PositionConstants::BAR_SIZE : PositionConstants::GEM_SIZE;
    int bemaniIdx = barNote ? -1 : ((int)colIdx - 1);
    auto edge = getColumnEdge(strikelinePosition, colCoords, sizeScale,
                               posEnd, PositionConstants::FRETBOARD_SCALE, bemaniIdx);
    auto perspParams = config->getPerspectiveParams();
    float colWidth = edge.rightX - edge.leftX;
    float colHeight = colWidth / (barNote ? perspParams.barNoteHeightRatio : perspParams.regularNoteHeightRatio);

    // Z offset: bar uses hitBarZOffset, gems use hitGemZOffset + per-column offset
    float zOff = barNote ? hitBarZOffset : hitGemZOffset;
    if (!barNote && isDrums) {
        uint drumIdx = drumColumnIndex(anim.lane) < DRUM_LANE_COUNT ? drumColumnIndex(anim.lane) : 1;
        zOff += drumColAdjust[drumIdx].z * resScale;   // .z is at REFERENCE_HEIGHT
    }

    // Arc offset to match note curvature (same formula as NoteRenderer)
    float arcOffset = 0.0f;
    if (noteCurvature != 0.0f && !barNote)
    {
        const auto& fbCoords = *config->fretboardCoords;
        float dist = PositionMath::columnDistFromCenter(fbCoords, colCoords);
        float fbWidthPx = colWidth * (fbCoords.normWidth1 / colCoords.normWidth1);
        arcOffset = fbWidthPx * noteCurvature * (1.0f - dist * dist);
    }

    juce::Rectangle<float> hitRect(edge.leftX, edge.centerY - colHeight * 0.5f + zOff + arcOffset, colWidth, colHeight);

    // Per-dynamic hit scale based on gem type
    // Guitar: HOPO_GHOST=HOPO, TAP_ACCENT=tap
    // Drums:  HOPO_GHOST=ghost, TAP_ACCENT=accent, CYM_GHOST=ghost, CYM_ACCENT=accent
    float dynScale = 1.0f;
    if (anim.gemType == Gem::HOPO_GHOST)
        dynScale = isGuitar ? hitTypeConfig.hopo : hitTypeConfig.ghost;
    else if (anim.gemType == Gem::CYM_GHOST)
        dynScale = hitTypeConfig.ghost;
    else if (anim.gemType == Gem::TAP_ACCENT)
        dynScale = isGuitar ? hitTypeConfig.tap : hitTypeConfig.accent;
    else if (anim.gemType == Gem::CYM_ACCENT)
        dynScale = hitTypeConfig.accent;

    if (anim.starPower && std::abs(hitTypeConfig.sp - 1.0f) > 0.001f)
        dynScale *= hitTypeConfig.sp;

    const auto& hs = barNote ? hitBarScale : hitGemScale;
    hitRect = hitRect.withSizeKeepingCentre(
        hitRect.getWidth() * hs.scale * hs.width * dynScale * offset.widthScale,
        hitRect.getHeight() * hs.scale * hs.height * dynScale * offset.heightScale
    ).translated(offset.xOffset, offset.yOffset);

    if (PositionMath::bemaniMode)
    {
        // Center on the strikeline pad, then apply the same Y nudge the gem
        // received in NoteRenderer::drawGemBemani so the hit lands where the
        // gem sat (cymbals tracked separately from toms).
        float padY = (float)cachedHeight * bemaniConfig.strikelinePos;
        bool isCymbalGem = isDrums && (anim.gemType == Gem::CYM
                                       || anim.gemType == Gem::CYM_GHOST
                                       || anim.gemType == Gem::CYM_ACCENT);
        float nudge = isCymbalGem ? bemaniConfig.cymNudge : bemaniConfig.gemNudge(isDrums);
        float pixelsPerUnit = PositionConstants::REFERENCE_HEIGHT * bemaniConfig.strikelinePos
                            / std::max(0.1f, PositionMath::bemaniHwyScale);
        float targetY = padY + pixelsPerUnit * nudge;
        hitRect.translate(0.0f, targetY - hitRect.getCentreY());
    }

    if (hitFrame)
    {
        g.setOpacity(HIT_FLASH_OPACITY);
        g.drawImage(*hitFrame, hitRect);
    }

    if (flareImage && anim.currentFrame <= HIT_FLARE_MAX_FRAME)
    {
        g.setOpacity(HIT_FLARE_OPACITY);
        g.drawImage(*flareImage, hitRect);
    }
}

//==============================================================================
// Frame Management

void AnimationRenderer::advanceFrames(double deltaSeconds)
{
    animationManager.advanceAllFrames(deltaSeconds);
}

void AnimationRenderer::reset()
{
    animationManager.reset();
    // Reset tracking of note times
    for (auto& noteTime : lastNoteTimePerColumn) {
        noteTime = -999.0;
    }
}
