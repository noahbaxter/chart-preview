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

// Write-mode gridline overlay config — drives optional STEP lines that mark
// the active snap division while authoring. When `active` is false the
// generator behaves exactly as before (MEASURE / BEAT / HALF_BEAT only).
struct WriteGridConfig
{
    bool active = false;       // writeModeActive AND snapEnabled
    int  stepDivision = 8;     // 1..64, e.g. 8 -> 1/8 notes
    int  tuplet = 0;           // 0 = off, else 3/5/7
};

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
    //   - writeGridConfig: Optional STEP overlay config (off by default)
    template<typename PPQToTimeFunc>
    static TimeBasedGridlineMap generateGridlines(
        const TempoTimeSignatureMap& tempoTimeSigMap,
        PPQ startPPQ,
        PPQ endPPQ,
        PPQ cursorPPQ,
        PPQToTimeFunc ppqToTime,
        const WriteGridConfig& writeGridConfig = WriteGridConfig{})
    {
        TimeBasedGridlineMap result;
        double cursorTime = ppqToTime(cursorPPQ.toDouble());

        // If no tempo/timesig events, use default 120 BPM, 4/4
        if (tempoTimeSigMap.empty())
        {
            generateGridlinesForSection(result, startPPQ, endPPQ, cursorPPQ, cursorTime,
                                       PPQ(0.0), 120.0, 4, 4, ppqToTime, writeGridConfig);
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
                                       ppqToTime, writeGridConfig);

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
        PPQ /*cursorPPQ*/,
        double cursorTime,
        PPQ tempoChangePos,
        double bpm,
        int timeSigNum,
        int timeSigDenom,
        PPQToTimeFunc ppqToTime,
        const WriteGridConfig& writeGridConfig)
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

        // STEP overlay: only when write mode + snap are active
        if (!writeGridConfig.active)
            return;
        if (writeGridConfig.stepDivision <= 0)
            return;

        // Spacing formula (in QN):
        //   No tuplet (T == 0): spacing = 4.0 / stepDivision
        //   Tuplet T > 0:        spacing = 8.0 / (stepDivision * T)
        double stepSpacing = (writeGridConfig.tuplet > 0)
            ? (8.0 / (static_cast<double>(writeGridConfig.stepDivision) * writeGridConfig.tuplet))
            : (4.0 / static_cast<double>(writeGridConfig.stepDivision));

        if (stepSpacing <= 0.0)
            return;

        // Snap to first step boundary at or after sectionStart, anchored to the
        // same measure anchor as the higher-priority lines.
        double stepCurrent = sectionStart.toDouble();
        double stepRel = stepCurrent - measureAnchor;
        if (stepRel < 0.0) stepRel = 0.0;
        double nextStep = std::ceil(stepRel / stepSpacing) * stepSpacing;
        stepCurrent = measureAnchor + nextStep;

        // Dedup epsilon: if a step lands within ~1 PPQ of a higher-priority
        // line (measure / beat / half-beat) we skip emitting it. Higher-priority
        // lines are spaced at halfBeatSpacing within this section.
        // PPQ here uses quarter-note units (1.0 == one QN), so 1 MIDI tick at
        // 480 PPQN ~= 1/480 QN. We use a slightly looser epsilon to absorb
        // floating-point error from tuplet divisions.
        const double dedupEpsilon = 1.0 / 480.0;

        int stepIterCount = 0;
        while (stepCurrent < sectionEnd.toDouble() && stepIterCount < maxIterations)
        {
            double stepRelPos = stepCurrent - measureAnchor;

            // Distance to nearest half-beat (covers MEASURE / BEAT / HALF_BEAT
            // — they're all on the half-beat lattice).
            double hbMod = std::fmod(stepRelPos, halfBeatSpacing);
            if (hbMod < 0.0) hbMod += halfBeatSpacing;
            double distToHalfBeat = std::min(hbMod, halfBeatSpacing - hbMod);

            if (distToHalfBeat > dedupEpsilon)
            {
                double time = ppqToTime(stepCurrent) - cursorTime;
                result.push_back({time, Gridline::STEP});
            }

            stepCurrent += stepSpacing;
            stepIterCount++;
        }
    }
};
