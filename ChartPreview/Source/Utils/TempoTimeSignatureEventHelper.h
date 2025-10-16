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

#include "Utils.h"
#include <map>
#include <cmath>

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
};
