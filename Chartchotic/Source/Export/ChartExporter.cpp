#include "ChartExporter.h"

ChartExporter::ChartExporter(ReaperMidiProvider& p)
    : provider(p), apis(p.getAPIs()), getReaperApi(p.getReaperGetFunc())
{
}

ChartMidiWriter::Result ChartExporter::writeNotesMidi(const TimeRange& range,
                                                      const juce::File& folder,
                                                      const juce::String& songName)
{
    ChartMidiWriter writer(provider);
    return writer.write(range.startSec, range.endSec, folder, songName);
}

juce::File ChartExporter::logFile()
{
    return juce::FileLogger::getSystemLogFileFolder()
             .getChildFile("Chartchotic")
             .getChildFile("export.log");
}

void ChartExporter::log(const juce::String& message)
{
    auto file = logFile();
    file.getParentDirectory().createDirectory();

    juce::String stamped;
    for (const auto& line : juce::StringArray::fromLines(message.trimEnd()))
        stamped << juce::Time::getCurrentTime().toString(false, true, true, true)
                << "  " << line << "\n";

    file.appendText(stamped, false, false, "\n");
}

void ChartExporter::logContext() const
{
    log("=== export requested ===\n" + describeContext());
}

void* ChartExporter::project() const
{
    return ReaperApiHelpers::getProject(getReaperApi);
}

bool ChartExporter::available() const
{
    return getReaperApi != nullptr
        && apis.PCM_Sink_Enum != nullptr
        && apis.GetSetProjectInfo_String != nullptr
        && apis.GetSet_LoopTimeRange2 != nullptr;
}

juce::String ChartExporter::Sink::readableFourcc() const
{
    char code[5] = {};
    code[0] = (char)((fourcc >> 24) & 0xFF);
    code[1] = (char)((fourcc >> 16) & 0xFF);
    code[2] = (char)((fourcc >> 8) & 0xFF);
    code[3] = (char)(fourcc & 0xFF);
    return juce::String(juce::CharPointer_UTF8(code));
}

juce::String ChartExporter::Sink::formatCode() const
{
    // PCM_Sink_Enum hands back the fourcc the way it reads ("wave", "mp3l"),
    // but RENDER_FORMAT wants it byte-reversed. Confirmed twice: project files
    // store the WAV sink as "evaw", and the SDK's own MP3 example is "l3pm".
    char code[5] = {};
    code[0] = (char)(fourcc & 0xFF);
    code[1] = (char)((fourcc >> 8) & 0xFF);
    code[2] = (char)((fourcc >> 16) & 0xFF);
    code[3] = (char)((fourcc >> 24) & 0xFF);
    return juce::String(juce::CharPointer_UTF8(code));
}

std::vector<ChartExporter::Sink> ChartExporter::availableSinks() const
{
    std::vector<Sink> sinks;
    if (!apis.PCM_Sink_Enum) return sinks;

    // Enumeration ends when the callback stops handing back a description.
    for (int i = 0; i < 64; ++i)
    {
        const char* desc = nullptr;
        unsigned int fourcc = apis.PCM_Sink_Enum(i, &desc);
        if (fourcc == 0 && desc == nullptr) break;
        sinks.push_back({ fourcc, desc ? juce::String(desc) : juce::String() });
    }
    return sinks;
}

std::vector<ChartExporter::Sink> ChartExporter::compressedSinks() const
{
    std::vector<Sink> out;
    for (const auto& s : availableSinks())
    {
        auto d = s.description.toLowerCase();
        if (d.contains("opus") || d.contains("ogg") || d.contains("vorbis") || d.contains("mp3"))
            out.push_back(s);
    }
    return out;
}

ChartExporter::TimeRange ChartExporter::timeSelection() const
{
    TimeRange range;
    void* proj = project();
    if (!proj || !apis.GetSet_LoopTimeRange2) return range;

    double start = 0.0, end = 0.0;
    apis.GetSet_LoopTimeRange2(proj, false, false, &start, &end, false);
    range.startSec = start;
    range.endSec = end;
    return range;
}

