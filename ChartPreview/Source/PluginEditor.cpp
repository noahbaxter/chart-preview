/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

// Version number
static constexpr const char* CHART_PREVIEW_VERSION = "v0.8.7";

//==============================================================================
ChartPreviewAudioProcessorEditor::ChartPreviewAudioProcessorEditor(ChartPreviewAudioProcessor &p, juce::ValueTree &state)
    : AudioProcessorEditor(&p),
      state(state),
      audioProcessor(p),
      midiInterpreter(state, audioProcessor.getNoteStateMapArray(), audioProcessor.getGridlineMap(), audioProcessor.getGridlineMapLock(), audioProcessor.getNoteStateMapLock()),
      highwayRenderer(state, midiInterpreter)
{
    // Set up resize constraints
    constrainer.setMinimumSize(minWidth, minHeight);
    constrainer.setFixedAspectRatio(aspectRatio);
    setConstrainer(&constrainer);
    setResizable(true, true);
    
    setSize(defaultWidth, defaultHeight);

    latencyInSeconds = audioProcessor.latencyInSeconds;
    initAssets();
    initMenus();
    loadState();

    startTimerHz(60);
}

ChartPreviewAudioProcessorEditor::~ChartPreviewAudioProcessorEditor()
{
}


void ChartPreviewAudioProcessorEditor::initAssets()
{
    backgroundImage = juce::ImageCache::getFromMemory(BinaryData::background_png, BinaryData::background_pngSize);
    trackDrumImage = juce::ImageCache::getFromMemory(BinaryData::track_drum_png, BinaryData::track_drum_pngSize);
    trackGuitarImage = juce::ImageCache::getFromMemory(BinaryData::track_guitar_png, BinaryData::track_guitar_pngSize);

    // Load REAPER logo SVG
    reaperLogo = juce::Drawable::createFromImageData(BinaryData::logoreaper_svg, BinaryData::logoreaper_svgSize);
}

void ChartPreviewAudioProcessorEditor::initMenus()
{
    // Create menus
    skillMenu.addItemList(skillLevelLabels, 1);
    skillMenu.addListener(this);
    addAndMakeVisible(skillMenu);

    partMenu.addItemList(partLabels, 1);
    partMenu.addListener(this);
    addAndMakeVisible(partMenu);

    drumTypeMenu.addItemList(drumTypeLabels, 1);
    drumTypeMenu.addListener(this);
    addAndMakeVisible(drumTypeMenu);

    framerateMenu.addItemList({"15 FPS", "30 FPS", "60 FPS", "120 FPS", "144 FPS"}, 1);
    framerateMenu.addListener(this);
    addAndMakeVisible(framerateMenu);

    latencyMenu.addItemList({"250ms", "500ms", "750ms", "1000ms", "1500ms"}, 1);
    latencyMenu.addListener(this);
    addAndMakeVisible(latencyMenu);
    
    autoHopoMenu.addItemList(hopoModeLabels, 1);
    autoHopoMenu.addListener(this);
    addAndMakeVisible(autoHopoMenu);

    reaperTrackMenu.addItemList({"Track 1", "Track 2", "Track 3", "Track 4", "Track 5", "Track 6", "Track 7", "Track 8"}, 1);
    reaperTrackMenu.addListener(this);
    addAndMakeVisible(reaperTrackMenu);

    // Zoom slider (time-based only)
    chartZoomSliderTime.setRange(0.4, 2.5, 0.05);
    chartZoomSliderTime.setSliderStyle(juce::Slider::LinearVertical);
    chartZoomSliderTime.setTextBoxStyle(juce::Slider::TextBoxAbove, false, 50, 20);
    chartZoomSliderTime.addListener(this);
    addAndMakeVisible(chartZoomSliderTime);

    chartZoomLabel.setText("Zoom", juce::dontSendNotification);
    addAndMakeVisible(chartZoomLabel);

    // Version label
    versionLabel.setText(CHART_PREVIEW_VERSION, juce::dontSendNotification);
    versionLabel.setJustificationType(juce::Justification::centredLeft);
    versionLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.6f));
    versionLabel.setFont(juce::Font(10.0f));
    addAndMakeVisible(versionLabel);

    // Toggles
    starPowerToggle.setButtonText("Star Power");
    starPowerToggle.addListener(this);
    addAndMakeVisible(starPowerToggle);
    
    kick2xToggle.setButtonText("Kick 2x");
    kick2xToggle.addListener(this);
    addAndMakeVisible(kick2xToggle);
    
    dynamicsToggle.setButtonText("Dynamics");
    dynamicsToggle.addListener(this);
    addAndMakeVisible(dynamicsToggle);

    #ifdef DEBUG
    // Debug toggle
    debugToggle.setButtonText("Debug");
    debugToggle.addListener(this);
    addAndMakeVisible(debugToggle);

    // Clear Logs button
    clearLogsButton.setButtonText("Clear Logs");
    clearLogsButton.addListener(this);
    addAndMakeVisible(clearLogsButton);

    // Create console output
    consoleOutput.setMultiLine(true);
    consoleOutput.setReadOnly(true);
    addAndMakeVisible(consoleOutput);
    #endif
}

