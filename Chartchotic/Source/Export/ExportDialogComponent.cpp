#include "ExportDialogComponent.h"

namespace
{
    // Chart difficulty ratings run 0 to 6. -1 is the unrated marker, and it is
    // deliberately not reachable by stepping: getting there by accident is how
    // a chart ships unrated.
    constexpr int kMinRating = 0;
    constexpr int kMaxRating = 6;

    const char* const kArtExtensions[] = { ".png", ".jpg", ".jpeg", nullptr };

    constexpr float kCardWidth = 940.0f;
    constexpr float kCardHeight = 600.0f;
    constexpr float kRowHeight = 28.0f;
    constexpr float kRowGap = 7.0f;
    constexpr float kPad = 24.0f;
    constexpr float kListWidth = 0.32f;

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

    // Album-wide values live on the root; each song hangs off it under its
    // region GUID, so renaming or dragging a region keeps its settings.
    const juce::Identifier kRemembered { "exportDialog" };
    const juce::Identifier kSong { "song" };
    const juce::Identifier kGuid { "guid" };
    const juce::Identifier kTitle { "title" }, kArtist { "artist" }, kAlbum { "album" };
    const juce::Identifier kGenre { "genre" }, kYear { "year" }, kTrack { "track" };
    const juce::Identifier kDiffDrums { "diffDrums" }, kDiffPro { "diffPro" };
    const juce::Identifier kProDrums { "proDrums" }, kFiveLane { "fiveLane" };
    const juce::Identifier kSelected { "selected" };
    const juce::Identifier kAlbumArt { "albumArt" }, kBackgroundArt { "backgroundArt" };
    const juce::Identifier kGenerateBackground { "generateBackground" };
    const juce::Identifier kBackgroundStyle { "backgroundStyle" };
    const juce::Identifier kPackAsSng { "packAsSng" }, kAudioFormat { "audioFormat" };
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
    g.drawText(label, getLocalBounds().removeFromLeft(proportionOfWidth(0.36f)),
               juce::Justification::centredLeft);
}