std::vector<ChartExporter::Region> ChartExporter::regions() const
{
    std::vector<Region> out;
    void* proj = project();
    if (!proj || !apis.CountProjectMarkers || !apis.EnumProjectMarkers3) return out;

    int markers = 0, regionCount = 0;
    int total = apis.CountProjectMarkers(proj, &markers, &regionCount);
    for (int i = 0; i < total; ++i)
    {
        bool isRegion = false;
        double pos = 0.0, rgnEnd = 0.0;
        const char* name = nullptr;
        int number = 0, colour = 0;
        if (!apis.EnumProjectMarkers3(proj, i, &isRegion, &pos, &rgnEnd, &name, &number, &colour))
            continue;
        if (!isRegion) continue;
        out.push_back({ name ? juce::String(name) : juce::String(), pos, rgnEnd, number });
    }
    return out;
}

namespace
{
    // Tag identifiers vary by container, so each field is looked up under
    // every spelling REAPER might report for it.
    const char* kArtistTags[] = { "ID3:TPE1", "VORBIS:ARTIST", "INFO:IART", "XMP:dm/artist", nullptr };
    const char* kAlbumTags[]  = { "ID3:TALB", "VORBIS:ALBUM",  "INFO:IPRD", "XMP:dm/album",  nullptr };
    const char* kTitleTags[]  = { "ID3:TIT2", "VORBIS:TITLE",  "INFO:INAM", "XMP:dc/title",  nullptr };
    const char* kTrackTags[]  = { "ID3:TRCK", "VORBIS:TRACKNUMBER", "INFO:ITRK", nullptr };
    // Only tags for these two: no filename convention carries them, so an
    // untagged source simply leaves them out of song.ini.
    const char* kGenreTags[]  = { "ID3:TCON", "VORBIS:GENRE", "INFO:IGNR", nullptr };
    const char* kYearTags[]   = { "ID3:TDRC", "ID3:TYER", "VORBIS:DATE", "INFO:ICRD", nullptr };

    // The convention is "Artist - Album - NN - Title" with spaced dashes.
    // Splitting on the bare character would tear apart any working filename
    // that merely contains a hyphen and let the fragments pose as metadata, so
    // the separator has to be the spaced form.
    const juce::String kFieldSeparator = " - ";

    juce::StringArray splitOnSeparator(const juce::String& text)
    {
        juce::StringArray parts;
        int from = 0;
        for (;;)
        {
            int at = text.indexOf(from, kFieldSeparator);
            if (at < 0) { parts.add(text.substring(from).trim()); break; }
            parts.add(text.substring(from, at).trim());
            from = at + kFieldSeparator.length();
        }
        return parts;
    }

    /** Single value if every non-empty entry agrees, otherwise empty. */
    juce::String consensus(const juce::StringArray& values, bool& conflicted)
    {
        juce::StringArray distinct;
        for (const auto& v : values)
            if (v.isNotEmpty() && !distinct.contains(v, true))
                distinct.add(v);

        conflicted = distinct.size() > 1;
        return distinct.size() == 1 ? distinct[0] : juce::String();
    }
}

juce::StringArray ChartExporter::ChartName::missingFields() const
{
    juce::StringArray missing;
    if (artist.isEmpty()) missing.add("artist");
    if (album.isEmpty())  missing.add("album");
    if (track.isEmpty())  missing.add("track");
    if (title.isEmpty())  missing.add("title");
    return missing;
}

bool ChartExporter::ChartName::complete() const
{
    return missingFields().isEmpty();
}

juce::String ChartExporter::ChartName::folderName() const
{
    if (!complete()) return {};

    // Track numbers are padded to two digits in the output regardless of how
    // they arrived, since that is the shape charts are published in.
    juce::String number = track.trim();
    if (number.containsOnly("0123456789") && number.length() < 2)
        number = number.paddedLeft('0', 2);

    return artist.trim() + kFieldSeparator + album.trim()
         + kFieldSeparator + number + kFieldSeparator + title.trim();
}

