#include "ChartExporter.h"

ChartExporter::ChartExporter(const ReaperAPIs& a, std::function<void*(const char*)> getFunc)
    : apis(a), getReaperApi(std::move(getFunc))
{
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

juce::String ChartExporter::Sink::formatCode() const
{
    // RENDER_FORMAT takes the sink's fourcc as four raw bytes. REAPER stores
    // these reversed relative to how they read, which is why the WAV sink shows
    // up in project files as "evaw" rather than "wave".
    char code[5] = {};
    code[0] = (char)((fourcc >> 24) & 0xFF);
    code[1] = (char)((fourcc >> 16) & 0xFF);
    code[2] = (char)((fourcc >> 8) & 0xFF);
    code[3] = (char)(fourcc & 0xFF);
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

juce::String ChartExporter::projectDirectory() const
{
    void* proj = project();
    if (!proj || !apis.GetProjectPathEx) return {};

    char buf[2048] = {};
    apis.GetProjectPathEx(proj, buf, sizeof(buf));
    return juce::String(juce::CharPointer_UTF8(buf));
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

    auto sinks = availableSinks();
    out << "[export] " << (int)sinks.size() << " render sinks\n";
    for (const auto& s : sinks)
        out << "    '" << s.formatCode() << "'  0x"
            << juce::String::toHexString((int)s.fourcc) << "  " << s.description << "\n";

    auto compressed = compressedSinks();
    out << "[export] " << (int)compressed.size() << " usable for song audio\n";
    for (const auto& s : compressed)
        out << "    '" << s.formatCode() << "'  " << s.description << "\n";

    auto sel = timeSelection();
    if (sel.valid())
        out << "[export] time selection " << juce::String(sel.startSec, 3) << "s to "
            << juce::String(sel.endSec, 3) << "s (" << juce::String(sel.length(), 3) << "s)\n";
    else
        out << "[export] NO TIME SELECTION\n";

    auto rgns = regions();
    out << "[export] " << (int)rgns.size() << " regions\n";
    for (const auto& r : rgns)
        out << "    " << r.index << "  " << juce::String(r.startSec, 3) << "s to "
            << juce::String(r.endSec, 3) << "s  " << r.name << "\n";

    return out;
}
