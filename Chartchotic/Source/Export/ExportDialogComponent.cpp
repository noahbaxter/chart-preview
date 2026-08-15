#include "ExportDialogComponent.h"
#include "ChartSettings.h"

namespace
{
    // Chart difficulty ratings run 0 to 6. -1 is the unrated marker, and it is
    // deliberately not reachable by stepping: getting there by accident is how
    // a chart ships unrated.
    constexpr int kMinRating = 0;
    constexpr int kMaxRating = 6;

    const char* const kArtExtensions[] = { ".png", ".jpg", ".jpeg", nullptr };

    constexpr float kCardWidth = 660.0f;
    constexpr float kCardHeight = 600.0f;
    constexpr float kRowHeight = 30.0f;
    constexpr float kRowGap = 8.0f;
    constexpr float kPad = 26.0f;

    juce::String ratingText(int rating)
    {
        return rating == ChartExporter::ExportOptions::kUnrated ? "unrated" : juce::String(rating);
    }

    /** First album.png / background.jpg style file found under any search path. */
    juce::File findArt(const juce::Array<juce::File>& paths, const juce::String& stem)
    {
        for (const auto& dir : paths)
        {
            if (!dir.isDirectory()) continue;
            for (int i = 0; kArtExtensions[i] != nullptr; ++i)
            {
                auto candidate = dir.getChildFile(stem + kArtExtensions[i]);
                if (candidate.existsAsFile()) return candidate;
            }
        }
        return {};
    }
}

//==============================================================================
ExportDialogComponent::Field::Field(const juce::String& labelText, bool numeric)
    : label(labelText)
{
    editor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(Theme::darkBgLighter));
    editor.setColour(juce::TextEditor::outlineColourId, juce::Colours::white.withAlpha(Theme::borderAlpha));
    editor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(Theme::coral));
    editor.setColour(juce::TextEditor::textColourId, juce::Colour(Theme::textWhite));
    editor.setColour(juce::TextEditor::highlightColourId, juce::Colour(Theme::coral).withAlpha(0.3f));
    if (numeric)
        editor.setInputRestrictions(4, "0123456789");
    addAndMakeVisible(editor);
}

void ExportDialogComponent::Field::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(Theme::textDim));
    g.setFont(Theme::getUIFont((float)getHeight() * 0.42f));
    g.drawText(label, getLocalBounds().removeFromLeft(proportionOfWidth(0.33f)),
               juce::Justification::centredLeft);
}

void ExportDialogComponent::Field::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromLeft(proportionOfWidth(0.33f));
    editor.setBounds(bounds);
    editor.setFont(Theme::getUIFont((float)getHeight() * 0.46f));
}

//==============================================================================
ExportDialogComponent::ArtSlot::ArtSlot(const juce::String& labelText)
    : label(labelText)
{
    chooseButton.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::darkBgLighter));
    chooseButton.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::textDim));
    clearButton.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::darkBgLighter));
    clearButton.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::textDim));

    chooseButton.onClick = [this]()
    {
        chooser = std::make_unique<juce::FileChooser>("Choose " + label.toLowerCase(),
                                                      file.existsAsFile() ? file : juce::File(),
                                                      "*.png;*.jpg;*.jpeg");
        chooser->launchAsync(juce::FileBrowserComponent::openMode
                               | juce::FileBrowserComponent::canSelectFiles,
                             [this](const juce::FileChooser& fc)
                             {
                                 auto picked = fc.getResult();
                                 if (picked.existsAsFile()) setFile(picked);
                             });
    };

    clearButton.onClick = [this]() { setFile({}); };

    addAndMakeVisible(chooseButton);
    addAndMakeVisible(clearButton);
}

void ExportDialogComponent::ArtSlot::setFile(const juce::File& newFile)
{
    file = newFile;
    thumbnail = file.existsAsFile() ? juce::ImageFileFormat::loadFrom(file) : juce::Image();
    clearButton.setVisible(file.existsAsFile());
    repaint();
}