std::vector<ChartExporter::SourceClaim> ChartExporter::claimsUnder(const TimeRange& range) const
{
    std::vector<SourceClaim> claims;
    void* proj = project();
    if (!proj || !range.exists()) return claims;
    if (!apis.CountMediaItems || !apis.GetMediaItem || !apis.GetActiveTake
        || !apis.GetMediaItemTake_Source || !apis.GetMediaSourceFileName
        || !apis.GetMediaItemInfo_Value)
        return claims;

    auto readTag = [this](void* source, const char* const* ids) -> juce::String
    {
        if (!apis.GetMediaFileMetadata) return {};
        for (int i = 0; ids[i] != nullptr; ++i)
        {
            char buf[1024] = {};
            if (apis.GetMediaFileMetadata(source, ids[i], buf, sizeof(buf)) > 0)
            {
                juce::String value = juce::String(juce::CharPointer_UTF8(buf)).trim();
                if (value.isNotEmpty() && value != "[Binary data]") return value;
            }
        }
        return {};
    };

    int count = apis.CountMediaItems(proj);
    for (int i = 0; i < count; ++i)
    {
        void* item = apis.GetMediaItem(proj, i);
        if (!item) continue;

        double pos = apis.GetMediaItemInfo_Value(item, "D_POSITION");
        double len = apis.GetMediaItemInfo_Value(item, "D_LENGTH");
        if (std::min(pos + len, range.endSec) - std::max(pos, range.startSec) <= 0.0)
            continue;

        void* take = apis.GetActiveTake(item);
        if (!take) continue;
        void* source = apis.GetMediaItemTake_Source(take);
        if (!source) continue;

        char path[2048] = {};
        apis.GetMediaSourceFileName(source, path, sizeof(path));
        juce::File file{ juce::String(juce::CharPointer_UTF8(path)) };
        if (file.getFileName().isEmpty()) continue;
        if (file.getFileExtension().equalsIgnoreCase(".mid")) continue;   // MIDI names nothing

        SourceClaim claim;
        claim.file = file.getFullPathName();
        claim.stem = file.getFileNameWithoutExtension();

        // Tags win where present; the filename is the fallback shape.
        claim.artist = readTag(source, kArtistTags);
        claim.album  = readTag(source, kAlbumTags);
        claim.title  = readTag(source, kTitleTags);
        claim.track  = readTag(source, kTrackTags);
        claim.genre  = readTag(source, kGenreTags);
        // Dates arrive in every shape from "2026" to "2026-04-13T00:00", and
        // song.ini wants the year on its own.
        claim.year   = readTag(source, kYearTags).retainCharacters("0123456789").substring(0, 4);

        auto parts = splitOnSeparator(claim.stem);
        claim.conventional = parts.size() >= 4;
        if (claim.conventional)
        {
            juce::StringArray rest;
            for (int k = 3; k < parts.size(); ++k) rest.add(parts[k]);

            if (claim.artist.isEmpty()) claim.artist = parts[0];
            if (claim.album.isEmpty())  claim.album  = parts[1];
            if (claim.track.isEmpty())  claim.track  = parts[2];
            if (claim.title.isEmpty())  claim.title  = rest.joinIntoString(kFieldSeparator);
        }

        claims.push_back(claim);
    }

    return claims;
}

ChartExporter::ChartName ChartExporter::inferChartName(const TimeRange& range) const
{
    ChartName name;
    auto claims = claimsUnder(range);
    if (claims.empty()) return name;

    // Only files that say something about themselves get a vote. A raw mix
    // stem sitting under the same range is not evidence of anything, and
    // letting it vote would blank every field on a perfectly clear export.
    juce::StringArray artists, albums, tracks, titles, genres, years;
    for (const auto& c : claims)
    {
        name.sources.add(c.file);
        if (!c.claimsAnything()) continue;
        artists.add(c.artist);
        albums.add(c.album);
        tracks.add(c.track);
        titles.add(c.title);
        genres.add(c.genre);
        years.add(c.year);
    }

    // Disagreement means we do not know, so the field is left empty rather
    // than guessed at from whichever item happened to come first.
    bool conflict = false;
    name.artist = consensus(artists, conflict);
    if (conflict) name.ambiguous.add("artist");
    name.album = consensus(albums, conflict);
    if (conflict) name.ambiguous.add("album");
    name.track = consensus(tracks, conflict);
    if (conflict) name.ambiguous.add("track");
    name.title = consensus(titles, conflict);
    if (conflict) name.ambiguous.add("title");
    name.genre = consensus(genres, conflict);
    if (conflict) name.ambiguous.add("genre");
    name.year = consensus(years, conflict);
    if (conflict) name.ambiguous.add("year");

    return name;
}