//==============================================================================
void ChartPreviewAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.drawImage(backgroundImage, getLocalBounds().toFloat());

    // Visual feedback for REAPER connection status
    if (audioProcessor.isReaperHost && audioProcessor.attemptReaperConnection())
    {
        // Draw REAPER logo in bottom left corner
        if (reaperLogo)
        {
            const int logoSize = 24;
            const int margin = 10;
            juce::Rectangle<float> logoBounds(margin, getHeight() - logoSize - margin, logoSize, logoSize);
            reaperLogo->drawWithin(g, logoBounds, juce::RectanglePlacement::centred, 0.8f);
        }
    }

    // Draw the track
    if (isPart(state, Part::DRUMS))
    {
        g.drawImage(trackDrumImage, juce::Rectangle<float>(0, 0, getWidth(), getHeight()), juce::RectanglePlacement::centred);
    }
    else if (isPart(state, Part::GUITAR))
    {
        g.drawImage(trackGuitarImage, juce::Rectangle<float>(0, 0, getWidth(), getHeight()), juce::RectanglePlacement::centred);
    }

    drumTypeMenu.setVisible(isPart(state, Part::DRUMS));
    kick2xToggle.setVisible(isPart(state, Part::DRUMS));
    dynamicsToggle.setVisible(isPart(state, Part::DRUMS));
    autoHopoMenu.setVisible(isPart(state, Part::GUITAR));

    // Hide latency menu in REAPER mode (no latency compensation needed)
    // Show REAPER track selector only in REAPER mode
    bool isReaperMode = audioProcessor.isReaperHost && audioProcessor.getReaperMidiProvider().isReaperApiAvailable();
    latencyMenu.setVisible(!isReaperMode);
    reaperTrackMenu.setVisible(isReaperMode);

    #ifdef DEBUG
    bool debugMode = debugToggle.getToggleState();
    consoleOutput.setVisible(debugMode);
    clearLogsButton.setVisible(debugMode);
    #endif

    // Draw the highway - delegate to mode-specific rendering
    if (isReaperMode)
    {
        paintReaperMode(g);
    }
    else
    {
        paintStandardMode(g);
    }
}

void ChartPreviewAudioProcessorEditor::paintReaperMode(juce::Graphics& g)
{
    // Use current position (cursor when paused, playhead when playing)
    PPQ trackWindowStartPPQ = lastKnownPosition;
    PPQ trackWindowEndPPQ = trackWindowStartPPQ + displaySizeInPPQ;
    PPQ latencyBufferEnd = trackWindowStartPPQ; // No latency in REAPER mode

    // Extend window for notes behind cursor
    PPQ extendedStart = trackWindowStartPPQ - displaySizeInPPQ;

    // Generate PPQ-based windows from MidiInterpreter
    TrackWindow ppqTrackWindow = midiInterpreter.generateTrackWindow(extendedStart, trackWindowEndPPQ);
    SustainWindow ppqSustainWindow = midiInterpreter.generateSustainWindow(extendedStart, trackWindowEndPPQ, latencyBufferEnd);
    GridlineMap ppqGridlineMap = midiInterpreter.generateGridlineWindow(extendedStart, trackWindowEndPPQ);

    // Create a lambda that uses REAPER's timeline conversion (handles ALL tempo changes)
    auto& reaperProvider = audioProcessor.getReaperMidiProvider();
    auto ppqToTime = [&reaperProvider](double ppq) { return reaperProvider.ppqToTime(ppq); };

    // Convert everything to time-based (seconds from cursor)
    PPQ cursorPPQ = trackWindowStartPPQ;  // Cursor is at the strikeline
    TimeBasedTrackWindow timeTrackWindow = TimeConverter::convertTrackWindow(ppqTrackWindow, cursorPPQ, ppqToTime);
    TimeBasedSustainWindow timeSustainWindow = TimeConverter::convertSustainWindow(ppqSustainWindow, cursorPPQ, ppqToTime);
    TimeBasedGridlineMap timeGridlineMap = TimeConverter::convertGridlineMap(ppqGridlineMap, cursorPPQ, ppqToTime);

    // Use the constant time window from the slider (in seconds)
    // Window is anchored at the strikeline (time 0), extending forward into the future
    double windowStartTime = 0.0;
    double windowEndTime = displayWindowTimeSeconds;

    highwayRenderer.paint(g, timeTrackWindow, timeSustainWindow, timeGridlineMap, windowStartTime, windowEndTime, audioProcessor.isPlaying);
}

