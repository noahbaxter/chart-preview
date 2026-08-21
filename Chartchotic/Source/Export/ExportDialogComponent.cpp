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

    constexpr int kRowHeight = 28;
    constexpr int kRowGap = 7;
    constexpr int kPad = 22;

    /**
        One label gutter for every field in the window, in pixels rather than a
        share of each control's width. This is the whole of what makes the two
        panels line up.
    */
    constexpr int kLabelWidth = 66;

    /** The song rail. Wide enough for a long title, and it does not grow much. */
    constexpr int kRailMin = 240;
    constexpr int kRailMax = 300;
    constexpr float kRailShare = 0.26f;

    constexpr int kSectionGap = 15;
    constexpr int kHeadingHeight = 15;

    /** Big enough to tell one cover from another at a glance. */
    constexpr int kArtTile = 84;

    /** The tick column. Sized to the box, not to the row, so titles get the rest. */
    constexpr int kTickColumn = 26;

    /** How long ago, in the roughest terms that are still useful. */
    juce::String exportedWhen(const juce::File& file)
    {
        const auto age = juce::Time::getCurrentTime() - file.getLastModificationTime();
        if (age.inMinutes() < 1)  return "just now";
        if (age.inHours() < 1)    return juce::String((int)age.inMinutes()) + "m ago";
        if (age.inDays() < 1)     return juce::String((int)age.inHours()) + "h ago";
        return juce::String((int)age.inDays()) + "d ago";
    }

    /**
        Where generated backgrounds wait until the chart is written.

        The exporter copies artwork into the chart folder, so these are
        staging and nothing else. The dialog clears the folder on the way in
        and on the way out: they are cheap to remake and leaving them behind
        litters the temp directory with every background ever previewed.
    */
    juce::File backgroundStagingFolder()
    {
        return juce::File::getSpecialLocation(juce::File::tempDirectory)
                 .getChildFile("chartchotic-backgrounds");
    }

    /** Where a song starts, in the form the REAPER ruler shows it. */
    juce::String timecodeOf(double seconds)
    {
        const int whole = (int)std::max(0.0, seconds);
        return juce::String(whole / 60) + ":" + juce::String(whole % 60).paddedLeft('0', 2);
    }

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
    const juce::Identifier kPackAsSng { "packAsSng" }, kAudioFormat { "audioFormat" };

    /**
        Identity for the song that is a bare time selection rather than a
        region. It needs one: without a key nothing about it is saved, so the
        difficulty you just typed is gone the moment the window closes.
    */
    const juce::String kSelectionKey { "selection" };
}

//==============================================================================
void ExportDialogComponent::MixedTextEditor::paintOverChildren(juce::Graphics& g)
{
    if (!mixed || !isEmpty()) return;

    g.setColour(juce::Colour(Theme::textDim));
    g.setFont(Theme::getMixedFont(getFont().getHeight()));
    g.drawText(Theme::mixedText, getLocalBounds().reduced(5, 0),
               juce::Justification::centredLeft);
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
    g.setFont(Theme::getUIFont((float)getHeight() * 0.40f));
    g.drawText(label, getLocalBounds().removeFromLeft(labelWidth),
               juce::Justification::centredLeft);
}

void ExportDialogComponent::Field::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromLeft(labelWidth);
    editor.setBounds(bounds);
    editor.setFont(Theme::getUIFont((float)getHeight() * 0.46f));
}

//==============================================================================
ExportDialogComponent::ArtSlot::ArtSlot(const juce::String& labelText)
    : label(labelText)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

juce::Rectangle<int> ExportDialogComponent::ArtSlot::previewBounds() const
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(kHeadingHeight);
    bounds.removeFromBottom(14);          // filename
    return bounds;
}

juce::Rectangle<int> ExportDialogComponent::ArtSlot::clearBounds() const
{
    return previewBounds().removeFromTop(22).removeFromRight(22).reduced(3);
}

juce::Rectangle<int> ExportDialogComponent::ArtSlot::optionsBounds() const
{
    return previewBounds().removeFromTop(22).removeFromLeft(22).reduced(3);
}

void ExportDialogComponent::ArtSlot::mouseMove(const juce::MouseEvent& e)
{
    const bool over = file.existsAsFile() && clearBounds().contains(e.getPosition());
    if (over == overClear) return;
    overClear = over;
    repaint();
}

void ExportDialogComponent::ArtSlot::mouseDown(const juce::MouseEvent& e)
{
    if (!previewBounds().contains(e.getPosition())) return;

    if (hasOptions && optionsBounds().contains(e.getPosition()))
    {
        if (onOptions) onOptions();
        return;
    }

    if (file.existsAsFile() && clearBounds().contains(e.getPosition()))
    {
        setFile({});
        if (onFileChanged) onFileChanged({});
        return;
    }

    chooser = std::make_unique<juce::FileChooser>("Choose " + label.toLowerCase(),
                                                  file.existsAsFile() ? file : juce::File(),
                                                  "*.png;*.jpg;*.jpeg");
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                           | juce::FileBrowserComponent::canSelectFiles,
                         [this](const juce::FileChooser& fc)
                         {
                             auto picked = fc.getResult();
                             if (!picked.existsAsFile()) return;
                             setFile(picked);
                             if (onFileChanged) onFileChanged(picked);
                         });
}

void ExportDialogComponent::ArtSlot::setFile(const juce::File& newFile)
{
    file = newFile;
    thumbnail = file.existsAsFile() ? juce::ImageFileFormat::loadFrom(file) : juce::Image();
    repaint();
}

