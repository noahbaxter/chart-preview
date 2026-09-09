#pragma once

#include <JuceHeader.h>
#include <vector>
#include "../Midi/Providers/REAPER/ReaperMidiProvider.h"

/**
    Writes notes.mid for one slice of the REAPER timeline.

    REAPER's own MIDI export is a modal dialog that consolidates whole tracks,
    so it can neither be driven from in here nor made to rebase a range, which
    is why this reads the project through the same APIs the renderer uses and
    writes the file itself.

    Rebasing is the point of the exercise. An album project has track 2
    starting at 513s, while the chart has to start at tick 0 to line up with
    audio that starts at zero, so every position written here is relative to
    the start of the export range.
*/
class ChartMidiWriter
{
public:
    explicit ChartMidiWriter(ReaperMidiProvider& provider);

    /**
        What REAPER's own MIDI export writes, and what the charts we ship are
        already at. 480 also works and is what click-midi normalises to, but
        measured against a shipped chart it cost a tick: one tempo marker
        landed a tick early and the shortest notes came out a tick long.
    */
    static constexpr int kTicksPerQuarter = 960;

    struct Result
    {
        bool ok = false;
        juce::String message;
        /** Per-track breakdown, for the export log. */
        juce::String detail;
        juce::File output;
        /** Chart track names written, so song.ini knows what is in the chart. */
        juce::StringArray trackNames;
        int tracksWritten = 0;
        int notesWritten = 0;
    };

    Result write(double startSec, double endSec,
                 const juce::File& folder, const juce::String& songName);

    /** Chart tracks the project currently has, in the names they export under. */
    juce::StringArray trackNames();

    /**
        Which kind of drum track this is, read off the notes.

        The track name cannot answer this: PART DRUMS covers standard 4-lane,
        4-lane pro and 5-lane alike, and the format has no way to say which
        (_refs/midi/drums.md). The notes can, and it is the same test the games
        fall back to: tom markers mean pro, a 5th-lane green means 5-lane.
    */
    struct DrumProfile
    {
        bool hasDrums = false;
        bool proDrums = false;
        bool fiveLane = false;
        int tomMarkers = 0;
        int fiveLaneGreens = 0;
    };

    DrumProfile analyseDrums(double startSec, double endSec);

private:
    /** One REAPER track that becomes one MIDI track. */
    struct TrackSpec
    {
        int reaperIndex = 0;
        juce::String name;
        /**
            Instrument tracks carry sticky local switches that have to survive
            the rebase; the global tracks carry positional events where only
            the most recent one still applies. See partTrack().
        */
        bool instrument = true;
    };

    std::vector<TrackSpec> chartTracks();

    juce::MidiMessageSequence tempoTrack(double startQN, double endQN,
                                         const juce::String& songName);

    juce::MidiMessageSequence partTrack(const TrackSpec& spec, double startQN, double endQN,
                                        int& notesOut, int& skippedOut);

    ReaperMidiProvider& provider;
};