void ExportDialogComponent::ArtSlot::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    auto preview = bounds.removeFromLeft(getHeight()).reduced(1);

    g.setColour(juce::Colour(Theme::darkBgLighter));
    g.fillRect(preview);
    if (thumbnail.isValid())
        g.drawImage(thumbnail, preview.toFloat(), juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
    g.setColour(juce::Colours::white.withAlpha(Theme::borderAlpha));
    g.drawRect(preview);

    bounds.removeFromLeft(8);
    auto textArea = bounds.removeFromLeft(bounds.getWidth() - (int)(getHeight() * 2.6f));

    g.setColour(juce::Colour(Theme::textDim));
    g.setFont(Theme::getUIFont((float)getHeight() * 0.3f));
    g.drawText(label, textArea.removeFromTop(textArea.getHeight() / 2), juce::Justification::bottomLeft);

    g.setColour(juce::Colour(file.existsAsFile() ? Theme::textWhite : Theme::textDim));
    g.setFont(Theme::getUIFont((float)getHeight() * 0.28f));
    g.drawText(file.existsAsFile() ? file.getFileName() : juce::String("none"),
               textArea, juce::Justification::topLeft, true);
}

void ExportDialogComponent::ArtSlot::resized()
{
    auto bounds = getLocalBounds();
    auto buttons = bounds.removeFromRight((int)(getHeight() * 2.5f));
    clearButton.setBounds(buttons.removeFromRight(getHeight() / 2).reduced(1));
    chooseButton.setBounds(buttons.reduced(1));
}