void ExportDialogComponent::Field::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromLeft(proportionOfWidth(0.36f));
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
        g.drawImage(thumbnail, preview.toFloat(),
                    juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
    g.setColour(juce::Colours::white.withAlpha(Theme::borderAlpha));
    g.drawRect(preview);

    bounds.removeFromLeft(8);
    auto textArea = bounds.removeFromLeft(juce::jmax(0, bounds.getWidth() - (int)(getHeight() * 2.6f)));

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
ExportDialogComponent::SongRow::SongRow()
{
    addAndMakeVisible(tick);
}

void ExportDialogComponent::SongRow::paint(juce::Graphics& g)
{
    if (current)
    {
        g.setColour(juce::Colour(Theme::darkBgLighter));
        g.fillRect(getLocalBounds());
    }

    auto bounds = getLocalBounds();
    bounds.removeFromLeft(getHeight());

    g.setColour(juce::Colour(current ? Theme::textWhite : Theme::textDim));
    g.setFont(Theme::getUIFont((float)getHeight() * 0.36f));
    g.drawText(name, bounds.removeFromTop(getHeight() * 3 / 5).reduced(4, 0),
               juce::Justification::bottomLeft, true);

    g.setColour(juce::Colour(ready ? Theme::green : Theme::coral));
    g.setFont(Theme::getUIFont((float)getHeight() * 0.28f));
    g.drawText(status, bounds.reduced(4, 0), juce::Justification::topLeft, true);
}

void ExportDialogComponent::SongRow::resized()
{
    tick.setBounds(getLocalBounds().removeFromLeft(getHeight()).reduced(5));
}

//==============================================================================
juce::String ExportDialogComponent::Song::blocker(bool albumNamed, bool drums) const
{
    if (title.isEmpty()) return "needs a title";
    if (track.isEmpty()) return "needs a track number";
    if (!albumNamed) return "needs album details";
    if (drums && (diffDrums == ChartExporter::ExportOptions::kUnrated
               || diffPro == ChartExporter::ExportOptions::kUnrated))
        return "needs a rating";
    return {};
}

//==============================================================================
ExportDialogComponent::ExportDialogComponent(Context ctx)
    : context(std::move(ctx))
{
    setInterceptsMouseClicks(true, true);
    setWantsKeyboardFocus(true);

    hasDrums = std::any_of(context.trackNames.begin(), context.trackNames.end(),
                           [](const juce::String& t) { return t.containsIgnoreCase("DRUM"); });

    artistField.setText(context.inferred.artist);
    albumField.setText(context.inferred.album);
    genreField.setText(context.inferred.genre);
    yearField.setText(context.inferred.year);

    // Charter identity comes from the machine, not the project, so it is typed
    // once ever rather than once per song.
    charterField.setText(context.charter);
    iconField.setText(context.icon);

    for (auto* field : { &titleField, &trackField, &artistField, &albumField,
                         &genreField, &yearField, &charterField, &iconField })
    {
        field->editor.onTextChange = [this]() { storeSong(current); refreshRows(); };
        addAndMakeVisible(*field);
    }

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

    auto stepped = [](int rating, int delta)
    {
        const int from = rating == ChartExporter::ExportOptions::kUnrated ? kMinRating - 1 : rating;
        return juce::jlimit(kMinRating, kMaxRating, from + delta);
    };

    drumsDifficulty.onStep = [this, stepped](int delta)
    {
        if (songs.empty()) return;
        auto& song = songs[(size_t)current];
        song.diffDrums = stepped(song.diffDrums, delta);
        drumsDifficulty.setDisplayValue(ratingText(song.diffDrums));
        refreshRows();
    };
    proDrumsDifficulty.onStep = [this, stepped](int delta)
    {
        if (songs.empty()) return;
        auto& song = songs[(size_t)current];
        song.diffPro = stepped(song.diffPro, delta);
        proDrumsDifficulty.setDisplayValue(ratingText(song.diffPro));
        refreshRows();
    };

    proDrumsToggle.onClick = [this]()
    {
        // Mutually exclusive: the format calls both being true invalid.
        if (proDrumsToggle.getToggleState()) fiveLaneToggle.setToggleState(false);
        storeSong(current);
    };
    fiveLaneToggle.onClick = [this]()
    {
        if (fiveLaneToggle.getToggleState()) proDrumsToggle.setToggleState(false);
        storeSong(current);
    };

    // REAPER's sink descriptions are sentences ("MP3 (encoder by LAME
    // project)"), which do not fit a segmented button.
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

    int opusIndex = 0;
    for (int i = 0; i < (int)context.audioFormats.size(); ++i)
        if (context.audioFormats[(size_t)i].description.containsIgnoreCase("opus")) opusIndex = i;
    formatButtons.setSelectedIndex(opusIndex);

    packagingButtons.setItems({ "Folder", ".sng" });
    packagingButtons.setSelectedIndex(0);

    renderAudioToggle.setToggleState(true);
    renderAudioToggle.onClick = [this]()
    {
        formatButtons.setEnabled(renderAudioToggle.getToggleState());
        repaint();
    };

    generateBackgroundToggle.setToggleState(true);
    generateBackgroundToggle.onClick = [this]()
    {
        backgroundStyleButtons.setEnabled(generateBackgroundToggle.getToggleState());
        repaint();
    };

    backgroundStyleButtons.setItems(BackgroundGenerator::styleNames());
    backgroundStyleButtons.setSelectedIndex(0);

    exportButton.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::coral).withAlpha(0.15f));
    exportButton.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::coral));
    exportButton.onClick = [this]()
    {
        storeSong(current);

        // Ticked rows that are not ready are skipped rather than blocking the
        // rest: ten finished charts should not wait on the eleventh.
        std::vector<ChartExporter::SongExport> batch;
        for (const auto& song : songs)
        {
            if (!song.selected) continue;
            if (song.blocker(albumBlocker().isEmpty(), hasDrums).isNotEmpty()) continue;
            batch.push_back({ song.region.range(), optionsFor(song), song.region.guid });
        }

        if (onExport && !batch.empty()) onExport(batch);
    };

    cancelButton.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::darkBgLighter));
    cancelButton.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::textDim));
    cancelButton.onClick = [this]() { if (onDismiss) onDismiss(); };

    listView.setViewedComponent(&listContent, false);
    listView.setScrollBarsShown(true, false);
    addAndMakeVisible(listView);

    const std::initializer_list<juce::Component*> everything {
        &drumsDifficulty, &proDrumsDifficulty, &proDrumsToggle, &fiveLaneToggle,
        &albumArt, &backgroundArt, &generateBackgroundToggle, &backgroundStyleButtons,
        &renderAudioToggle, &formatButtons, &packagingButtons, &exportButton, &cancelButton
    };
    for (auto* component : everything)
        addAndMakeVisible(*component);

    // A project with no drum track has nothing to rate or flag.
    const std::initializer_list<juce::Component*> drumOnly {
        &drumsDifficulty, &proDrumsDifficulty, &proDrumsToggle, &fiveLaneToggle
    };
    for (auto* component : drumOnly)
        component->setVisible(hasDrums);

    buildSongs();
    prefillArtwork();
    restore(context.remembered);
    loadSong(0);
    refreshRows();
}

