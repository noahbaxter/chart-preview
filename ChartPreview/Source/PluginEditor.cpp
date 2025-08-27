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
    consoleOutput.setVisible(debugToggle.getToggleState());

    // Update display size if in time-based mode to account for tempo changes
    bool isDynamicZoom = (bool)state.getProperty("dynamicZoom");
    if (!isDynamicZoom && audioProcessor.isPlaying)
    {
        updateDisplaySizeFromZoomSlider();
    }

    // Draw the highway
    PPQ trackWindowStartPPQ = currentPlayheadPositionInPPQ();
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
    skillMenu.setBounds(10, 10, 100, 20);
    partMenu.setBounds(120, 10, 100, 20);
    drumTypeMenu.setBounds(230, 10, 100, 20);

    starPowerToggle.setBounds(getWidth() - 120, 10, 100, 20);
    kick2xToggle.setBounds(getWidth() - 120, 30, 100, 20);
    dynamicsToggle.setBounds(getWidth() - 120, 50, 100, 20);

    framerateMenu.setBounds(getWidth() - 120, getHeight() - 30, 100, 20);
    latencyMenu.setBounds(getWidth() - 120, getHeight() - 50, 100, 20);
    
    chartZoomLabel.setBounds(getWidth() - 90, getHeight() - 270, 40, 20);
    chartZoomSliderPPQ.setBounds(getWidth() - 120, getHeight() - 240, 100, 150);
    chartZoomSliderTime.setBounds(getWidth() - 120, getHeight() - 240, 100, 150);
    dynamicZoomToggle.setBounds(getWidth() - 120, getHeight() - 90, 80, 20);

    debugToggle.setBounds(340, 10, 100, 20);

    consoleOutput.setBounds(10, 40, getWidth() - 20, getHeight() - 50);
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
    if (!state.isValid()) { state = juce::ValueTree("state"); }
    
    skillMenu.setSelectedId((int)state.getProperty("skillLevel", 4), juce::dontSendNotification);           // Default Expert
    partMenu.setSelectedId((int)state.getProperty("part", 2), juce::dontSendNotification);                  // Default Drums
    drumTypeMenu.setSelectedId((int)state.getProperty("drumType", 2), juce::dontSendNotification);          // Default Pro Drums
    framerateMenu.setSelectedId((int)state.getProperty("framerate", 3), juce::dontSendNotification);        // Default 60fps
    latencyMenu.setSelectedId((int)state.getProperty("latency", 2), juce::dontSendNotification);            // Default 500ms
    
    starPowerToggle.setToggleState((bool)state.getProperty("starPower", true), juce::dontSendNotification);
    kick2xToggle.setToggleState((bool)state.getProperty("kick2x", true), juce::dontSendNotification);
    dynamicsToggle.setToggleState((bool)state.getProperty("dynamics", true), juce::dontSendNotification);
    dynamicZoomToggle.setToggleState((bool)state.getProperty("dynamicZoom", false), juce::dontSendNotification);
    
    chartZoomSliderPPQ.setValue((double)state.getProperty("zoomPPQ", 1.5), juce::dontSendNotification);     // Default 1.5 PPQ
    chartZoomSliderTime.setValue((double)state.getProperty("zoomTime", 0.8), juce::dontSendNotification);   // Default 0.8 seconds
    
    // Apply functional state that requires side effects
    applyLatencySetting((int)state.getProperty("latency", 2));
    
    // Framerate
    auto framerateValue = (int)state.getProperty("framerate", 3);
    int frameRate = (framerateValue == 1) ? 15 : (framerateValue == 2) ? 30 : 60;
    startTimerHz(frameRate);
    
    // Slider visibility and display size
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
