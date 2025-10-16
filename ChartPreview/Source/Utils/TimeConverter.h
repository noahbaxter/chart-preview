/*
  ==============================================================================

    TimeConverter.h
    Created: 10 Oct 2025
    Author:  Noah Baxter

    Converts PPQ-based chart events to time-based (seconds) events for rendering.
    All events are converted relative to the cursor position (time zero).

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Utils.h"
#include "PPQ.h"

//==============================================================================
// TEMPO TIME SIGNATURE MAP - Similar structure to NoteStateMap

using TempoTimeSignatureMap = std::map<PPQ, TempoTimeSignatureEvent>;

//==============================================================================
// TIME-BASED DATA STRUCTURES (for rendering)

using TimeBasedTrackFrame = std::array<Gem, LANE_COUNT>;
using TimeBasedTrackWindow = std::map<double, TimeBasedTrackFrame>;  // double = seconds from cursor

struct TimeBasedSustainEvent
{
    double startTime;  // seconds from cursor
    double endTime;    // seconds from cursor
    uint gemColumn;
    SustainType sustainType;
    Gem gemType;
};

using TimeBasedSustainWindow = std::vector<TimeBasedSustainEvent>;

struct TimeBasedGridline
{
    double time;  // seconds from cursor
    Gridline type;
};

using TimeBasedGridlineMap = std::vector<TimeBasedGridline>;

//==============================================================================
// TIME CONVERTER

class TimeConverter
{
public:
    TimeConverter() = default;

    // Convert TrackWindow (PPQ-based) to TimeBasedTrackWindow (time-based)
    // Uses a conversion function that takes PPQ and returns absolute time
    template<typename PPQToTimeFunc>
    static TimeBasedTrackWindow convertTrackWindow(
        const TrackWindow& trackWindow,
        PPQ cursorPPQ,
        PPQToTimeFunc ppqToTime)
    {
        TimeBasedTrackWindow result;
        double cursorTime = ppqToTime(cursorPPQ.toDouble());

        for (const auto& [ppq, frame] : trackWindow)
        {
            double noteTime = ppqToTime(ppq.toDouble());
            double timeFromCursor = noteTime - cursorTime;
            result[timeFromCursor] = frame;
        }

        return result;
    }

    // Convert SustainWindow (PPQ-based) to TimeBasedSustainWindow (time-based)
    template<typename PPQToTimeFunc>
    static TimeBasedSustainWindow convertSustainWindow(
        const SustainWindow& sustainWindow,
        PPQ cursorPPQ,
        PPQToTimeFunc ppqToTime)
    {
        TimeBasedSustainWindow result;
        double cursorTime = ppqToTime(cursorPPQ.toDouble());

        for (const auto& sustain : sustainWindow)
        {
            TimeBasedSustainEvent timeSustain;
            timeSustain.startTime = ppqToTime(sustain.startPPQ.toDouble()) - cursorTime;
            timeSustain.endTime = ppqToTime(sustain.endPPQ.toDouble()) - cursorTime;
            timeSustain.gemColumn = sustain.gemColumn;
            timeSustain.sustainType = sustain.sustainType;
            timeSustain.gemType = sustain.gemType;
            result.push_back(timeSustain);
        }

        return result;
    }

    // Convert GridlineMap (PPQ-based) to TimeBasedGridlineMap (time-based)
    template<typename PPQToTimeFunc>
    static TimeBasedGridlineMap convertGridlineMap(
        const GridlineMap& gridlineMap,
        PPQ cursorPPQ,
        PPQToTimeFunc ppqToTime)
    {
        TimeBasedGridlineMap result;
        double cursorTime = ppqToTime(cursorPPQ.toDouble());

        for (const auto& [ppq, gridlineType] : gridlineMap)
        {
            TimeBasedGridline timeGridline;
            timeGridline.time = ppqToTime(ppq.toDouble()) - cursorTime;
            timeGridline.type = gridlineType;
            result.push_back(timeGridline);
        }

        return result;
    }
};
