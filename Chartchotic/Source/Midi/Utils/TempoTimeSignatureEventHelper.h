/*
  ==============================================================================

    TempoTimeSignatureEventHelper.h
    Created: 16 Oct 2025
    Author:  Noah Baxter

    Utility functions for working with TempoTimeSignatureEvent data.
    Provides helpers for event classification, comparison, and queries.

  ==============================================================================
*/

#pragma once

#include "../../Utils/ChartTypes.h"
#include "../../Visual/Utils/DrawingConstants.h"
#include <map>
#include <cmath>
#include <functional>

class TempoTimeSignatureEventHelper
{
public:
    // Check if an event represents a time signature change (has valid timesig)
    static bool isTimeSignatureEvent(const TempoTimeSignatureEvent& event)
    {
        return event.timeSigNumerator > 0 && event.timeSigDenominator > 0;
    }

    // Check if an event represents a tempo change (has valid BPM)
    static bool isTempoEvent(const TempoTimeSignatureEvent& event)
    {
        return event.bpm > 0.0;
    }

    // Check if this event differs in tempo from the previous one
    static bool hasTempoChange(const TempoTimeSignatureEvent& current,
                              const TempoTimeSignatureEvent& previous)
    {
        return std::abs(current.bpm - previous.bpm) > 0.01; // Small epsilon for floating point comparison
    }

    // Check if this event differs in time signature from the previous one
    static bool hasTimeSignatureChange(const TempoTimeSignatureEvent& current,
                                       const TempoTimeSignatureEvent& previous)
    {
        return current.timeSigNumerator != previous.timeSigNumerator ||
               current.timeSigDenominator != previous.timeSigDenominator;
    }

    // Structure representing the tempo and time signature state at a given point
    struct TempoTimeSigState {
        TempoTimeSignatureEvent tempoEvent;   // Last tempo change before position
        TempoTimeSignatureEvent timeSigEvent; // Last timesig change before position
        bool hasValidTempo;                   // Whether tempoEvent contains valid data
        bool hasValidTimeSig;                 // Whether timeSigEvent contains valid data
        bool timeSigReset;                    // Whether the timesig was explicitly reset (not just carried forward)
    };

    /**
     * Find the closest tempo AND time signature state BEFORE a given PPQ position.
     *
     * Important: Tempo changes and time signature changes may occur at different times.
     * This function returns the most recent valid tempo and most recent valid timesig
     * separately, allowing you to know if they occurred at different events.
     *
     * @param map The tempo/timesig event map
     * @param ppq The position to search before
     * @return TempoTimeSigState with the last valid tempo and timesig before ppq
     */
    static TempoTimeSigState getStateBeforePPQ(const TempoTimeSignatureMap& map, PPQ ppq)
    {
        TempoTimeSigState result{
            {PPQ(0.0), 120.0, 4, 4},  // Default tempoEvent
            {PPQ(0.0), 120.0, 4, 4},  // Default timeSigEvent
            false,
            false,
            false  // timeSigReset
        };

        if (map.empty())
            return result;

        // Find the first event at or after ppq, then go back one
        auto it = map.upper_bound(ppq);
        if (it != map.begin())
            --it;
        else
            return result; // Nothing at or before ppq

        // Search backwards for last valid tempo and timesig changes
        // We need to search back because events may only have tempo OR timesig set
        while (true)
        {
            if (!result.hasValidTempo && isTempoEvent(it->second))
            {
                result.tempoEvent = it->second;
                result.hasValidTempo = true;
            }
            if (!result.hasValidTimeSig && isTimeSignatureEvent(it->second))
            {
                result.timeSigEvent = it->second;
                result.hasValidTimeSig = true;
                result.timeSigReset = it->second.timeSigReset;  // Capture reset flag
            }

            // If we've found both, we're done
            if (result.hasValidTempo && result.hasValidTimeSig)
                break;

            // Move to previous event
            if (it == map.begin())
                break;

            --it;
        }

        return result;
    }

    /**
     * Get the effective tempo and time signature at a specific PPQ position.
     * This is a convenience wrapper that returns the most recent valid values.
     *
     * @param map The tempo/timesig event map
     * @param ppq The position to query
     * @return A TempoTimeSignatureEvent with the effective tempo and timesig at this position
     */
    static TempoTimeSignatureEvent getEffectiveStateAtPPQ(const TempoTimeSignatureMap& map, PPQ ppq)
    {
        auto state = getStateBeforePPQ(map, ppq);

        TempoTimeSignatureEvent result;
        result.ppqPosition = ppq;
        result.bpm = state.hasValidTempo ? state.tempoEvent.bpm : 120.0;
        result.timeSigNumerator = state.hasValidTimeSig ? state.timeSigEvent.timeSigNumerator : 4;
        result.timeSigDenominator = state.hasValidTimeSig ? state.timeSigEvent.timeSigDenominator : 4;

        return result;
    }

