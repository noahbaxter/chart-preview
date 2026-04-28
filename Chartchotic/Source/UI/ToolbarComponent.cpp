#include "ToolbarComponent.h"
#include "TooltipStrings.h"
#include "../Editor/WriteController.h"

static const juce::StringArray framerateLabels = { "15 FPS", "30 FPS", "60 FPS", "Native" };
static const juce::StringArray latencyLabels = { "0ms", "250ms", "500ms", "750ms", "1000ms", "1500ms" };
static const juce::StringArray hopoThresholdLabels = { "Tight", "Default", "Loose" };

ToolbarComponent::ToolbarComponent(juce::ValueTree& state, WriteController& writeController)
    : state(state), writeController(writeController)
{
    initTopBar();
    initChartPanel();
    initSettingsPanel();
}

ToolbarComponent::~ToolbarComponent()
{
    instrumentSelector.dismissPanel();
    difficultySelector.dismissPanel();
    chartButton.dismissPanel();
    settingsButton.dismissPanel();
}

//==============================================================================
// Top bar controls

void ToolbarComponent::initTopBar()
{
    // Menu groups — circle selectors and square buttons each get mutual exclusion
    circleMenuGroup.add(&instrumentSelector, [this]() { instrumentSelector.dismissPanel(); });
    circleMenuGroup.add(&difficultySelector, [this]() { difficultySelector.dismissPanel(); });
    instrumentSelector.setMenuGroup(&circleMenuGroup);
    difficultySelector.setMenuGroup(&circleMenuGroup);

    // Logo
    addAndMakeVisible(logo);

    initManualInstrumentSelector();
    addAndMakeVisible(instrumentSelector);

    // Difficulty selector (text circles: X H M E)
    difficultySelector.setItems({
        { "X", {}, juce::Colour(Theme::red),    "Expert" },
        { "H", {}, juce::Colour(Theme::orange), "Hard" },
        { "M", {}, juce::Colour(Theme::yellow), "Medium" },
        { "E", {}, juce::Colour(Theme::green),  "Easy" }
    });
    difficultySelector.onSelectionChanged = [this](int index) {
        // Single-select mode (non-REAPER): 0=X(4), 1=H(3), 2=M(2), 3=E(1)
        if (onSkillChanged) onSkillChanged(4 - index);
    };
    difficultySelector.onMultiSelectionChanged = [this](int index, bool modifierHeld) {
        if (index == -1)
        {
            // "All" clicked
            if (onAllDifficultiesClicked) onAllDifficultiesClicked();
        }
        else
        {
            // Map index to SkillLevel: 0=Expert(4), 1=Hard(3), 2=Medium(2), 3=Easy(1)
            static const SkillLevel indexToSkill[] = { SkillLevel::EXPERT, SkillLevel::HARD,
                                                       SkillLevel::MEDIUM, SkillLevel::EASY };
            if (index >= 0 && index < 4)
            {
                if (onDifficultyClicked) onDifficultyClicked(indexToSkill[index], modifierHeld);
            }
        }
    };
    addAndMakeVisible(difficultySelector);

    // Note Speed stepper (no label — tooltip on hover)
    noteSpeedStepper.setDisplayValue(noteSpeed);
    noteSpeedStepper.setLabelRatio(0.0f);
    noteSpeedStepper.setTooltip("Note Speed");
    noteSpeedStepper.onStep = [this](int delta) {
        noteSpeed = juce::jlimit(NOTE_SPEED_MIN, NOTE_SPEED_MAX, noteSpeed + delta);
        noteSpeedStepper.setDisplayValue(noteSpeed);
        if (onNoteSpeedChanged) onNoteSpeedChanged(noteSpeed);
    };
    noteSpeedStepper.onValueEdited = [this](const juce::String& text) {
        noteSpeed = juce::jlimit(NOTE_SPEED_MIN, NOTE_SPEED_MAX, text.getIntValue());
        noteSpeedStepper.setDisplayValue(noteSpeed);
        if (onNoteSpeedChanged) onNoteSpeedChanged(noteSpeed);
    };
    addAndMakeVisible(noteSpeedStepper);

    // Highway Length stepper (no label — tooltip on hover, % suffix)
    highwayLengthStepper.setLabelRatio(0.0f);
    highwayLengthStepper.setTooltip("Highway Length");
    addAndMakeVisible(highwayLengthStepper);
}

//==============================================================================
// Chart panel — Modifiers + Elements