namespace
{
    // Every render setting this touches, so the whole set can be put back
    // exactly as found rather than reset to some assumed default.
    const char* kNumericRenderKeys[] = {
        "RENDER_SETTINGS", "RENDER_BOUNDSFLAG", "RENDER_STARTPOS", "RENDER_ENDPOS",
        "RENDER_CHANNELS", "RENDER_SRATE", "RENDER_ADDTOPROJ", "RENDER_TAILFLAG",
        nullptr
    };
    const char* kStringRenderKeys[] = { "RENDER_FILE", "RENDER_PATTERN", "RENDER_FORMAT", nullptr };

    constexpr int kRenderActionId = 41824;   // render with last settings, auto-close
}

ChartExporter::RenderResult ChartExporter::renderSongAudio(const TimeRange& range,
                                                           const juce::File& folder,
                                                           const juce::String& formatCode) const
{
    RenderResult result;
    void* proj = project();

    if (!proj || !apis.GetSetProjectInfo || !apis.GetSetProjectInfo_String || !apis.Main_OnCommand)
    { result.message = "render APIs unavailable"; return result; }

    if (!range.exists())
    { result.message = "no time selection to render"; return result; }

    if (formatCode.isEmpty())
    { result.message = "no audio format chosen"; return result; }

    folder.createDirectory();

    // --- save ---------------------------------------------------------------
    std::vector<double> numericBackup;
    for (int i = 0; kNumericRenderKeys[i]; ++i)
        numericBackup.push_back(apis.GetSetProjectInfo(proj, kNumericRenderKeys[i], 0.0, false));

    std::vector<juce::String> stringBackup;
    for (int i = 0; kStringRenderKeys[i]; ++i)
    {
        char buf[4096] = {};
        apis.GetSetProjectInfo_String(proj, kStringRenderKeys[i], buf, false);
        stringBackup.push_back(juce::String(juce::CharPointer_UTF8(buf)));
    }

    auto restore = [&]()
    {
        for (int i = 0; kNumericRenderKeys[i]; ++i)
            apis.GetSetProjectInfo(proj, kNumericRenderKeys[i], numericBackup[(size_t)i], true);
        for (int i = 0; kStringRenderKeys[i]; ++i)
        {
            auto copy = stringBackup[(size_t)i];
            std::vector<char> buf(4096, 0);
            copy.copyToUTF8(buf.data(), (int)buf.size());
            apis.GetSetProjectInfo_String(proj, kStringRenderKeys[i], buf.data(), true);
        }
    };

    auto setString = [&](const char* key, const juce::String& value)
    {
        std::vector<char> buf(4096, 0);
        value.copyToUTF8(buf.data(), (int)buf.size());
        return apis.GetSetProjectInfo_String(proj, key, buf.data(), true);
    };

    // --- configure ----------------------------------------------------------
    // Explicit start/end rather than "use the time selection", so the render is
    // pinned to the range we resolved even if the selection moves.
    apis.GetSetProjectInfo(proj, "RENDER_SETTINGS",  0.0, true);   // master mix
    apis.GetSetProjectInfo(proj, "RENDER_BOUNDSFLAG", 0.0, true);  // custom bounds
    apis.GetSetProjectInfo(proj, "RENDER_STARTPOS", range.startSec, true);
    apis.GetSetProjectInfo(proj, "RENDER_ENDPOS",   range.endSec, true);
    apis.GetSetProjectInfo(proj, "RENDER_CHANNELS", 2.0, true);
    apis.GetSetProjectInfo(proj, "RENDER_SRATE",    0.0, true);    // project rate
    apis.GetSetProjectInfo(proj, "RENDER_ADDTOPROJ", 0.0, true);   // do not re-import
    apis.GetSetProjectInfo(proj, "RENDER_TAILFLAG", 0.0, true);    // no tail

    setString("RENDER_FILE", folder.getFullPathName());
    setString("RENDER_PATTERN", "song");
    setString("RENDER_FORMAT", formatCode);

    // --- verify before firing ----------------------------------------------
    juce::String targets;
    {
        char buf[8192] = {};
        if (apis.GetSetProjectInfo_String(proj, "RENDER_TARGETS", buf, false))
            targets = juce::String(juce::CharPointer_UTF8(buf));
    }
    log("[render] format '" + formatCode + "'  bounds "
        + juce::String(range.startSec, 3) + "s to " + juce::String(range.endSec, 3)
        + "s\n[render] targets: " + (targets.isEmpty() ? "(none reported)" : targets));

    if (targets.isEmpty())
    {
        restore();
        result.message = "REAPER reported no render target, refusing to fire";
        return result;
    }

    result.output = juce::File(targets.upToFirstOccurrenceOf(";", false, false).trim());

    // --- fire ---------------------------------------------------------------
    apis.Main_OnCommand(kRenderActionId, 0);
    restore();

    if (!result.output.existsAsFile())
    {
        result.message = "render produced nothing at " + result.output.getFullPathName()
                       + " (action " + juce::String(kRenderActionId) + " may be wrong)";
        return result;
    }

    result.ok = true;
    result.message = "rendered " + juce::File::descriptionOfSizeInBytes(result.output.getSize())
                   + " to " + result.output.getFileName();
    return result;
}

