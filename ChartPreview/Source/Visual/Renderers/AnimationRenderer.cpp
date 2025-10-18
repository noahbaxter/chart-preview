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

            if (isPart(state, Part::GUITAR))
            {
                if (gemColumn == 0) {
                    // Open note (kick for guitar)
                    animationManager.triggerKick(true, false);
                } else if (gemColumn >= 1 && gemColumn <= 5) {
                    // Regular fret (1=green, 2=red, 3=yellow, 4=blue, 5=orange)
                    animationManager.triggerHit(gemColumn, false);
                }
            }
            else // Part::DRUMS
            {
                if (gemColumn == 0) {
                    // Regular kick
                    animationManager.triggerKick(false, false);
                } else if (gemColumn == 6) {
                    // 2x kick
                    animationManager.triggerKick(false, true);
                } else if (gemColumn >= 1 && gemColumn <= 4) {
                    // Drum pads
                    animationManager.triggerHit(gemColumn, false);
                }
            }
        }
    }
}

//==============================================================================
// Sustain State Management

void AnimationRenderer::updateSustainStates(const TimeBasedSustainWindow& sustainWindow)
{
    // Strikeline is at time 0 (current playback position)
    // Check if each lane is currently in a sustain (sustain crosses the strikeline)
    std::array<bool, 6> lanesSustaining = {false, false, false, false, false, false};

    for (const auto& sustain : sustainWindow)
    {
        // Sustain is active at the strikeline if startTime <= 0 <= endTime
        if (sustain.startTime <= 0.0 && sustain.endTime >= 0.0)
        {
            // Only track sustains (not lanes)
            if (sustain.sustainType == SustainType::SUSTAIN && sustain.gemColumn < lanesSustaining.size())
            {
                lanesSustaining[sustain.gemColumn] = true;
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

void AnimationRenderer::render(juce::Graphics &g, uint width, uint height)
{
    const auto& animations = animationManager.getActiveAnimations();

    for (const auto& anim : animations)
    {
        if (!anim.isActive()) continue;

        if (anim.isBar)
        {
            renderKickAnimation(g, anim, width, height);
        }
        else
        {
            renderFretAnimation(g, anim, width, height);
        }
    }
}

void AnimationRenderer::renderKickAnimation(juce::Graphics &g, const AnimationConstants::HitAnimation& anim, uint width, uint height)
{
    // Strikeline is where notes are when frameTime = 0 (at the cursor position)
    float strikelinePosition = 0.0f;

    // Draw bar animation at bar position (gemColumn 0 for open/kick, or 6 for 2x kick)
    // For guitar open notes, use the open animation frames; otherwise use kick frames
    juce::Image* animFrame = nullptr;
    if (isPart(state, Part::GUITAR) && anim.isOpen) {
        animFrame = assetManager.getOpenAnimationFrame(anim.currentFrame);
    } else {
        animFrame = assetManager.getKickAnimationFrame(anim.currentFrame);
    }

    if (animFrame)
    {
        juce::Rectangle<float> kickRect;
        if (isPart(state, Part::GUITAR)) {
            kickRect = glyphRenderer.getGuitarGlyphRect(0, strikelinePosition, width, height);
        } else {
            kickRect = glyphRenderer.getDrumGlyphRect(anim.is2xKick ? 6 : 0, strikelinePosition, width, height);
        }

        // Scale up the animation (wider and MUCH taller to match the bar note height)
        kickRect = kickRect.withSizeKeepingCentre(kickRect.getWidth() * KICK_ANIMATION_WIDTH_SCALE, kickRect.getHeight() * KICK_ANIMATION_HEIGHT_SCALE);

        g.setOpacity(1.0f);
        g.drawImage(*animFrame, kickRect);
    }
}

void AnimationRenderer::renderFretAnimation(juce::Graphics &g, const AnimationConstants::HitAnimation& anim, uint width, uint height)
{
    // Strikeline is where notes are when frameTime = 0 (at the cursor position)
    float strikelinePosition = 0.0f;

    // Draw fret hit animation (flash + flare)
    auto hitFrame = assetManager.getHitAnimationFrame(anim.currentFrame);
    Part currentPart = isPart(state, Part::GUITAR) ? Part::GUITAR : Part::DRUMS;
    auto flareImage = assetManager.getHitFlareImage(anim.lane, currentPart);

    juce::Rectangle<float> hitRect;
    if (currentPart == Part::GUITAR) {
        hitRect = glyphRenderer.getGuitarGlyphRect(anim.lane, strikelinePosition, width, height);
    } else {
        hitRect = glyphRenderer.getDrumGlyphRect(anim.lane, strikelinePosition, width, height);
    }

    // Scale up the animation (wider and much taller)
    hitRect = hitRect.withSizeKeepingCentre(hitRect.getWidth() * HIT_ANIMATION_WIDTH_SCALE, hitRect.getHeight() * HIT_ANIMATION_HEIGHT_SCALE);

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