void ToolbarComponent::initChartPanel()
{
    // --- Modifiers ---

    starPowerToggle.onClick = [this]() {
        if (onStarPowerChanged) onStarPowerChanged(starPowerToggle.getToggleState());
    };

    // Auto HOPO toggle + threshold (guitar only)
    autoHopoToggle.setToggleState(DEFAULT_AUTO_HOPO);
    autoHopoToggle.onClick = [this]() {
        bool on = autoHopoToggle.getToggleState();
        if (onAutoHopoChanged) onAutoHopoChanged(on);
        // Relayout to show/hide threshold
        if (chartButton.isPanelVisible())
            chartButton.relayoutPanel();
    };

    hopoThresholdStepper.setLabelRatio(0.0f);
    hopoThresholdStepper.setTooltip("HOPO Threshold");
    hopoThresholdStepper.setDisplayValue(hopoThresholdLabels[hopoThresholdIndex]);
    hopoThresholdStepper.onStep = [this](int delta) {
        int count = hopoThresholdLabels.size();
        hopoThresholdIndex = juce::jlimit(0, count - 1, hopoThresholdIndex + delta);
        hopoThresholdStepper.setDisplayValue(hopoThresholdLabels[hopoThresholdIndex]);
        if (onHopoThresholdChanged) onHopoThresholdChanged(hopoThresholdIndex);
    };

    // Drum modifiers
    dynamicsToggle.onClick = [this]() {
        if (onDynamicsChanged) onDynamicsChanged(dynamicsToggle.getToggleState());
    };

    kick2xToggle.onClick = [this]() {
        if (onKick2xChanged) onKick2xChanged(kick2xToggle.getToggleState());
    };

    cymbalsToggle.onClick = [this]() {
        // Cymbals ON = Pro (id 2), OFF = Normal (id 1)
        if (onDrumTypeChanged) onDrumTypeChanged(cymbalsToggle.getToggleState() ? 2 : 1);
        // Relayout to update disco flip disabled state
        if (chartButton.isPanelVisible())
            chartButton.relayoutPanel();
    };

    discoFlipToggle.onClick = [this]() {
        if (onDiscoFlipChanged) onDiscoFlipChanged(discoFlipToggle.getToggleState());
    };

    discoFlipTooltip.setText(TooltipStrings::discoFlip);
    discoFlipTooltip.attachTo(discoFlipToggle);

    hopoThresholdTooltip.setText(TooltipStrings::hopoThreshold);
    hopoThresholdTooltip.attachTo(hopoThresholdStepper);

    // --- Chart + Scene ---

    gemsToggle.onClick = [this]() {
        if (onGemsChanged) onGemsChanged(gemsToggle.getToggleState());
    };

    barsToggle.onClick = [this]() {
        if (onBarsChanged) onBarsChanged(barsToggle.getToggleState());
    };

    sustainsToggle.onClick = [this]() {
        if (onSustainsChanged) onSustainsChanged(sustainsToggle.getToggleState());
    };

    lanesToggle.onClick = [this]() {
        if (onLanesChanged) onLanesChanged(lanesToggle.getToggleState());
    };

    gridlinesToggle.onClick = [this]() {
        if (onGridlinesChanged) onGridlinesChanged(gridlinesToggle.getToggleState());
    };

    hitIndicatorsToggle.onClick = [this]() {
        if (onHitIndicatorsChanged) onHitIndicatorsChanged(hitIndicatorsToggle.getToggleState());
    };

    trackToggle.onClick = [this]() {
        if (onTrackChanged) onTrackChanged(trackToggle.getToggleState());
    };

    laneSeparatorsToggle.onClick = [this]() {
        if (onLaneSeparatorsChanged) onLaneSeparatorsChanged(laneSeparatorsToggle.getToggleState());
    };

    strikelineToggle.onClick = [this]() {
        if (onStrikelineChanged) onStrikelineChanged(strikelineToggle.getToggleState());
    };

    highwayToggle.onClick = [this]() {
        if (onHighwayChanged) onHighwayChanged(highwayToggle.getToggleState());
    };

    // Register all children
    chartButton.addPanelChild(&modifiersHeader);
    chartButton.addPanelChild(&starPowerToggle);
    chartButton.addPanelChild(&autoHopoToggle);
    chartButton.addPanelChild(&hopoThresholdStepper);
    chartButton.addPanelChild(&dynamicsToggle);
    chartButton.addPanelChild(&kick2xToggle);
    chartButton.addPanelChild(&cymbalsToggle);
    chartButton.addPanelChild(&discoFlipToggle);
    chartButton.addPanelChild(&chartHeader);
    chartButton.addPanelChild(&gemsToggle);
    chartButton.addPanelChild(&barsToggle);
    chartButton.addPanelChild(&sustainsToggle);
    chartButton.addPanelChild(&lanesToggle);
    chartButton.addPanelChild(&gridlinesToggle);
    chartButton.addPanelChild(&hitIndicatorsToggle);
    chartButton.addPanelChild(&sceneHeader);
    chartButton.addPanelChild(&trackToggle);
    chartButton.addPanelChild(&laneSeparatorsToggle);
    chartButton.addPanelChild(&strikelineToggle);
    chartButton.addPanelChild(&highwayToggle);
    chartButton.setPanelSize(175, 340);
    chartButton.onLayoutPanel = [this](juce::Component* panel) { layoutChartPanel(panel); };
    chartButton.setMenuGroup(&squareMenuGroup);
    squareMenuGroup.add(&chartButton, [this]() { chartButton.dismissPanel(); });
    addAndMakeVisible(chartButton);
}

//==============================================================================
// Settings panel (gear) — Style, Playback, Sync