//==============================================================================
ExportDialogComponent::ExportDialogComponent(Context ctx)
    : context(std::move(ctx))
{
    setInterceptsMouseClicks(true, true);
    setWantsKeyboardFocus(true);

    hasDrums = std::any_of(context.trackNames.begin(), context.trackNames.end(),
                           [](const juce::String& t) { return t.containsIgnoreCase("DRUM"); });

    titleField.setText(context.inferred.title);
    artistField.setText(context.inferred.artist);
    albumField.setText(context.inferred.album);
    genreField.setText(context.inferred.genre);
    yearField.setText(context.inferred.year);
    trackField.setText(context.inferred.track);

    // Charter identity comes from the machine, not the project, so it is typed
    // once ever rather than once per song. Handed in rather than read from
    // here, so what the dialog shows is decided in one place.
    charterField.setText(context.charter);
    iconField.setText(context.icon);

    for (auto* field : { &titleField, &artistField, &albumField, &genreField,
                         &yearField, &trackField, &charterField, &iconField })
    {
        field->editor.onTextChange = [this]() { refreshValidity(); repaint(); };
        addAndMakeVisible(*field);
    }

    auto stepRating = [this](int& rating, ValueStepper& stepper, int delta)
    {
        const int from = rating == ChartExporter::ExportOptions::kUnrated ? kMinRating - 1 : rating;
        rating = juce::jlimit(kMinRating, kMaxRating, from + delta);
        stepper.setDisplayValue(ratingText(rating));
        refreshValidity();
    };

    drumsDifficulty.setDisplayValue(ratingText(drumsRating));
    drumsDifficulty.onStep = [this, stepRating](int delta) { stepRating(drumsRating, drumsDifficulty, delta); };
    drumsDifficulty.onValueEdited = [this](const juce::String& typed)
    {
        drumsRating = juce::jlimit(kMinRating, kMaxRating, typed.getIntValue());
        drumsDifficulty.setDisplayValue(ratingText(drumsRating));
        refreshValidity();
    };

    proDrumsDifficulty.setDisplayValue(ratingText(proDrumsRating));
    proDrumsDifficulty.onStep = [this, stepRating](int delta) { stepRating(proDrumsRating, proDrumsDifficulty, delta); };
    proDrumsDifficulty.onValueEdited = [this](const juce::String& typed)
    {
        proDrumsRating = juce::jlimit(kMinRating, kMaxRating, typed.getIntValue());
        proDrumsDifficulty.setDisplayValue(ratingText(proDrumsRating));
        refreshValidity();
    };

    // Read off the notes rather than asked for. The track name cannot say
    // which kind of drum track this is, but tom markers and 5th-lane greens
    // can, so these come up already answered and are only an override.
    proDrumsToggle.setToggleState(context.drums.proDrums);
    fiveLaneToggle.setToggleState(context.drums.fiveLane);
    // The two are mutually exclusive: the format calls both being true an
    // invalid state, and a reader is free to pick either one.
    proDrumsToggle.onClick = [this]() { if (proDrumsToggle.getToggleState()) fiveLaneToggle.setToggleState(false); };
    fiveLaneToggle.onClick = [this]() { if (fiveLaneToggle.getToggleState()) proDrumsToggle.setToggleState(false); };

    // REAPER's sink descriptions are sentences ("MP3 (encoder by LAME
    // project)"), which do not fit a segmented button. The format name is the
    // part anyone is choosing between.
    auto shortFormatName = [](const juce::String& description)
    {
        for (const auto* name : { "Opus", "Vorbis", "MP3", "FLAC", "WAV" })
            if (description.containsIgnoreCase(name)) return juce::String(name);
        return description.upToFirstOccurrenceOf(" ", false, false);
    };

    juce::StringArray formatLabels;
    for (const auto& sink : context.audioFormats)
        formatLabels.add(shortFormatName(sink.description));
    formatButtons.setItems(formatLabels);
    // Opus first if it is there: smallest file the games all read.
    int opusIndex = 0;
    for (int i = 0; i < (int)context.audioFormats.size(); ++i)
        if (context.audioFormats[(size_t)i].description.containsIgnoreCase("opus")) opusIndex = i;
    formatButtons.setSelectedIndex(opusIndex);

    packagingButtons.setItems({ "Folder", ".sng" });
    packagingButtons.setSelectedIndex(ChartSettings::packAsSng() ? 1 : 0);
    packagingButtons.onSelectionChanged = [](int index) { ChartSettings::setPackAsSng(index == 1); };

    exportButton.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::coral).withAlpha(0.15f));
    exportButton.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::coral));
    exportButton.onClick = [this]()
    {
        if (onExport) onExport(collect());
    };

    cancelButton.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::darkBgLighter));
    cancelButton.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::textDim));
    cancelButton.onClick = [this]() { if (onDismiss) onDismiss(); };

    iconField.editor.onTextChange = [this]()
    {
        iconPreview.setIconName(iconField.text());
        refreshValidity();
        repaint();
    };
    iconPreview.setIconName(iconField.text());
    addAndMakeVisible(iconPreview);

    browseIconsButton.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::darkBgLighter));
    browseIconsButton.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::textDim));
    browseIconsButton.onClick = []() { IconPreview::browseUrl().launchInDefaultBrowser(); };
    addAndMakeVisible(browseIconsButton);

    addAndMakeVisible(drumsDifficulty);
    addAndMakeVisible(proDrumsDifficulty);
    addAndMakeVisible(proDrumsToggle);
    addAndMakeVisible(fiveLaneToggle);
    addAndMakeVisible(formatButtons);
    addAndMakeVisible(packagingButtons);
    addAndMakeVisible(albumArt);
    addAndMakeVisible(backgroundArt);

    generateBackgroundToggle.setToggleState(true);
    generateBackgroundToggle.onClick = [this]() { repaint(); };
    addAndMakeVisible(generateBackgroundToggle);
    addAndMakeVisible(exportButton);
    addAndMakeVisible(cancelButton);

    for (auto* stepper : { &drumsDifficulty, &proDrumsDifficulty })
        stepper->setVisible(hasDrums);
    proDrumsToggle.setVisible(hasDrums);
    fiveLaneToggle.setVisible(hasDrums);

    prefillArtwork();
    restore(context.remembered);
    refreshValidity();
}