void ExportDialogComponent::ArtSlot::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(Theme::textDim));
    g.setFont(Theme::getUIFont(10.0f));
    g.drawText(label, getLocalBounds().removeFromTop(kHeadingHeight),
               juce::Justification::centredLeft);

    auto preview = previewBounds();

    g.setColour(juce::Colour(Theme::darkBgLighter));
    g.fillRect(preview);
    if (thumbnail.isValid())
    {
        g.drawImage(thumbnail, preview.toFloat(),
                    juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
    }
    else if (mixed)
    {
        g.setColour(juce::Colour(Theme::textDim));
        g.setFont(Theme::getMixedFont(10.0f));
        g.drawText(Theme::mixedText, preview, juce::Justification::centred);
    }
    else
    {
        // The picture is the control, so an empty one says what to do with it.
        g.setColour(juce::Colour(Theme::textDim).withAlpha(hovering ? 0.9f : 0.5f));
        g.setFont(Theme::getUIFont(10.0f));
        g.drawText("click to choose", preview, juce::Justification::centred);
    }

    g.setColour(hovering ? juce::Colour(Theme::coral).withAlpha(0.7f)
                         : juce::Colours::white.withAlpha(Theme::borderAlpha));
    g.drawRect(preview);

    // Clearing is a hover affordance rather than a button competing with the
    // picture for space.
    if (hovering && file.existsAsFile())
    {
        auto badge = clearBounds();
        g.setColour(juce::Colour(Theme::darkBg).withAlpha(0.85f));
        g.fillRoundedRectangle(badge.toFloat(), 2.0f);
        g.setColour(juce::Colour(overClear ? Theme::coral : Theme::textDim));
        g.drawRoundedRectangle(badge.toFloat(), 2.0f, 1.0f);

        const auto cross = badge.reduced(4).toFloat();
        g.drawLine(cross.getX(), cross.getY(), cross.getRight(), cross.getBottom(), 1.2f);
        g.drawLine(cross.getX(), cross.getBottom(), cross.getRight(), cross.getY(), 1.2f);
    }

    // The gear lives opposite the clear badge, so the two never collide.
    if (hasOptions && hovering)
    {
        auto badge = optionsBounds();
        g.setColour(juce::Colour(Theme::darkBg).withAlpha(0.85f));
        g.fillRoundedRectangle(badge.toFloat(), 2.0f);
        g.setColour(juce::Colour(Theme::textDim));
        g.drawRoundedRectangle(badge.toFloat(), 2.0f, 1.0f);

        const auto centre = badge.toFloat().getCentre();
        const float outer = badge.getWidth() * 0.30f;
        for (int tooth = 0; tooth < 6; ++tooth)
        {
            const float angle = juce::MathConstants<float>::twoPi * (float)tooth / 6.0f;
            g.drawLine(centre.x + std::cos(angle) * outer * 0.55f,
                       centre.y + std::sin(angle) * outer * 0.55f,
                       centre.x + std::cos(angle) * outer,
                       centre.y + std::sin(angle) * outer, 1.2f);
        }
        g.drawEllipse(centre.x - outer * 0.45f, centre.y - outer * 0.45f,
                      outer * 0.9f, outer * 0.9f, 1.2f);
    }

    if (file.existsAsFile())
    {
        auto footer = getLocalBounds().removeFromBottom(14);

        g.setColour(juce::Colour(Theme::textDim));
        g.setFont(Theme::getUIFont(9.5f));
        if (thumbnail.isValid())
        {
            auto size = juce::String(thumbnail.getWidth()) + "\xc3\x97"
                      + juce::String(thumbnail.getHeight());
            g.drawText(size, footer.removeFromRight(70), juce::Justification::centredRight);
        }

        g.setColour(juce::Colour(Theme::textWhite));
        g.setFont(Theme::getUIFont(10.0f));
        g.drawText(caption.isNotEmpty() ? caption : file.getFileName(), footer,
                   juce::Justification::centredLeft, true);
    }
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

        // Coral edge: reads at a glance where the lighter fill alone does not.
        g.setColour(juce::Colour(Theme::coral));
        g.fillRect(getLocalBounds().removeFromLeft(2));
    }

    auto bounds = getLocalBounds();
    bounds.removeFromLeft(kTickColumn);

    auto numberArea = bounds.removeFromLeft((int)(getHeight() * 0.52f));
    g.setColour(juce::Colour(current ? Theme::coral : Theme::textDim).withAlpha(current ? 1.0f : 0.6f));
    g.setFont(Theme::getUIFont((float)getHeight() * 0.30f));
    g.drawText(number, numberArea, juce::Justification::centredLeft);

    auto top = bounds.removeFromTop(getHeight() * 3 / 5).reduced(2, 0);

    // The timecode is what tells two untitled rows apart, so it is on the
    // title line rather than tucked under it.
    g.setColour(juce::Colour(Theme::textDim).withAlpha(0.7f));
    g.setFont(Theme::getUIFont((float)getHeight() * 0.28f));
    auto timeArea = top.removeFromRight(
        juce::jmin(top.getWidth() / 2,
                   (int)Theme::getUIFont((float)getHeight() * 0.28f).getStringWidthFloat(timecode) + 6));
    g.drawText(timecode, timeArea, juce::Justification::bottomRight);

    g.setColour(juce::Colour(untitled ? Theme::textDim
                                      : (current ? Theme::textWhite : Theme::textDim))
                    .withMultipliedAlpha(untitled ? 0.6f : 1.0f));
    g.setFont(untitled ? Theme::getMixedFont((float)getHeight() * 0.36f)
                       : Theme::getUIFont((float)getHeight() * 0.36f));
    g.drawText(name, top, juce::Justification::bottomLeft, true);

    auto lower = bounds.reduced(2, 0);

    g.setColour(juce::Colour(Theme::textDim).withAlpha(0.6f));
    g.setFont(Theme::getUIFont((float)getHeight() * 0.26f));
    auto sourceArea = lower.removeFromRight(
        juce::jmin(lower.getWidth() / 2,
                   (int)Theme::getUIFont((float)getHeight() * 0.26f).getStringWidthFloat(source) + 6));
    g.drawText(source, sourceArea, juce::Justification::topRight, true);

    g.setColour(juce::Colour(!ready ? Theme::coral : exported ? Theme::textDim : Theme::green));
    g.setFont(Theme::getUIFont((float)getHeight() * 0.28f));
    g.drawText(status, lower, juce::Justification::topLeft, true);
}

void ExportDialogComponent::SongRow::resized()
{
    tick.setBounds(getLocalBounds().removeFromLeft(kTickColumn).reduced(4));
}