void ToolbarComponent::initSettingsPanel()
{
    // --- Style ---

    backgroundStepper.setDisplayValue("n/a");
    backgroundStepper.setFolderIconVisible(true);
    backgroundStepper.setValueClickable(true);
    backgroundStepper.onFolderClick = [this]() {
        if (onOpenBackgroundFolder) onOpenBackgroundFolder();
    };
    backgroundStepper.onStep = [this](int delta) {
        int count = backgroundNames.size();
        if (count == 0) return;

        backgroundIndex += delta;
        if (backgroundIndex < 0) backgroundIndex = count - 1;
        if (backgroundIndex >= count) backgroundIndex = 0;

        backgroundStepper.setDisplayValue(backgroundNames[backgroundIndex]);
        if (onBackgroundChanged) onBackgroundChanged(backgroundNames[backgroundIndex]);
    };

    gemScaleStepper.setDisplayValue(gemScalePct);
    gemScaleStepper.onStep = [this](int delta) {
        int snapped = (int)std::round(gemScalePct / (double)GEM_SCALE_STEP_PCT) * GEM_SCALE_STEP_PCT;
        gemScalePct = juce::jlimit(GEM_SCALE_MIN_PCT, GEM_SCALE_MAX_PCT, snapped + delta * GEM_SCALE_STEP_PCT);
        gemScaleStepper.setDisplayValue(gemScalePct);
        if (onGemScaleChanged) onGemScaleChanged(gemScalePct / 100.0f);
    };
    gemScaleStepper.onValueEdited = [this](const juce::String& text) {
        gemScalePct = juce::jlimit(GEM_SCALE_MIN_PCT, GEM_SCALE_MAX_PCT, text.getIntValue());
        gemScaleStepper.setDisplayValue(gemScalePct);
        if (onGemScaleChanged) onGemScaleChanged(gemScalePct / 100.0f);
    };

    barScaleStepper.setDisplayValue(barScalePct);
    barScaleStepper.onStep = [this](int delta) {
        int snapped = (int)std::round(barScalePct / (double)BAR_SCALE_STEP_PCT) * BAR_SCALE_STEP_PCT;
        barScalePct = juce::jlimit(BAR_SCALE_MIN_PCT, BAR_SCALE_MAX_PCT, snapped + delta * BAR_SCALE_STEP_PCT);
        barScaleStepper.setDisplayValue(barScalePct);
        if (onBarScaleChanged) onBarScaleChanged(barScalePct / 100.0f);
    };
    barScaleStepper.onValueEdited = [this](const juce::String& text) {
        barScalePct = juce::jlimit(BAR_SCALE_MIN_PCT, BAR_SCALE_MAX_PCT, text.getIntValue());
        barScaleStepper.setDisplayValue(barScalePct);
        if (onBarScaleChanged) onBarScaleChanged(barScalePct / 100.0f);
    };

    highwayTextureStepper.setDisplayValue("n/a");
    highwayTextureStepper.setFolderIconVisible(true);
    highwayTextureStepper.setValueClickable(true);
    highwayTextureStepper.onFolderClick = [this]() {
        if (onOpenTextureFolder) onOpenTextureFolder();
    };
    highwayTextureStepper.onStep = [this](int delta) {
        int count = highwayTextureNames.size();
        if (count == 0) return;

        highwayTextureIndex += delta;
        if (highwayTextureIndex < 0) highwayTextureIndex = count - 1;
        if (highwayTextureIndex >= count) highwayTextureIndex = 0;

        highwayTextureStepper.setDisplayValue(highwayTextureNames[highwayTextureIndex]);
        highwayTextureStepper.setValueClickable(false);
        if (onHighwayTextureChanged) onHighwayTextureChanged(highwayTextureNames[highwayTextureIndex]);
    };

    textureScaleLabel.setText("Scale", juce::dontSendNotification);
    textureScaleLabel.setFont(Theme::getUIFont(Theme::fontSize));
    textureScaleLabel.setColour(juce::Label::textColourId, juce::Colour(Theme::textDim));
    textureScaleLabel.setJustificationType(juce::Justification::centred);

    textureOpacityLabel.setText("Opacity", juce::dontSendNotification);
    textureOpacityLabel.setFont(Theme::getUIFont(Theme::fontSize));
    textureOpacityLabel.setColour(juce::Label::textColourId, juce::Colour(Theme::textDim));
    textureOpacityLabel.setJustificationType(juce::Justification::centred);

    textureScaleStepper.setLabelRatio(0.0f);
    textureScaleStepper.setDisplayValue(texScalePct);
    textureScaleStepper.onStep = [this](int delta) {
        int snapped = (int)std::round(texScalePct / (double)TEX_SCALE_STEP_PCT) * TEX_SCALE_STEP_PCT;
        texScalePct = juce::jlimit(TEX_SCALE_MIN_PCT, TEX_SCALE_MAX_PCT, snapped + delta * TEX_SCALE_STEP_PCT);
        textureScaleStepper.setDisplayValue(texScalePct);
        if (onTextureScaleChanged) onTextureScaleChanged(texScalePct / 100.0f);
    };
    textureScaleStepper.onValueEdited = [this](const juce::String& text) {
        texScalePct = juce::jlimit(TEX_SCALE_MIN_PCT, TEX_SCALE_MAX_PCT, text.getIntValue());
        textureScaleStepper.setDisplayValue(texScalePct);
        if (onTextureScaleChanged) onTextureScaleChanged(texScalePct / 100.0f);
    };

    textureOpacityStepper.setLabelRatio(0.0f);
    textureOpacityStepper.setDisplayValue(textureOpacityPct);
    textureOpacityStepper.onStep = [this](int delta) {
        int snapped = (int)std::round(textureOpacityPct / (double)TEX_OPACITY_STEP) * TEX_OPACITY_STEP;
        textureOpacityPct = juce::jlimit(TEX_OPACITY_MIN, TEX_OPACITY_MAX, snapped + delta * TEX_OPACITY_STEP);
        textureOpacityStepper.setDisplayValue(textureOpacityPct);
        if (onTextureOpacityChanged) onTextureOpacityChanged(textureOpacityPct / 100.0f);
    };
    textureOpacityStepper.onValueEdited = [this](const juce::String& text) {
        textureOpacityPct = juce::jlimit(TEX_OPACITY_MIN, TEX_OPACITY_MAX, text.getIntValue());
        textureOpacityStepper.setDisplayValue(textureOpacityPct);
        if (onTextureOpacityChanged) onTextureOpacityChanged(textureOpacityPct / 100.0f);
    };

    // --- Playback ---

    highwayLengthStepper.setDisplayValue(highwayLengthPct);
    highwayLengthStepper.onStep = [this](int delta) {
        // Adaptive step: fine at low values, coarser at high
        int step = highwayLengthPct < 100 ? 5 : (highwayLengthPct < 200 ? 10 : 25);
        int snapped = (int)std::round(highwayLengthPct / (double)step) * step;
        highwayLengthPct = juce::jlimit(HWY_LENGTH_MIN_PCT, HWY_LENGTH_MAX_PCT,
            snapped + delta * step);
        highwayLengthStepper.setDisplayValue(highwayLengthPct);
        if (onHighwayLengthChanged) onHighwayLengthChanged(highwayLengthPct / 100.0f);
    };
    highwayLengthStepper.onValueEdited = [this](const juce::String& text) {
        highwayLengthPct = juce::jlimit(HWY_LENGTH_MIN_PCT, HWY_LENGTH_MAX_PCT, text.getIntValue());
        highwayLengthStepper.setDisplayValue(highwayLengthPct);
        if (onHighwayLengthChanged) onHighwayLengthChanged(highwayLengthPct / 100.0f);
    };

    framerateButtons.setItems(framerateLabels);
    framerateButtons.setSelectedIndex(FRAMERATE_DEFAULT - 1);
    framerateButtons.onSelectionChanged = [this](int index) {
        if (onFramerateChanged) onFramerateChanged(index + 1);
    };

    showBackgroundToggle.setToggleState(state.hasProperty("showBackground") && (bool)state["showBackground"]);
    showBackgroundToggle.onClick = [this]() {
        bool on = showBackgroundToggle.getToggleState();
        state.setProperty("showBackground", on, nullptr);
        if (onShowBackgroundChanged) onShowBackgroundChanged(on);
    };

    showFpsToggle.setToggleState(DEFAULT_SHOW_FPS);
    showFpsToggle.onClick = [this]() {
        bool on = showFpsToggle.getToggleState();
        state.setProperty("showFps", on, nullptr);
        if (onShowFpsChanged) onShowFpsChanged(on);
    };

    stretchToggle.setToggleState(false);
    stretchToggle.onClick = [this]() {
        bool on = stretchToggle.getToggleState();
        state.setProperty("stretchToFill", on, nullptr);
        if (onStretchChanged) onStretchChanged(on);
    };

    trackDiscoveryToggle.setToggleState(!state.hasProperty("trackDiscovery") || (bool)state["trackDiscovery"]);
    trackDiscoveryToggle.onClick = [this]() {
        bool on = trackDiscoveryToggle.getToggleState();
        state.setProperty("trackDiscovery", on, nullptr);
        if (onTrackDiscoveryChanged) onTrackDiscoveryChanged(on);
    };
    trackDiscoveryTooltip.setText(TooltipStrings::trackDiscovery);
    trackDiscoveryTooltip.attachTo(trackDiscoveryToggle);

    bemaniModeToggle.setToggleState(false);
    bemaniModeToggle.onClick = [this]() {
        bool on = bemaniModeToggle.getToggleState();
        state.setProperty("bemaniMode", on, nullptr);
        // Hide stretch in Bemani mode and relayout
        stretchToggle.setVisible(!on);
        resized();
        // Speed stays the same — internal ratio handles the visual equivalence
        if (onNoteSpeedChanged) onNoteSpeedChanged(noteSpeed);
        if (onBemaniModeChanged) onBemaniModeChanged(on);
    };

    // --- Sync ---

    latencyStepper.setDisplayValue(latencyLabels[LATENCY_DEFAULT - 1]);
    latencyStepper.onStep = [this](int delta) {
        int count = latencyLabels.size();
        latencyIndex = juce::jlimit(0, count - 1, latencyIndex + delta);
        latencyStepper.setDisplayValue(latencyLabels[latencyIndex]);
        if (onLatencyChanged) onLatencyChanged(latencyIndex + 1);
    };

    syncOffsetStepper.setDisplayValue(syncOffsetMs);
    syncOffsetStepper.onStep = [this](int delta) {
        int snapped = (int)std::round(syncOffsetMs / (double)CALIBRATION_STEP_MS) * CALIBRATION_STEP_MS;
        syncOffsetMs = juce::jlimit(syncOffsetMin, syncOffsetMax, snapped + delta * CALIBRATION_STEP_MS);
        syncOffsetStepper.setDisplayValue(syncOffsetMs);
        state.setProperty("latencyOffsetMs", syncOffsetMs, nullptr);
        if (onLatencyOffsetChanged) onLatencyOffsetChanged(syncOffsetMs);
    };
    syncOffsetStepper.onValueEdited = [this](const juce::String& text) {
        syncOffsetMs = juce::jlimit(syncOffsetMin, syncOffsetMax, text.getIntValue());
        syncOffsetStepper.setDisplayValue(syncOffsetMs);
        state.setProperty("latencyOffsetMs", syncOffsetMs, nullptr);
        if (onLatencyOffsetChanged) onLatencyOffsetChanged(syncOffsetMs);
    };

    calibrationTooltip.setText(TooltipStrings::calibration);
    calibrationTooltip.attachTo(syncOffsetStepper);

    latencyTooltip.setText(TooltipStrings::latency);
    latencyTooltip.attachTo(latencyStepper);

    // Register all children
    settingsButton.addPanelChild(&displayHeader);
    settingsButton.addPanelChild(&framerateButtons);
    settingsButton.addPanelChild(&showFpsToggle);
    settingsButton.addPanelChild(&showBackgroundToggle);
    settingsButton.addPanelChild(&highwayHeader);
    settingsButton.addPanelChild(&highwayTextureStepper);
    settingsButton.addPanelChild(&textureScaleLabel);
    settingsButton.addPanelChild(&textureOpacityLabel);
    settingsButton.addPanelChild(&textureScaleStepper);
    settingsButton.addPanelChild(&textureOpacityStepper);
    settingsButton.addPanelChild(&stretchToggle);
    settingsButton.addPanelChild(&bemaniModeToggle);
    settingsButton.addPanelChild(&trackDiscoveryToggle);
    settingsButton.addPanelChild(&backgroundStepper);
    settingsButton.addPanelChild(&gemScaleStepper);
    settingsButton.addPanelChild(&barScaleStepper);
    settingsButton.addPanelChild(&syncHeader);
    settingsButton.addPanelChild(&syncOffsetStepper);
    settingsButton.addPanelChild(&latencyStepper);
    settingsButton.setPanelSize(240, 356);
    settingsButton.onLayoutPanel = [this](juce::Component* panel) { layoutSettingsPanel(panel); };
    settingsButton.setMenuGroup(&squareMenuGroup);
    squareMenuGroup.add(&settingsButton, [this]() { settingsButton.dismissPanel(); });
    addAndMakeVisible(settingsButton);

#ifdef DEBUG
    addAndMakeVisible(debugPanel.getButton());
    addAndMakeVisible(tuningPanel.getButton());
#endif
}