namespace
{
    // One place for the property names, since they are written on close and
    // read on open and a typo in either would silently lose a field.
    const juce::Identifier kRemembered { "exportDialog" };
    const juce::Identifier kTitle { "title" }, kArtist { "artist" }, kAlbum { "album" };
    const juce::Identifier kGenre { "genre" }, kYear { "year" }, kTrack { "track" };
    const juce::Identifier kDiffDrums { "diffDrums" }, kDiffPro { "diffPro" };
    const juce::Identifier kProDrums { "proDrums" }, kFiveLane { "fiveLane" };
    const juce::Identifier kAlbumArt { "albumArt" }, kBackgroundArt { "backgroundArt" };
    const juce::Identifier kGenerateBackground { "generateBackground" };
}

juce::ValueTree ExportDialogComponent::toValueTree() const
{
    juce::ValueTree tree(kRemembered);
    tree.setProperty(kTitle, titleField.text(), nullptr);
    tree.setProperty(kArtist, artistField.text(), nullptr);
    tree.setProperty(kAlbum, albumField.text(), nullptr);
    tree.setProperty(kGenre, genreField.text(), nullptr);
    tree.setProperty(kYear, yearField.text(), nullptr);
    tree.setProperty(kTrack, trackField.text(), nullptr);
    tree.setProperty(kDiffDrums, drumsRating, nullptr);
    tree.setProperty(kDiffPro, proDrumsRating, nullptr);
    tree.setProperty(kProDrums, proDrumsToggle.getToggleState(), nullptr);
    tree.setProperty(kFiveLane, fiveLaneToggle.getToggleState(), nullptr);
    tree.setProperty(kAlbumArt, albumArt.file.getFullPathName(), nullptr);
    tree.setProperty(kBackgroundArt, backgroundArt.file.getFullPathName(), nullptr);
    tree.setProperty(kGenerateBackground, generateBackgroundToggle.getToggleState(), nullptr);
    return tree;
}

void ExportDialogComponent::restore(const juce::ValueTree& tree)
{
    if (!tree.isValid()) return;

    // Remembered values win over inferred ones: anything in here was typed by
    // hand, which is a stronger claim than anything read off a filename.
    auto put = [&tree](Field& field, const juce::Identifier& key)
    {
        auto value = tree.getProperty(key).toString();
        if (value.isNotEmpty()) field.setText(value);
    };

    put(titleField, kTitle);
    put(artistField, kArtist);
    put(albumField, kAlbum);
    put(genreField, kGenre);
    put(yearField, kYear);
    put(trackField, kTrack);

    if (tree.hasProperty(kDiffDrums))
    {
        drumsRating = (int)tree.getProperty(kDiffDrums);
        drumsDifficulty.setDisplayValue(ratingText(drumsRating));
    }
    if (tree.hasProperty(kDiffPro))
    {
        proDrumsRating = (int)tree.getProperty(kDiffPro);
        proDrumsDifficulty.setDisplayValue(ratingText(proDrumsRating));
    }

    // The drum flags are detected, so a remembered value only matters when it
    // disagreed, which is exactly the case worth keeping.
    if (tree.hasProperty(kProDrums)) proDrumsToggle.setToggleState((bool)tree.getProperty(kProDrums));
    if (tree.hasProperty(kFiveLane)) fiveLaneToggle.setToggleState((bool)tree.getProperty(kFiveLane));

    if (tree.hasProperty(kGenerateBackground))
        generateBackgroundToggle.setToggleState((bool)tree.getProperty(kGenerateBackground));

    // Art paths can go stale between sessions, so a file that has since moved
    // falls back to whatever prefill found rather than showing a dead path.
    juce::File album(tree.getProperty(kAlbumArt).toString());
    if (album.existsAsFile()) albumArt.setFile(album);
    juce::File background(tree.getProperty(kBackgroundArt).toString());
    if (background.existsAsFile()) backgroundArt.setFile(background);
}

