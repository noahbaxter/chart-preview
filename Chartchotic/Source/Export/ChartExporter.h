#pragma once

#include <JuceHeader.h>
#include "../Midi/Providers/REAPER/ReaperApiHelpers.h"

/**
    Builds a playable chart folder out of the current REAPER project.

    REAPER has no "render now" call, so audio export means writing the
    project's render settings, firing the render action, and putting the user's
    settings back. Everything here is therefore careful to leave the project as
    it found it.

    This is being built incrementally; right now it only reports what it can
    see, which is what settles the open questions about format support.
*/
class ChartExporter
{
public:
    ChartExporter(const ReaperAPIs& apis, std::function<void*(const char*)> getReaperApi);

    struct Sink
    {
        unsigned int fourcc = 0;
        juce::String description;
        /** The fourcc as it reads, e.g. "wave". Diagnostics only. */
        juce::String readableFourcc() const;
        /** Byte-reversed, which is what RENDER_FORMAT actually wants ("evaw"). */
        juce::String formatCode() const;
    };

    /**
        A sanity floor, not a tuned value: exporting a sub-second "song" is a
        confusing failure, so it is worth asking rather than proceeding. Both
        questions are reported separately since "a selection exists" and "this
        looks deliberate" are not the same thing.
    */
    static constexpr double kMinimumExportSeconds = 2.0;

    struct TimeRange
    {
        double startSec = 0.0;
        double endSec = 0.0;
        bool   exists() const { return endSec > startSec; }
        double length() const { return endSec - startSec; }
        bool   plausible() const { return length() >= kMinimumExportSeconds; }
    };

    struct Region
    {
        juce::String name;
        double startSec = 0.0;
        double endSec = 0.0;
        int    index = 0;
    };

    bool available() const;

    /** Every render sink this REAPER install offers. */
    std::vector<Sink> availableSinks() const;

    /** Sinks whose description mentions a lossy format we would ship. */
    std::vector<Sink> compressedSinks() const;

    /** The current time selection. Invalid when the user has not made one. */
    TimeRange timeSelection() const;

    std::vector<Region> regions() const;

    /** Directory holding the project file, where the chart folder goes. */
    juce::String projectDirectory() const;

    /** Human-readable dump of everything above, for wiring up and diagnosis. */
    juce::String describeContext() const;

    /**
        Everything the exporter does goes here rather than to DBG, so it can be
        read after the fact without attaching a console to REAPER.
        ~/Library/Logs/Chartchotic/export.log on macOS.
    */
    static juce::File logFile();
    static void log(const juce::String& message);

    /** Writes a session banner plus describeContext() to the log. */
    void logContext() const;

private:
    const ReaperAPIs& apis;
    std::function<void*(const char*)> getReaperApi;

    void* project() const;
};
