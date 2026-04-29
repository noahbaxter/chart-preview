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

    /**
     * Return the 0-indexed bar/measure number at a given PPQ, accumulated
     * from PPQ(0) by walking the tempo/timesig map.
     *
     * Each segment contributes (segmentLengthQN / measureLenQN) measures.
     * Measure length = timeSigNum * (4 / timeSigDenom) QN. Mid-bar time-sig
     * changes are flattened to the next bar boundary (REAPER's default
     * authoring convention) — accurate for the typical case, off by one
     * for projects that author time-sig changes mid-measure.
     *
     * REAPER's TimeMap2_timeToBeats handles those mid-bar cases natively;
     * if cross-host divergence shows up in practice, swap the renderer to
     * use the host API directly via the provider.
     *
     * @param targetPpq  Position to compute measure number for
     * @param map        Tempo/timesig event map
     * @return           0-indexed bar number (0 == first bar of song)
     */
    static int pPqToMeasureNumber(PPQ targetPpq, const TempoTimeSignatureMap& map)
    {
        double t = targetPpq.toDouble();
        if (t <= 0.0) return 0;

        int totalMeasures = 0;
        double prevPpq = 0.0;
        int prevNum = 4, prevDenom = 4;

        for (const auto& [eventPpq, event] : map)
        {
            double ePpq = eventPpq.toDouble();
            if (ePpq >= t) break;

            double measureLenQN = (double)prevNum * (4.0 / (double)prevDenom);
            if (measureLenQN > 0.0)
                totalMeasures += (int)std::floor((ePpq - prevPpq) / measureLenQN + 0.5);

            prevPpq = ePpq;
            if (event.timeSigNumerator > 0 && event.timeSigDenominator > 0)
            {
                prevNum = event.timeSigNumerator;
                prevDenom = event.timeSigDenominator;
            }
        }

        // Final segment: prevPpq → targetPpq with the time sig in effect.
        // Floor (not round) here — we want the measure number CONTAINING
        // targetPpq, i.e., the count of completed bars before it.
        double measureLenQN = (double)prevNum * (4.0 / (double)prevDenom);
        if (measureLenQN > 0.0)
            totalMeasures += (int)std::floor((t - prevPpq) / measureLenQN + 1e-6);

        return totalMeasures;
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
