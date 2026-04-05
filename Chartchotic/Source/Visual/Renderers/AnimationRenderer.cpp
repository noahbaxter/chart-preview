/*
    ==============================================================================

        AnimationRenderer.cpp
        Created by Claude Code (refactoring animation logic)
        Author: Noah Baxter

        Encapsulates animation detection, state management, and rendering.

    ==============================================================================
*/

#include "AnimationRenderer.h"

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
    bool is2xKick = isDrums && gemColumn == 6;
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
    bool isGuitar = isGuitarLike(activePart);
    bool isDrums = !isGuitar;
    Part currentPart = isGuitar ? Part::GUITAR : Part::DRUMS;
    float resScale = (float)height / PositionConstants::REFERENCE_HEIGHT;

    for (const auto& anim : animations)
    {
        if (!anim.isActive()) continue;

        if (anim.isBar)
        {
            uint column = anim.is2xKick ? 6 : 0;
            CoordinateOffset offset = isGuitar
                ? GUITAR_ANIMATION_OFFSETS[0]
                : DRUM_ANIMATION_OFFSETS[0];
            offset.xOffset *= resScale;
            offset.yOffset *= resScale;

            uint colIdx = 0;
            const auto& colCoords = isDrums
                ? laneCoordsDrums[colIdx]
                : laneCoordsGuitar[colIdx];

            bool useWhiteSP = anim.starPower && hitTypeConfig.spWhiteFlare;
            juce::Image* animFrame = nullptr;
            if (useWhiteSP)
                animFrame = assetManager.getHitAnimationFrame(anim.currentFrame);
            else if (isGuitar && anim.isOpen)
                animFrame = assetManager.getOpenAnimationFrame(anim.currentFrame);
            else
                animFrame = assetManager.getKickAnimationFrame(anim.currentFrame);

            juce::Image* flareImage = useWhiteSP ? assetManager.getHitFlareWhiteImage() : nullptr;

            float userBarScale = state.hasProperty("barScale") ? (float)state["barScale"] : 1.0f;

            // Bemani bar rect uses the note glyph aspect (not the animation frame aspect)
            float noteAspect = 1.0f;
            if (animFrame) {
                juce::Image* noteGlyph = isDrums
                    ? assetManager.getDrumGlyphImage(GemWrapper(Gem::NOTE, false), 0, false)
                    : assetManager.getGuitarGlyphImage(GemWrapper(Gem::NOTE, false), 0, false);
                noteAspect = (noteGlyph && noteGlyph->getHeight() > 0)
                    ? (float)noteGlyph->getWidth() / (float)noteGlyph->getHeight()
                    : (float)animFrame->getWidth() / (float)animFrame->getHeight();
            }

            AnimationPainter::KickParams kp {
                anim, animFrame, flareImage, noteAspect,
                activePart, width, height, posEnd, strikePos,
                colCoords, hitBarZOffset, hitBarScale, offset, userBarScale
            };

            drawCallMap[static_cast<int>(DrawOrder::BAR_ANIMATION)][column].push_back([kp](juce::Graphics &g) {
                AnimationPainter::paintKick(g, kp);
            });
        }
        else
        {
            CoordinateOffset offset = isGuitar
                ? GUITAR_ANIMATION_OFFSETS[anim.lane]
                : DRUM_ANIMATION_OFFSETS[anim.lane];
            offset.xOffset *= resScale;
            offset.yOffset *= resScale;

            uint colIdx = anim.lane;
            if (isDrums)
                colIdx = (anim.lane == 6) ? 0 : ((anim.lane < DRUM_LANE_COUNT) ? anim.lane : 1);
            else
                colIdx = (anim.lane < GUITAR_LANE_COUNT) ? anim.lane : 1;

            const auto& colCoords = isDrums
                ? laneCoordsDrums[colIdx]
                : laneCoordsGuitar[colIdx];

            bool barNote = isBarNote(anim.lane, currentPart);
            float sizeScale = barNote ? BAR_SIZE : GEM_SIZE;
            int bemaniIdx = barNote ? -1 : ((int)colIdx - 1);

            // Z offset
            float zOff = barNote ? hitBarZOffset : hitGemZOffset;
            if (!barNote && isDrums) {
                uint drumIdx = (anim.lane == 6) ? 0 : ((anim.lane < DRUM_LANE_COUNT) ? anim.lane : 1);
                zOff += drumColZAdjust[drumIdx];
            }

            // Dynamic scale
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

            auto hitFrame = assetManager.getHitAnimationFrame(anim.currentFrame);

            bool useWhite = (anim.starPower && hitTypeConfig.spWhiteFlare);
            bool usePurple = (isGuitar && anim.gemType == Gem::TAP_ACCENT && hitTypeConfig.tapPurpleFlare);
            auto flareImage = useWhite
                ? assetManager.getHitFlareWhiteImage()
                : usePurple
                    ? assetManager.getHitFlarePurpleImage()
                    : assetManager.getHitFlareImage(anim.lane, currentPart);

            const auto& hs = barNote ? hitBarScale : hitGemScale;

            AnimationPainter::FretParams fp {
                anim, hitFrame, flareImage,
                activePart, width, height, posEnd, strikePos,
                colCoords, sizeScale, bemaniIdx,
                zOff, noteCurvature, dynScale, hs, offset
            };

            drawCallMap[static_cast<int>(DrawOrder::NOTE_ANIMATION)][anim.lane].push_back([fp](juce::Graphics &g) {
                AnimationPainter::paintFret(g, fp);
            });
        }
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
