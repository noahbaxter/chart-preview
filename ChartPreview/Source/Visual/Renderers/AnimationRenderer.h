/*
    ==============================================================================

        AnimationRenderer.h
        Created by Claude Code (refactoring animation logic)
        Author: Noah Baxter

        Encapsulates animation detection, state management, and rendering.
        Combines detection (note crossing), state updates (sustains), and rendering.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include "../../Midi/Processing/MidiInterpreter.h"
#include "../../Utils/Utils.h"
#include "../../Utils/TimeConverter.h"
#include "../Managers/AnimationManager.h"
#include "GlyphRenderer.h"
#include "../Managers/AssetManager.h"
#include "../Utils/DrawingConstants.h"

class AnimationRenderer
{
public:
    AnimationRenderer(juce::ValueTree &state, MidiInterpreter &midiInterpreter);
    ~AnimationRenderer();

    /**
     * Detect notes that have crossed the strikeline and trigger animations.
     * Should be called once per frame when playback is active.
     */
    void detectAndTriggerAnimations(const TimeBasedTrackWindow& trackWindow);

    /**
     * Update sustain states based on current sustain window.
     * Animations hold at frame 1 during sustains.
     * If isPlaying and sustain is active but no animation exists, force-trigger it.
     */
    void updateSustainStates(const TimeBasedSustainWindow& sustainWindow, bool isPlaying);

    /**
     * Populate drawCallMap with animation render calls.
     * Animations are added to BAR_ANIMATION and NOTE_ANIMATION layers for proper Z-ordering.
     * Call before rendering the drawCallMap.
     */
    void renderToDrawCallMap(DrawCallMap& drawCallMap, uint width, uint height);

    /**
     * Advance all active animations by one frame.
     * Call this once per render frame after rendering.
     */
    void advanceFrames();

    /**
     * Clear all animations (e.g., when transport stops).
     */
    void reset();

private:
    juce::ValueTree &state;
    MidiInterpreter &midiInterpreter;
    AnimationManager animationManager;
    GlyphRenderer glyphRenderer;
    AssetManager assetManager;

    // Track the last note time per column to ensure every note triggers an animation
    std::array<double, 7> lastNoteTimePerColumn = {-999.0, -999.0, -999.0, -999.0, -999.0, -999.0, -999.0};

    // Helper: Trigger animation for a specific gem column
    void triggerAnimationForColumn(uint gemColumn);

    // Helper: Determine if a gem column is a bar note (kick/open)
    bool isBarNote(uint gemColumn, Part part)
    {
        if (part == Part::GUITAR)
        {
            return gemColumn == 0;
        }
        else // if (part == Part::DRUMS)
        {
            return gemColumn == 0 || gemColumn == 6;
        }
    }

    // Rendering helpers
    void renderKickAnimation(juce::Graphics &g, const AnimationConstants::HitAnimation& anim, uint width, uint height, const PositionConstants::CoordinateOffset& offset);
    void renderFretAnimation(juce::Graphics &g, const AnimationConstants::HitAnimation& anim, uint width, uint height, const PositionConstants::CoordinateOffset& offset);
};
