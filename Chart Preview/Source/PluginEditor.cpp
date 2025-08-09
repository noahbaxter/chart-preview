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
      midiInterpreter(state, audioProcessor.getNoteStateMapArray()),
      highwayRenderer(state, midiInterpreter)
{
    setSize(defaultWidth, defaultHeight);

    latencyInSeconds = audioProcessor.latencyInSeconds;
    initState();
    initAssets();
    initMenus();

    startTimerHz(60);
}

ChartPreviewAudioProcessorEditor::~ChartPreviewAudioProcessorEditor()
{
}

void ChartPreviewAudioProcessorEditor::initState()
{
    if (!state.isValid())
    {
        state = juce::ValueTree("state");
        state.setProperty("skillLevel", (int)SkillLevel::EXPERT, nullptr);
        state.setProperty("part", (int)Part::DRUMS, nullptr);
        state.setProperty("drumType", (int)DrumType::PRO, nullptr);

        state.setProperty("starPower", true, nullptr);
        state.setProperty("kick2x", true, nullptr);
        state.setProperty("dynamics", true, nullptr);

        state.setProperty("framerate", 3, nullptr);
    }
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
    skillMenu.setSelectedId(state.getProperty("skillLevel"), juce::NotificationType::dontSendNotification);
    skillMenu.addListener(this);
    addAndMakeVisible(skillMenu);

    partMenu.addItemList(partLabels, 1);
    partMenu.setSelectedId(state.getProperty("part"), juce::NotificationType::dontSendNotification);
    partMenu.addListener(this);
    addAndMakeVisible(partMenu);

    drumTypeMenu.addItemList(drumTypeLabels, 1);
    drumTypeMenu.setSelectedId(state.getProperty("drumType"), juce::NotificationType::dontSendNotification);
    drumTypeMenu.addListener(this);
    addAndMakeVisible(drumTypeMenu);

    framerateMenu.addItemList({"15 FPS", "30 FPS", "60 FPS"}, 1);
    framerateMenu.setSelectedId(state.getProperty("framerate"), juce::NotificationType::dontSendNotification);
    framerateMenu.addListener(this);
    addAndMakeVisible(framerateMenu);

    latencyMenu.addItemList({"250ms", "500ms", "750ms", "1000ms"}, 1);
    latencyMenu.setSelectedId(state.getProperty("latency"), juce::NotificationType::dontSendNotification);
    latencyMenu.addListener(this);
    addAndMakeVisible(latencyMenu);
    
    chartZoomSlider.setRange(0.40, 1.2, 0.05);
    chartZoomSlider.setValue(0.60);
    chartZoomSlider.setSliderStyle(juce::Slider::LinearVertical);
    chartZoomSlider.setTextBoxStyle(juce::Slider::TextBoxAbove, false, 50, 20);
    chartZoomSlider.addListener(this);
    addAndMakeVisible(chartZoomSlider);

    chartZoomLabel.setText("Zoom", juce::dontSendNotification);
    addAndMakeVisible(chartZoomLabel);

    // Create toggles
    starPowerToggle.setButtonText("Star Power");
    starPowerToggle.setToggleState(true, juce::NotificationType::dontSendNotification);
    starPowerToggle.addListener(this);
    addAndMakeVisible(starPowerToggle);
    
    kick2xToggle.setButtonText("Kick 2x");
    kick2xToggle.setToggleState(true, juce::NotificationType::dontSendNotification);
    kick2xToggle.addListener(this);
    addAndMakeVisible(kick2xToggle);
    
    dynamicsToggle.setButtonText("Dynamics");
    dynamicsToggle.setToggleState(true, juce::NotificationType::dontSendNotification);
    dynamicsToggle.addListener(this);
    addAndMakeVisible(dynamicsToggle);

    // Debug toggle
    debugToggle.setButtonText("Debug");
    addAndMakeVisible(debugToggle);

    // Create console output
    consoleOutput.setMultiLine(true);
    consoleOutput.setReadOnly(true);
    addAndMakeVisible(consoleOutput);
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

    // Draw the highway
    uint trackWindowStart = currentPlayheadPositionInSamples();
    if (audioProcessor.isPlaying)
    {
        // Shift the start position back by the latency to account for the delay
        trackWindowStart = std::max(0, (int)trackWindowStart - int(latencyInSeconds * audioProcessor.getSampleRate()));
    }
    uint trackWindowEnd = trackWindowStart + displaySizeInSamples;
    highwayRenderer.paint(g, trackWindowStart, trackWindowEnd, displaySizeInSamples);
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
    
    chartZoomLabel.setBounds(getWidth() - 90, getHeight() - 250, 40, 20);
    chartZoomSlider.setBounds(getWidth() - 120, getHeight() - 220, 100, 150);

    debugToggle.setBounds(340, 10, 100, 20);

    consoleOutput.setBounds(10, 40, getWidth() - 20, getHeight() - 50);

    highwayRenderer.width = getWidth();
    highwayRenderer.height = getHeight();
}