void ExportDialogComponent::buildSongs()
{
    songs.clear();

    for (const auto& region : context.regions)
    {
        Song song;
        song.region = region;
        // The region name is a default, not a convention: it is the most
        // likely title and nothing downstream depends on its shape.
        song.title = region.name.trim();
        song.proDrums = context.drums.proDrums;
        song.fiveLane = context.drums.fiveLane;
        songs.push_back(song);
    }

    // No regions means the old behaviour: whatever is selected on the
    // timeline, as one song, ticked because it is the only thing there.
    if (songs.empty() && context.selection.plausible())
    {
        Song song;
        song.region.name = "time selection";
        song.region.startSec = context.selection.startSec;
        song.region.endSec = context.selection.endSec;
        song.title = context.inferred.title;
        song.track = context.inferred.track;
        song.selected = true;
        song.proDrums = context.drums.proDrums;
        song.fiveLane = context.drums.fiveLane;
        songs.push_back(song);
    }

    // Track numbers default to timeline order, which is album order whenever
    // the project is laid out the way the album plays.
    int number = 1;
    for (auto& song : songs)
        if (song.track.isEmpty())
            song.track = juce::String(number++).paddedLeft('0', 2);

    rows.clear();
    listContent.removeAllChildren();
    for (size_t i = 0; i < songs.size(); ++i)
    {
        auto row = std::make_unique<SongRow>();
        row->onSelect = [this, i]()
        {
            storeSong(current);
            current = (int)i;
            loadSong(current);
            refreshRows();
        };
        row->tick.onClick = [this, i]()
        {
            songs[i].selected = rows[i]->tick.getToggleState();
            refreshValidity();
            repaint();
        };
        listContent.addAndMakeVisible(*row);
        rows.push_back(std::move(row));
    }
}

void ExportDialogComponent::loadSong(int index)
{
    if (!juce::isPositiveAndBelow(index, (int)songs.size())) return;
    const auto& song = songs[(size_t)index];

    titleField.setText(song.title);
    trackField.setText(song.track);
    drumsDifficulty.setDisplayValue(ratingText(song.diffDrums));
    proDrumsDifficulty.setDisplayValue(ratingText(song.diffPro));
    proDrumsToggle.setToggleState(song.proDrums);
    fiveLaneToggle.setToggleState(song.fiveLane);
}

void ExportDialogComponent::storeSong(int index)
{
    if (!juce::isPositiveAndBelow(index, (int)songs.size())) return;
    auto& song = songs[(size_t)index];

    song.title = titleField.text();
    song.track = trackField.text();
    song.proDrums = proDrumsToggle.getToggleState();
    song.fiveLane = fiveLaneToggle.getToggleState();
}

