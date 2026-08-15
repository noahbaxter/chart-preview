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

    ExportDialogComponent::Context makeContext()
    {
        ExportDialogComponent::Context context;
        context.range.startSec = 513.0;
        context.range.endSec = 717.235;

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

    for (int i = 2; i < argc - 1; ++i)
    {
        juce::String flag(argv[i]);
        if (flag == "--width")  width = juce::String(argv[++i]).getIntValue();
        if (flag == "--height") height = juce::String(argv[++i]).getIntValue();
        if (flag == "--icon")   iconName = juce::String(argv[++i]);
    }

    // --background <art.png> <out.png> checks the generator instead of the dialog.
    if (juce::String(argv[1]) == "--background" && argc >= 4)
    {
        BackgroundGenerator::Options options;
        const bool ok = BackgroundGenerator::writeTo(
            juce::File::getCurrentWorkingDirectory().getChildFile(argv[3]),
            juce::File::getCurrentWorkingDirectory().getChildFile(argv[2]), options);
        std::cout << (ok ? "generated " : "failed ") << argv[3] << "\n";
        Theme::clearTypefaces();
        return ok ? 0 : 1;
    }

    ExportDialogComponent dialog(makeContext());
    dialog.setBounds(0, 0, width, height);

    // The icon lookup is debounced and runs on its own thread, so the snapshot
    // has to wait for it or it captures "checking" every time.
    const auto deadline = juce::Time::getMillisecondCounter() + 8000;
    while (juce::Time::getMillisecondCounter() < deadline)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
        if (dialog.iconSettled()) break;
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

    juce::PNGImageFormat png;
    png.writeImageToStream(canvas, os);
    std::cout << "wrote " << out.getFullPathName() << "\n";

    // Theme caches its typeface in a static that would otherwise be destroyed
    // after JUCE has shut down, which aborts on the way out.
    Theme::clearTypefaces();
    return 0;
}