//==============================================================================
juce::String ExportDialogComponent::Song::blocker(bool charterNamed, bool drums) const
{
    // Asked of the song rather than of the window, so a session can hold one
    // finished record and one half-typed single and still export the record.
    if (title.isEmpty()) return "needs a title";
    if (artist.isEmpty()) return "needs an artist";
    if (album.isEmpty()) return "needs an album";
    if (track.isEmpty()) return "needs a track number";
    if (!charterNamed) return "needs a charter";
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

    // Charter identity comes from the machine, not the project, so it is typed
    // once ever rather than once per song.
    charterField.setText(context.charter);
    iconField.setText(context.icon);

    // Typing resolves a mixed field, for everything selected. Each field
    // writes only its own member, leaving the others alone.
    auto edits = [this](Field& field, juce::String Song::*member)
    {
        field.editor.onTextChange = [this, &field, member]()
        {
            auto value = field.text();
            applyToSelection([member, &value](Song& song) { song.*member = value; });
        };
    };

    edits(titleField, &Song::title);
    edits(trackField, &Song::track);
    edits(artistField, &Song::artist);
    edits(albumField, &Song::album);
    edits(genreField, &Song::genre);
    edits(yearField, &Song::year);

    for (auto* field : { &titleField, &trackField, &artistField, &albumField,
                         &genreField, &yearField, &charterField, &iconField })
        addAndMakeVisible(*field);

    charterField.editor.onTextChange = [this]() { refreshRows(); };

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

    // Stepping a mixed rating steps from the first selected song's value, which
    // is what resolves the disagreement: one click and they all agree.
    auto steps = [this, stepped](ValueStepper& stepper, int Song::*member)
    {
        stepper.onStep = [this, stepped, &stepper, member](int delta)
        {
            if (songs.empty() || editing.empty()) return;
            const int from = songs[(size_t)*editing.begin()].*member;
            const int to = stepped(from, delta);
            applyToSelection([member, to](Song& song) { song.*member = to; });
            stepper.setDisplayValue(ratingText(to));
        };
    };

    steps(drumsDifficulty, &Song::diffDrums);
    steps(proDrumsDifficulty, &Song::diffPro);

    proDrumsToggle.onClick = [this]()
    {
        // Mutually exclusive: the format calls both being true invalid.
        const bool on = proDrumsToggle.getToggleState();
        if (on) fiveLaneToggle.setToggleState(false);
        applyToSelection([on](Song& song)
                         { song.proDrums = on; if (on) song.fiveLane = false; });
    };
    fiveLaneToggle.onClick = [this]()
    {
        const bool on = fiveLaneToggle.getToggleState();
        if (on) proDrumsToggle.setToggleState(false);
        applyToSelection([on](Song& song)
                         { song.fiveLane = on; if (on) song.proDrums = false; });
    };

    albumArt.onFileChanged = [this](const juce::File& file)
    {
        applyToSelection([&file](Song& song) { song.albumArtFile = file; });

        // A cover arriving where there is no background makes one, in the
        // template already chosen. Only for songs with nothing there: an
        // existing background was put there deliberately.
        if (file.existsAsFile())
            generateBackgrounds(true);
    };
    backgroundArt.onFileChanged = [this](const juce::File& file)
    { applyToSelection([&file](Song& song) { song.backgroundFile = file; }); };

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

    // Reuse cannot work against a .sng: the audio is inside the container and
    // reading it back means parsing the index. Forced on until that exists.
    auto followPackaging = [this]()
    {
        const bool sng = packagingButtons.getSelectedIndex() == 1;
        if (sng) renderAudioToggle.setToggleState(true);
        renderAudioToggle.setEnabled(!sng);
        formatButtons.setEnabled(renderAudioToggle.getToggleState());
        repaint();
    };

    packagingButtons.onSelectionChanged = [followPackaging](int) { followPackaging(); };

    renderAudioToggle.setToggleState(true);
    renderAudioToggle.onClick = [this]()
    {
        formatButtons.setEnabled(renderAudioToggle.getToggleState());
        repaint();
    };

    // Behind a gear on the slot itself: three template names on permanent
    // display took a row of the window to say something set once.
    backgroundArt.hasOptions = true;
    backgroundArt.onOptions = [this]()
    {
        juce::PopupMenu menu;
        menu.addItem(1, "Auto background from cover", true, ChartSettings::autoBackground());
        menu.addSeparator();

        const auto names = BackgroundGenerator::styleNames();
        const int chosen = ChartSettings::backgroundStyle();
        for (int i = 0; i < names.size(); ++i)
            menu.addItem(10 + i, names[i], true, i == chosen);

        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&backgroundArt),
                           [this](int result)
                           {
                               if (result == 1)
                               {
                                   ChartSettings::setAutoBackground(!ChartSettings::autoBackground());
                               }
                               else if (result >= 10)
                               {
                                   // Picking a template is what makes it, so the
                                   // three can be compared by choosing them.
                                   ChartSettings::setBackgroundStyle(result - 10);
                                   generateBackgrounds();
                               }
                           });
    };

    exportButton.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::coral).withAlpha(0.15f));
    exportButton.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::coral));
    exportButton.onClick = [this]()
    {
        // Ticked rows that are not ready are skipped rather than blocking the
        // rest: ten finished charts should not wait on the eleventh.
        std::vector<ChartExporter::SongExport> batch;
        for (const auto& song : songs)
        {
            if (!song.selected) continue;
            if (song.blocker(albumBlocker().isEmpty(), hasDrums).isNotEmpty()) continue;

            // The sentinel is ours. SongExport::regionGuid must be empty when
            // there is no region, which is how the exporter picks what to
            // make a region for.
            const bool isRegion = song.region.guid != kSelectionKey;
            batch.push_back({ song.region.range(), optionsFor(song),
                              isRegion ? song.region.guid : juce::String() });
        }

        if (onExport && !batch.empty()) onExport(batch);
    };

    cancelButton.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::darkBgLighter));
    cancelButton.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::textDim));
    cancelButton.onClick = [this]() { if (onDismiss) onDismiss(); };

    createRegionButton.setColour(juce::TextButton::buttonColourId,
                                 juce::Colour(Theme::coral).withAlpha(0.15f));
    createRegionButton.setColour(juce::TextButton::textColourOffId, juce::Colour(Theme::coral));
    createRegionButton.onClick = [this]()
    {
        if (!createRegion || !context.selection.plausible()) return;

        // A song already sitting on this selection is the one being made real,
        // and its title names the region. Otherwise this is a new region and
        // the next poll picks it up as a song.
        auto pending = std::find_if(songs.begin(), songs.end(),
                                    [](const Song& s) { return s.region.guid == kSelectionKey; });

        auto name = (pending != songs.end() && pending->title.isNotEmpty())
                      ? pending->title
                      : context.inferred.title.isNotEmpty() ? context.inferred.title
                                                            : juce::String("song");

        auto guid = createRegion(context.selection, name);
        if (guid.isEmpty()) return;

        if (pending != songs.end())
        {
            pending->region.guid = guid;
            pending->region.name = name;
        }

        refreshRows();
        resized();
    };
    addAndMakeVisible(createRegionButton);

    listView.setViewedComponent(&listContent, false);
    listView.setScrollBarsShown(true, false);
    addAndMakeVisible(listView);

    const std::initializer_list<juce::Component*> everything {
        &drumsDifficulty, &proDrumsDifficulty, &proDrumsToggle, &fiveLaneToggle,
        &albumArt, &backgroundArt,
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

    prefillArtwork();

    // Built through the same path the timer uses, so opening and updating
    // cannot drift apart.
    auto initial = songs;
    Snapshot snapshot { context.selection, context.regions, context.trackNames };
    context.regions.clear();          // force the first apply to do its work
    applySnapshot(snapshot);

    restore(context.remembered);
    regroup();
    loadSelection();
    refreshRows();

    // After restore, since that is what decides whether we reopened into .sng.
    followPackaging();

    startTimer(400);
}

ExportDialogComponent::~ExportDialogComponent()
{
    // The exporter has already copied whatever shipped into the chart folder,
    // so anything still here is a preview nobody needs.
    auto staging = backgroundStagingFolder();
    if (staging.isDirectory()
        && staging.isAChildOf(juce::File::getSpecialLocation(juce::File::tempDirectory)))
        staging.deleteRecursively();
}

bool ExportDialogComponent::differsFromCurrent(const Snapshot& snapshot) const
{
    if (snapshot.trackNames != context.trackNames) return true;

    // Always, not only when there are no regions. The readout and the create
    // button both track the selection, so a project that already has regions
    // still has to notice it moving.
    if (snapshot.selection.startSec != context.selection.startSec
        || snapshot.selection.endSec != context.selection.endSec)
        return true;

    if (snapshot.regions.size() != context.regions.size()) return true;
    for (size_t i = 0; i < snapshot.regions.size(); ++i)
    {
        const auto& a = snapshot.regions[i];
        const auto& b = context.regions[i];
        if (a.guid != b.guid || a.name != b.name
            || a.startSec != b.startSec || a.endSec != b.endSec)
            return true;
    }
    return false;
}

