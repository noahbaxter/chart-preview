#include "ChartMidiWriter.h"

#include "../Host/ReaperTrackDetector.h"
#include "../Midi/Discovery/TrackDiscovery.h"
#include "../UI/ControlConstants.h"

#include <algorithm>
#include <string>

namespace
{
    constexpr int kTextMeta = 1;
    constexpr int kTrackNameMeta = 3;
    constexpr double kMicrosecondsPerMinute = 60000000.0;

    const juce::String kDynamicsEvent{ "ENABLE_CHART_DYNAMICS" };

    // Chart tracks we do not interpret but still have to carry: sections live
    // on EVENTS, and dropping any of these silently costs the export features
    // the project already authored.
    const char* kPassthroughTracks[] = {
        "EVENTS", "BEAT", "VENUE", "PART VOCALS", "HARM1", "HARM2", "HARM3", nullptr
    };

    /**
        Where an event sorts against others on the same tick. A note-off has to
        land before a note-on sharing its tick, or a parser reading the same
        pitch twice in a row cannot tell which off closes which on.
    */
    enum EventOrder { orderText = 0, orderNoteOff = 1, orderNoteOn = 2 };

    struct PendingEvent
    {
        int tick = 0;
        int order = orderText;
        juce::MidiMessage message;
    };

    /**
        The spelling the chart format expects, or empty when the track is not
        an instrument part.

        A REAPER track called "PART ELITE DRUMS" has to be written as
        "PART ELITE_DRUMS" or nothing will load it. Parts we do not implement
        keep their REAPER name instead: several of them share one Part
        (REAL_GUITAR and REAL_GUITAR_22 are different tracks), so there is no
        single canonical spelling to map back to.
    */
    juce::String chartTrackName(const std::string& reaperName)
    {
        Part part;
        if (!matchTrackNameToPart(reaperName, part))
            return {};

        for (const auto& entry : getImplementedTrackNames())
            if (entry.part == part)
                return juce::String(entry.name);

        return juce::String(reaperName);
    }
}

ChartMidiWriter::ChartMidiWriter(ReaperMidiProvider& p)
    : provider(p)
{
}

std::vector<ChartMidiWriter::TrackSpec> ChartMidiWriter::chartTracks()
{
    std::vector<TrackSpec> specs;

    auto getFunc = provider.getReaperGetFunc();
    if (!getFunc) return specs;

    auto alreadyTaken = [&specs](const juce::String& name)
    {
        return std::any_of(specs.begin(), specs.end(),
                           [&name](const TrackSpec& s) { return s.name == name; });
    };

    // REAPER's track order is the user's order and the format does not care
    // about ordering, so it is carried through rather than resorted here.
    const int count = ReaperTrackDetector::getTrackCount(getFunc);
    for (int i = 0; i < count; ++i)
    {
        std::string reaperName = ReaperTrackDetector::getTrackName(getFunc, i);

        // Duplicates would write two MIDI tracks under one name, which is not
        // a chart any game can read, so the first one wins as it does in
        // discovery.
        auto part = chartTrackName(reaperName);
        if (part.isNotEmpty())
        {
            if (!alreadyTaken(part))
                specs.push_back({ i, part, true });
            continue;
        }

        for (int k = 0; kPassthroughTracks[k] != nullptr; ++k)
        {
            juce::String global{ kPassthroughTracks[k] };
            if (juce::String(reaperName).trim().equalsIgnoreCase(global))
            {
                if (!alreadyTaken(global))
                    specs.push_back({ i, global, false });
                break;
            }
        }
    }

    return specs;
}

juce::StringArray ChartMidiWriter::trackNames()
{
    juce::StringArray names;
    for (const auto& spec : chartTracks())
        names.add(spec.name);
    return names;
}

ChartMidiWriter::DrumProfile ChartMidiWriter::analyseDrums(double startSec, double endSec)
{
    // Tom markers, which make yellow/blue/green toms instead of the cymbals
    // 4-lane notes default to. Their presence is what says "pro".
    static constexpr int kTomMarkers[] = { 110, 111, 112 };
    // The 5th lane, per difficulty. Nothing else distinguishes 5-lane.
    static constexpr int kFiveLaneGreens[] = { 101, 89, 77, 65 };

    DrumProfile profile;

    const double startQN = provider.timeToPpq(startSec);
    const double endQN = provider.timeToPpq(endSec);

    for (const auto& spec : chartTracks())
    {
        if (!spec.name.containsIgnoreCase("DRUM")) continue;
        profile.hasDrums = true;

        for (const auto& note : provider.getAllNotesFromTrack(spec.reaperIndex))
        {
            if (note.muted) continue;
            if (note.startPPQ < startQN || note.startPPQ >= endQN) continue;

            for (int pitch : kTomMarkers)
                if (note.pitch == pitch) ++profile.tomMarkers;
            for (int pitch : kFiveLaneGreens)
                if (note.pitch == pitch) ++profile.fiveLaneGreens;
        }
    }

    // Pro wins a tie: the spec says to prefer it when both are detected, and a
    // pro chart read as 5-lane puts notes in lanes that do not exist.
    profile.proDrums = profile.tomMarkers > 0;
    profile.fiveLane = profile.fiveLaneGreens > 0 && !profile.proDrums;
    return profile;
}

