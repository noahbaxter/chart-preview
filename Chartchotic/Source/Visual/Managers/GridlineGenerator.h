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
#include "../../Utils/ChartTypes.h"
#include "../../Midi/Utils/PPQ.h"
#include "../../Midi/Utils/TimeConverter.h"
#include "../../Midi/Utils/TempoTimeSignatureEventHelper.h"

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
    // stepDivision: 0 = no step grid, else 1/N note (4=quarter, 8=eighth, 16=sixteenth, etc.)
    // tuplet: 0 = normal, 3 = triplet (2-in-space-of-3), 5 = quintuplet, 7 = septuplet, etc.
    //         Divides each step by tuplet and multiplies by (tuplet-1) to get the tuplet feel.
    template<typename PPQToTimeFunc>
    static TimeBasedGridlineMap generateGridlines(
        const TempoTimeSignatureMap& tempoTimeSigMap,
        PPQ startPPQ,
        PPQ endPPQ,
        PPQ cursorPPQ,
        PPQToTimeFunc ppqToTime,
        int stepDivision = 0,
        int tuplet = 0)
    {
        TimeBasedGridlineMap result;
        double cursorTime = ppqToTime(cursorPPQ.toDouble());

        // If no tempo/timesig events, use default 120 BPM, 4/4
        if (tempoTimeSigMap.empty())
        {
            generateGridlinesForSection(result, startPPQ, endPPQ, cursorPPQ, cursorTime,
                                       PPQ(0.0), 120.0, 4, 4, ppqToTime,
                                       stepDivision, tuplet);
            return result;
        }

        // Find the tempo/timesig event at or before startPPQ
        auto it = tempoTimeSigMap.upper_bound(startPPQ);
        if (it != tempoTimeSigMap.begin()) --it;

        // Find the last time signature reset before startPPQ (for measure anchor)
        PPQ measureAnchor = TempoTimeSignatureEventHelper::getLastTimeSigResetPosition(tempoTimeSigMap, startPPQ);

        // Generate gridlines for each tempo/timesig section
        while (it != tempoTimeSigMap.end())
        {
            const auto& event = it->second;
            PPQ sectionStart = std::max(startPPQ, event.ppqPosition);

            // Find the next tempo/timesig change
            auto nextIt = std::next(it);
            PPQ sectionEnd = (nextIt != tempoTimeSigMap.end()) ? nextIt->first : endPPQ;
            sectionEnd = std::min(sectionEnd, endPPQ);

            // Determine measure anchor for this section
            // Reset if this event explicitly changed the time signature, otherwise carry forward
            PPQ sectionMeasureAnchor = measureAnchor;
            if (event.timeSigReset)
            {
                sectionMeasureAnchor = event.ppqPosition;
                measureAnchor = event.ppqPosition;
            }

            // Generate gridlines for this section
            generateGridlinesForSection(result, sectionStart, sectionEnd, cursorPPQ, cursorTime,
                                       sectionMeasureAnchor, event.bpm,
                                       event.timeSigNumerator, event.timeSigDenominator,
                                       ppqToTime, stepDivision, tuplet);

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
        PPQToTimeFunc ppqToTime,
        int stepDivision = 0,
        int tuplet = 0)
    {
        // Safety check: invalid time signature or empty section
        if (timeSigDenom <= 0 || timeSigNum <= 0 || sectionStart >= sectionEnd)
            return;

        // Calculate spacing in PPQ
        double measureLength = static_cast<double>(timeSigNum) * (4.0 / timeSigDenom);
        double beatSpacing = 4.0 / timeSigDenom;

        double halfBeatSpacing = beatSpacing / 2.0;
        double stepSpacing = 0.0;
        if (stepDivision > 0)
        {
            stepSpacing = 4.0 / stepDivision;
            // Tuplet: N notes in the space of (N-1), e.g. 3 = triplet (3 in space of 2)
            if (tuplet >= 3) stepSpacing *= (static_cast<double>(tuplet - 1) / tuplet);
        }

        // When step grid is active, iterate at step spacing (even if coarser than half-beat).
        // Half-beat lines only appear when no step grid is set.
        double iterSpacing = (stepSpacing > 0.0) ? stepSpacing : halfBeatSpacing;

        // Safety check: prevent infinite loops
        if (measureLength <= 0.0 || beatSpacing <= 0.0 || iterSpacing <= 0.0)
            return;

        // Find the first measure boundary at or after tempoChangePos
        double measureAnchor = tempoChangePos.toDouble();

        // Find first grid position at or after sectionStart
        double currentPPQ = sectionStart.toDouble();
        double relativeToAnchor = currentPPQ - measureAnchor;

        // Snap to next grid boundary at or after sectionStart
        if (relativeToAnchor < 0.0)
            relativeToAnchor = 0.0;

        double nextGridPos = std::ceil(relativeToAnchor / iterSpacing) * iterSpacing;
        currentPPQ = measureAnchor + nextGridPos;

        // Generate gridlines from first position to section end
        int iterationCount = 0;
        const int maxIterations = 100000; // Safety limit

        while (currentPPQ < sectionEnd.toDouble() && iterationCount < maxIterations)
        {
            double relativePos = currentPPQ - measureAnchor;

            // Determine gridline type — measure and beat always take priority
            double measureMod = std::fmod(relativePos, measureLength);
            double beatMod = std::fmod(relativePos, beatSpacing);
            double halfBeatMod = std::fmod(relativePos, halfBeatSpacing);

            Gridline lineType;
            if (std::abs(measureMod) < 0.001 || std::abs(measureMod - measureLength) < 0.001)
            {
                lineType = Gridline::MEASURE;
            }
            else if (std::abs(beatMod) < 0.001 || std::abs(beatMod - beatSpacing) < 0.001)
            {
                lineType = Gridline::BEAT;
            }
            else if (std::abs(halfBeatMod) < 0.001 || std::abs(halfBeatMod - halfBeatSpacing) < 0.001)
            {
                lineType = Gridline::HALF_BEAT;
            }
            else
            {
                lineType = Gridline::STEP;
            }

            // Add the gridline
            double time = ppqToTime(currentPPQ) - cursorTime;
            result.push_back({time, lineType});

            // Move to next grid position
            currentPPQ += iterSpacing;
            iterationCount++;
        }
    }
};