//==============================================================================
// Paint & Layout

void ToolbarComponent::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(Theme::toolbarBg));
    g.fillRect(getLocalBounds());
}

void ToolbarComponent::resized()
{
    if (getHeight() == 0) return;
    int stripH = getStripHeight();
    float scale = (float)stripH / (float)referenceHeight;

    int h = juce::roundToInt(28.0f * scale);
    Theme::setControlHeight((float)h);
    int gap = juce::roundToInt(6.0f * scale);
    int y = (stripH - h) / 2;

    int margin = juce::roundToInt(6.0f * scale);
    int x = margin;

    // Instrument circle — fixed width in multi-select (fits up to 5 stacked)
    {
        int instW = h;
        if (showMultiInstrument)
            instW = h + juce::roundToInt(h * CircleIconSelector::multiStackOffset * 4.0f); // room for 5 stacked
        instrumentSelector.setBounds(x, y, instW, h);
        x += instW + gap;
    }

    // Difficulty circle — fixed width in multi-select (fits up to 4 stacked)
    {
        int diffW = h;
        if (showMultiDifficulty)
            diffW = h + juce::roundToInt(h * CircleIconSelector::multiStackOffset * 3.0f);
        difficultySelector.setBounds(x, y, diffW, h);
        x += diffW + gap;
    }

    // Logo
    int logoH = stripH;
    logo.setFontSize((float)logoH * logoFontRatio);
    int logoW = (int)std::ceil(logo.getIdealWidth()) + juce::roundToInt(8.0f * scale);
    logo.setBounds(x, 0, logoW, logoH);
    int leftEdge = logo.getRight();

    // Right side: popup buttons (compute positions first for centering)
    int btnW = juce::roundToInt(46.0f * scale);
    int gearW = juce::roundToInt(32.0f * scale);
    int rx = getWidth() - margin;

    chartButton.setPanelTopMargin(getHeight());
    settingsButton.setPanelTopMargin(getHeight());
    chartButton.setScale(scale);
    settingsButton.setScale(scale);

    rx -= gearW;
    settingsButton.setBounds(rx, y, gearW, h);
    rx -= (gap + btnW);
    chartButton.setBounds(rx, y, btnW, h);

#ifdef DEBUG
    // Debug (D) and Tuning (T) as small stacked squares
    debugPanel.getButton().setScale(scale);
    // Tuning panel intentionally doesn't scale — content is fixed-size, scaling just wastes chart width.
    int sqSize = (h - juce::roundToInt(2.0f * scale)) / 2;
    rx -= (gap + sqSize);
    debugPanel.getButton().setBounds(rx, y, sqSize, sqSize);
    debugPanel.getButton().setPanelTopMargin(getHeight());
    tuningPanel.getButton().setPanelTopMargin(getHeight());
    tuningPanel.getButton().setBounds(rx, y + h - sqSize, sqSize, sqSize);
#endif

    // Note Speed + Highway Length steppers — centered between logo and right-side buttons
    int stepW = juce::roundToInt(66.0f * scale);
    int stepGap = juce::roundToInt(4.0f * scale);
    bool bemani = bemaniModeToggle.getToggleState();
    if (bemani)
    {
        // Bemani: hide highway length, center note speed
        highwayLengthStepper.setVisible(false);
        int cx = leftEdge + (rx - leftEdge - stepW) / 2;
        noteSpeedStepper.setBounds(cx, y, stepW, h);
    }
    else
    {
        highwayLengthStepper.setVisible(true);
        int totalW = stepW * 2 + stepGap;
        int cx = leftEdge + (rx - leftEdge - totalW) / 2;
        noteSpeedStepper.setBounds(cx, y, stepW, h);
        highwayLengthStepper.setBounds(cx + stepW + stepGap, y, stepW, h);
    }
}