void ChartPreviewAudioProcessorEditor::paintStandardMode(juce::Graphics& g)
{
    // Use current position (cursor when paused, playhead when playing)
    PPQ trackWindowStartPPQ = lastKnownPosition;

    // Apply latency compensation when playing
    if (audioProcessor.isPlaying)
    {
        // Use smoothed tempo-aware latency to prevent jitter during tempo changes
        PPQ smoothedLatency = smoothedLatencyInPPQ();
        trackWindowStartPPQ = std::max(PPQ(0.0), trackWindowStartPPQ - smoothedLatency);
    }

    PPQ trackWindowEndPPQ = trackWindowStartPPQ + displaySizeInPPQ;
    PPQ latencyBufferEnd = trackWindowStartPPQ + smoothedLatencyInPPQ();

    // Update MidiProcessor's visual window bounds to prevent premature cleanup of visible events
    audioProcessor.setMidiProcessorVisualWindowBounds(trackWindowStartPPQ, trackWindowEndPPQ);

    // In non-REAPER mode, use current BPM from playhead (no tempo map available)
    double currentBPM = 120.0;
    if (audioProcessor.getPlayHead())
    {
        auto positionInfo = audioProcessor.getPlayHead()->getPosition();
        if (positionInfo.hasValue())
        {
            currentBPM = positionInfo->getBpm().orFallback(120.0);
        }
    }

    // Extend window for notes behind cursor
    PPQ extendedStart = trackWindowStartPPQ - displaySizeInPPQ;

    // Generate PPQ-based windows from MidiInterpreter
    TrackWindow ppqTrackWindow = midiInterpreter.generateTrackWindow(extendedStart, trackWindowEndPPQ);
    SustainWindow ppqSustainWindow = midiInterpreter.generateSustainWindow(extendedStart, trackWindowEndPPQ, latencyBufferEnd);
    GridlineMap ppqGridlineMap = midiInterpreter.generateGridlineWindow(extendedStart, trackWindowEndPPQ);

    // Simple constant BPM conversion for non-REAPER mode
    auto ppqToTime = [currentBPM](double ppq) { return ppq * (60.0 / currentBPM); };

    // Convert everything to time-based (seconds from cursor)
    PPQ cursorPPQ = trackWindowStartPPQ;
    TimeBasedTrackWindow timeTrackWindow = TimeConverter::convertTrackWindow(ppqTrackWindow, cursorPPQ, ppqToTime);
    TimeBasedSustainWindow timeSustainWindow = TimeConverter::convertSustainWindow(ppqSustainWindow, cursorPPQ, ppqToTime);
    TimeBasedGridlineMap timeGridlineMap = TimeConverter::convertGridlineMap(ppqGridlineMap, cursorPPQ, ppqToTime);

    // Use the constant time window from the slider (in seconds)
    // Window is anchored at the strikeline (time 0), extending forward into the future
    double windowStartTime = 0.0;
    double windowEndTime = displayWindowTimeSeconds;

    highwayRenderer.paint(g, timeTrackWindow, timeSustainWindow, timeGridlineMap, windowStartTime, windowEndTime, audioProcessor.isPlaying);
}

