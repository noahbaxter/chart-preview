/*
    ==============================================================================

        dialog_main.cpp

        Renders the export dialog to a PNG with a fake context. No plugin host,
        no REAPER: the dialog only collects and validates, so nothing it draws
        needs a live project.

        Usage:
            dialog_harness <out.png> [--width W] [--height H]

    ==============================================================================
*/

#include <JuceHeader.h>
#include <iostream>

#include "Export/ExportDialogComponent.h"
#include "Export/BackgroundGenerator.h"

namespace
{
    juce::String iconName { "toki" };
    bool noRegions = false;
    bool selectAll = false;
    bool compilation = false;
    bool becomesRegion = false;
    bool collapse = false;
    bool noSelection = false;
    bool settingsOpen = false;

    ExportDialogComponent::Context makeContext()
    {
        ExportDialogComponent::Context context;
        context.selection.startSec = 513.0;
        context.selection.endSec = 717.235;

        // An album's worth of regions, named the way a project actually is.
        const char* const names[] = {
            "Gray World", "Vengeance", "Rattle of Death", "I'm the Same",
            "Speak Without Words", "Inglorius", nullptr
        };
        double at = 0.0;
        for (int i = 0; !noRegions && !becomesRegion && !collapse && names[i] != nullptr; ++i)
        {
            ChartExporter::Region region;
            region.name = names[i];
            region.startSec = at;
            region.endSec = at + 204.0;
            region.index = i + 1;
            region.guid = juce::String("{region-") + juce::String(i) + "}";
            context.regions.push_back(region);
            at = region.endSec + 2.0;
        }

        context.inferred.artist = "Lockslip";
        context.inferred.album = "The Conversation";
        context.inferred.track = "01";
        context.inferred.title = "Gray World";
        context.inferred.genre = "Mathcore";
        context.inferred.year = "2026";

        context.trackNames = { "PART DRUMS", "PART ELITE_DRUMS" };

        // As detected off the notes: Gray World has 203 tom markers and no
        // 5th-lane greens, so it is a 4-lane pro chart.
        context.drums.hasDrums = true;
        context.drums.proDrums = true;
        context.drums.tomMarkers = 203;

        context.audioFormats = {
            { 0, "MP3 (encoder by LAME project)" },
            { 0, "OGG Opus" },
            { 0, "OGG Vorbis" },
        };

        context.destinationRoot = juce::File::getCurrentWorkingDirectory().getChildFile("export");

        // Typed against a bare time selection, before any region exists. The
        // run then creates the region underneath it; none of this may be lost.
        if (becomesRegion || collapse)
        {
            juce::ValueTree tree("exportDialog");
            juce::ValueTree child("song");
            child.setProperty("guid", "selection", nullptr);
            child.setProperty("title", "Typed By Hand", nullptr);
            child.setProperty("artist", "Typed Artist", nullptr);
            child.setProperty("album", "Typed Album", nullptr);
            child.setProperty("genre", "Typed Genre", nullptr);
            child.setProperty("year", "1999", nullptr);
            child.setProperty("track", "07", nullptr);
            child.setProperty("diffDrums", 5, nullptr);
            child.setProperty("diffPro", 4, nullptr);
            tree.appendChild(child, nullptr);
            context.remembered = tree;
        }

        // A session that is not one album: two releases by different artists,
        // plus a loose track with nothing to group it by. Fed in the way the
        // plugin feeds it, so this exercises restore() as well as the rail.
        if (compilation)
        {
            struct Entry { const char* artist; const char* album; const char* year; };
            const Entry entries[] = {
                { "Lockslip", "The Conversation", "2026" },
                { "Lockslip", "The Conversation", "2026" },
                { "Lockslip", "The Conversation", "2026" },
                { "Vein Ledger", "Salt Harbour", "2024" },
                { "Vein Ledger", "Salt Harbour", "2024" },
                { "", "", "" },
            };

            juce::ValueTree tree("exportDialog");
            for (int i = 0; i < 6; ++i)
            {
                juce::ValueTree child("song");
                child.setProperty("guid", juce::String("{region-") + juce::String(i) + "}", nullptr);
                child.setProperty("artist", entries[i].artist, nullptr);
                child.setProperty("album", entries[i].album, nullptr);
                child.setProperty("year", entries[i].year, nullptr);
                tree.appendChild(child, nullptr);
            }
            context.remembered = tree;
        }

        // A real icon name, so the snapshot shows the lookup resolving.
        context.charter = "Dichotic";
        context.icon = iconName;
        return context;
    }
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    if (argc < 2)
    {
        std::cerr << "usage: dialog_harness <out.png> [--width W] [--height H]\n";
        return 1;
    }