//==============================================================================
// State

void ToolbarComponent::loadState()
{
    // Difficulty (1-based skill → reversed index: 1=Easy→3, 4=Expert→0)
    // In multi-select mode, skip — enabledDifficulties controls display
    if (!showMultiDifficulty)
    {
        int skill = (int)state["skillLevel"];
        if (skill >= 1 && skill <= 4)
            difficultySelector.setSelectedIndex(4 - skill);
    }

    // Part — only applies in manual (non-discovery) mode
    if (!showMultiInstrument)
    {
        int part = (int)state["part"];
        if (part == (int)Part::DRUMS)
            instrumentSelector.setSelectedIndex(1);
        else if (part == (int)Part::GUITAR)
            instrumentSelector.setSelectedIndex(0);
    }

    // Note speed
    noteSpeed = state.hasProperty("noteSpeed") ? (int)state["noteSpeed"] : NOTE_SPEED_DEFAULT;
    noteSpeedStepper.setDisplayValue(noteSpeed);

    // Drum type → cymbals toggle (Pro = 2 = on)
    int drumType = (int)state["drumType"];
    cymbalsToggle.setToggleState(drumType == 2);

    // Auto HOPO
    {
        bool hopoOn = state.hasProperty("autoHopo") ? (bool)state["autoHopo"] : DEFAULT_AUTO_HOPO;
        autoHopoToggle.setToggleState(hopoOn);

        if (state.hasProperty("hopoThresh"))
        {
            hopoThresholdIndex = juce::jlimit(0, hopoThresholdLabels.size() - 1, (int)state["hopoThresh"]);
            hopoThresholdStepper.setDisplayValue(hopoThresholdLabels[hopoThresholdIndex]);
        }
    }

    // Toggles
    gemsToggle.setToggleState(!state.hasProperty("showGems") || (bool)state["showGems"]);
    barsToggle.setToggleState(!state.hasProperty("showBars") || (bool)state["showBars"]);
    sustainsToggle.setToggleState(!state.hasProperty("showSustains") || (bool)state["showSustains"]);
    lanesToggle.setToggleState(!state.hasProperty("showLanes") || (bool)state["showLanes"]);
    starPowerToggle.setToggleState(!state.hasProperty("starPower") || (bool)state["starPower"]);
    gridlinesToggle.setToggleState(!state.hasProperty("showGridlines") || (bool)state["showGridlines"]);
    hitIndicatorsToggle.setToggleState(!state.hasProperty("hitIndicators") || (bool)state["hitIndicators"]);
    trackToggle.setToggleState(!state.hasProperty("showTrack") || (bool)state["showTrack"]);
    laneSeparatorsToggle.setToggleState(!state.hasProperty("showLaneSeparators") || (bool)state["showLaneSeparators"]);
    strikelineToggle.setToggleState(!state.hasProperty("showStrikeline") || (bool)state["showStrikeline"]);
    highwayToggle.setToggleState(!state.hasProperty("showHighway") || (bool)state["showHighway"]);
    kick2xToggle.setToggleState(!state.hasProperty("kick2x") || (bool)state["kick2x"]);
    dynamicsToggle.setToggleState(!state.hasProperty("dynamics") || (bool)state["dynamics"]);
    discoFlipToggle.setToggleState(!state.hasProperty("discoFlip") || (bool)state["discoFlip"]);
    trackDiscoveryToggle.setToggleState(!state.hasProperty("trackDiscovery") || (bool)state["trackDiscovery"]);

    // Bemani mode (restore before stretch so we can hide it if needed)
    bool bemaniOn = state.hasProperty("bemaniMode") && (bool)state["bemaniMode"];
    bemaniModeToggle.setToggleState(bemaniOn);

    // Stretch to fill (hidden in Bemani mode)
    stretchToggle.setVisible(!bemaniOn);
    stretchToggle.setToggleState(state.hasProperty("stretchToFill") && (bool)state["stretchToFill"]);

    // Background
    if (backgroundNames.isEmpty())
    {
        backgroundStepper.setDisplayValue("n/a");
        backgroundStepper.setValueClickable(true);
    }
    else
    {
        juce::String savedBg = state.getProperty("background").toString();
        backgroundIndex = savedBg.isNotEmpty() ? backgroundNames.indexOf(savedBg) : 0;
        if (backgroundIndex < 0) backgroundIndex = 0;
        backgroundStepper.setDisplayValue(backgroundNames[backgroundIndex]);
        backgroundStepper.setValueClickable(false);
    }

    // Framerate (1-based → 0-based)
    int savedFramerate = (int)state["framerate"];
    if (savedFramerate < 1 || savedFramerate > 4) savedFramerate = 4;
    framerateButtons.setSelectedIndex(savedFramerate - 1);

    if (state.hasProperty("showFps"))
        showFpsToggle.setToggleState((bool)state["showFps"]);

    // Latency (1-based → 0-based)
    int savedLatency = (int)state["latency"];
    if (savedLatency >= 1 && savedLatency <= latencyLabels.size())
    {
        latencyIndex = savedLatency - 1;
        latencyStepper.setDisplayValue(latencyLabels[latencyIndex]);
    }

    // Sync offset
    syncOffsetMs = juce::jlimit(syncOffsetMin, syncOffsetMax, (int)state["latencyOffsetMs"]);
    syncOffsetStepper.setDisplayValue(syncOffsetMs);

    // Gem scale
    float savedGemScale = state.hasProperty("gemScale") ? (float)state["gemScale"] : GEM_SCALE_DEFAULT_PCT / 100.0f;
    gemScalePct = juce::jlimit(GEM_SCALE_MIN_PCT, GEM_SCALE_MAX_PCT, juce::roundToInt(savedGemScale * 100.0f));
    gemScaleStepper.setDisplayValue(gemScalePct);

    // Bar scale
    float savedBarScale = state.hasProperty("barScale") ? (float)state["barScale"] : BAR_SCALE_DEFAULT_PCT / 100.0f;
    barScalePct = juce::jlimit(BAR_SCALE_MIN_PCT, BAR_SCALE_MAX_PCT, juce::roundToInt(savedBarScale * 100.0f));
    barScaleStepper.setDisplayValue(barScalePct);

    // Highway length
    if (state.hasProperty("highwayLength"))
    {
        int savedPct = juce::roundToInt((float)state["highwayLength"] * 100.0f);
        highwayLengthPct = juce::jlimit(HWY_LENGTH_MIN_PCT, HWY_LENGTH_MAX_PCT, savedPct);
        highwayLengthStepper.setDisplayValue(highwayLengthPct);
    }

    // Texture scale
    if (state.hasProperty("textureScale"))
    {
        int savedPct = juce::roundToInt((float)state["textureScale"] * 100.0f);
        texScalePct = juce::jlimit(TEX_SCALE_MIN_PCT, TEX_SCALE_MAX_PCT, savedPct);
        textureScaleStepper.setDisplayValue(texScalePct);
    }

    // Texture opacity
    if (state.hasProperty("textureOpacity"))
    {
        textureOpacityPct = juce::jlimit(TEX_OPACITY_MIN, TEX_OPACITY_MAX, juce::roundToInt((float)state["textureOpacity"] * 100.0f));
        textureOpacityStepper.setDisplayValue(textureOpacityPct);
    }

    // Highway texture
    if (highwayTextureNames.isEmpty())
    {
        highwayTextureStepper.setDisplayValue("n/a");
        highwayTextureStepper.setValueClickable(true);
    }
    else
    {
        juce::String savedTexture = state.getProperty("highwayTexture").toString();
        highwayTextureIndex = savedTexture.isNotEmpty() ? highwayTextureNames.indexOf(savedTexture) : 0;
        if (highwayTextureIndex < 0) highwayTextureIndex = 0;
        highwayTextureStepper.setDisplayValue(highwayTextureNames[highwayTextureIndex]);
        highwayTextureStepper.setValueClickable(false);
    }

    updateVisibility();
}

