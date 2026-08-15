#pragma once

#include <JuceHeader.h>
#include <memory>
#include <vector>

#include "ChartExporter.h"
#include "IconPreview.h"
#include "../UI/Theme.h"
#include "../UI/Controls/CheckboxToggle.h"
#include "../UI/Controls/SegmentedButtons.h"
#include "../UI/Controls/ValueStepper.h"

/**
    The export dialog: pick which songs to export, and what goes in them.

    Songs are project regions, which is what lets one album project hold eleven
    charts. Nothing is imposed on how regions are named: a region's name is the
    default title and nothing more, and which rows are ticked is the whole of
    the selection, so a project can be laid out however suits it.

    Fields are split by what they belong to. Artist, album, year, genre,
    artwork and packaging are the same for every song on an album and are asked
    once; title, track number and difficulty belong to one song and follow its
    region around.

    It collects and validates only. Nothing here writes a file; Export hands
    back one SongExport per ticked row and the caller runs them.

    Lives as an overlay child of the editor rather than a DialogWindow, so it
    picks up the plugin's look and feel and never opens an OS window inside a
    host that may not want one.
*/
class ExportDialogComponent : public juce::Component
{
public:
    struct Context
    {
        /** Used when the project has no regions at all. */
        ChartExporter::TimeRange selection;
        std::vector<ChartExporter::Region> regions;

        ChartExporter::ChartName inferred;
        juce::StringArray trackNames;
        std::vector<ChartExporter::Sink> audioFormats;
        juce::File destinationRoot;
        juce::Array<juce::File> artworkSearchPaths;
        ChartMidiWriter::DrumProfile drums;

        /** Album-wide values, with a child per song keyed by region GUID. */
        juce::ValueTree remembered;
        juce::String charter, icon;
    };

    explicit ExportDialogComponent(Context context);

    /** Fired with one entry per ticked song. The dialog is still on screen. */
    std::function<void(const std::vector<ChartExporter::SongExport>&)> onExport;
    std::function<void()> onDismiss;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void parentSizeChanged() override;

    /** Everything typed here, so reopening starts where it left off. */
    juce::ValueTree toValueTree() const;

    /** False while the icon lookup is still in flight. For snapshot tests. */
    bool iconSettled() const { return iconPreview.settled(); }

private:
    /** A label and a text box, which is most of this dialog. */
    struct Field : public juce::Component
    {
        Field(const juce::String& labelText, bool numeric = false);
        void paint(juce::Graphics& g) override;
        void resized() override;

        juce::String text() const { return editor.getText().trim(); }
        void setText(const juce::String& value) { editor.setText(value, false); }

        juce::String label;
        juce::TextEditor editor;
    };

    /** An artwork slot: what it is, what is in it, and a button to change it. */
    struct ArtSlot : public juce::Component
    {
        ArtSlot(const juce::String& labelText);
        void paint(juce::Graphics& g) override;
        void resized() override;
        void setFile(const juce::File& file);

        juce::String label;
        juce::File file;
        juce::Image thumbnail;
        juce::TextButton chooseButton { "Choose" };
        juce::TextButton clearButton { "x" };
        std::unique_ptr<juce::FileChooser> chooser;
    };

    /** One song, and everything about it not shared with the album. */
    struct Song
    {
        ChartExporter::Region region;
        bool selected = false;
        juce::String title, track;
        int diffDrums = ChartExporter::ExportOptions::kUnrated;
        int diffPro = ChartExporter::ExportOptions::kUnrated;
        bool proDrums = true;
        bool fiveLane = false;

        /** Empty when ready, otherwise why it cannot be exported. */
        juce::String blocker(bool albumNamed, bool drums) const;
    };

    /** A row in the song list: tick, name, and whether it is ready. */
    struct SongRow : public juce::Component
    {
        SongRow();
        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent&) override { if (onSelect) onSelect(); }

        CheckboxToggle tick { "" };
        juce::String name, status;
        bool ready = false;
        bool current = false;
        std::function<void()> onSelect;
    };

    void buildSongs();
    void loadSong(int index);
    void storeSong(int index);
    void refreshRows();
    void refreshValidity();
    void prefillArtwork();
    void restore(const juce::ValueTree& tree);

    ChartExporter::ExportOptions optionsFor(const Song& song) const;
    juce::String albumBlocker() const;
    juce::Rectangle<float> cardBounds() const;
    float scale() const;

    Context context;
    std::vector<Song> songs;
    int current = 0;
    bool hasDrums = false;

    juce::Viewport listView;
    juce::Component listContent;
    std::vector<std::unique_ptr<SongRow>> rows;

    // Per song
    Field titleField { "TITLE" };
    Field trackField { "TRACK", true };
    ValueStepper drumsDifficulty { "DIFF DRUMS" };
    ValueStepper proDrumsDifficulty { "DIFF PRO" };
    CheckboxToggle proDrumsToggle { "Pro drums" };
    CheckboxToggle fiveLaneToggle { "5-lane drums" };

    // Album wide
    Field artistField { "ARTIST" };
    Field albumField { "ALBUM" };
    Field genreField { "GENRE" };
    Field yearField { "YEAR", true };
    Field charterField { "CHARTER" };
    Field iconField { "ICON" };
    IconPreview iconPreview;
    juce::TextButton browseIconsButton { "Browse icons" };

    ArtSlot albumArt { "ALBUM ART" };
    ArtSlot backgroundArt { "BACKGROUND" };
    CheckboxToggle generateBackgroundToggle { "Generate background from artwork" };
    SegmentedButtons backgroundStyleButtons;

    CheckboxToggle renderAudioToggle { "Re-render audio" };
    SegmentedButtons formatButtons;
    SegmentedButtons packagingButtons;

    juce::TextButton exportButton { "Export" };
    juce::TextButton cancelButton { "Cancel" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExportDialogComponent)
};
