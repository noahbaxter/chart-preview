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
        /** The 4-byte string RENDER_FORMAT wants, derived from the fourcc. */
        juce::String formatCode() const;
    };

    struct TimeRange
    {
        double startSec = 0.0;
        double endSec = 0.0;
        bool   valid() const { return endSec > startSec; }
        double length() const { return endSec - startSec; }
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

private:
    const ReaperAPIs& apis;
    std::function<void*(const char*)> getReaperApi;

    void* project() const;
};