void ExportDialogComponent::refreshRows()
{
    const bool named = albumBlocker().isEmpty();
    for (size_t i = 0; i < rows.size() && i < songs.size(); ++i)
    {
        auto& song = songs[i];
        auto blocked = song.blocker(named, hasDrums);

        rows[i]->name = song.title.isNotEmpty() ? song.title : song.region.name;
        rows[i]->status = blocked.isEmpty() ? "ready" : blocked;
        rows[i]->ready = blocked.isEmpty();
        rows[i]->current = ((int)i == current);
        rows[i]->tick.setToggleState(song.selected);
        rows[i]->repaint();
    }

    refreshValidity();
    repaint();
}

juce::String ExportDialogComponent::albumBlocker() const
{
    if (artistField.text().isEmpty()) return "needs an artist";
    if (albumField.text().isEmpty()) return "needs an album";
    if (charterField.text().isEmpty()) return "needs a charter";
    return {};
}

void ExportDialogComponent::refreshValidity()
{
    const bool named = albumBlocker().isEmpty();
    const int ready = (int)std::count_if(songs.begin(), songs.end(),
                                         [this, named](const Song& song)
                                         { return song.selected && song.blocker(named, hasDrums).isEmpty(); });
    exportButton.setEnabled(ready > 0);
}

void ExportDialogComponent::prefillArtwork()
{
    albumArt.setFile(findArt(context.artworkSearchPaths, "album"));
    backgroundArt.setFile(findArt(context.artworkSearchPaths, "background"));
}

ChartExporter::ExportOptions ExportDialogComponent::optionsFor(const Song& song) const
{
    ChartExporter::ExportOptions options;
    options.title = song.title;
    options.track = song.track;
    options.artist = artistField.text();
    options.album = albumField.text();
    options.genre = genreField.text();
    options.year = yearField.text();
    options.charter = charterField.text();
    options.icon = iconField.text();

    options.diffDrums = hasDrums ? song.diffDrums : ChartExporter::ExportOptions::kUnrated;
    options.diffDrumsReal = hasDrums ? song.diffPro : ChartExporter::ExportOptions::kUnrated;
    options.proDrums = song.proDrums;
    options.fiveLaneDrums = song.fiveLane;

    options.albumArt = albumArt.file;
    options.backgroundArt = backgroundArt.file;
    options.generateBackground = generateBackgroundToggle.getToggleState();
    options.backgroundOptions.style =
        (BackgroundGenerator::Style)juce::jmax(0, backgroundStyleButtons.getSelectedIndex());

    const int format = formatButtons.getSelectedIndex();
    if (juce::isPositiveAndBelow(format, (int)context.audioFormats.size()))
        options.audioFormatCode = context.audioFormats[(size_t)format].formatCode();

    options.packAsSng = packagingButtons.getSelectedIndex() == 1;
    options.destinationRoot = context.destinationRoot;

    ChartExporter::ChartName name;
    name.artist = options.artist;
    name.album = options.album;
    name.track = options.track;
    name.title = options.title;
    options.folderName = name.folderName();

    // A chart with no audio yet always needs a render, whatever the toggle
    // says: the toggle is about skipping work already done, not about
    // shipping a chart with nothing to play.
    const auto existing = ChartExporter::existingAudio(
        context.destinationRoot.getChildFile(options.folderName));
    options.renderAudio = renderAudioToggle.getToggleState() || !existing.existsAsFile();

    return options;
}

