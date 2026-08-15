#pragma once

#include <JuceHeader.h>
#include "ChartMidiWriter.h"
#include "SngPacker.h"
#include "BackgroundGenerator.h"
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
    explicit ChartExporter(ReaperMidiProvider& provider);

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

    /**
        What the chart is called, taken from the audio item sitting under the
        export range. Your library already names those files
        "Artist - Album - NN - Title.wav", which is exactly the chart folder
        name, so nothing needs typing in. Fields stay empty when the filename
        does not split that way.
    */
    struct ChartName
    {
        juce::String artist;
        juce::String album;
        juce::String track;
        juce::String title;

        /**
            Tag-only, and only used by song.ini. Neither is part of the folder
            name, so neither counts towards completeness.
        */
        juce::String genre;
        juce::String year;

        juce::StringArray sources;     // every audio file considered
        juce::StringArray ambiguous;   // fields blanked because sources disagreed

        /**
            Charts are always written as "ARTIST - ALBUM - ## - TITLE",
            assembled from the fields rather than copied from any input
            filename. Empty until every field has a value, which is what the
            export dialog is for.
        */
        juce::String folderName() const;
        bool complete() const;
        juce::StringArray missingFields() const;
    };

    ChartName inferChartName(const TimeRange& range) const;

    /** Everything one audio file claims about itself, filename and tags. */
    struct SourceClaim
    {
        juce::String file;
        juce::String stem;
        juce::String artist, album, track, title, genre, year;
        /** Filename follows "Artist - Album - NN - Title". */
        bool conventional = false;
        /** Says something about itself, so it gets a vote on the name. */
        bool claimsAnything() const
        {
            return conventional || artist.isNotEmpty() || album.isNotEmpty()
                || track.isNotEmpty() || title.isNotEmpty();
        }
    };

    std::vector<SourceClaim> claimsUnder(const TimeRange& range) const;

    /** Directory holding the project file. */
    juce::String projectDirectory() const;

    /** Charts are written into an "export" folder beside the project file. */
    juce::File exportRoot() const;

    struct RenderResult
    {
        bool ok = false;
        juce::String message;
        juce::File output;
    };

    /**
        Renders the master mix over `range` into `folder` as song.opus.

        REAPER has no render API, so this writes the project's render settings,
        fires the render action, and restores the previous settings afterwards.
        The restore happens unconditionally: leaving someone's project pointed
        at a chart folder would be a nasty surprise the next time they render.
    */
    RenderResult renderSongAudio(const TimeRange& range, const juce::File& folder,
                                 const juce::String& formatCode) const;

    /**
        Everything the export dialog collects. Nothing here is inferred at
        write time: the dialog proposes, the charter confirms, and this is what
        comes back, so what gets written is always what was on screen.
    */
    struct ExportOptions
    {
        juce::String title, artist, album, genre, year, track;
        juce::String charter, icon;

        /**
            A rating is a judgement about the chart that cannot be read off it,
            so both stay unset until someone types a number, and the dialog
            will not export while they are.
        */
        int diffDrums = kUnrated;
        int diffDrumsReal = kUnrated;

        bool proDrums = true;
        bool fiveLaneDrums = false;

        juce::File albumArt;
        juce::File backgroundArt;

        /**
            Build background.png out of the album art rather than needing a
            second image. Ignored when a background was picked explicitly:
            something chosen beats something derived.
        */
        bool generateBackground = true;
        BackgroundGenerator::Options backgroundOptions;

        /** RENDER_FORMAT code, from Sink::formatCode(). */
        juce::String audioFormatCode;

        /** One .sng container rather than a folder of loose files. */
        bool packAsSng = false;

        /**
            Re-render the audio. Off once a chart has audio, because a
            re-export is nearly always about the chart: the audio takes the
            longest, blocks REAPER while it runs, and comes out the same.
        */
        bool renderAudio = true;

        juce::File destinationRoot;
        juce::String folderName;

        static constexpr int kUnrated = -1;
        bool rated() const { return diffDrums != kUnrated && diffDrumsReal != kUnrated; }
        bool nameable() const { return title.isNotEmpty() && artist.isNotEmpty(); }
    };

    struct ExportOutcome
    {
        bool ok = false;
        juce::String message;
        /** The finished chart: a folder, or the .sng when packed. */
        juce::File output;
    };

    /**
        Writes the whole chart: notes.mid, song.ini, artwork, rendered audio,
        and the .sng container when one was asked for.

        Order is deliberate. The chart is written before the audio because the
        render is the slow half and the half that can be repeated by hand, so a
        failed render should still leave something worth looking at.
    */
    ExportOutcome runExport(const TimeRange& range, const ExportOptions& options);

    /** Chart tracks the project has right now, named as they will export. */
    juce::StringArray chartTrackNames();

    /** Song audio already sitting in a chart folder, if any. */
    static juce::File existingAudio(const juce::File& folder);

    /** Drum type read off the notes, since the track name cannot say. */
    ChartMidiWriter::DrumProfile drumProfile(const TimeRange& range);

    /** Directories worth searching for album and background art. */
    juce::Array<juce::File> artworkSearchPaths(const TimeRange& range) const;

    /**
        Writes notes.mid for `range` into `folder`, rebased so the range start
        is tick 0 and the chart lines up with audio that starts at zero.
    */
    ChartMidiWriter::Result writeNotesMidi(const TimeRange& range, const juce::File& folder,
                                           const juce::String& songName);

    struct IniResult
    {
        bool ok = false;
        juce::String message;
        juce::File output;
    };

    /**
        song.ini as key/value pairs.

        Both output modes read from here: a folder gets these written to
        song.ini, and a .sng carries them as its metadata section instead,
        since the container has no song.ini file in it.

        The drum flags matter more than the metadata. Without `pro_drums` the
        game falls back to sniffing the notes for tom markers, so a song
        charted without any loads as plain 4-lane. See _refs/midi/drums.md.
    */
    juce::StringPairArray songIniValues(const TimeRange& range, const ExportOptions& options,
                                        const ChartMidiWriter::Result& midi) const;

    IniResult writeSongIni(const juce::File& folder, const juce::StringPairArray& values) const;

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
    ReaperMidiProvider& provider;
    const ReaperAPIs& apis;
    std::function<void*(const char*)> getReaperApi;

    void* project() const;
};
