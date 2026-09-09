#pragma once

#include <JuceHeader.h>
#include "../../Utils/ChartTypes.h"
#include "../Utils/DrawingConstants.h"

namespace AnimationConstants {
    // Animation frame counts
    static constexpr int HIT_ANIMATION_FRAMES = 5;
    static constexpr int KICK_ANIMATION_FRAMES = 7;

    struct HitAnimation {
        int currentFrame = 0;  // 0 = no animation, 1-5 for hit, 1-7 for bar
        double elapsedTime = 0.0;
        bool isBar = false;
        int lane = 0;
        bool isOpen = false;
        bool is2xKick = false;
        bool inSustain = false;
        Gem gemType = Gem::NOTE;
        bool starPower = false;

        void reset() {
            currentFrame = 0;
            elapsedTime = 0.0;
            isBar = false;
            lane = 0;
            isOpen = false;
            is2xKick = false;
            inSustain = false;
            gemType = Gem::NOTE;
            starPower = false;
        }

        bool isActive() const {
            return currentFrame > 0;
        }

        void advanceTime(double deltaSeconds) {
            if (currentFrame == 0) return;

            if (inSustain) {
                currentFrame = 1;
                return;
            }

            elapsedTime += deltaSeconds;

            int maxFrames = isBar ? KICK_ANIMATION_FRAMES : HIT_ANIMATION_FRAMES;
            double duration = isBar ? KICK_ANIMATION_DURATION : HIT_ANIMATION_DURATION;
            double frameTime = duration / maxFrames;

            int newFrame = 1 + (int)(elapsedTime / frameTime);
            if (newFrame > maxFrames) {
                reset();
            } else {
                currentFrame = newFrame;
            }
        }

        void trigger(bool bar, int laneIndex, bool open = false, bool twoXKick = false, Gem gem = Gem::NOTE, bool sp = false) {
            currentFrame = 1;
            elapsedTime = 0.0;
            isBar = bar;
            lane = laneIndex;
            isOpen = open;
            is2xKick = twoXKick;
            inSustain = false;
            gemType = gem;
            starPower = sp;
        }

        void setSustainState(bool sustaining) {
            if (currentFrame > 0) {
                inSustain = sustaining;
                if (inSustain) {
                    currentFrame = 1;
                    elapsedTime = 0.0;
                }
            }
        }
    };
}

class AnimationManager {
public:
    AnimationManager() {
        animations.resize(LANE_COUNT);   // was 6; elite uses columns up to 11
    }

    void triggerHit(int gemColumn, Part part, bool is2xKick = false, Gem gem = Gem::NOTE, bool sp = false) {
        // Part-aware: elite bars are kick/2x-kick/stomp/splash (not just col 0/6), so the bar check
        // and the guitar-open case both need the real part, not a drums/guitar flatten.
        bool isBar = isBarNote(gemColumn, part);
        int animSlot = isBar ? 0 : gemColumn;

        if (animSlot >= 0 && animSlot < (int)animations.size()) {
            bool isOpen = (gemColumn == 0 && isGuitarLike(part));
            animations[animSlot].trigger(isBar, animSlot, isOpen, is2xKick, gem, sp);
        }
    }

    void advanceAllFrames(double deltaSeconds) {
        for (auto& anim : animations) {
            anim.advanceTime(deltaSeconds);
        }
    }

    void setSustainState(int lane, bool sustaining) {
        if (lane >= 0 && lane < animations.size()) {
            animations[lane].setSustainState(sustaining);
        }
    }

    const std::vector<AnimationConstants::HitAnimation>& getActiveAnimations() const {
        return animations;
    }

    void reset() {
        for (auto& anim : animations) {
            anim.reset();
        }
    }

private:
    std::vector<AnimationConstants::HitAnimation> animations;
};