void ExportDialogComponent::mouseDown(const juce::MouseEvent& event)
{
    // Clicks land here only when they missed every control, so anything
    // outside the card is a click on the scrim.
    if (!cardBounds().contains(event.position))
        if (onDismiss) onDismiss();
}

void ExportDialogComponent::parentSizeChanged()
{
    if (auto* parent = getParentComponent())
        setBounds(parent->getLocalBounds());
}

void ExportDialogComponent::prefillArtwork()
{
    albumArt.setFile(findArt(context.artworkSearchPaths, "album"));
    backgroundArt.setFile(findArt(context.artworkSearchPaths, "background"));
}

ChartExporter::ExportOptions ExportDialogComponent::collect() const
{
    ChartExporter::ExportOptions options;
    options.title = titleField.text();
    options.artist = artistField.text();
    options.album = albumField.text();
    options.genre = genreField.text();
    options.year = yearField.text();
    options.track = trackField.text();
    options.charter = charterField.text();
    options.icon = iconField.text();

    options.diffDrums = hasDrums ? drumsRating : ChartExporter::ExportOptions::kUnrated;
    options.diffDrumsReal = hasDrums ? proDrumsRating : ChartExporter::ExportOptions::kUnrated;
    options.proDrums = proDrumsToggle.getToggleState();
    options.fiveLaneDrums = fiveLaneToggle.getToggleState();

    options.albumArt = albumArt.file;
    options.backgroundArt = backgroundArt.file;
    options.generateBackground = generateBackgroundToggle.getToggleState();

    const int format = formatButtons.getSelectedIndex();
    if (juce::isPositiveAndBelow(format, (int)context.audioFormats.size()))
        options.audioFormatCode = context.audioFormats[(size_t)format].formatCode();

    options.packAsSng = packagingButtons.getSelectedIndex() == 1;
    options.destinationRoot = context.destinationRoot;

    // Built from the fields rather than copied from any input filename, so
    // what the preview shows is what lands on disk.
    ChartExporter::ChartName name;
    name.artist = options.artist;
    name.album = options.album;
    name.track = options.track;
    name.title = options.title;
    options.folderName = name.folderName();

    return options;
}

void ExportDialogComponent::refreshValidity()
{
    auto options = collect();
    // A chart with no rating and a chart with no name are both exports someone
    // has to redo, so neither is allowed to start.
    const bool rated = !hasDrums || options.rated();
    exportButton.setEnabled(options.nameable() && options.folderName.isNotEmpty() && rated);
}

float ExportDialogComponent::scale() const
{
    return juce::jlimit(0.65f, 1.4f, (float)getHeight() / 720.0f);
}

juce::Rectangle<float> ExportDialogComponent::cardBounds() const
{
    const float s = scale();
    return juce::Rectangle<float>(kCardWidth * s, kCardHeight * s)
             .withCentre(getLocalBounds().toFloat().getCentre());
}

void ExportDialogComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black.withAlpha(Theme::overlayDimAlpha));

    const float s = scale();
    auto card = cardBounds();

    g.setColour(juce::Colour(Theme::darkBg));
    g.fillRoundedRectangle(card, Theme::cardRadius);
    g.setColour(juce::Colours::white.withAlpha(Theme::borderAlpha));
    g.drawRoundedRectangle(card, Theme::cardRadius, 1.0f);

    auto inner = card.reduced(kPad * s);

    g.setColour(juce::Colour(Theme::coral));
    g.setFont(Theme::getUIFont(22.0f * s));
    g.drawText("Export Chart", inner.removeFromTop(30.0f * s), juce::Justification::topLeft);

    // The summary is what catches a wrong time selection before anyone waits
    // on a render, so it says what will actually be written.
    juce::String summary;
    summary << juce::String(context.range.length(), 2) << "s selected  ("
            << juce::String(context.range.startSec, 2) << "s to "
            << juce::String(context.range.endSec, 2) << "s)";
    if (!context.trackNames.isEmpty())
        summary << "   tracks: " << context.trackNames.joinIntoString(", ");
    else
        summary << "   NO CHART TRACKS FOUND";

    g.setColour(juce::Colour(Theme::textDim));
    g.setFont(Theme::getUIFont(12.0f * s));
    g.drawText(summary, inner.removeFromTop(22.0f * s), juce::Justification::topLeft, true);

    // Destination preview, bottom of the card above the buttons.
    auto options = collect();
    auto destination = options.destinationRoot.getChildFile(
        options.folderName + (options.packAsSng ? ".sng" : juce::String()));

    auto footer = card.reduced(kPad * s).removeFromBottom(58.0f * s);
    g.setColour(juce::Colour(Theme::textDim));
    g.setFont(Theme::getUIFont(11.0f * s));
    g.drawText(options.folderName.isEmpty() ? juce::String("needs a title and an artist")
                                            : destination.getFullPathName(),
               footer.removeFromTop(20.0f * s), juce::Justification::centredLeft, true);
}

void ExportDialogComponent::resized()
{
    const float s = scale();
    auto inner = cardBounds().reduced(kPad * s).toNearestInt();

    inner.removeFromTop((int)(58.0f * s));           // title and summary, painted
    auto buttons = inner.removeFromBottom((int)(34.0f * s));
    inner.removeFromBottom((int)(24.0f * s));        // destination preview, painted

    const int row = (int)(kRowHeight * s);
    const int gap = (int)(kRowGap * s);
    const int artRow = (int)(row * 1.4f);

    auto nextRow = [row, gap](juce::Rectangle<int>& area)
    {
        auto r = area.removeFromTop(row);
        area.removeFromTop(gap);
        return r;
    };

    // Artwork and the output choices are laid out from the bottom up, so they
    // stay put whatever is above them. Flowing them downwards means the card
    // getting shorter silently pushes the last row off the end.
    auto formatRow = inner.removeFromBottom(row);
    formatButtons.setBounds(formatRow.removeFromLeft((int)(formatRow.getWidth() * 0.62f)));
    formatRow.removeFromLeft(gap);
    packagingButtons.setBounds(formatRow);
    inner.removeFromBottom(gap * 2);

    generateBackgroundToggle.setBounds(inner.removeFromBottom(row));
    inner.removeFromBottom(gap);
    backgroundArt.setBounds(inner.removeFromBottom(artRow));
    inner.removeFromBottom(gap);
    albumArt.setBounds(inner.removeFromBottom(artRow));
    inner.removeFromBottom(gap * 2);

    auto columns = inner;
    auto left = columns.removeFromLeft(columns.getWidth() / 2 - gap);
    columns.removeFromLeft(gap * 2);
    auto right = columns;

    for (auto* field : { &titleField, &artistField, &albumField, &genreField, &yearField, &trackField })
        field->setBounds(nextRow(left));

    charterField.setBounds(nextRow(right));
    iconField.setBounds(nextRow(right));

    auto iconRow = nextRow(right);
    browseIconsButton.setBounds(iconRow.removeFromRight((int)(94.0f * s)));
    iconRow.removeFromRight(gap);
    iconPreview.setBounds(iconRow);
    drumsDifficulty.setBounds(nextRow(right));
    proDrumsDifficulty.setBounds(nextRow(right));

    auto toggles = nextRow(right);
    proDrumsToggle.setBounds(toggles.removeFromLeft(toggles.getWidth() / 2));
    fiveLaneToggle.setBounds(toggles);

    cancelButton.setBounds(buttons.removeFromRight((int)(100.0f * s)));
    buttons.removeFromRight(gap);
    exportButton.setBounds(buttons.removeFromRight((int)(120.0f * s)));
}

bool ExportDialogComponent::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onDismiss) onDismiss();
        return true;
    }
    return false;
}
