#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <set>
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

    Fills whatever window it is given rather than drawing its own card. See
    ExportWindow for why it stopped being an overlay inside the editor.
*/
class ExportDialogComponent : public juce::Component,
                             private juce::Timer
{
public:
    /**
        What the window re-reads from REAPER while it is open.

        The project carries on being edited with this on screen: a time
        selection gets made, a region gets added or renamed. Reading it once at
        open time meant the window sat there insisting there was nothing to
        export while the answer was right there on the timeline.
    */
    struct Snapshot
    {
        ChartExporter::TimeRange selection;
        std::vector<ChartExporter::Region> regions;
        juce::StringArray trackNames;
    };
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

    /** Clears the background staging folder. Nothing there outlives the window. */
    ~ExportDialogComponent() override;

    /**
        Below this the two field columns start overlapping. The window refuses
        to go smaller rather than letting the layout fold up.
    */
    static constexpr int kMinimumWidth = 900;
    static constexpr int kMinimumHeight = 620;

    /**
        Polled a few times a second. Cheap things only: enumerating regions and
        reading the time selection, not scanning notes.
    */
    std::function<Snapshot()> pollProject;

    /**
        Called only when the ranges actually change, since it reads every note
        on every drum track and is far too expensive to poll.
    */
    std::function<ChartMidiWriter::DrumProfile(const ChartExporter::TimeRange&)> analyseDrums;

    /** Fired with one entry per ticked song. The dialog is still on screen. */
    std::function<void(const std::vector<ChartExporter::SongExport>&)> onExport;
    std::function<void()> onDismiss;

    /**
        Make the time selection a region, returning its GUID. Offered whenever
        the selection is not already inside one; export still makes any region
        it needs, so this is the deliberate way rather than the only way.
    */
    std::function<juce::String(const ChartExporter::TimeRange&, const juce::String& name)> createRegion;

    /**
        Drive the project's time selection from the list.

        Selecting a song and selecting its time are the same act: clicking a
        row moves the timeline to it, and losing the time selection drops the
        row. Multiple songs selected at once is its own state and does not
        try to express itself as one range.
    */
    std::function<void(const ChartExporter::TimeRange&)> setTimeSelection;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    /**
        Takes on the regions the export just made, so the settings saved on
        close key to them rather than to a time selection that is about to
        stop being the way this song is identified.
    */
    void adoptRegions(const juce::StringPairArray& folderNameToGuid);

    /** Everything typed here, so reopening starts where it left off. */
    juce::ValueTree toValueTree() const;

    /** False while the icon lookup is still in flight. For snapshot tests. */
    bool iconSettled() const { return iconPreview.settled(); }

    /** Select every song, as shift-clicking the rail end to end would. For
        snapshot tests, which cannot otherwise reach the mixed state. */
    void selectAllForEditing();

private:
    /**
        A text box that can report that the selected songs disagree.

        Drawn over the editor rather than through setTextToShowWhenEmpty,
        which cannot italicise the marker.
    */
    struct MixedTextEditor : public juce::TextEditor
    {
        void paintOverChildren(juce::Graphics& g) override;
        bool mixed = false;
    };

    /** A label and a text box, which is most of this dialog. */
    struct Field : public juce::Component
    {
        Field(const juce::String& labelText, bool numeric = false);
        void paint(juce::Graphics& g) override;
        void resized() override;

        juce::String text() const { return editor.getText().trim(); }
        void setText(const juce::String& value)
        {
            editor.mixed = false;
            editor.setText(value, false);   // no notification: not a user edit
        }

        /** Show the marker. Cleared by setText and by the user typing. */
        void setMixed()
        {
            editor.setText({}, false);
            editor.mixed = true;
            editor.repaint();
        }

        bool isMixed() const { return editor.mixed; }

        /**
            Label gutter in pixels, not a proportion of this field's width.
            Shared by every field in a panel so the boxes line up.
        */
        int labelWidth = 66;

        juce::String label;
        MixedTextEditor editor;
    };

    /**
        An artwork slot: heading, then the picture.

        The picture is the control. Clicking it picks a file, and a clear badge
        appears over its corner on hover, so there is no button row competing
        with the thing it acts on.
    */
    struct ArtSlot : public juce::Component
    {
        ArtSlot(const juce::String& labelText);
        void paint(juce::Graphics& g) override;
        void setFile(const juce::File& file);

        void mouseDown(const juce::MouseEvent& e) override;
        void mouseMove(const juce::MouseEvent& e) override;
        void mouseEnter(const juce::MouseEvent&) override { hovering = true; repaint(); }
        void mouseExit(const juce::MouseEvent&) override
        {
            hovering = false;
            overClear = false;
            repaint();
        }

        /** The picture itself, below the heading and above the filename. */
        juce::Rectangle<int> previewBounds() const;

        /** Top-right of the picture, and only while hovering something. */
        juce::Rectangle<int> clearBounds() const;

        /** Top-left of the picture. Only the slot that has options shows one. */
        juce::Rectangle<int> optionsBounds() const;

        /**
            Where this file will be once the chart is written. Generated
            artwork lives somewhere temporary until then, and showing that
            path says nothing useful about what is going to ship.
        */
        juce::String caption;

        bool hasOptions = false;
        std::function<void()> onOptions;

        /** The selected songs have different artwork, so none is shown. */
        void setMixed(bool isMixed) { mixed = isMixed; repaint(); }

        /**
            Fired when the user picks or clears a picture, never when the slot
            is filled from the selection. Loading must not look like editing.
        */
        std::function<void(const juce::File&)> onFileChanged;

        juce::String label;
        juce::File file;
        bool mixed = false;
        bool hovering = false;
        bool overClear = false;
        juce::Image thumbnail;
        std::unique_ptr<juce::FileChooser> chooser;
    };

    /**
        One song and everything about it.

        All metadata lives here, not on the album: a session can be a
        compilation or a split, not just one record. Only the charter identity
        is session-wide. Repeated typing is avoided by the fields editing every
        selected song at once.
    */
    struct Song
    {
        ChartExporter::Region region;

        /** Ticked for export. Distinct from being selected for editing. */
        bool selected = false;

        juce::String title, track;
        juce::String artist, album, genre, year;
        int diffDrums = ChartExporter::ExportOptions::kUnrated;
        int diffPro = ChartExporter::ExportOptions::kUnrated;
        bool proDrums = true;
        bool fiveLane = false;
        juce::File albumArtFile, backgroundFile;

        /** The chart already on disk for this song, if there is one. */
        juce::File exported;

        /** Empty when ready, otherwise why it cannot be exported. */
        juce::String blocker(bool charterNamed, bool drums) const;
    };

    /** A row in the song list: tick, track number, name, and whether it is ready. */
    struct SongRow : public juce::Component
    {
        SongRow();
        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override { if (onSelect) onSelect(e.mods); }

        CheckboxToggle tick { "" };

        /** Track number, shown on every row so gaps and duplicates show up. */
        juce::String number;

        /**
            Where this song sits on the timeline, and what region it is. An
            untitled row is otherwise blank and two of them are then
            indistinguishable.
        */
        juce::String timecode, source;

        juce::String name, status;
        bool untitled = false;
        bool ready = false;

        /** Being edited by the fields on the right. Not the same as ticked. */
        bool current = false;

        bool exported = false;
        std::function<void(const juce::ModifierKeys&)> onSelect;
    };

    void timerCallback() override;
    void applySnapshot(const Snapshot& snapshot);
    bool differsFromCurrent(const Snapshot& snapshot) const;
    void rebuildRows();

    /** Rebuild the rail's order and headings from the current metadata. */
    void regroup();

    /** Fill every control from the selection, marking disagreements mixed. */
    void loadSelection();

    /**
        Apply one field to every selected song. The only write path, which is
        what keeps a mixed field mixed until the user edits it.
    */
    void applyToSelection(std::function<void(Song&)> change);

    /** Click, shift-click and cmd-click, in the rail's display order. */
    void selectSong(int index, const juce::ModifierKeys& mods);

    void refreshRows();
    void refreshValidity();
    void prefillArtwork();
    void restore(const juce::ValueTree& tree);

    ChartExporter::ExportOptions optionsFor(const Song& song) const;
    juce::String albumBlocker() const;

    /** How many ticked songs would actually be written if Export were pressed. */
    int readyCount() const;

    /** True when the time selection lies within a region that already exists. */
    bool selectionInsideRegion() const;

    /**
        Render a background from each selected song's album art in the chosen
        template and hand it to that song. Writes a real file, so export ships
        the image that was on screen rather than making its own later.

        With onlyMissing, songs that already have a background are left alone.
    */
    void generateBackgrounds(bool onlyMissing = false);

    /** Panel rects measured in resized(), drawn in paint(). */
    juce::Rectangle<int> railBounds, songPanel, albumPanel, footerBounds, selectionPanel;

    /**
        What the time selection currently covers, and what is in it.

        Reading every note on every drum track is far too expensive to do while
        a selection is being dragged, so it is analysed only once the range has
        stopped moving: lastPolledSelection is the previous tick's range and
        analysedSelection is the one selectionDrums describes.
    */
    ChartExporter::TimeRange lastPolledSelection, analysedSelection;
    ChartMidiWriter::DrumProfile selectionDrums;

    Context context;
    std::vector<Song> songs;

    /** Songs the fields are editing, by index into songs. Never empty. */
    std::set<int> editing;

    /** Where a shift-click measures its range from. */
    int anchor = 0;

    bool hasDrums = false;

    juce::Viewport listView;
    juce::Component listContent;
    std::vector<std::unique_ptr<SongRow>> rows;

    /** Song indices per release, and the rail's order those produce. */
    std::vector<std::vector<int>> groups;
    std::vector<int> displayOrder;

    // Per song
    Field titleField { "TITLE" };
    Field trackField { "TRACK", true };
    ValueStepper drumsDifficulty { "DRUMS" };
    ValueStepper proDrumsDifficulty { "PRO" };
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

    /** Artwork found beside the project, used to seed songs as they appear. */
    juce::File prefilledAlbumArt, prefilledBackground;

    CheckboxToggle renderAudioToggle { "Re-render audio" };
    SegmentedButtons formatButtons;
    SegmentedButtons packagingButtons;

    juce::TextButton exportButton { "Export" };
    juce::TextButton cancelButton { "Cancel" };
    juce::TextButton createRegionButton { "Create region" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExportDialogComponent)
};