juce::MidiMessageSequence ChartMidiWriter::tempoTrack(double startQN, double endQN,
                                                      const juce::String& songName)
{
    juce::MidiMessageSequence sequence;
    sequence.addEvent(juce::MidiMessage::textMetaEvent(
                          kTrackNameMeta, songName.isNotEmpty() ? songName : juce::String("notes")));

    auto tempoMessage = [](double beatsPerMinute)
    {
        return juce::MidiMessage::tempoMetaEvent(
            juce::roundToInt(kMicrosecondsPerMinute / juce::jmax(1.0, beatsPerMinute)));
    };

    auto events = provider.getAllTempoTimeSignatureEvents();

    // Whatever is in effect where the range starts has to be restated at tick
    // 0. The marker that set it usually sits far behind the range, and a chart
    // whose tempo only arrives at the first marker inside the range plays
    // everything before that marker at the wrong speed.
    double bpm = 120.0;
    int numerator = 4, denominator = 4;
    for (const auto& event : events)
    {
        if ((double)event.ppqPosition > startQN) break;
        bpm = event.bpm;
        numerator = event.timeSigNumerator;
        denominator = event.timeSigDenominator;
    }

    sequence.addEvent(juce::MidiMessage::timeSignatureMetaEvent(numerator, denominator));
    sequence.addEvent(tempoMessage(bpm));

    for (const auto& event : events)
    {
        const double qn = (double)event.ppqPosition;
        if (qn <= startQN || qn >= endQN) continue;

        const double tick = (double)juce::roundToInt((qn - startQN) * kTicksPerQuarter);

        // REAPER reports a time signature on tempo-only markers too, so
        // emitting on the flag alone restates 4/4 partway through a bar. A
        // parser that counts measures from signature events would start a new
        // bar there, which is how you get a chart whose bar lines drift off
        // the notes. Only an actual change is worth writing.
        if (event.timeSigNumerator != numerator || event.timeSigDenominator != denominator)
        {
            numerator = event.timeSigNumerator;
            denominator = event.timeSigDenominator;
            sequence.addEvent(juce::MidiMessage::timeSignatureMetaEvent(numerator, denominator), tick);
        }

        sequence.addEvent(tempoMessage(event.bpm), tick);
    }

    return sequence;
}