    /**
     * Find the last event where timeSigReset is true, searching backwards from a given position.
     * Used for determining the measure anchor point for gridline generation.
     *
     * @param map The tempo/timesig event map
     * @param searchFromPPQ Search backwards from this position (inclusive)
     * @return PPQ position of the last timeSigReset event, or 0.0 if none found
     */
    static PPQ getLastTimeSigResetPosition(const TempoTimeSignatureMap& map, PPQ searchFromPPQ)
    {
        if (map.empty())
            return PPQ(0.0);

        // Find the first event at or after searchFromPPQ, then go back one
        auto it = map.upper_bound(searchFromPPQ);
        if (it != map.begin())
            --it;
        else
            return PPQ(0.0);

        // Search backwards for the last timeSigReset event
        while (true)
        {
            if (it->second.timeSigReset)
            {
                return it->second.ppqPosition;
            }

            if (it == map.begin())
                break;

            --it;
        }

        // Check the first element
        if (map.begin()->second.timeSigReset)
        {
            return map.begin()->second.ppqPosition;
        }

        return PPQ(0.0);
    }

    // 0-indexed measure plus the 1-indexed beat within it (1.0 on the downbeat).
    struct MeasureBeat
    {
        int    measure       = 0;
        double beatInMeasure = 1.0;
    };

    static MeasureBeat pPqToMeasureBeat(PPQ targetPpq, const TempoTimeSignatureMap& map)
    {
        double t = targetPpq.toDouble();
        if (t <= 0.0 || map.empty()) return {};

        auto it = map.upper_bound(targetPpq);
        if (it != map.begin()) --it;

        const auto& anchor = it->second;
        int denom = anchor.timeSigDenominator > 0 ? anchor.timeSigDenominator : 4;
        int num = anchor.timeSigNumerator > 0 ? anchor.timeSigNumerator : 4;
        double beatSpacing  = 4.0 / (double)denom;
        double measureLenQN = (double)num * beatSpacing;
        if (measureLenQN <= 0.0) return { anchor.measurePos, 1.0 };

        double beatPosQN = anchor.beatPos * beatSpacing;
        double measureStartPpq = anchor.ppqPosition.toDouble() - beatPosQN;
        double rel = t - measureStartPpq;

        double measOff = std::fmod(rel, measureLenQN);
        if (measOff < 0.0) measOff += measureLenQN;

        return { anchor.measurePos + (int)std::floor(rel / measureLenQN + 1e-6),
                 measOff / beatSpacing + 1.0 };
    }

    /** Return the 0-indexed bar/measure number at a given PPQ. */
    static int pPqToMeasureNumber(PPQ targetPpq, const TempoTimeSignatureMap& map)
    {
        return pPqToMeasureBeat(targetPpq, map).measure;
    }

    // One MIDI tick at 480 PPQN, in beats. A fraction closer than this to a
    // shorter decimal is that decimal, not a position of its own.
    static constexpr double beatTick = 1.0 / 480.0;

    /** "M", "M.B" or "M.B.frac". Shared by the gridline column and ghost cursor. */
    static juce::String formatMeasureBeat(int measureOneIndexed, double beatInMeasure)
    {
        int beatInt = (int)std::floor(beatInMeasure + beatTick);
        double frac = beatInMeasure - (double)beatInt;

        // Print at the fewest decimals that still land within a tick of the
        // real value, so an inexact 0.2505 reads ".25" and a triplet's 0.3333
        // keeps all three places.
        for (int dp = 0; dp <= 3; ++dp)
        {
            double scale = std::pow(10.0, dp);
            double rounded = std::round(frac * scale) / scale;
            if (std::abs(frac - rounded) >= beatTick) continue;

            if (rounded >= 1.0) { ++beatInt; rounded = 0.0; }
            juce::String out = juce::String(measureOneIndexed) + "." + juce::String(beatInt);
            if (rounded <= 0.0) return out;

            juce::String fracStr = juce::String(rounded, dp).substring(1);
            while (fracStr.endsWith("0")) fracStr = fracStr.dropLastCharacters(1);
            return out + fracStr;
        }

        return juce::String(measureOneIndexed) + "." + juce::String(beatInt);
    }

    /**
     * Build event markers for tempo and time signature changes.
     * Skips the first event (initial state) and emits a marker for each
     * subsequent change in BPM or time signature.
     *
     * @param map The tempo/timesig event map
     * @param cursorPPQ Current cursor position (for relative time conversion)
     * @param ppqToTime Function converting PPQ to absolute time in seconds
     * @return Vector of event markers with labels like "BPM 140.0", "3/4", or "BPM 140.0  3/4"
     */
    static TimeBasedEventMarkers buildTempoEventMarkers(
        const TempoTimeSignatureMap& map,
        PPQ cursorPPQ,
        const std::function<double(double)>& ppqToTime)
    {
        TimeBasedEventMarkers markers;
        double cursorTime = ppqToTime(cursorPPQ.toDouble());
        TempoTimeSignatureEvent prev;
        bool first = true;

        for (const auto& [ppq, event] : map)
        {
            if (first) { prev = event; first = false; continue; }

            bool tempoChanged = hasTempoChange(event, prev);
            bool timeSigChanged = hasTimeSignatureChange(event, prev);

            if (tempoChanged || timeSigChanged)
            {
                double markerTime = ppqToTime(ppq.toDouble()) - cursorTime;
                juce::String label;
                if (tempoChanged)
                    label = "BPM " + juce::String(event.bpm, 1);
                if (timeSigChanged)
                {
                    if (label.isNotEmpty()) label += "  ";
                    label += juce::String(event.timeSigNumerator) + "/"
                           + juce::String(event.timeSigDenominator);
                }
                markers.push_back({markerTime, label});
            }
            prev = event;
        }
        return markers;
    }
};