void ExportDialogComponent::applySnapshot(const Snapshot& snapshot)
{
    const bool rangesChanged = snapshot.regions.size() != context.regions.size()
                            || snapshot.selection.startSec != context.selection.startSec
                            || snapshot.selection.endSec != context.selection.endSec;

    context.selection = snapshot.selection;
    context.regions = snapshot.regions;
    context.trackNames = snapshot.trackNames;

    hasDrums = std::any_of(context.trackNames.begin(), context.trackNames.end(),
                           [](const juce::String& t) { return t.containsIgnoreCase("DRUM"); });

    if (rangesChanged && analyseDrums)
        context.drums = analyseDrums(context.regions.empty() ? context.selection
                                                             : context.regions.front().range());

    // Carried across so a region moving or being renamed keeps whatever was
    // typed for it. Identity first; failing that, the range, which is what
    // carries a time selection over at the moment it becomes a region. Typed
    // values are never dropped on the floor for want of a matching GUID.
    auto previous = songs;
    std::vector<bool> claimed(previous.size(), false);

    auto carry = [&previous, &claimed](const ChartExporter::Region& region) -> const Song*
    {
        for (size_t i = 0; i < previous.size(); ++i)
            if (!claimed[i] && region.guid.isNotEmpty() && previous[i].region.guid == region.guid)
            {
                claimed[i] = true;
                return &previous[i];
            }

        // One previous song can only become one region, so a claimed match is
        // never handed out twice.
        for (size_t i = 0; i < previous.size(); ++i)
            if (!claimed[i]
                && std::abs(previous[i].region.startSec - region.startSec) < 0.01
                && std::abs(previous[i].region.endSec - region.endSec) < 0.01)
            {
                claimed[i] = true;
                return &previous[i];
            }

        // Failing that, the best overlap. A region drawn by hand never lands
        // exactly on the selection it came from, and an exact match was
        // dropping everything typed on the floor for a few milliseconds.
        int best = -1;
        double bestOverlap = 0.0;

        for (size_t i = 0; i < previous.size(); ++i)
        {
            if (claimed[i]) continue;

            const auto& was = previous[i].region;
            const double overlap = juce::jmin(was.endSec, region.endSec)
                                 - juce::jmax(was.startSec, region.startSec);
            if (overlap <= 0.0) continue;

            // Over half the shorter of the two, so a region cannot steal a
            // song it merely brushes against.
            const double shorter = juce::jmin(was.endSec - was.startSec,
                                              region.endSec - region.startSec);
            if (shorter <= 0.0 || overlap < shorter * 0.5) continue;

            if (overlap > bestOverlap) { bestOverlap = overlap; best = (int)i; }
        }

        if (best >= 0)
        {
            claimed[(size_t)best] = true;
            return &previous[(size_t)best];
        }

        return nullptr;
    };

    songs.clear();
    for (const auto& region : context.regions)
    {
        Song song;
        song.region = region;

        if (const auto* old = carry(region))
        {
            song = *old;
            song.region = region;

            // The region's name only fills a title nobody has typed.
            if (song.title.isEmpty()) song.title = region.name.trim();
        }
        else
        {
            // The region name is a default, not a convention: it is the most
            // likely title and nothing downstream depends on its shape.
            song.title = region.name.trim();
            song.proDrums = context.drums.proDrums;
            song.fiveLane = context.drums.fiveLane;

            // A starting point only; a compilation overtypes it per song.
            song.artist = context.inferred.artist;
            song.album = context.inferred.album;
            song.genre = context.inferred.genre;
            song.year = context.inferred.year;
            song.albumArtFile = prefilledAlbumArt;
            song.backgroundFile = prefilledBackground;
        }

        songs.push_back(song);
    }

    // A song that is not a region yet.
    //
    // It outlives the time selection that made it: clicking the ruler
    // collapses the selection, and dropping the song then took everything
    // typed into it along with it.
    auto carried = std::find_if(previous.begin(), previous.end(),
                                [](const Song& s) { return s.region.guid == kSelectionKey; });
    const bool hadProvisional = carried != previous.end();

    // Only when there is nothing else: once the project has regions, they are
    // the songs, and the button below is how a selection becomes another one.
    // An existing one is kept regardless, since it holds what was typed.
    if (hadProvisional || (songs.empty() && context.selection.plausible()))
    {
        Song song;
        if (hadProvisional)
        {
            song = *carried;
        }
        else
        {
            song.title = context.inferred.title;
            song.track = context.inferred.track;
            song.proDrums = context.drums.proDrums;
            song.fiveLane = context.drums.fiveLane;

            // Seeded like any other new song. Leaving these out is why a
            // remade selection came back asking for an artist.
            song.artist = context.inferred.artist;
            song.album = context.inferred.album;
            song.genre = context.inferred.genre;
            song.year = context.inferred.year;
            song.albumArtFile = prefilledAlbumArt;
            song.backgroundFile = prefilledBackground;
        }

        song.region.name = "time selection";
        song.region.guid = kSelectionKey;

        // Only a real selection moves it. A collapsed one leaves the range
        // where it was, so the row keeps pointing at the same music.
        if (context.selection.plausible())
        {
            song.region.startSec = context.selection.startSec;
            song.region.endSec = context.selection.endSec;
        }

        song.selected = true;
        songs.push_back(song);
    }

    // Track numbers default to timeline order, which is album order whenever
    // the project is laid out the way the album plays.
    // Numbers already in use are skipped, so filling a gap cannot hand two
    // songs the same track number.
    juce::SortedSet<int> taken;
    for (const auto& song : songs)
        if (song.track.isNotEmpty()) taken.add(song.track.getIntValue());

    int number = 1;
    for (auto& song : songs)
    {
        if (song.track.isNotEmpty()) continue;
        while (taken.contains(number)) ++number;
        taken.add(number);
        song.track = juce::String(number).paddedLeft('0', 2);
    }

    // Drop selected songs that went away; never leave the selection empty.
    std::set<int> surviving;
    for (int index : editing)
        if (juce::isPositiveAndBelow(index, (int)songs.size()))
            surviving.insert(index);
    editing = surviving;
    anchor = juce::jlimit(0, juce::jmax(0, (int)songs.size() - 1), anchor);

    // Selecting a song is selecting its time, so a selection that has moved
    // off it drops the row rather than leaving it looking selected. Several
    // songs at once is its own state and is left alone.
    if (editing.size() == 1)
    {
        const auto& range = songs[(size_t)*editing.begin()].region;
        const bool matches = context.selection.plausible()
                          && std::abs(context.selection.startSec - range.startSec) < 0.05
                          && std::abs(context.selection.endSec - range.endSec) < 0.05;
        if (!matches) editing.clear();
    }

    rebuildRows();
    loadSelection();
    refreshRows();
    resized();
}

void ExportDialogComponent::rebuildRows()
{
    rows.clear();
    listContent.removeAllChildren();

    for (size_t i = 0; i < songs.size(); ++i)
    {
        auto row = std::make_unique<SongRow>();
        row->onSelect = [this, i](const juce::ModifierKeys& mods) { selectSong((int)i, mods); };
        row->tick.onClick = [this, i]()
        {
            songs[i].selected = rows[i]->tick.getToggleState();
            refreshValidity();
            repaint();
        };
        listContent.addAndMakeVisible(*row);
        rows.push_back(std::move(row));
    }

    regroup();
}