juce::StringPairArray ChartExporter::songIniValues(const TimeRange& range,
                                                   const ExportOptions& options,
                                                   const ChartMidiWriter::Result& midi) const
{
    juce::StringPairArray values;

    // name, artist and charter are the three a chart cannot load without.
    values.set("name", options.title);
    values.set("artist", options.artist);
    values.set("album", options.album);
    values.set("genre", options.genre);
    values.set("year", options.year);
    values.set("album_track", options.track);
    values.set("charter", options.charter);
    values.set("icon", options.icon);

    const bool hasDrums = std::any_of(midi.trackNames.begin(), midi.trackNames.end(),
                                      [](const juce::String& t) { return t.containsIgnoreCase("DRUM"); });
    if (hasDrums)
    {
        values.set("pro_drums", options.proDrums ? "1" : "0");
        values.set("five_lane_drums", options.fiveLaneDrums ? "1" : "0");
        values.set("diff_drums", juce::String(options.diffDrums));
        values.set("diff_drums_real", juce::String(options.diffDrumsReal));
    }

    // Metadata only as far as the games are concerned, but chart_validate
    // checks it against the audio, which is what catches a render and a range
    // that disagree.
    values.set("song_length", juce::String(juce::roundToInt(range.length() * 1000.0)));

    return values;
}

ChartExporter::IniResult ChartExporter::writeSongIni(const juce::File& folder,
                                                     const juce::StringPairArray& values) const
{
    IniResult result;
    result.output = folder.getChildFile("song.ini");

    juce::String ini;
    ini << "[Song]\n";
    for (const auto& key : values.getAllKeys())
        ini << key << " = " << values[key] << "\n";

    if (!result.output.replaceWithText(ini, false, false, "\n"))
    {
        result.message = "could not write " + result.output.getFullPathName();
        return result;
    }

    result.ok = true;
    result.message = "wrote song.ini, " + juce::String(values.size()) + " keys";
    return result;
}

juce::StringArray ChartExporter::chartTrackNames()
{
    ChartMidiWriter writer(provider);
    return writer.trackNames();
}

ChartMidiWriter::DrumProfile ChartExporter::drumProfile(const TimeRange& range)
{
    ChartMidiWriter writer(provider);
    return writer.analyseDrums(range.startSec, range.endSec);
}

juce::Array<juce::File> ChartExporter::artworkSearchPaths(const TimeRange& range) const
{
    juce::Array<juce::File> paths;

    // Beside the project first: art that belongs to this chart lives with the
    // chart. The album's own folder second, since a whole album shares one
    // cover and that is where it will already be sitting.
    auto dir = projectDirectory();
    if (dir.isNotEmpty()) paths.addIfNotAlreadyThere(juce::File(dir));

    for (const auto& claim : claimsUnder(range))
    {
        auto parent = juce::File(claim.file).getParentDirectory();
        if (parent.isDirectory()) paths.addIfNotAlreadyThere(parent);
    }

    return paths;
}