void ToolbarComponent::updateVisibility()
{
    // No top-bar visibility changes needed anymore — drum type moved to chart panel
}

void ToolbarComponent::setReaperMode(bool isReaper)
{
    reaperMode = isReaper;
}

//==============================================================================
// Panel layouts

void ToolbarComponent::layoutChartPanel(juce::Component* panel)
{
    float s = chartButton.getScale();
    int margin = juce::roundToInt(12.0f * s);
    int pillH = juce::roundToInt(26.0f * s);
    int stepperH = juce::roundToInt(24.0f * s);
    int headerH = juce::roundToInt(18.0f * s);
    int gap = juce::roundToInt(5.0f * s);
    int sectionGap = juce::roundToInt(8.0f * s);
    int y = margin;
    int w = panel->getWidth() - margin * 2;
    int col2 = w / 2;
    int pillW = col2 - juce::roundToInt(2.0f * s);
    // Show toggles based on what's currently visible
    bool hasDrums = false, hasGuitar = false;
    if (enabledParts.empty())
    {
        // Non-REAPER fallback: use selected part
        Part current = getPartFromState(state);
        hasDrums = isDrumLike(current);
        hasGuitar = isGuitarLike(current);
    }
    else
    {
        for (auto p : enabledParts)
        {
            if (isDrumLike(p)) hasDrums = true;
            if (isGuitarLike(p)) hasGuitar = true;
        }
    }

    // --- Modifiers ---
    modifiersHeader.setBounds(margin, y, w, headerH);
    y += headerH + gap;

    // Flow modifier pills left-to-right in 2-column rows
    int col = 0;
    for (auto& mod : modToggles)
    {
        bool visible = (mod.scope == ModScope::ALL)
                     || (mod.scope == ModScope::GUITAR && hasGuitar)
                     || (mod.scope == ModScope::DRUMS && hasDrums);
        mod.pill->setVisible(visible);
        if (!visible) continue;

        mod.pill->setBounds(margin + col * col2, y, pillW, pillH);
        col++;
        if (col >= 2) { col = 0; y += pillH + gap; }
    }
    if (col > 0) y += pillH + gap;

    if (hasDrums)
        discoFlipToggle.setDisabled(!cymbalsToggle.getToggleState());

    if (hasGuitar && autoHopoToggle.getToggleState())
    {
        hopoThresholdStepper.setBounds(margin, y, w, stepperH);
        hopoThresholdStepper.setVisible(true);
        y += stepperH + gap;
    }
    else
    {
        hopoThresholdStepper.setVisible(false);
    }

    y += sectionGap - gap;

    // --- Chart (note/chart primitives) ---
    chartHeader.setBounds(margin, y, w, headerH);
    y += headerH + gap;

    int elemH = juce::roundToInt(22.0f * s);
    int elemGap = juce::roundToInt(3.0f * s);
    int col3 = w / 3;
    int elemW = col3 - juce::roundToInt(2.0f * s);

    gemsToggle.setBounds(margin, y, elemW, elemH);
    barsToggle.setBounds(margin + col3, y, elemW, elemH);
    sustainsToggle.setBounds(margin + col3 * 2, y, elemW, elemH);
    y += elemH + elemGap;

    lanesToggle.setBounds(margin, y, elemW, elemH);
    gridlinesToggle.setBounds(margin + col3, y, elemW, elemH);
    hitIndicatorsToggle.setBounds(margin + col3 * 2, y, elemW, elemH);
    y += elemH + sectionGap;

    // --- Scene (static environment) ---
    sceneHeader.setBounds(margin, y, w, headerH);
    y += headerH + gap;

    int col4 = w / 4;
    int elemW4 = col4 - juce::roundToInt(2.0f * s);

    trackToggle.setBounds(margin, y, elemW4, elemH);
    laneSeparatorsToggle.setBounds(margin + col4, y, elemW4, elemH);
    strikelineToggle.setBounds(margin + col4 * 2, y, elemW4, elemH);
    highwayToggle.setBounds(margin + col4 * 3, y, elemW4, elemH);
    y += elemH;

    panel->setSize(panel->getWidth(), y + margin);
}

