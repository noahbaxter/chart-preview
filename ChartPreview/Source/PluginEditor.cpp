/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    
    // Sliders
    chartZoomSliderPPQ.setRange(1.0, 8.0, 0.1);
    chartZoomSliderPPQ.setSliderStyle(juce::Slider::LinearVertical);
    chartZoomSliderPPQ.setTextBoxStyle(juce::Slider::TextBoxAbove, false, 50, 20);
    chartZoomSliderPPQ.addListener(this);
    addAndMakeVisible(chartZoomSliderPPQ);
    
    chartZoomSliderTime.setRange(0.4, 2.0, 0.05);
    chartZoomSliderTime.setSliderStyle(juce::Slider::LinearVertical);
    chartZoomSliderTime.setTextBoxStyle(juce::Slider::TextBoxAbove, false, 50, 20);
    chartZoomSliderTime.addListener(this);
    addAndMakeVisible(chartZoomSliderTime);

    chartZoomLabel.setText("Zoom", juce::dontSendNotification);
    addAndMakeVisible(chartZoomLabel);
    
    // Toggles
    dynamicZoomToggle.setButtonText("Dynamic");
    dynamicZoomToggle.addListener(this);
    addAndMakeVisible(dynamicZoomToggle);

    starPowerToggle.setButtonText("Star Power");
    starPowerToggle.addListener(this);
    addAndMakeVisible(starPowerToggle);
    
    kick2xToggle.setButtonText("Kick 2x");
    kick2xToggle.addListener(this);
    addAndMakeVisible(kick2xToggle);
    
    dynamicsToggle.setButtonText("Dynamics");
    dynamicsToggle.addListener(this);
    addAndMakeVisible(dynamicsToggle);

    // // Debug toggle
    // debugToggle.setButtonText("Debug");
    // addAndMakeVisible(debugToggle);

    // // Create console output
    // consoleOutput.setMultiLine(true);
    // consoleOutput.setReadOnly(true);
    // addAndMakeVisible(consoleOutput);
}

//==============================================================================
void ChartPreviewAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.drawImage(backgroundImage, getLocalBounds().toFloat());

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
    consoleOutput.setVisible(debugToggle.getToggleState());

    // Update display size if in time-based mode to account for tempo changes
    bool isDynamicZoom = (bool)state.getProperty("dynamicZoom");
    if (!isDynamicZoom && audioProcessor.isPlaying)
    {
        updateDisplaySizeFromZoomSlider();
    }

    // Draw the highway
    // Use current position (cursor when paused, playhead when playing)
    PPQ trackWindowStartPPQ = lastKnownPosition;
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
    highwayRenderer.paint(g, trackWindowStartPPQ, trackWindowEndPPQ, displaySizeInPPQ, latencyBufferEnd);
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

    // Top row - right side controls (anchored to right edge)
    starPowerToggle.setBounds(getWidth() - 120, 10, controlWidth, controlHeight);
    kick2xToggle.setBounds(getWidth() - 120, 35, controlWidth, controlHeight);
    dynamicsToggle.setBounds(getWidth() - 120, 60, controlWidth, controlHeight);

    // Bottom right controls (anchored to bottom-right corner)
    framerateMenu.setBounds(getWidth() - 120, getHeight() - 30, controlWidth, controlHeight);
    latencyMenu.setBounds(getWidth() - 120, getHeight() - 55, controlWidth, controlHeight);
    
    // Zoom controls (anchored to bottom-right)
    chartZoomLabel.setBounds(getWidth() - 90, getHeight() - 270, 40, controlHeight);
    chartZoomSliderPPQ.setBounds(getWidth() - 120, getHeight() - 240, controlWidth, 150);
    chartZoomSliderTime.setBounds(getWidth() - 120, getHeight() - 240, controlWidth, 150);
    dynamicZoomToggle.setBounds(getWidth() - 120, getHeight() - 90, 80, controlHeight);

    // Console output (responsive width and height)
    consoleOutput.setBounds(margin, 40, getWidth() - (2 * margin), getHeight() - 50);
}

void ChartPreviewAudioProcessorEditor::updateDisplaySizeFromZoomSlider()
{
    bool isDynamicZoom = (bool)state.getProperty("dynamicZoom");
    
    if (isDynamicZoom)
    {
        // Dynamic (PPQ-based) mode: slider directly represents PPQ (beats)
        displaySizeInPPQ = PPQ(chartZoomSliderPPQ.getValue());
    }
    else
    {
        // Time-based mode: slider represents seconds
        double timeInSeconds = chartZoomSliderTime.getValue();
        
        // Convert to PPQ using current BPM
        if (audioProcessor.getPlayHead() == nullptr)
        {
            displaySizeInPPQ = PPQ(timeInSeconds * (120.0 / 60.0)); // Default 120 BPM
        }
        else
        {
            auto positionInfo = audioProcessor.getPlayHead()->getPosition();
            if (positionInfo.hasValue())
            {
                double bpm = positionInfo->getBpm().orFallback(120.0);
                displaySizeInPPQ = PPQ(timeInSeconds * (bpm / 60.0));
            }
            else
            {
                displaySizeInPPQ = PPQ(timeInSeconds * (120.0 / 60.0));
            }
        }
    }
}

void ChartPreviewAudioProcessorEditor::loadState()
{
    skillMenu.setSelectedId((int)state["skillLevel"], juce::dontSendNotification);
    partMenu.setSelectedId((int)state["part"], juce::dontSendNotification);
    drumTypeMenu.setSelectedId((int)state["drumType"], juce::dontSendNotification);
    framerateMenu.setSelectedId((int)state["framerate"], juce::dontSendNotification);
    latencyMenu.setSelectedId((int)state["latency"], juce::dontSendNotification);
    autoHopoMenu.setSelectedId((int)state["autoHopo"], juce::dontSendNotification);

    starPowerToggle.setToggleState((bool)state["starPower"], juce::dontSendNotification);
    kick2xToggle.setToggleState((bool)state["kick2x"], juce::dontSendNotification);
    dynamicsToggle.setToggleState((bool)state["dynamics"], juce::dontSendNotification);
    dynamicZoomToggle.setToggleState((bool)state["dynamicZoom"], juce::dontSendNotification);

    chartZoomSliderPPQ.setValue((double)state["zoomPPQ"], juce::dontSendNotification);
    chartZoomSliderTime.setValue((double)state["zoomTime"], juce::dontSendNotification);

    // Apply side-effects that your listeners would normally do
    applyLatencySetting((int)state["latency"]);
    int fr = ((int)state["framerate"] == 1) ? 15 : 
             ((int)state["framerate"] == 2) ? 30 : 
             ((int)state["framerate"] == 3) ? 60 : 
             ((int)state["framerate"] == 4) ? 120 : 
             ((int)state["framerate"] == 5) ? 144 : 60;
    startTimerHz(fr);

    updateSliderVisibility();
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

void ChartPreviewAudioProcessorEditor::updateSliderVisibility()
{
    bool isDynamicZoom = (bool)state.getProperty("dynamicZoom");
    
    chartZoomSliderPPQ.setVisible(isDynamicZoom);
    chartZoomSliderTime.setVisible(!isDynamicZoom);
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