    juce::File out(juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]));
    int width = 1280;
    int height = 720;

    for (int i = 2; i < argc; ++i)
    {
        juce::String flag(argv[i]);
        if (flag == "--width" && i + 1 < argc)  width = juce::String(argv[++i]).getIntValue();
        if (flag == "--height" && i + 1 < argc) height = juce::String(argv[++i]).getIntValue();
        if (flag == "--icon" && i + 1 < argc)   iconName = juce::String(argv[++i]);
        if (flag == "--no-regions") noRegions = true;
        if (flag == "--select-all") selectAll = true;
        if (flag == "--compilation") compilation = true;
        if (flag == "--becomes-region") becomesRegion = true;
        if (flag == "--collapse") collapse = true;
        if (flag == "--no-selection") noSelection = true;
        if (flag == "--settings") settingsOpen = true;
    }

    // --background <art.png> <out.png> checks the generator instead of the dialog.
    if (juce::String(argv[1]) == "--background" && argc >= 4)
    {
        BackgroundGenerator::Options options;
        for (int i = 4; i < argc; ++i)
        {
            juce::String flag(argv[i]);
            if (flag == "--style" && i + 1 < argc)
            {
                juce::String name(argv[++i]);
                if (name == "blur")  options.style = BackgroundGenerator::Style::blur;
                if (name == "tiled") options.style = BackgroundGenerator::Style::tiled;
            }
        }
        const bool ok = BackgroundGenerator::writeTo(
            juce::File::getCurrentWorkingDirectory().getChildFile(argv[3]),
            juce::File::getCurrentWorkingDirectory().getChildFile(argv[2]), options);
        std::cout << (ok ? "generated " : "failed ") << argv[3] << "\n";
        Theme::clearTypefaces();
        return ok ? 0 : 1;
    }

    ExportDialogComponent dialog(makeContext());
    dialog.setBounds(0, 0, width, height);

    if (selectAll) dialog.selectAllForEditing();
    if (settingsOpen) dialog.openSettingsForTests();

    // Present in every run: the button only shows when there is a provisional
    // song, and the dialog hides it when nothing can make a region.
    dialog.createRegion = [](const ChartExporter::TimeRange&, const juce::String&)
    { return juce::String("{made-up-region-guid}"); };

    // A steady project, so the readout's debounce settles and the panel shows
    // an answer. The cases below replace this with something that moves.
    {
        auto steady = makeContext();
        dialog.pollProject = [steady]()
        {
            ExportDialogComponent::Snapshot snapshot;
            snapshot.selection = steady.selection;
            snapshot.regions = steady.regions;
            snapshot.trackNames = steady.trackNames;

            // Nothing selected on the timeline: the readout and the create
            // button both have to go away, not sit there stale.
            if (noSelection) snapshot.selection.endSec = snapshot.selection.startSec;
            return snapshot;
        };
    }

    // What the selection readout reports. Same numbers the fixture claims for
    // the project, so the panel shows a real answer rather than "reading".
    dialog.analyseDrums = [](const ChartExporter::TimeRange&)
    {
        ChartMidiWriter::DrumProfile profile;
        profile.hasDrums = true;
        profile.proDrums = true;
        profile.tomMarkers = 203;
        return profile;
    };

    // The region appears under the song that was typed against a bare time
    // selection, exactly as it does when the export makes one. The poll below
    // picks it up and everything typed has to come through unchanged.
    // Select a stretch, click a point so the selection collapses, then select
    // it again. Nothing typed against it may be lost on the way through.
    if (collapse)
    {
        dialog.pollProject = []()
        {
            static int poll = 0;
            const bool selected = (poll++ / 3) % 2 == 0;

            ExportDialogComponent::Snapshot snapshot;
            snapshot.trackNames = { "PART DRUMS", "PART ELITE_DRUMS" };
            snapshot.selection.startSec = 513.0;
            snapshot.selection.endSec = selected ? 717.235 : 513.0;
            return snapshot;
        };
    }

    if (becomesRegion)
    {
        dialog.pollProject = []()
        {
            ExportDialogComponent::Snapshot snapshot;
            snapshot.selection.startSec = 513.0;
            snapshot.selection.endSec = 717.235;
            snapshot.trackNames = { "PART DRUMS", "PART ELITE_DRUMS" };

            ChartExporter::Region region;
            region.name = "Gray World";
            region.startSec = 513.0;
            region.endSec = 717.235;
            region.index = 1;
            region.guid = "{brand-new-region-guid}";
            snapshot.regions.push_back(region);
            return snapshot;
        };
    }

    // The icon lookup is debounced and runs on its own thread, so the snapshot
    // has to wait for it or it captures "checking" every time.
    // The poll timer runs at 400ms, so a case driven by pollProject has to be
    // given real time to run rather than stopping the moment the icon lands.
    const int pollMs = (becomesRegion || collapse) ? 3000 : 1400;
    const auto started = juce::Time::getMillisecondCounter();
    const auto deadline = started + 8000;

    while (juce::Time::getMillisecondCounter() < deadline)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil(100);

        const bool polledEnough =
            (int)(juce::Time::getMillisecondCounter() - started) >= pollMs;
        if (dialog.iconSettled() && polledEnough) break;
    }

    // Something behind the scrim, so the overlay reads the way it will in the
    // plugin rather than floating on nothing.
    juce::Image canvas(juce::Image::ARGB, width, height, true);
    {
        juce::Graphics g(canvas);
        g.fillAll(juce::Colour(0xFF101010));
        g.setColour(juce::Colour(0xFF1E1E1E));
        for (int y = 0; y < height; y += 40)
            g.fillRect(0, y, width, 20);
    }

    juce::Graphics g(canvas);
    dialog.paintEntireComponent(g, true);

    out.deleteFile();
    juce::FileOutputStream os(out);
    if (!os.openedOk())
    {
        std::cerr << "could not open " << out.getFullPathName() << "\n";
        return 1;
    }

    // The model as the dialog holds it, to tell "the value was lost" apart
    // from "the value is there but the box is not showing it".
    if (becomesRegion || collapse)
        std::cout << dialog.toValueTree().toXmlString() << "\n";

    juce::PNGImageFormat png;
    png.writeImageToStream(canvas, os);
    std::cout << "wrote " << out.getFullPathName() << "\n";

    // Theme caches its typeface in a static that would otherwise be destroyed
    // after JUCE has shut down, which aborts on the way out.
    Theme::clearTypefaces();
    return 0;
}
