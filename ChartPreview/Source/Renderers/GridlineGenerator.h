/*
  ==============================================================================

    GridlineGenerator.h
    Created: 15 Jan 2025
    Author:  Noah Baxter

    Generates gridlines on-demand from tempo/time signature events.
    Takes a TempoTimeSignatureMap and generates gridlines for the visible window.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Utils/Utils.h"
#include "../Utils/PPQ.h"
#include "../Utils/TimeConverter.h"

class GridlineGenerator
{
public:
    GridlineGenerator() = default;

    // Generate time-based gridlines from tempo/timesig events for rendering
    // Parameters:
    //   - tempoTimeSigMap: The tempo/time signature events
    //   - startPPQ: Start of the visible window
    //   - endPPQ: End of the visible window
    //   - cursorPPQ: Current playback position (time zero)
    //   - ppqToTime: Function to convert PPQ to absolute time
    template<typename PPQToTimeFunc>
    static TimeBasedGridlineMap generateGridlines(
        const TempoTimeSignatureMap& tempoTimeSigMap,
        PPQ startPPQ,
        PPQ endPPQ,
        PPQ cursorPPQ,
        PPQToTimeFunc ppqToTime)
    {
        TimeBasedGridlineMap result;
        double cursorTime = ppqToTime(cursorPPQ.toDouble());

        // If no tempo/timesig events, use default 120 BPM, 4/4
        if (tempoTimeSigMap.empty())
        {
            generateGridlinesForSection(result, startPPQ, endPPQ, cursorPPQ, cursorTime,
                                       PPQ(0.0), 120.0, 4, 4, ppqToTime);
            return result;
        }

        // Find the tempo/timesig event at or before startPPQ
        auto it = tempoTimeSigMap.upper_bound(startPPQ);
        if (it != tempoTimeSigMap.begin()) --it;

        // Generate gridlines for each tempo/timesig section
        while (it != tempoTimeSigMap.end())
        {
            const auto& event = it->second;
            PPQ sectionStart = std::max(startPPQ, event.ppqPosition);

            // Find the next tempo/timesig change
            auto nextIt = std::next(it);
            PPQ sectionEnd = (nextIt != tempoTimeSigMap.end()) ? nextIt->first : endPPQ;
            sectionEnd = std::min(sectionEnd, endPPQ);

            // Generate gridlines for this section
            generateGridlinesForSection(result, sectionStart, sectionEnd, cursorPPQ, cursorTime,
                                       event.ppqPosition, event.bpm,
                                       event.timeSigNumerator, event.timeSigDenominator,
                                       ppqToTime);

            // Move to next section
            ++it;
            if (sectionEnd >= endPPQ) break;
        }

        return result;
    }

private:
    // Generate gridlines for a single tempo/timesig section
    template<typename PPQToTimeFunc>
    static void generateGridlinesForSection(
        TimeBasedGridlineMap& result,
        PPQ sectionStart,
        PPQ sectionEnd,
        PPQ cursorPPQ,
        double cursorTime,
        PPQ tempoChangePos,
        double bpm,
        int timeSigNum,
        int timeSigDenom,
        PPQToTimeFunc ppqToTime)
    {
        // Safety check: invalid time signature or empty section
        if (timeSigDenom <= 0 || timeSigNum <= 0 || sectionStart >= sectionEnd)
            return;

        // Calculate spacing in PPQ
        double measureLength = static_cast<double>(timeSigNum) * (4.0 / timeSigDenom);
        double beatSpacing = 4.0 / timeSigDenom;
        double halfBeatSpacing = beatSpacing / 2.0;

        // Safety check: prevent infinite loops
        if (measureLength <= 0.0 || beatSpacing <= 0.0 || halfBeatSpacing <= 0.0)
            return;

        // Find the first measure boundary at or after tempoChangePos
        double measureAnchor = tempoChangePos.toDouble();

        // Find first half-beat position at or after sectionStart
        double currentPPQ = sectionStart.toDouble();
        double relativeToAnchor = currentPPQ - measureAnchor;

        // Snap to next half-beat boundary at or after sectionStart
        if (relativeToAnchor < 0.0)
            relativeToAnchor = 0.0;

        double nextHalfBeat = std::ceil(relativeToAnchor / halfBeatSpacing) * halfBeatSpacing;
        currentPPQ = measureAnchor + nextHalfBeat;

        // Generate gridlines from first half-beat to section end
        int iterationCount = 0;
        const int maxIterations = 100000; // Safety limit

        while (currentPPQ < sectionEnd.toDouble() && iterationCount < maxIterations)
        {
            double relativePos = currentPPQ - measureAnchor;

            // Determine gridline type based on position relative to measure anchor
            // Use modulo to check if it's on a measure, beat, or half-beat boundary
            double measureMod = std::fmod(relativePos, measureLength);
            double beatMod = std::fmod(relativePos, beatSpacing);

            Gridline lineType;
            if (std::abs(measureMod) < 0.001 || std::abs(measureMod - measureLength) < 0.001)
            {
                lineType = Gridline::MEASURE;
            }
            else if (std::abs(beatMod) < 0.001 || std::abs(beatMod - beatSpacing) < 0.001)
            {
                lineType = Gridline::BEAT;
            }
            else
            {
                lineType = Gridline::HALF_BEAT;
            }

            // Add the gridline
            double time = ppqToTime(currentPPQ) - cursorTime;
            result.push_back({time, lineType});

            // Move to next half-beat
            currentPPQ += halfBeatSpacing;
            iterationCount++;
        }
    }
};