void ChartPreviewAudioProcessorEditor::resized()
{
    // Keep original control sizes but use responsive positioning
    const int controlWidth = 100;
    const int controlHeight = 20;
    const int margin = 10;
    
    // Top row - left side controls (fixed positions, will bunch up or spread out naturally)
    skillMenu.setBounds(10, 10, controlWidth, controlHeight);
    partMenu.setBounds(120, 10, controlWidth, controlHeight);
    drumTypeMenu.setBounds(230, 10, controlWidth, controlHeight);
    autoHopoMenu.setBounds(230, 10, controlWidth, controlHeight);
    debugToggle.setBounds(340, 10, controlWidth, controlHeight);
    clearLogsButton.setBounds(450, 10, controlWidth, controlHeight);

    // Top row - right side controls (anchored to right edge)
    starPowerToggle.setBounds(getWidth() - 120, 10, controlWidth, controlHeight);
    kick2xToggle.setBounds(getWidth() - 120, 35, controlWidth, controlHeight);
    dynamicsToggle.setBounds(getWidth() - 120, 60, controlWidth, controlHeight);

    // Bottom right controls (anchored to bottom-right corner)
    framerateMenu.setBounds(getWidth() - 120, getHeight() - 30, controlWidth, controlHeight);
    latencyMenu.setBounds(getWidth() - 120, getHeight() - 55, controlWidth, controlHeight);
    reaperTrackMenu.setBounds(getWidth() - 120, getHeight() - 55, controlWidth, controlHeight);
    
    // Zoom controls (anchored to bottom-right)
    chartZoomLabel.setBounds(getWidth() - 90, getHeight() - 270, 40, controlHeight);
    chartZoomSliderTime.setBounds(getWidth() - 120, getHeight() - 240, controlWidth, 150);

    // Version label (bottom-left, next to REAPER logo)
    const int versionWidth = 60;
    const int versionHeight = 15;
    versionLabel.setBounds(45, getHeight() - versionHeight - 12, versionWidth, versionHeight);

    // Console output (responsive width and height)
    consoleOutput.setBounds(margin, 40, getWidth() - (2 * margin), getHeight() - 50);
}

void ChartPreviewAudioProcessorEditor::updateDisplaySizeFromZoomSlider()
{
    // Slider directly represents seconds for rendering
    displayWindowTimeSeconds = chartZoomSliderTime.getValue();

    // Use a generous worst-case PPQ window for MIDI fetching to prevent pop-in at extreme tempos
    const double WORST_CASE_PPQ_WINDOW = 30.0;  // quarter notes
    displaySizeInPPQ = PPQ(WORST_CASE_PPQ_WINDOW);

    // Sync the display size to the processor so processBlock can use it
    audioProcessor.setDisplayWindowSize(displaySizeInPPQ);
}

void ChartPreviewAudioProcessorEditor::loadState()
{
    skillMenu.setSelectedId((int)state["skillLevel"], juce::dontSendNotification);
    partMenu.setSelectedId((int)state["part"], juce::dontSendNotification);
    drumTypeMenu.setSelectedId((int)state["drumType"], juce::dontSendNotification);
    framerateMenu.setSelectedId((int)state["framerate"], juce::dontSendNotification);
    latencyMenu.setSelectedId((int)state["latency"], juce::dontSendNotification);
    autoHopoMenu.setSelectedId((int)state["autoHopo"], juce::dontSendNotification);
    reaperTrackMenu.setSelectedId((int)state["reaperTrack"], juce::dontSendNotification);

    starPowerToggle.setToggleState((bool)state["starPower"], juce::dontSendNotification);
    kick2xToggle.setToggleState((bool)state["kick2x"], juce::dontSendNotification);
    dynamicsToggle.setToggleState((bool)state["dynamics"], juce::dontSendNotification);

    chartZoomSliderTime.setValue((double)state["zoomTime"], juce::dontSendNotification);

    // Apply side-effects that your listeners would normally do
    applyLatencySetting((int)state["latency"]);
    int fr = ((int)state["framerate"] == 1) ? 15 :
             ((int)state["framerate"] == 2) ? 30 :
             ((int)state["framerate"] == 3) ? 60 :
             ((int)state["framerate"] == 4) ? 120 :
             ((int)state["framerate"] == 5) ? 144 : 60;
    startTimerHz(fr);

    updateDisplaySizeFromZoomSlider();
}

void ChartPreviewAudioProcessorEditor::applyLatencySetting(int latencyValue)
{
    switch (latencyValue) {
    case 1: latencyInSeconds = 0.250; break;
    case 2: latencyInSeconds = 0.500; break;
    case 3: latencyInSeconds = 0.750; break;
    case 4: latencyInSeconds = 1.000; break;
    case 5: latencyInSeconds = 1.500; break;
    default: latencyInSeconds = 0.500; break;
    }
    audioProcessor.setLatencyInSeconds(latencyInSeconds);
}

juce::ComponentBoundsConstrainer* ChartPreviewAudioProcessorEditor::getConstrainer()
{
    return &constrainer;
}

void ChartPreviewAudioProcessorEditor::parentSizeChanged()
{
    // This method is called when the parent component size changes
    // We can use this to handle host-specific resize behavior if needed
    AudioProcessorEditor::parentSizeChanged();
}