//==============================================================================
juce::ValueTree ExportDialogComponent::toValueTree() const
{
    juce::ValueTree tree(kRemembered);
    tree.setProperty(kArtist, artistField.text(), nullptr);
    tree.setProperty(kAlbum, albumField.text(), nullptr);
    tree.setProperty(kGenre, genreField.text(), nullptr);
    tree.setProperty(kYear, yearField.text(), nullptr);
    tree.setProperty(kAlbumArt, albumArt.file.getFullPathName(), nullptr);
    tree.setProperty(kBackgroundArt, backgroundArt.file.getFullPathName(), nullptr);
    tree.setProperty(kGenerateBackground, generateBackgroundToggle.getToggleState(), nullptr);
    tree.setProperty(kBackgroundStyle, backgroundStyleButtons.getSelectedIndex(), nullptr);
    tree.setProperty(kPackAsSng, packagingButtons.getSelectedIndex() == 1, nullptr);
    tree.setProperty(kAudioFormat, formatButtons.getSelectedIndex(), nullptr);

    for (const auto& song : songs)
    {
        if (song.region.guid.isEmpty()) continue;

        juce::ValueTree child(kSong);
        child.setProperty(kGuid, song.region.guid, nullptr);
        child.setProperty(kTitle, song.title, nullptr);
        child.setProperty(kTrack, song.track, nullptr);
        child.setProperty(kDiffDrums, song.diffDrums, nullptr);
        child.setProperty(kDiffPro, song.diffPro, nullptr);
        child.setProperty(kProDrums, song.proDrums, nullptr);
        child.setProperty(kFiveLane, song.fiveLane, nullptr);
        child.setProperty(kSelected, song.selected, nullptr);
        tree.appendChild(child, nullptr);
    }

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

    put(artistField, kArtist);
    put(albumField, kAlbum);
    put(genreField, kGenre);
    put(yearField, kYear);

    if (tree.hasProperty(kGenerateBackground))
        generateBackgroundToggle.setToggleState((bool)tree.getProperty(kGenerateBackground));
    if (tree.hasProperty(kBackgroundStyle))
        backgroundStyleButtons.setSelectedIndex((int)tree.getProperty(kBackgroundStyle));
    if (tree.hasProperty(kPackAsSng))
        packagingButtons.setSelectedIndex((bool)tree.getProperty(kPackAsSng) ? 1 : 0);
    if (tree.hasProperty(kAudioFormat))
        formatButtons.setSelectedIndex((int)tree.getProperty(kAudioFormat));

    // Art paths can go stale between sessions, so a file that has since moved
    // falls back to whatever prefill found rather than showing a dead path.
    juce::File album(tree.getProperty(kAlbumArt).toString());
    if (album.existsAsFile()) albumArt.setFile(album);
    juce::File background(tree.getProperty(kBackgroundArt).toString());
    if (background.existsAsFile()) backgroundArt.setFile(background);

    for (auto& song : songs)
    {
        auto child = tree.getChildWithProperty(kGuid, song.region.guid);
        if (!child.isValid()) continue;

        auto title = child.getProperty(kTitle).toString();
        if (title.isNotEmpty()) song.title = title;
        auto track = child.getProperty(kTrack).toString();
        if (track.isNotEmpty()) song.track = track;

        if (child.hasProperty(kDiffDrums)) song.diffDrums = (int)child.getProperty(kDiffDrums);
        if (child.hasProperty(kDiffPro))   song.diffPro = (int)child.getProperty(kDiffPro);
        if (child.hasProperty(kProDrums))  song.proDrums = (bool)child.getProperty(kProDrums);
        if (child.hasProperty(kFiveLane))  song.fiveLane = (bool)child.getProperty(kFiveLane);
        if (child.hasProperty(kSelected))  song.selected = (bool)child.getProperty(kSelected);
    }
}

//==============================================================================
float ExportDialogComponent::scale() const
{
    return juce::jlimit(0.6f, 1.4f, (float)getHeight() / 800.0f);
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
    g.drawText("Export Chart", inner.removeFromTop(28.0f * s), juce::Justification::topLeft);

    const bool named = albumBlocker().isEmpty();
    const int ready = (int)std::count_if(songs.begin(), songs.end(),
                                         [this, named](const Song& song)
                                         { return song.selected && song.blocker(named, hasDrums).isEmpty(); });

    juce::String summary;
    summary << (int)songs.size() << (songs.size() == 1 ? " song" : " songs")
            << ", " << ready << " ready";
    if (!context.trackNames.isEmpty())
        summary << "   tracks: " << context.trackNames.joinIntoString(", ");
    else
        summary << "   NO CHART TRACKS FOUND";

    g.setColour(juce::Colour(Theme::textDim));
    g.setFont(Theme::getUIFont(12.0f * s));
    g.drawText(summary, inner.removeFromTop(20.0f * s), juce::Justification::topLeft, true);

    auto footer = card.reduced(kPad * s).removeFromBottom(54.0f * s);
    auto blocked = albumBlocker();
    g.setColour(juce::Colour(blocked.isNotEmpty() ? Theme::coral : Theme::textDim));
    g.setFont(Theme::getUIFont(11.0f * s));
    g.drawText(blocked.isNotEmpty() ? "album " + blocked
                                    : context.destinationRoot.getFullPathName(),
               footer.removeFromTop(18.0f * s), juce::Justification::centredLeft, true);
}