void ExportDialogComponent::regroup()
{
    // Album first, artist second: two singles by one artist belong together
    // more than they belong apart, and an album is the tighter claim of the
    // two when both are there.
    auto keyFor = [](const Song& song)
    {
        if (song.album.isNotEmpty()) return "a:" + song.album.trim().toLowerCase();
        if (song.artist.isNotEmpty()) return "b:" + song.artist.trim().toLowerCase();
        return juce::String();
    };

    groups.clear();
    displayOrder.clear();

    juce::StringArray seen;
    for (size_t i = 0; i < songs.size(); ++i)
    {
        auto key = keyFor(songs[i]);

        // Songs with nothing to group by each stand alone rather than being
        // herded into one "unknown" pile that would select them together.
        if (key.isEmpty())
        {
            groups.push_back({ (int)i });
            seen.add({});
            continue;
        }

        const int at = seen.indexOf(key);
        if (at >= 0) groups[(size_t)at].push_back((int)i);
        else { groups.push_back({ (int)i }); seen.add(key); }
    }

    for (const auto& group : groups)
        for (int index : group)
            displayOrder.push_back(index);

}

void ExportDialogComponent::selectSong(int index, const juce::ModifierKeys& mods)
{
    if (!juce::isPositiveAndBelow(index, (int)songs.size())) return;

    auto positionOf = [this](int songIndex)
    {
        for (size_t i = 0; i < displayOrder.size(); ++i)
            if (displayOrder[i] == songIndex) return (int)i;
        return 0;
    };

    if (mods.isShiftDown() && !editing.empty())
    {
        // Measured down the rail as it is drawn, not through the underlying
        // indices: a range means the rows between these two on screen.
        const int from = positionOf(anchor);
        const int to = positionOf(index);
        editing.clear();
        for (int p = juce::jmin(from, to); p <= juce::jmax(from, to); ++p)
            editing.insert(displayOrder[(size_t)p]);
    }
    else if (mods.isCommandDown() || mods.isCtrlDown())
    {
        if (editing.count(index) && editing.size() > 1) editing.erase(index);
        else editing.insert(index);
        anchor = index;
    }
    else
    {
        editing = { index };
        anchor = index;

        // One song selected is one range on the timeline. Several is a state
        // no single range describes, so the timeline is left alone.
        if (setTimeSelection)
        {
            const auto& range = songs[(size_t)index].region;
            context.selection.startSec = range.startSec;
            context.selection.endSec = range.endSec;
            lastPolledSelection = context.selection;
            setTimeSelection(context.selection);
        }
    }

    loadSelection();
    refreshRows();
}

void ExportDialogComponent::selectAllForEditing()
{
    editing.clear();
    for (size_t i = 0; i < songs.size(); ++i)
        editing.insert((int)i);

    loadSelection();
    refreshRows();
}

void ExportDialogComponent::applyToSelection(std::function<void(Song&)> change)
{
    for (int index : editing)
        if (juce::isPositiveAndBelow(index, (int)songs.size()))
            change(songs[(size_t)index]);

    regroup();
    refreshRows();
    resized();
}

void ExportDialogComponent::loadSelection()
{
    // Nothing selected: the panel shows nothing rather than the last song's
    // values, which would read as though that song were still being edited.
    if (songs.empty() || editing.empty())
    {
        for (auto* field : { &titleField, &trackField, &artistField,
                             &albumField, &genreField, &yearField })
            field->setText({});

        drumsDifficulty.setDisplayValue(ratingText(ChartExporter::ExportOptions::kUnrated));
        proDrumsDifficulty.setDisplayValue(ratingText(ChartExporter::ExportOptions::kUnrated));
        proDrumsToggle.setMixed(false);
        fiveLaneToggle.setMixed(false);
        albumArt.setMixed(false);
        albumArt.setFile({});
        backgroundArt.setMixed(false);
        backgroundArt.setFile({});
        return;
    }

    // Every control fills the same way: agree and show the value, disagree
    // and mark it mixed. Nothing here writes back.
    auto text = [this](Field& field, juce::String Song::*member)
    {
        const auto& first = songs[(size_t)*editing.begin()];
        const bool same = std::all_of(editing.begin(), editing.end(),
                                      [this, member, &first](int i)
                                      { return songs[(size_t)i].*member == first.*member; });
        if (same) field.setText(first.*member);
        else      field.setMixed();
    };

    text(titleField, &Song::title);
    text(trackField, &Song::track);
    text(artistField, &Song::artist);
    text(albumField, &Song::album);
    text(genreField, &Song::genre);
    text(yearField, &Song::year);

    auto rating = [this](ValueStepper& stepper, int Song::*member)
    {
        const auto& first = songs[(size_t)*editing.begin()];
        const bool same = std::all_of(editing.begin(), editing.end(),
                                      [this, member, &first](int i)
                                      { return songs[(size_t)i].*member == first.*member; });
        if (same) stepper.setDisplayValue(ratingText(first.*member));
        else      stepper.setMixed();
    };

    rating(drumsDifficulty, &Song::diffDrums);
    rating(proDrumsDifficulty, &Song::diffPro);

    auto flag = [this](CheckboxToggle& toggle, bool Song::*member)
    {
        const auto& first = songs[(size_t)*editing.begin()];
        const bool same = std::all_of(editing.begin(), editing.end(),
                                      [this, member, &first](int i)
                                      { return songs[(size_t)i].*member == first.*member; });
        toggle.setMixed(!same);
        if (same) toggle.setToggleState(first.*member);
    };

    flag(proDrumsToggle, &Song::proDrums);
    flag(fiveLaneToggle, &Song::fiveLane);

    auto art = [this](ArtSlot& slot, juce::File Song::*member)
    {
        // Named for where it lands in the chart, not for the staging file it
        // happens to be sitting in now.
        slot.caption = (&slot == &albumArt) ? "album.png" : "background.png";

        const auto& first = songs[(size_t)*editing.begin()];
        const bool same = std::all_of(editing.begin(), editing.end(),
                                      [this, member, &first](int i)
                                      { return songs[(size_t)i].*member == first.*member; });
        slot.setFile(same ? first.*member : juce::File());
        slot.setMixed(!same);
    };

    art(albumArt, &Song::albumArtFile);
    art(backgroundArt, &Song::backgroundFile);
}

void ExportDialogComponent::timerCallback()
{
    if (!pollProject) return;

    auto snapshot = pollProject();
    if (differsFromCurrent(snapshot))
        applySnapshot(snapshot);

    auto same = [](const ChartExporter::TimeRange& a, const ChartExporter::TimeRange& b)
    {
        return std::abs(a.startSec - b.startSec) < 0.001
            && std::abs(a.endSec - b.endSec) < 0.001;
    };

    // One tick of stillness before reading the notes, so dragging a selection
    // does not analyse the project on every frame of the drag.
    if (analyseDrums
        && snapshot.selection.plausible()
        && same(snapshot.selection, lastPolledSelection)
        && !same(snapshot.selection, analysedSelection))
    {
        selectionDrums = analyseDrums(snapshot.selection);
        analysedSelection = snapshot.selection;
        repaint();
    }

    lastPolledSelection = snapshot.selection;
}