ChartExporter::ExportOutcome ChartExporter::runExport(const TimeRange& range,
                                                      const ExportOptions& options)
{
    ExportOutcome outcome;

    if (!range.plausible())
    {
        outcome.message = "no usable time selection";
        return outcome;
    }
    if (!options.nameable() || options.folderName.isEmpty())
    {
        outcome.message = "chart needs a title and an artist";
        return outcome;
    }
    if (!options.rated())
    {
        outcome.message = "chart needs a difficulty rating";
        return outcome;
    }

    const juce::File root = options.destinationRoot.getFullPathName().isNotEmpty()
                              ? options.destinationRoot : exportRoot();
    if (root.getFullPathName().isEmpty())
    {
        outcome.message = "nowhere to export to, project has no folder";
        return outcome;
    }

    // In .sng mode the loose files are staging rather than output, so they are
    // assembled somewhere temporary and the container is the only thing that
    // lands beside the project.
    const juce::File staging = options.packAsSng
        ? juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("chartchotic-export").getChildFile(options.folderName)
        : root.getChildFile(options.folderName);

    // Only ever recursive on our own temp staging: a stale file left behind
    // from a previous run would otherwise get packed into the container.
    if (options.packAsSng && staging.isAChildOf(juce::File::getSpecialLocation(juce::File::tempDirectory)))
        staging.deleteRecursively();

    staging.createDirectory();
    log("=== export: " + options.folderName + (options.packAsSng ? " (.sng)" : " (folder)") + " ===");

    auto midi = writeNotesMidi(range, staging, options.title);
    log(juce::String("[export] ") + (midi.ok ? "OK " : "FAILED ") + midi.message);
    if (midi.detail.isNotEmpty()) log(midi.detail.trimEnd());
    if (!midi.ok)
    {
        outcome.message = midi.message;
        return outcome;
    }

    // Artwork keeps its own extension: the games accept png and jpg, and
    // renaming a jpg to .png makes a file nothing will open.
    auto copyArt = [&staging](const juce::File& source, const juce::String& stem)
    {
        if (!source.existsAsFile()) return juce::String();
        auto target = staging.getChildFile(stem + source.getFileExtension().toLowerCase());
        if (!source.copyFileTo(target))
            return "could not copy " + source.getFileName();
        return juce::String();
    };

    for (const auto& art : { std::make_pair(options.albumArt, juce::String("album")),
                             std::make_pair(options.backgroundArt, juce::String("background")) })
    {
        auto failure = copyArt(art.first, art.second);
        if (failure.isNotEmpty()) log("[export] " + failure);
    }

    // Only when nothing was picked: a background someone chose beats one
    // derived from the cover.
    if (options.generateBackground && !options.backgroundArt.existsAsFile()
        && options.albumArt.existsAsFile())
    {
        const bool made = BackgroundGenerator::writeTo(staging.getChildFile("background.png"),
                                                       options.albumArt, options.backgroundOptions);
        log(juce::String("[export] ") + (made ? "generated background from album art"
                                              : "could not generate a background"));
    }

    const auto values = songIniValues(range, options, midi);

    // A .sng carries these as its metadata section, so writing song.ini into
    // the staging folder would only pack a file the format has no slot for.
    if (!options.packAsSng)
    {
        auto ini = writeSongIni(staging, values);
        log(juce::String("[export] ") + (ini.ok ? "OK " : "FAILED ") + ini.message);
    }

    auto rendered = renderSongAudio(range, staging, options.audioFormatCode);
    log(juce::String("[export] ") + (rendered.ok ? "OK " : "FAILED ") + rendered.message);

    if (!options.packAsSng)
    {
        outcome.ok = true;
        outcome.output = staging;
        outcome.message = rendered.ok ? "chart written" : "chart written, audio failed: " + rendered.message;
        return outcome;
    }

    std::vector<SngPacker::Entry> entries;
    for (const auto& file : staging.findChildFiles(juce::File::findFiles, false))
        entries.push_back({ file.getFileName().toLowerCase(), file });

    auto packed = SngPacker::pack(root.getChildFile(options.folderName + ".sng"), values, entries);
    log(juce::String("[export] ") + (packed.ok ? "OK " : "FAILED ") + packed.message);

    staging.deleteRecursively();

    outcome.ok = packed.ok;
    outcome.output = packed.output;
    outcome.message = packed.ok && !rendered.ok
                        ? "packed, audio failed: " + rendered.message
                        : packed.message;
    return outcome;
}

