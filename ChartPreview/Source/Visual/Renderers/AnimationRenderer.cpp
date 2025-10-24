/*
    ==============================================================================

        AnimationRenderer.cpp
        Created by Claude Code (refactoring animation logic)
        Author: Noah Baxter

        Encapsulates animation detection, state management, and rendering.

    ==============================================================================
*/

#include "AnimationRenderer.h"
#include "../Utils/PositionConstants.h"

using namespace AnimationConstants;
using namespace PositionConstants;

//==============================================================================

AnimationRenderer::AnimationRenderer(juce::ValueTree &state, MidiInterpreter &midiInterpreter)
    : state(state), midiInterpreter(midiInterpreter)
{
}

AnimationRenderer::~AnimationRenderer()
{
}

//==============================================================================
// Helper: Trigger animation for a specific gem column

void AnimationRenderer::triggerAnimationForColumn(uint gemColumn)
{
    bool isDrums = !isPart(state, Part::GUITAR);
    bool is2xKick = isDrums && gemColumn == 6;
    animationManager.triggerHit(gemColumn, isDrums, is2xKick);
}

//==============================================================================
// Animation Detection

void AnimationRenderer::detectAndTriggerAnimations(const TimeBasedTrackWindow& trackWindow)
{
    // Strikeline is at time 0 (current playback position)
    // For each column, find the closest note that has passed the strikeline
    // If it's a new note (different from last frame), trigger the animation

    std::array<double, 7> closestPastNotePerColumn = {999.0, 999.0, 999.0, 999.0, 999.0, 999.0, 999.0};

    // Find the closest note that has just crossed (or is at) the strikeline for each column
    for (const auto &frameItem : trackWindow)
    {
        double frameTime = frameItem.first;  // Time in seconds from cursor
        const auto& gems = frameItem.second;

        // We only care about notes that have crossed or are at the strikeline (frameTime <= 0)
        // And are close enough to be considered "just hit" (within a small past window)
        if (frameTime <= 0.0 && frameTime >= -0.05)  // 50ms past window
        {
            for (uint gemColumn = 0; gemColumn < gems.size(); ++gemColumn)
            {
                if (gems[gemColumn].gem != Gem::NONE)
                {
                    // This note is past the strikeline - check if it's the closest one
                    if (std::abs(frameTime) < std::abs(closestPastNotePerColumn[gemColumn]))
                    {
                        closestPastNotePerColumn[gemColumn] = frameTime;
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
            triggerAnimationForColumn(gemColumn);
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
                triggerAnimationForColumn(sustain.gemColumn);
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

void AnimationRenderer::renderToDrawCallMap(DrawCallMap& drawCallMap, uint width, uint height)
{
    const auto& animations = animationManager.getActiveAnimations();
    bool isGuitar = isPart(state, Part::GUITAR);

    for (const auto& anim : animations)
    {
        if (!anim.isActive()) continue;

        if (anim.isBar)
        {
            // Add kick animation to BAR_ANIMATION layer
            uint column = anim.is2xKick ? 6 : 0;
            CoordinateOffset offset = isGuitar
                ? GUITAR_ANIMATION_OFFSETS[0]
                : DRUM_ANIMATION_OFFSETS[0];

            drawCallMap[DrawOrder::BAR_ANIMATION][column].push_back([this, anim, width, height, offset](juce::Graphics &g) {
                this->renderKickAnimation(g, anim, width, height, offset);
            });
        }
        else
        {
            // Add fret animation to NOTE_ANIMATION layer
            CoordinateOffset offset = isGuitar
                ? GUITAR_ANIMATION_OFFSETS[anim.lane]
                : DRUM_ANIMATION_OFFSETS[anim.lane];

            drawCallMap[DrawOrder::NOTE_ANIMATION][anim.lane].push_back([this, anim, width, height, offset](juce::Graphics &g) {
                this->renderFretAnimation(g, anim, width, height, offset);
            });
        }
    }
}

void AnimationRenderer::renderKickAnimation(juce::Graphics &g, const AnimationConstants::HitAnimation& anim, uint width, uint height, const PositionConstants::CoordinateOffset& offset)
{
    // Strikeline is where notes are when frameTime = 0 (at the cursor position)
    float strikelinePosition = 0.0f;
    bool isGuitar = isPart(state, Part::GUITAR);

    // Draw bar animation at bar position (gemColumn 0 for open/kick, or 6 for 2x kick)
    // For guitar open notes, use the open animation frames; otherwise use kick frames
    juce::Image* animFrame = nullptr;
    if (isGuitar && anim.isOpen) {
        animFrame = assetManager.getOpenAnimationFrame(anim.currentFrame);
    } else {
        animFrame = assetManager.getKickAnimationFrame(anim.currentFrame);
    }

    if (animFrame)
    {
        juce::Rectangle<float> kickRect;
        if (isGuitar) {
            kickRect = glyphRenderer.getGuitarGlyphRect(0, strikelinePosition, width, height);
        } else {
            kickRect = glyphRenderer.getDrumGlyphRect(anim.is2xKick ? 6 : 0, strikelinePosition, width, height);
        }

        // Apply animation-specific positioning and scaling
        kickRect = kickRect.withSizeKeepingCentre(
            kickRect.getWidth() * offset.widthScale,
            kickRect.getHeight() * offset.heightScale
        ).translated(offset.xOffset, offset.yOffset);

        g.setOpacity(1.0f);
        g.drawImage(*animFrame, kickRect);
    }
}

void AnimationRenderer::renderFretAnimation(juce::Graphics &g, const AnimationConstants::HitAnimation& anim, uint width, uint height, const PositionConstants::CoordinateOffset& offset)
{
    // Strikeline is where notes are when frameTime = 0 (at the cursor position)
    float strikelinePosition = 0.0f;
    bool isGuitar = isPart(state, Part::GUITAR);
    Part currentPart = isGuitar ? Part::GUITAR : Part::DRUMS;

    // Draw fret hit animation (flash + flare)
    auto hitFrame = assetManager.getHitAnimationFrame(anim.currentFrame);
    auto flareImage = assetManager.getHitFlareImage(anim.lane, currentPart);

    juce::Rectangle<float> hitRect;
    if (isGuitar) {
        hitRect = glyphRenderer.getGuitarGlyphRect(anim.lane, strikelinePosition, width, height);
    } else {
        hitRect = glyphRenderer.getDrumGlyphRect(anim.lane, strikelinePosition, width, height);
    }

    // Apply fret hit animation positioning and scaling
    hitRect = hitRect.withSizeKeepingCentre(
        hitRect.getWidth() * offset.widthScale,
        hitRect.getHeight() * offset.heightScale
    ).translated(offset.xOffset, offset.yOffset);

    // Draw the flash frame
    if (hitFrame)
    {
        g.setOpacity(HIT_FLASH_OPACITY);
        g.drawImage(*hitFrame, hitRect);
    }

    // Draw the colored flare on top (with tint for the lane color)
    if (flareImage && anim.currentFrame <= HIT_FLARE_MAX_FRAME)
    {
        g.setOpacity(HIT_FLARE_OPACITY);
        g.drawImage(*flareImage, hitRect);
    }
}

//==============================================================================
// Frame Management

void AnimationRenderer::advanceFrames()
{
    animationManager.advanceAllFrames();
}

void AnimationRenderer::reset()
{
    animationManager.reset();
    // Reset tracking of note times
    for (auto& noteTime : lastNoteTimePerColumn) {
        noteTime = -999.0;
    }
}