void ToolbarComponent::layoutSettingsPanel(juce::Component* panel)
{
    float s = settingsButton.getScale();
    int margin = juce::roundToInt(10.0f * s);
    int stepperH = juce::roundToInt(22.0f * s);
    int headerH = juce::roundToInt(16.0f * s);
    int gap = juce::roundToInt(4.0f * s);
    int sectionGap = juce::roundToInt(6.0f * s);
    int y = margin;
    int w = panel->getWidth() - margin * 2;

    // --- Display ---
    displayHeader.setBounds(margin, y, w, headerH);
    y += headerH + gap;

    framerateButtons.setBounds(margin, y, w, stepperH);
    y += stepperH + gap;

    showFpsToggle.setBounds(margin, y, w, stepperH);
    y += stepperH + gap;

    showBackgroundToggle.setBounds(margin, y, w, stepperH);
    y += stepperH + gap;

    backgroundStepper.setBounds(margin, y, w, stepperH);
    y += stepperH + gap;

    gemScaleStepper.setBounds(margin, y, w, stepperH);
    y += stepperH + gap;

    barScaleStepper.setBounds(margin, y, w, stepperH);
    y += stepperH + sectionGap;

    // --- Highway ---
    highwayHeader.setBounds(margin, y, w, headerH);
    y += headerH + gap;

    highwayTextureStepper.setBounds(margin, y, w, stepperH);
    y += stepperH + gap;

    // Scale + Opacity: labels above, value-only steppers below, two columns
    {
        int colW = (w - gap) / 2;
        int labelH = juce::roundToInt(14.0f * s);
        textureScaleLabel.setFont(Theme::getUIFont(Theme::fontSize));
        textureOpacityLabel.setFont(Theme::getUIFont(Theme::fontSize));
        textureScaleLabel.setBounds(margin, y, colW, labelH);
        textureOpacityLabel.setBounds(margin + colW + gap, y, colW, labelH);
        y += labelH + juce::roundToInt(1.0f * s);
        textureScaleStepper.setBounds(margin, y, colW, stepperH);
        textureOpacityStepper.setBounds(margin + colW + gap, y, colW, stepperH);
        y += stepperH + gap;
    }

    {
        bool bemaniOn = bemaniModeToggle.getToggleState();
        stretchToggle.setVisible(!bemaniOn);
        if (!bemaniOn)
        {
            int halfW = (w - gap) / 2;
            bemaniModeToggle.setBounds(margin, y, halfW, stepperH);
            stretchToggle.setBounds(margin + halfW + gap, y, halfW, stepperH);
        }
        else
        {
            bemaniModeToggle.setBounds(margin, y, w, stepperH);
        }
        y += stepperH + gap;
    }

    trackDiscoveryToggle.setVisible(reaperMode);
    if (reaperMode)
    {
        trackDiscoveryToggle.setBounds(margin, y, w, stepperH);
        y += stepperH + sectionGap;
    }
    else
    {
        y += sectionGap - gap;
    }

    // --- Sync ---
    syncHeader.setBounds(margin, y, w, headerH);
    y += headerH + gap;

    syncOffsetStepper.setBounds(margin, y, w, stepperH);

    if (!reaperMode)
    {
        y += stepperH + gap;
        latencyStepper.setBounds(margin, y, w, stepperH);
        latencyStepper.setVisible(true);
    }
    else
    {
        latencyStepper.setVisible(false);
    }

    y += stepperH;
    panel->setSize(panel->getWidth(), y + margin);
}

