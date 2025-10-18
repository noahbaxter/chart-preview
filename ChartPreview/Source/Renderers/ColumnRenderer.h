/*
    ==============================================================================

        ColumnRenderer.h
        Created by Claude Code (refactoring column/sustain rendering logic)
        Author: Noah Baxter

        This file contains all column and sustain path generation logic.
        Handles rendering of vertical elements like sustains, BREs, sections, etc.
        Uses PositionConstants for lane coordinate calculations.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PositionConstants.h"

// Windows compatibility
#if defined(_WIN32) || defined(_WIN64) || defined(__WINDOWS__) || defined(_MSC_VER)
    typedef unsigned int uint;
#endif

class ColumnRenderer
{
public:
    using LaneCorners = PositionConstants::LaneCorners;

    ColumnRenderer() = default;
    ~ColumnRenderer() = default;

    //==============================================================================
    // Trapezoid and cap path generation for column rendering
    juce::Path createTrapezoidPath(LaneCorners start, LaneCorners end, float startWidth, float endWidth);
    juce::Path createRoundedCapPath(LaneCorners coords, float width, float radius, float heightScale = 1.0f);

    //==============================================================================
    // Offscreen rendering for column compositing
    juce::Image createOffscreenColumnImage(
        const juce::Path& trapezoid,
        const juce::Path& startCap,
        const juce::Path& endCap,
        juce::Colour colour
    );
};