void ExportDialogComponent::refreshRows()
{
    const bool named = albumBlocker().isEmpty();
    for (size_t i = 0; i < rows.size() && i < songs.size(); ++i)
    {
        auto& song = songs[i];
        auto blocked = song.blocker(named, hasDrums);

        // Whether this song has been exported before, under the name it would
        // export as now. Renaming it makes the old one invisible here, which
        // is correct: a different name is a different chart.
        song.exported = juce::File();
        const auto folderName = optionsFor(song).folderName;
        if (folderName.isNotEmpty())
        {
            auto folder = context.destinationRoot.getChildFile(folderName);
            auto packed = context.destinationRoot.getChildFile(folderName + ".sng");
            if (folder.isDirectory())        song.exported = folder;
            else if (packed.existsAsFile())  song.exported = packed;
        }

        rows[i]->number = song.track;

        const bool isSelection = song.region.guid == kSelectionKey;
        rows[i]->untitled = song.title.isEmpty() && song.region.name.isEmpty();
        rows[i]->name = song.title.isNotEmpty() ? song.title
                      : song.region.name.isNotEmpty() ? song.region.name
                                                      : juce::String("untitled");
        // Where it is and how long it runs, which is what tells two rows
        // apart. The release is on the fields, not repeated down the list.
        rows[i]->timecode = timecodeOf(song.region.startSec) + " \xe2\x80\x93 "
                          + timecodeOf(song.region.endSec);
        rows[i]->source = isSelection
                            ? juce::String("time selection")
                            : timecodeOf(song.region.endSec - song.region.startSec);
        rows[i]->status = blocked.isNotEmpty() ? blocked
                        : song.exported.exists() ? "exported " + exportedWhen(song.exported)
                                                        : "ready";
        rows[i]->ready = blocked.isEmpty();
        rows[i]->exported = song.exported.exists();
        rows[i]->current = editing.count((int)i) > 0;
        rows[i]->tick.setToggleState(song.selected);
        rows[i]->repaint();
    }

    // Offered whenever the timeline is showing something that is not a song
    // yet, so making one is never something you have to already know about.
    const bool offer = context.selection.plausible() && !selectionInsideRegion();

    // The rail gives up a row for it, so appearing has to relayout.
    if (offer != createRegionButton.isVisible())
    {
        createRegionButton.setVisible(offer);
        resized();
    }

    refreshValidity();
    repaint();
}

juce::String ExportDialogComponent::albumBlocker() const
{
    // All that is left of the window-wide checks. Everything else moved onto
    // the song, because only the charter is a fact about the session rather
    // than about the music in it.
    if (charterField.text().isEmpty()) return "needs a charter";
    return {};
}

void ExportDialogComponent::generateBackgrounds(bool onlyMissing)
{
    BackgroundGenerator::Options options;
    options.style = (BackgroundGenerator::Style)juce::jmax(0, ChartSettings::backgroundStyle());

    auto folder = backgroundStagingFolder();
    folder.createDirectory();

    for (int index : editing)
    {
        if (!juce::isPositiveAndBelow(index, (int)songs.size())) continue;

        auto& song = songs[(size_t)index];
        if (!song.albumArtFile.existsAsFile()) continue;
        if (onlyMissing && song.backgroundFile.existsAsFile()) continue;

        // Named for the song so two of them cannot land on the same file.
        auto target = folder.getChildFile(juce::String(song.region.guid.hashCode64())
                                            + "-background.png");
        if (BackgroundGenerator::writeTo(target, song.albumArtFile, options))
            song.backgroundFile = target;
    }

    loadSelection();
    refreshRows();
}

bool ExportDialogComponent::selectionInsideRegion() const
{
    if (!context.selection.plausible()) return false;

    // Contained, not merely overlapping: a selection spilling past a region is
    // a different stretch of music and may well want to be its own song.
    for (const auto& region : context.regions)
        if (context.selection.startSec >= region.startSec - 0.01
            && context.selection.endSec <= region.endSec + 0.01)
            return true;

    return false;
}

int ExportDialogComponent::readyCount() const
{
    const bool named = albumBlocker().isEmpty();
    return (int)std::count_if(songs.begin(), songs.end(),
                              [this, named](const Song& song)
                              { return song.selected && song.blocker(named, hasDrums).isEmpty(); });
}

void ExportDialogComponent::refreshValidity()
{
    const int ready = readyCount();
    exportButton.setEnabled(ready > 0);
    exportButton.setButtonText(ready > 0 ? "Export " + juce::String(ready)
                                             + (ready == 1 ? " song" : " songs")
                                         : "Export");
}

void ExportDialogComponent::prefillArtwork()
{
    // Held, not shown: artwork is per song, so this seeds each new song.
    prefilledAlbumArt = findArt(context.artworkSearchPaths, "album");
    prefilledBackground = findArt(context.artworkSearchPaths, "background");
}