void ExportDialogComponent::resized()
{
    const float s = scale();
    auto inner = cardBounds().reduced(kPad * s).toNearestInt();

    inner.removeFromTop((int)(52.0f * s));           // title and summary, painted
    auto buttons = inner.removeFromBottom((int)(32.0f * s));
    inner.removeFromBottom((int)(20.0f * s));        // destination, painted

    const int row = (int)(kRowHeight * s);
    const int gap = (int)(kRowGap * s);
    const int artRow = (int)(row * 1.4f);

    auto nextRow = [row, gap](juce::Rectangle<int>& area)
    {
        auto r = area.removeFromTop(row);
        area.removeFromTop(gap);
        return r;
    };

    // Laid out from the bottom up so the rows that have to stay visible do,
    // whatever the card height works out to.
    auto formatRow = inner.removeFromBottom(row);
    formatButtons.setBounds(formatRow.removeFromLeft((int)(formatRow.getWidth() * 0.55f)));
    formatRow.removeFromLeft(gap);
    packagingButtons.setBounds(formatRow);
    inner.removeFromBottom(gap);

    renderAudioToggle.setBounds(inner.removeFromBottom(row));
    inner.removeFromBottom(gap);

    auto backgroundRow = inner.removeFromBottom(row);
    backgroundStyleButtons.setBounds(backgroundRow.removeFromRight((int)(170.0f * s)));
    backgroundRow.removeFromRight(gap);
    generateBackgroundToggle.setBounds(backgroundRow);
    inner.removeFromBottom(gap);

    backgroundArt.setBounds(inner.removeFromBottom(artRow));
    inner.removeFromBottom(gap);
    albumArt.setBounds(inner.removeFromBottom(artRow));
    inner.removeFromBottom(gap * 2);

    auto list = inner.removeFromLeft((int)(inner.getWidth() * kListWidth));
    listView.setBounds(list);

    const int listRow = (int)(row * 1.6f);
    listContent.setSize(juce::jmax(1, list.getWidth() - 12), juce::jmax(1, (int)rows.size() * listRow));
    for (size_t i = 0; i < rows.size(); ++i)
        rows[i]->setBounds(0, (int)i * listRow, listContent.getWidth(), listRow);

    inner.removeFromLeft(gap * 2);

    auto columns = inner;
    auto left = columns.removeFromLeft(columns.getWidth() / 2 - gap);
    columns.removeFromLeft(gap * 2);
    auto right = columns;

    // Per song on the left, album wide on the right.
    titleField.setBounds(nextRow(left));
    trackField.setBounds(nextRow(left));
    drumsDifficulty.setBounds(nextRow(left));
    proDrumsDifficulty.setBounds(nextRow(left));
    auto toggles = nextRow(left);
    proDrumsToggle.setBounds(toggles.removeFromLeft(toggles.getWidth() / 2));
    fiveLaneToggle.setBounds(toggles);

    artistField.setBounds(nextRow(right));
    albumField.setBounds(nextRow(right));
    genreField.setBounds(nextRow(right));
    yearField.setBounds(nextRow(right));
    charterField.setBounds(nextRow(right));
    iconField.setBounds(nextRow(right));

    auto iconRow = nextRow(right);
    browseIconsButton.setBounds(iconRow.removeFromRight((int)(90.0f * s)));
    iconRow.removeFromRight(gap);
    iconPreview.setBounds(iconRow);

    cancelButton.setBounds(buttons.removeFromRight((int)(96.0f * s)));
    buttons.removeFromRight(gap);
    exportButton.setBounds(buttons.removeFromRight((int)(116.0f * s)));
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
