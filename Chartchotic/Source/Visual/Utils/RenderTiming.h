/*
  ==============================================================================

    RenderTiming.h
    Created: 4 Mar 2026
    Author:  Noah Baxter

    Phase timing for SceneRenderer. Header-only.

  ==============================================================================
*/

#pragma once

#include <algorithm>
#include <chrono>
#include "../../Utils/ChartTypes.h"
#include "DrawingConstants.h"

struct PhaseTiming
{
    double notes_us = 0.0;
    double sustains_us = 0.0;
    double gridlines_us = 0.0;
    double animation_us = 0.0;
    double execute_us = 0.0;
    double total_us = 0.0;
    double layer_us[DRAW_ORDER_COUNT] = {};
    // Draw calls executed this frame. execute_us on its own can't tell "too many sprites"
    // apart from "each sprite is slow", which is the first question when a part renders
    // slower than another.
    int drawCalls = 0;
};

class ScopedPhaseMeasure
{
public:
    ScopedPhaseMeasure(double& target, bool enabled)
        : target(target), enabled(enabled)
    {
        if (enabled)
            start = std::chrono::high_resolution_clock::now();
    }

    ~ScopedPhaseMeasure()
    {
        if (enabled)
        {
            auto end = std::chrono::high_resolution_clock::now();
            target = std::chrono::duration<double, std::micro>(end - start).count();
        }
    }

private:
    double& target;
    bool enabled;
    std::chrono::high_resolution_clock::time_point start;
};
