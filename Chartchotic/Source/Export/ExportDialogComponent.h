#pragma once

#include <JuceHeader.h>
#include <memory>
#include <vector>

#include "ChartExporter.h"
#include "../UI/Theme.h"
#include "../UI/Controls/CheckboxToggle.h"
#include "../UI/Controls/SegmentedButtons.h"
#include "../UI/Controls/ValueStepper.h"
#include "IconPreview.h"

/**
    The export dialog: everything that goes into a chart, on one screen, with
    the inferred values already filled in.

    It collects and validates only. Nothing here writes a file; pressing Export
    hands an ExportOptions back to the caller, which is what runs the export.
    That split keeps the writing testable without a UI and keeps this from
    needing to know what a .sng is.

    Lives as an overlay child of the editor rather than a DialogWindow, so it
    picks up the plugin's look and feel and never opens an OS window inside a
    host that may not want one.
*/
class ExportDialogComponent : public juce::Component
{
public:
    /** What the dialog needs to fill itself in. */
    struct Context
    {
        ChartExporter::TimeRange range;
        ChartExporter::ChartName inferred;
        juce::StringArray trackNames;      // chart tracks found in the project
        std::vector<ChartExporter::Sink> audioFormats;
        juce::File destinationRoot;
        /** Searched for album.* and background.* to pre-fill the artwork. */
        juce::Array<juce::File> artworkSearchPaths;
        /** Drum type read off the notes, which is the only place it exists. */
        ChartMidiWriter::DrumProfile drums;
        /** Per-project values from a previous open, so nothing is retyped. */
        juce::ValueTree remembered;
        /** Charter identity, which belongs to the machine rather than the song. */
        juce::String charter, icon;
    };

    explicit ExportDialogComponent(Context context);

    /** Fired with the collected options. The dialog is still on screen. */
    std::function<void(const ChartExporter::ExportOptions&)> onExport;
    std::function<void()> onDismiss;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void parentSizeChanged() override;

    /** Everything typed here, so reopening in this project starts where it left off. */
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

    ChartExporter::ExportOptions collect() const;
    void restore(const juce::ValueTree& tree);
    void refreshValidity();
    void prefillArtwork();
    juce::Rectangle<float> cardBounds() const;
    float scale() const;

    Context context;

    Field titleField { "TITLE" };
    Field artistField { "ARTIST" };
    Field albumField { "ALBUM" };
    Field genreField { "GENRE" };
    Field yearField { "YEAR", true };
    Field trackField { "TRACK", true };
    Field charterField { "CHARTER" };
    Field iconField { "ICON" };
    IconPreview iconPreview;
    juce::TextButton browseIconsButton { "Browse icons" };

    ValueStepper drumsDifficulty { "DIFF DRUMS" };
    ValueStepper proDrumsDifficulty { "DIFF PRO" };

    CheckboxToggle proDrumsToggle { "Pro drums" };
    CheckboxToggle fiveLaneToggle { "5-lane drums" };

    SegmentedButtons formatButtons;
    SegmentedButtons packagingButtons;

    ArtSlot albumArt { "ALBUM ART" };
    ArtSlot backgroundArt { "BACKGROUND" };
    CheckboxToggle generateBackgroundToggle { "Generate background from artwork" };

    juce::TextButton exportButton { "Export" };
    juce::TextButton cancelButton { "Cancel" };

    /** Unrated until typed, and Export stays disabled while either is. */
    int drumsRating = ChartExporter::ExportOptions::kUnrated;
    int proDrumsRating = ChartExporter::ExportOptions::kUnrated;
    bool hasDrums = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExportDialogComponent)
};