juce::File ChartExporter::exportRoot() const
{
    auto dir = projectDirectory();
    if (dir.isEmpty()) return {};
    return juce::File(dir).getChildFile("export");
}

juce::String ChartExporter::projectDirectory() const
{
    // Deliberately not GetProjectPathEx: that returns the media folder, which
    // for this project is .../lockslip/Media. The chart belongs beside the RPP,
    // so ask EnumProjects for the project file itself and take its parent.
    if (!getReaperApi) return {};

    using EnumProjectsFn = void* (*)(int, char*, int);
    auto enumProjects = (EnumProjectsFn)getReaperApi("EnumProjects");
    if (!enumProjects) return {};

    char path[2048] = {};
    if (!enumProjects(-1, path, sizeof(path))) return {};

    juce::File projectFile{ juce::String(juce::CharPointer_UTF8(path)) };
    if (projectFile.getFullPathName().isEmpty()) return {};
    return projectFile.getParentDirectory().getFullPathName();
}

juce::String ChartExporter::describeContext() const
{
    juce::String out;
    out << "[export] available=" << (available() ? "yes" : "no") << "\n";

    if (!available())
    {
        out << "[export] required REAPER APIs missing, cannot export\n";
        return out;
    }

    out << "[export] project dir: " << projectDirectory() << "\n";

    auto compressed = compressedSinks();
    out << "[export] " << (int)compressed.size() << " sinks usable for song audio\n";
    for (const auto& s : compressed)
        out << "    " << s.readableFourcc() << " -> RENDER_FORMAT '" << s.formatCode()
            << "'   " << s.description << "\n";

    auto sel = timeSelection();
    if (!sel.exists())
        out << "[export] NO TIME SELECTION\n";
    else if (!sel.plausible())
        out << "[export] time selection only " << juce::String(sel.length(), 3)
            << "s, under the " << juce::String(kMinimumExportSeconds, 1)
            << "s minimum — treating as leftover, not a deliberate range\n";
    else
        out << "[export] time selection " << juce::String(sel.startSec, 3) << "s to "
            << juce::String(sel.endSec, 3) << "s (" << juce::String(sel.length(), 3) << "s)\n";

    auto claims = claimsUnder(sel);
    out << "[export] " << (int)claims.size() << " audio items under the range\n";
    for (const auto& c : claims)
    {
        out << "    " << c.stem << (c.claimsAnything() ? "" : "   (no claim, ignored)") << "\n";
        if (c.claimsAnything())
            out << "        artist '" << c.artist << "'  album '" << c.album
                << "'  track '" << c.track << "'  title '" << c.title << "'\n";
    }

    auto name = inferChartName(sel);
    if (!name.ambiguous.isEmpty())
        out << "[export] left blank, sources disagree: "
            << name.ambiguous.joinIntoString(", ") << "\n";

    out << "[export] proposed  artist '" << name.artist << "'  album '" << name.album
        << "'  track '" << name.track << "'  title '" << name.title << "'\n";

    auto missing = name.missingFields();
    if (!missing.isEmpty())
        out << "[export] needs filling in: " << missing.joinIntoString(", ") << "\n";
    else
        out << "[export] would write to: "
            << exportRoot().getChildFile(name.folderName()).getFullPathName() << "\n";

    auto rgns = regions();
    out << "[export] " << (int)rgns.size() << " regions\n";
    for (const auto& r : rgns)
        out << "    " << r.index << "  " << juce::String(r.startSec, 3) << "s to "
            << juce::String(r.endSec, 3) << "s  " << r.name << "\n";

    return out;
}