void ToolbarComponent::initManualInstrumentSelector()
{
    auto guitarImg = juce::ImageCache::getFromMemory(BinaryData::icon_guitar_png, BinaryData::icon_guitar_pngSize);
    auto drumsImg  = juce::ImageCache::getFromMemory(BinaryData::icon_drums_png, BinaryData::icon_drums_pngSize);
    instrumentSelector.setMultiSelectMode(false);
    instrumentSelector.setItems({
        { "Guitar", guitarImg },
        { "Drums",  drumsImg }
    });
    instrumentSelector.onSelectionChanged = [this](int index) {
        static constexpr int selectorToPart[] = { (int)Part::GUITAR, (int)Part::DRUMS };
        if (onPartChanged && index >= 0 && index < 2)
            onPartChanged(selectorToPart[index]);
        updateVisibility();
        if (chartButton.isPanelVisible())
        {
            chartButton.dismissPanel();
            chartButton.showPanel();
        }
    };
}

void ToolbarComponent::resetToManualMode()
{
    showMultiInstrument = false;
    discoveredParts.clear();
    enabledParts.clear();

    initManualInstrumentSelector();

    // Set selector to match current state
    int part = (int)state["part"];
    if (part == (int)Part::DRUMS)
        instrumentSelector.setSelectedIndex(1);
    else
        instrumentSelector.setSelectedIndex(0);

    resized();
}

void ToolbarComponent::setDiscoveredParts(const std::vector<Part>& parts)
{
    discoveredParts = parts;
    showMultiInstrument = true;

    // Build CircleItems from discovered parts (with per-instrument icons)
    std::vector<CircleItem> circleItems;
    for (auto part : parts)
    {
        CircleItem item;
        item.tooltip = getPartDisplayName(part);
        auto icon = getPartIcon(part);
        item.image = juce::ImageCache::getFromMemory(icon.data, icon.size);
        circleItems.push_back(std::move(item));
    }

    instrumentSelector.setItems(std::move(circleItems));
    instrumentSelector.setMultiSelectMode(true, true); // with "All" option

    // Wire multi-select callback
    instrumentSelector.onMultiSelectionChanged = [this](int index, bool modifierHeld) {
        if (index == -1)
        {
            // "All" clicked
            if (onAllInstrumentsClicked) onAllInstrumentsClicked();
            return;
        }
        if (index >= 0 && index < (int)discoveredParts.size())
        {
            if (onInstrumentClicked)
                onInstrumentClicked(discoveredParts[index], modifierHeld);
        }
    };

    // Default: all selected
    std::set<int> allIndices;
    for (int i = 0; i < (int)parts.size(); ++i)
        allIndices.insert(i);
    instrumentSelector.setMultiSelectedIndices(allIndices);

    resized();
}

void ToolbarComponent::setEnabledParts(const std::set<Part>& enabled)
{
    enabledParts = enabled;

    // Update multi-select indices on instrumentSelector
    std::set<int> indices;
    for (int i = 0; i < (int)discoveredParts.size(); ++i)
        if (enabled.count(discoveredParts[i]) > 0)
            indices.insert(i);
    instrumentSelector.setMultiSelectedIndices(indices);
    if (chartButton.isPanelVisible())
        chartButton.relayoutPanel();
    resized(); // stacking width may change
}

void ToolbarComponent::setEnabledDifficulties(const std::set<SkillLevel>& diffs)
{
    // Map SkillLevel to index: Expert→0, Hard→1, Medium→2, Easy→3
    std::set<int> indices;
    for (auto s : diffs)
    {
        switch (s)
        {
            case SkillLevel::EXPERT: indices.insert(0); break;
            case SkillLevel::HARD:   indices.insert(1); break;
            case SkillLevel::MEDIUM: indices.insert(2); break;
            case SkillLevel::EASY:   indices.insert(3); break;
        }
    }
    difficultySelector.setMultiSelectedIndices(indices);
    resized(); // stacking width may change
}

void ToolbarComponent::enableMultiDifficultyMode(bool enabled)
{
    showMultiDifficulty = enabled;
    difficultySelector.setMultiSelectMode(enabled, enabled); // with "All" option
    resized();
}

void ToolbarComponent::setLatencyOffsetRange(int minMs, int maxMs)
{
    syncOffsetMin = minMs;
    syncOffsetMax = maxMs;
    syncOffsetMs = juce::jlimit(minMs, maxMs, syncOffsetMs);
    syncOffsetStepper.setDisplayValue(syncOffsetMs);
}

#ifdef DEBUG
void ToolbarComponent::setDebugPlay(bool playing)
{
    debugPanel.setDebugPlay(playing);
}
#endif