juce::MidiMessageSequence ChartMidiWriter::partTrack(const TrackSpec& spec,
                                                     double startQN, double endQN,
                                                     int& notesOut, int& skippedOut)
{
    std::vector<PendingEvent> pending;

    auto tickFor = [startQN](double qn)
    {
        return juce::jmax(0, juce::roundToInt((qn - startQN) * kTicksPerQuarter));
    };

    // Text events before the range still describe the state the range starts
    // in, so they are pinned to tick 0 rather than dropped. Without this, an
    // album track starting at 513s loses the ENABLE_CHART_DYNAMICS sitting at
    // 0s and the game silently ignores every ghost and accent in the export.
    bool hasDynamicsSwitch = false;
    std::vector<ChartTextEvent> carried;
    for (const auto& event : provider.getAllTextEventsFromTrack(spec.reaperIndex))
    {
        const double qn = (double)event.position;
        if (qn >= endQN) continue;
        if (event.text.containsIgnoreCase(kDynamicsEvent)) hasDynamicsSwitch = true;

        if (qn >= startQN)
        {
            pending.push_back({ tickFor(qn), orderText,
                                juce::MidiMessage::textMetaEvent(kTextMeta, event.text) });
            continue;
        }

        if (spec.instrument)
        {
            // Local switches are independent of each other and stay in force
            // until replaced, so the latest of each distinct one is carried.
            auto same = std::find_if(carried.begin(), carried.end(),
                                     [&event](const ChartTextEvent& e) { return e.text == event.text; });
            if (same != carried.end())
                *same = event;
            else
                carried.push_back(event);
        }
        else
        {
            // Global tracks are positional: you are inside exactly one section
            // when the range opens, and the earlier ones are behind you.
            carried.assign(1, event);
        }
    }

    for (const auto& event : carried)
        pending.push_back({ 0, orderText,
                            juce::MidiMessage::textMetaEvent(kTextMeta, event.text) });

    for (const auto& note : provider.getAllNotesFromTrack(spec.reaperIndex))
    {
        if (note.muted) { ++skippedOut; continue; }

        // A note that starts before the range belongs to whatever came before
        // it, so only the start position decides membership.
        if (note.startPPQ < startQN || note.startPPQ >= endQN) continue;

        const int startTick = tickFor(note.startPPQ);
        // Rounding can collapse a short note onto its own start, and a
        // zero-length note is unhittable, so every note keeps at least a tick.
        const int endTick = juce::jmax(startTick + 1, tickFor(juce::jmin(note.endPPQ, endQN)));

        const int channel = juce::jlimit(1, 16, note.channel + 1);
        // Velocity carries drum dynamics, but a zero would be written as a
        // note-off and lose the note entirely.
        const int velocity = juce::jlimit(1, 127, note.velocity);

        pending.push_back({ startTick, orderNoteOn,
                            juce::MidiMessage::noteOn(channel, note.pitch, (juce::uint8)velocity) });
        pending.push_back({ endTick, orderNoteOff,
                            juce::MidiMessage::noteOff(channel, note.pitch) });
        ++notesOut;
    }

    // Every drum chart gets this, whether or not it currently has a dynamic
    // note on it. Without the switch the game ignores ghost and accent
    // velocities entirely, and gating on "does this export happen to contain
    // one" means the first ghost added after an export silently does nothing.
    // Nothing is lost by shipping it on a chart that never uses dynamics.
    // Written bare, as every chart that carries the event at all writes it.
    if (!hasDynamicsSwitch && spec.name.containsIgnoreCase("DRUM"))
        pending.push_back({ 0, orderText,
                            juce::MidiMessage::textMetaEvent(kTextMeta, kDynamicsEvent) });

    // MidiMessageSequence keeps insertion order within a tick, so sorting here
    // is what settles the order events end up in the file.
    std::stable_sort(pending.begin(), pending.end(),
                     [](const PendingEvent& a, const PendingEvent& b)
                     { return a.tick != b.tick ? a.tick < b.tick : a.order < b.order; });

    juce::MidiMessageSequence sequence;
    sequence.addEvent(juce::MidiMessage::textMetaEvent(kTrackNameMeta, spec.name));
    for (const auto& event : pending)
        sequence.addEvent(event.message, (double)event.tick);

    return sequence;
}

ChartMidiWriter::Result ChartMidiWriter::write(double startSec, double endSec,
                                               const juce::File& folder,
                                               const juce::String& songName)
{
    Result result;

    if (endSec <= startSec)
    {
        result.message = "no range to write";
        return result;
    }

    const double startQN = provider.timeToPpq(startSec);
    const double endQN   = provider.timeToPpq(endSec);

    auto specs = chartTracks();
    if (specs.empty())
    {
        result.message = "no chart tracks in the project";
        return result;
    }

    juce::MidiFile file;
    file.setTicksPerQuarterNote(kTicksPerQuarter);
    file.addTrack(tempoTrack(startQN, endQN, songName));

    juce::String detail;
    for (const auto& spec : specs)
    {
        int notes = 0, skipped = 0;
        file.addTrack(partTrack(spec, startQN, endQN, notes, skipped));
        result.notesWritten += notes;
        result.trackNames.add(spec.name);

        detail << "    " << spec.name << "  " << notes << " notes";
        if (skipped > 0) detail << "  (" << skipped << " muted, skipped)";
        detail << "\n";
    }

    result.tracksWritten = file.getNumTracks();
    result.output = folder.getChildFile("notes.mid");

    folder.createDirectory();
    result.output.deleteFile();

    juce::FileOutputStream stream(result.output);
    if (!stream.openedOk())
    {
        result.message = "could not open " + result.output.getFullPathName();
        return result;
    }

    // Type 1 only: charts do not support type 0 or 2.
    if (!file.writeTo(stream, 1))
    {
        result.message = "writing notes.mid failed";
        return result;
    }
    stream.flush();

    result.ok = true;
    result.detail = detail;
    result.message = "wrote notes.mid, " + juce::String(result.tracksWritten) + " tracks, "
                   + juce::String(result.notesWritten) + " notes, rebased from "
                   + juce::String(startSec, 3) + "s";
    return result;
}