ChartExporter::ExportOptions ExportDialogComponent::optionsFor(const Song& song) const
{
    ChartExporter::ExportOptions options;
    options.title = song.title;
    options.track = song.track;
    options.artist = song.artist;
    options.album = song.album;
    options.genre = song.genre;
    options.year = song.year;
    options.charter = charterField.text();
    options.icon = iconField.text();

    options.diffDrums = hasDrums ? song.diffDrums : ChartExporter::ExportOptions::kUnrated;
    options.diffDrumsReal = hasDrums ? song.diffPro : ChartExporter::ExportOptions::kUnrated;
    options.proDrums = song.proDrums;
    options.fiveLaneDrums = song.fiveLane;

    options.albumArt = song.albumArtFile;
    options.backgroundArt = song.backgroundFile;
    // Generating is something the dialog does when asked, so the exporter is
    // never left to invent a background nobody has looked at.
    options.generateBackground = false;
    options.backgroundOptions.style =
        (BackgroundGenerator::Style)juce::jmax(0, ChartSettings::backgroundStyle());

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
void ExportDialogComponent::adoptRegions(const juce::StringPairArray& folderNameToGuid)
{
    for (auto& song : songs)
    {
        auto guid = folderNameToGuid[optionsFor(song).folderName];
        if (guid.isNotEmpty()) song.region.guid = guid;
    }
}

juce::ValueTree ExportDialogComponent::toValueTree() const
{
    juce::ValueTree tree(kRemembered);
    tree.setProperty(kPackAsSng, packagingButtons.getSelectedIndex() == 1, nullptr);
    tree.setProperty(kAudioFormat, formatButtons.getSelectedIndex(), nullptr);

    for (const auto& song : songs)
    {
        if (song.region.guid.isEmpty()) continue;

        juce::ValueTree child(kSong);
        child.setProperty(kGuid, song.region.guid, nullptr);
        child.setProperty(kTitle, song.title, nullptr);
        child.setProperty(kTrack, song.track, nullptr);
        child.setProperty(kArtist, song.artist, nullptr);
        child.setProperty(kAlbum, song.album, nullptr);
        child.setProperty(kGenre, song.genre, nullptr);
        child.setProperty(kYear, song.year, nullptr);
        child.setProperty(kAlbumArt, song.albumArtFile.getFullPathName(), nullptr);
        child.setProperty(kBackgroundArt, song.backgroundFile.getFullPathName(), nullptr);
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

    if (tree.hasProperty(kPackAsSng))
        packagingButtons.setSelectedIndex((bool)tree.getProperty(kPackAsSng) ? 1 : 0);
    if (tree.hasProperty(kAudioFormat))
        formatButtons.setSelectedIndex((int)tree.getProperty(kAudioFormat));

    // Trees written before the metadata moved onto the song carry artist,
    // album, genre, year and artwork on the root, meant for every song at
    // once. Read them first so reopening an old project finds what was typed
    // into it, then let each song's own values override.
    for (auto& song : songs)
    {
        auto legacy = [&tree](juce::String& target, const juce::Identifier& key)
        {
            auto value = tree.getProperty(key).toString();
            if (value.isNotEmpty()) target = value;
        };

        legacy(song.artist, kArtist);
        legacy(song.album, kAlbum);
        legacy(song.genre, kGenre);
        legacy(song.year, kYear);

        juce::File legacyArt(tree.getProperty(kAlbumArt).toString());
        if (legacyArt.existsAsFile()) song.albumArtFile = legacyArt;
        juce::File legacyBackground(tree.getProperty(kBackgroundArt).toString());
        if (legacyBackground.existsAsFile()) song.backgroundFile = legacyBackground;
    }

    for (auto& song : songs)
    {
        auto child = tree.getChildWithProperty(kGuid, song.region.guid);

        // Trees written before the GUID lookup worked are keyed by the name
        // fallback. Only for named regions: every unnamed one shares the key
        // "name:" and would all restore from whichever child came first.
        if (!child.isValid() && song.region.name.isNotEmpty())
            child = tree.getChildWithProperty(kGuid, "name:" + song.region.name);

        if (!child.isValid()) continue;

        // Remembered values win over inferred ones: anything in here was typed
        // by hand, which is a stronger claim than anything read off a filename.
        auto put = [&child](juce::String& target, const juce::Identifier& key)
        {
            auto value = child.getProperty(key).toString();
            if (value.isNotEmpty()) target = value;
        };

        put(song.title, kTitle);
        put(song.track, kTrack);
        put(song.artist, kArtist);
        put(song.album, kAlbum);
        put(song.genre, kGenre);
        put(song.year, kYear);

        // Art paths can go stale between sessions, so a file that has since
        // moved falls back to prefill rather than showing a dead path.
        juce::File art(child.getProperty(kAlbumArt).toString());
        if (art.existsAsFile()) song.albumArtFile = art;
        juce::File background(child.getProperty(kBackgroundArt).toString());
        if (background.existsAsFile()) song.backgroundFile = background;

        if (child.hasProperty(kDiffDrums)) song.diffDrums = (int)child.getProperty(kDiffDrums);
        if (child.hasProperty(kDiffPro))   song.diffPro = (int)child.getProperty(kDiffPro);
        if (child.hasProperty(kProDrums))  song.proDrums = (bool)child.getProperty(kProDrums);
        if (child.hasProperty(kFiveLane))  song.fiveLane = (bool)child.getProperty(kFiveLane);
        if (child.hasProperty(kSelected))  song.selected = (bool)child.getProperty(kSelected);
    }
}

//==============================================================================
void ExportDialogComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(Theme::darkBg));

    auto inner = getLocalBounds().reduced(kPad);

    g.setColour(juce::Colour(Theme::coral));
    g.setFont(Theme::getUIFont(20.0f));
    g.drawText("Export Chart", inner.removeFromTop(26), juce::Justification::topLeft);

    juce::String summary;
    if (songs.empty())
    {
        // Worth saying out loud rather than refusing to open: the fix is in
        // REAPER, not in here.
        summary << "no regions in this project and no time selection, "
                << "so there is nothing to export";
    }
    else
    {
        summary << (int)songs.size() << (songs.size() == 1 ? " song" : " songs")
                << " \xc2\xb7 " << readyCount() << " ready";
        if (!context.trackNames.isEmpty())
            summary << "   tracks: " << context.trackNames.joinIntoString(", ");
        else
            summary << "   NO CHART TRACKS FOUND";
    }

    g.setColour(juce::Colour(songs.empty() ? Theme::coral : Theme::textDim));
    g.setFont(Theme::getUIFont(12.0f));
    g.drawText(summary, inner.removeFromTop(20), juce::Justification::topLeft, true);

    // Section headings. Which song the fields belong to is the one thing this
    // window has to say plainly, so the heading counts what is being edited
    // rather than just naming the section.
    auto heading = [&g](juce::Rectangle<int> area, const juce::String& text)
    {
        g.setColour(juce::Colour(Theme::textDim));
        g.setFont(Theme::getUIFont(10.5f));
        g.drawText(text, area, juce::Justification::centredLeft);

        g.setColour(juce::Colours::white.withAlpha(Theme::borderAlpha));
        g.fillRect(area.removeFromBottom(1));
    };

    heading(railBounds.withHeight(kHeadingHeight), "SONGS");

    // What the timeline is showing right now: the range, how long it is, and
    // what is actually in it, so making it a region is an informed click.
    if (!selectionPanel.isEmpty())
    {
        auto panel = selectionPanel;
        heading(panel.removeFromTop(kHeadingHeight), "TIME SELECTION");
        panel.removeFromTop(2);

        const auto& range = context.selection;

        auto rangeRow = panel.removeFromTop(16);
        g.setColour(juce::Colour(Theme::textWhite));
        g.setFont(Theme::getUIFont(11.5f));
        g.drawText(timecodeOf(range.startSec) + " \xe2\x80\x93 " + timecodeOf(range.endSec),
                   rangeRow, juce::Justification::centredLeft);

        g.setColour(juce::Colour(Theme::textDim));
        g.setFont(Theme::getUIFont(10.5f));
        g.drawText(timecodeOf(range.endSec - range.startSec),
                   rangeRow, juce::Justification::centredRight);

        // Reported off the notes in this range, not off the project.
        juce::String contents;
        const bool analysed = std::abs(analysedSelection.startSec - range.startSec) < 0.001
                           && std::abs(analysedSelection.endSec - range.endSec) < 0.001;

        if (!analysed)                     contents = "reading notes";
        else if (!selectionDrums.hasDrums) contents = "no drum notes here";
        else
        {
            contents = "drums";
            if (selectionDrums.proDrums) contents << " \xc2\xb7 pro";
            if (selectionDrums.fiveLane) contents << " \xc2\xb7 5-lane";
            if (selectionDrums.tomMarkers > 0)
                contents << " \xc2\xb7 " << selectionDrums.tomMarkers << " toms";
        }

        g.setColour(juce::Colour(analysed && !selectionDrums.hasDrums ? Theme::coral
                                                                     : Theme::textDim));
        g.setFont(Theme::getUIFont(10.5f));
        g.drawText(contents, panel.removeFromTop(14), juce::Justification::centredLeft, true);
    }

    juce::String songHeading("SONG");
    if (editing.empty())         songHeading = "NO SONG SELECTED";
    else if (editing.size() > 1) songHeading = juce::String((int)editing.size()) + " SONGS SELECTED";
    heading(songPanel.withHeight(kHeadingHeight), songHeading);

    heading(albumPanel.withHeight(kHeadingHeight), "CHARTER");

    auto footer = footerBounds;
    auto blocked = albumBlocker();

    g.setColour(juce::Colour(Theme::textDim));
    g.setFont(Theme::getUIFont(10.5f));
    auto outputRow = footer.removeFromTop(16);
    g.drawText("OUTPUT", outputRow.removeFromLeft(kLabelWidth), juce::Justification::centredLeft);

    g.setColour(juce::Colour(blocked.isNotEmpty() ? Theme::coral : Theme::textWhite));
    g.setFont(Theme::getUIFont(11.0f));
    g.drawText(blocked.isNotEmpty() ? "every song " + blocked
                                    : context.destinationRoot.getFullPathName(),
               outputRow, juce::Justification::centredLeft, true);
}

