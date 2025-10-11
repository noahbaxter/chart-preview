#pragma once

#include <JuceHeader.h>
#include "../Utils/Utils.h"

/**
 * Manages hit animations for notes on the strikeline.
 *
 * Each lane can have an active animation (hit flash + flare).
 * Animations play for a fixed number of frames (5 for standard hits, 7 for kicks).
 * If a new note hits before animation completes, it resets to frame 1.
 */
class HitAnimationManager {
public:
    static constexpr int HIT_ANIMATION_FRAMES = 5;
    static constexpr int KICK_ANIMATION_FRAMES = 7;

    /**
     * Represents an active hit animation on a specific lane.
     */
    struct HitAnimation {
        int currentFrame = 0;  // 0 = no animation, 1-5 for hit, 1-7 for bar
        bool isBar = false;    // true for bar note (kick/open), false for standard fret
        int lane = 0;          // 0-4 for guitar (green, red, yellow, blue, orange), 0 for bar
        bool isOpen = false;   // true for open notes (purple bar flash for guitar)
        bool is2xKick = false; // true for 2x kick (different color)
        bool inSustain = false; // true if currently in a sustain (holds at frame 1)

        void reset() {
            currentFrame = 0;
            isBar = false;
            lane = 0;
            isOpen = false;
            is2xKick = false;
            inSustain = false;
        }

        bool isActive() const {
            return currentFrame > 0;
        }

        void advanceFrame() {
            if (currentFrame == 0) return;

            if (inSustain) { // Hold at frame 1 during sustain
                currentFrame = 1;
                return;
            }

            int maxFrames = isBar ? KICK_ANIMATION_FRAMES : HIT_ANIMATION_FRAMES;
            currentFrame++;
            if (currentFrame > maxFrames) {
                reset();
            }
        }

        void trigger(bool bar, int laneIndex, bool open = false, bool twoXKick = false) {
            currentFrame = 1;  // Start at frame 1
            isBar = bar;
            lane = laneIndex;
            isOpen = open;
            is2xKick = twoXKick;
            inSustain = false;
        }

        void setSustainState(bool sustaining) {
            if (currentFrame > 0) {
                inSustain = sustaining;
                if (inSustain) {
                    currentFrame = 1;  // Reset to frame 1 when entering sustain
                }
            }
        }
    };

    HitAnimationManager() {
        // Initialize animation slots for guitar (5 lanes) + drums (kick + 4 pads)
        animations.resize(6);  // 0=kick, 1-5=guitar/drum lanes
    }

    /**
     * Trigger a hit animation for a guitar/drum lane.
     * Resets animation to frame 1 if already playing.
     */
    void triggerHit(int lane, bool isOpen = false) {
        if (lane >= 0 && lane < animations.size()) {
            animations[lane].trigger(false, lane, isOpen, false);
        }
    }

    /**
     * Trigger a kick hit animation.
     * Resets animation to frame 1 if already playing.
     */
    void triggerKick(bool isOpen = false, bool is2xKick = false) {
        animations[0].trigger(true, 0, isOpen, is2xKick);
    }

    /**
     * Advance all active animations by one frame.
     * Call this once per render frame.
     * Animations always advance to allow them to complete when paused.
     */
    void advanceAllFrames() {
        for (auto& anim : animations) {
            anim.advanceFrame();
        }
    }

    /**
     * Update sustain state for a specific lane.
     * When a lane is sustaining, its animation will hold at frame 1.
     */
    void setSustainState(int lane, bool sustaining) {
        if (lane >= 0 && lane < animations.size()) {
            animations[lane].setSustainState(sustaining);
        }
    }

    /**
     * Get all currently active animations for rendering.
     */
    const std::vector<HitAnimation>& getActiveAnimations() const {
        return animations;
    }

    /**
     * Clear all animations (e.g., when transport stops).
     */
    void reset() {
        for (auto& anim : animations) {
            anim.reset();
        }
    }

private:
    std::vector<HitAnimation> animations;
};
