#include "ChartExporter.h"

ChartExporter::ChartExporter(const ReaperAPIs& a, std::function<void*(const char*)> getFunc)
    : apis(a), getReaperApi(std::move(getFunc))
{
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
    juce::StringArray stems, artists, albums, tracks, titles;
    for (const auto& c : claims)
    {
        name.sources.add(c.file);
        if (!c.claimsAnything()) continue;
        if (c.conventional) stems.add(c.stem);
        artists.add(c.artist);
        albums.add(c.album);
        tracks.add(c.track);
        titles.add(c.title);
    }

    // Disagreement means we do not know, so the field is left empty rather
    // than guessed at from whichever item happened to come first.
    bool conflict = false;
    name.folder = consensus(stems, conflict);
    if (conflict) name.ambiguous.add("folder");
    name.artist = consensus(artists, conflict);
    if (conflict) name.ambiguous.add("artist");
    name.album = consensus(albums, conflict);
    if (conflict) name.ambiguous.add("album");
    name.track = consensus(tracks, conflict);
    if (conflict) name.ambiguous.add("track");
    name.title = consensus(titles, conflict);
    if (conflict) name.ambiguous.add("title");

    return name;
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

    if (!name.resolved())
        out << "[export] cannot name the chart from the audio under the range\n";
    else
    {
        out << "[export] chart folder: " << name.folder << "\n";
        out << "[export]   artist '" << name.artist << "'  album '" << name.album
            << "'  track '" << name.track << "'  title '" << name.title << "'\n";
        out << "[export] would write to: "
            << exportRoot().getChildFile(name.folder).getFullPathName() << "\n";
    }

    auto rgns = regions();
    out << "[export] " << (int)rgns.size() << " regions\n";
    for (const auto& r : rgns)
        out << "    " << r.index << "  " << juce::String(r.startSec, 3) << "s to "
            << juce::String(r.endSec, 3) << "s  " << r.name << "\n";

    return out;
}