void ExportDialogComponent::resized()
{
    const int row = kRowHeight;
    const int gap = kRowGap;

    auto nextRow = [row, gap](juce::Rectangle<int>& area)
    {
        auto r = area.removeFromTop(row);
        area.removeFromTop(gap);
        return r;
    };

    /** Two controls side by side in one row, sharing the row's gap. */
    auto splitRow = [gap](juce::Rectangle<int> area, float leftShare)
    {
        auto left = area.removeFromLeft((int)(area.getWidth() * leftShare) - gap);
        area.removeFromLeft(gap);
        return std::make_pair(left, area);
    };

    auto inner = getLocalBounds().reduced(kPad);
    inner.removeFromTop(50);                      // title and summary, painted

    //==========================================================================
    // The footer: us, and where the files go. Taken off the bottom first
    // because its height is fixed and everything above it flexes.
    auto buttons = inner.removeFromBottom(row);
    cancelButton.setBounds(buttons.removeFromRight(96));
    buttons.removeFromRight(gap);
    exportButton.setBounds(buttons.removeFromRight(140));
    inner.removeFromBottom(gap);

    auto outputRow = inner.removeFromBottom(row);
    {
        auto area = outputRow;
        area.removeFromLeft(kLabelWidth);

        // Sized to their content rather than stretched edge to edge, so two
        // unrelated choices stop reading as one bar.
        formatButtons.setBounds(area.removeFromLeft(200));
        area.removeFromLeft(gap * 2);
        packagingButtons.setBounds(area.removeFromLeft(140));
        area.removeFromLeft(gap * 2);

        // Whatever is left: a fixed width here truncated the label with the
        // rest of the row standing empty.
        renderAudioToggle.setBounds(area);
    }
    inner.removeFromBottom(18);                   // destination line, painted
    inner.removeFromBottom(gap);

    auto charterRow = inner.removeFromBottom(row);
    {
        auto area = charterRow;
        charterField.setBounds(area.removeFromLeft(area.getWidth() / 2 - gap));
        area.removeFromLeft(gap * 2);

        browseIconsButton.setBounds(area.removeFromRight(96));
        area.removeFromRight(gap);
        iconPreview.setBounds(area.removeFromRight(110));
        area.removeFromRight(gap);
        iconField.setBounds(area);
    }

    inner.removeFromBottom(gap);
    albumPanel = inner.removeFromBottom(kHeadingHeight);
    inner.removeFromBottom(kSectionGap);

    //==========================================================================
    // The rail, full height, so extra height goes to the list rather than
    // opening a gap mid-window.
    railBounds = inner.removeFromLeft(
        juce::jlimit(kRailMin, kRailMax, (int)(inner.getWidth() * kRailShare)));
    inner.removeFromLeft(kSectionGap);

    auto rail = railBounds;
    rail.removeFromTop(kHeadingHeight + gap);

    // Under the rail: what the timeline is showing, then what to do with it.
    if (createRegionButton.isVisible())
    {
        createRegionButton.setBounds(rail.removeFromBottom(kRowHeight));
        rail.removeFromBottom(gap);
    }

    if (context.selection.plausible())
    {
        selectionPanel = rail.removeFromBottom(kHeadingHeight + 32);
        rail.removeFromBottom(gap);
    }
    else
    {
        selectionPanel = {};
    }

    listView.setBounds(rail);

    const int listRow = (int)(row * 1.6f);

    int contentHeight = 0;
    for (const auto& group : groups)
        contentHeight += (int)group.size() * listRow;

    listContent.setSize(juce::jmax(1, rail.getWidth() - 12), juce::jmax(1, contentHeight));

    // Grouping still decides the order, so a release stays together, but it
    // does not put the release's name down the list beside every song in it.
    int y = 0;
    for (int index : displayOrder)
    {
        rows[(size_t)index]->setBounds(0, y, listContent.getWidth(), listRow);
        y += listRow;
    }
    listContent.setSize(listContent.getWidth(), juce::jmax(1, y));

    //==========================================================================
    // The song panel: everything about whatever is selected.
    songPanel = inner;
    auto fields = inner;
    fields.removeFromTop(kHeadingHeight + gap);

    titleField.setBounds(nextRow(fields));

    // Artist and album share a row: two short values were each claiming the
    // full width of the panel, which is most of why this felt so empty.
    {
        auto [artist, album] = splitRow(nextRow(fields), 0.5f);
        artistField.setBounds(artist);
        albumField.setBounds(album);
        albumField.labelWidth = 52;
    }

    {
        // Short labels on the narrow half of the row, so a four-character
        // label does not claim the same gutter as CHARTER. Set before
        // setBounds, which is what lays the editor out.
        yearField.labelWidth = 40;
        trackField.labelWidth = 44;

        auto [left, right] = splitRow(nextRow(fields), 0.5f);
        genreField.setBounds(left);
        auto [year, track] = splitRow(right, 0.5f);
        yearField.setBounds(year);
        trackField.setBounds(track);
    }

    if (hasDrums)
    {
        auto [left, right] = splitRow(nextRow(fields), 0.5f);
        drumsDifficulty.setBounds(left);
        proDrumsDifficulty.setBounds(right);

        // Share the fields' label gutter instead of splitting their own width,
        // so the boxes line up with the rows above.
        drumsDifficulty.setLabelRatio((float)kLabelWidth / (float)juce::jmax(1, left.getWidth()));
        proDrumsDifficulty.setLabelRatio((float)kLabelWidth / (float)juce::jmax(1, right.getWidth()));

        auto toggles = nextRow(fields);
        toggles.removeFromLeft(kLabelWidth);
        proDrumsToggle.setBounds(toggles.removeFromLeft(toggles.getWidth() / 2));
        fiveLaneToggle.setBounds(toggles);
    }

    fields.removeFromTop(gap);

    // Both pictures are the same height and their own shape: a cover is
    // square, a background is 16:9, and seeing that is half of knowing which
    // slot is which.
    // As large as the panel allows. A cover you cannot see is not a preview,
    // and with the templates behind the gear there is room for them now: the
    // square and the 16:9 side by side, bounded by whichever runs out first.
    const int available = fields.getWidth() - kLabelWidth - kSectionGap;
    const int byWidth = (available * 9) / (9 + 16);
    const int byHeight = fields.getHeight() - kHeadingHeight - 16;
    const int pictureHeight = juce::jlimit(kArtTile, 260, juce::jmin(byWidth, byHeight));
    const int backgroundWidth = (pictureHeight * 16) / 9;

    auto artRow = fields.removeFromTop(kHeadingHeight + pictureHeight + 14);
    artRow.removeFromLeft(kLabelWidth);

    albumArt.setBounds(artRow.removeFromLeft(pictureHeight));
    artRow.removeFromLeft(kSectionGap);
    backgroundArt.setBounds(artRow.removeFromLeft(backgroundWidth));

    footerBounds = juce::Rectangle<int>(kPad, outputRow.getY() - 18 - gap,
                                        getWidth() - kPad * 2, 18);
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
