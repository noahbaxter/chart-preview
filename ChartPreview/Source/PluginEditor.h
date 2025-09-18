/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Midi/MidiInterpreter.h"
#include "Renderers/HighwayRenderer.h"
#include "Utils/Utils.h"

//==============================================================================
/**
*/
class ChartPreviewAudioProcessorEditor  : 
    public juce::AudioProcessorEditor,
    private juce::ComboBox::Listener,
    private juce::Slider::Listener,
    private juce::ToggleButton::Listener,
    private juce::Timer
{
public:
    ChartPreviewAudioProcessorEditor (ChartPreviewAudioProcessor&, juce::ValueTree &state);
    ~ChartPreviewAudioProcessorEditor() override;

    //==============================================================================
    void timerCallback() override
    {
        printCallback();

        // Check for position changes (cursor movement when paused, playhead when playing)
        bool positionChanged = false;

        if (auto* playHead = audioProcessor.getPlayHead()) {
            auto positionInfo = playHead->getPosition();
            if (positionInfo.hasValue()) {
                PPQ currentPosition = PPQ(positionInfo->getPpqPosition().orFallback(0.0));
                bool isCurrentlyPlaying = positionInfo->getIsPlaying();

                // Check if position or playing state changed
                if (currentPosition != lastKnownPosition || isCurrentlyPlaying != lastPlayingState) {
                    lastKnownPosition = currentPosition;
                    lastPlayingState = isCurrentlyPlaying;
                    positionChanged = true;
                }
            }
        }

        // Repaint if playing or if position changed while paused
        if (audioProcessor.isPlaying || positionChanged) {
            repaint();
        }
    }

    void paint (juce::Graphics&) override;
    void resized() override;
    
    // Resizable constraints
    juce::ComponentBoundsConstrainer* getConstrainer();
    void parentSizeChanged() override;

    //==============================================================================
    // UI Callbacks

    void comboBoxChanged(juce::ComboBox *comboBoxThatHasChanged) override
    {
        if (comboBoxThatHasChanged == &skillMenu)
        {
            auto skillValue = skillMenu.getSelectedId();
            state.setProperty("skillLevel", skillValue, nullptr);
        }
        else if (comboBoxThatHasChanged == &partMenu)
        {
            auto partValue = partMenu.getSelectedId();
            state.setProperty("part", partValue, nullptr);
        }
        else if (comboBoxThatHasChanged == &drumTypeMenu)
        {
            auto drumTypeValue = drumTypeMenu.getSelectedId();
            state.setProperty("drumType", drumTypeValue, nullptr);
        }
        else if (comboBoxThatHasChanged == &framerateMenu)
        {
            auto framerateValue = framerateMenu.getSelectedId();
            state.setProperty("framerate", framerateValue, nullptr);
            int frameRate;
            switch (framerateValue)
            {
            case 1:
                frameRate = 15;
                break;
            case 2:
                frameRate = 30;
                break;
            case 3:
                frameRate = 60;
                break;
            case 4:
                frameRate = 120;
                break;
            case 5:
                frameRate = 144;
                break;
            }
            startTimerHz(frameRate);
        }
        else if (comboBoxThatHasChanged == &latencyMenu)
        {
            auto latencyValue = latencyMenu.getSelectedId();
            state.setProperty("latency", latencyValue, nullptr);
            applyLatencySetting(latencyValue);
        }
        else if (comboBoxThatHasChanged == &autoHopoMenu)
        {
            auto autoHopoValue = autoHopoMenu.getSelectedId();
            state.setProperty("autoHopo", autoHopoValue, nullptr);
        }

        audioProcessor.refreshMidiDisplay();
    }

    void sliderValueChanged(juce::Slider *slider) override
    {
        if (slider == &chartZoomSliderPPQ)
        {
            state.setProperty("zoomPPQ", slider->getValue(), nullptr);
            updateDisplaySizeFromZoomSlider();
        }
        else if (slider == &chartZoomSliderTime)
        {
            state.setProperty("zoomTime", slider->getValue(), nullptr);
            updateDisplaySizeFromZoomSlider();
        }
    }

    void buttonClicked(juce::Button * button) override
    {
        if (button == &starPowerToggle)
        {
            bool buttonState = button->getToggleState();
            state.setProperty("starPower", buttonState ? 1 : 0, nullptr);
        }
        else if (button == &kick2xToggle)
        {
            bool buttonState = button->getToggleState();
            state.setProperty("kick2x", buttonState ? 1 : 0, nullptr);
        }
        else if (button == &dynamicsToggle)
        {
            bool buttonState = button->getToggleState();
            state.setProperty("dynamics", buttonState ? 1 : 0, nullptr);
        }
        else if (button == &dynamicZoomToggle)
        {
            bool buttonState = button->getToggleState();
            state.setProperty("dynamicZoom", buttonState ? 1 : 0, nullptr);
            
            updateSliderVisibility();
            updateDisplaySizeFromZoomSlider();
        }
        audioProcessor.refreshMidiDisplay();
    }

private:
    juce::ValueTree& state;

    ChartPreviewAudioProcessor& audioProcessor;
    MidiInterpreter midiInterpreter;
    HighwayRenderer highwayRenderer;

    //==============================================================================
    // UI Elements
    static constexpr int defaultWidth = 800;
    static constexpr int defaultHeight = 600;
    static constexpr double aspectRatio = (double)defaultWidth / defaultHeight;
    static constexpr int minWidth = 400;
    static constexpr int minHeight = 300;

    // Background Assets
    juce::Image backgroundImage;
    juce::Image trackDrumImage;
    juce::Image trackGuitarImage;

    juce::Label chartZoomLabel;
    juce::ComboBox skillMenu, partMenu, drumTypeMenu, framerateMenu, latencyMenu, autoHopoMenu;
    juce::ToggleButton starPowerToggle, kick2xToggle, dynamicsToggle, dynamicZoomToggle;
    juce::Slider chartZoomSliderPPQ, chartZoomSliderTime;

    juce::TextEditor consoleOutput;
    juce::ToggleButton debugToggle;

    //==============================================================================

    void initAssets();
    void initMenus();
    void loadState();
    void updateDisplaySizeFromZoomSlider();
    void updateSliderVisibility();
    void applyLatencySetting(int latencyValue);

    float latencyInSeconds = 0.0;
    
    // Resize constraints
    juce::ComponentBoundsConstrainer constrainer;
    
    // Dirty checking state
    mutable PPQ lastDisplayStartPPQ = 0.0;
    mutable bool lastIsPlaying = false;

    // Position tracking for cursor vs playhead separation
    PPQ lastKnownPosition = 0.0;
    bool lastPlayingState = false;

    PPQ displaySizeInPPQ = 1.5; // 4 beats (1 measure in 4/4)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChartPreviewAudioProcessorEditor)

    // Position helpers

    int currentPlayheadPositionInSamples()
    {
        return audioProcessor.playheadPositionInSamples;
    }

    PPQ currentPlayheadPositionInPPQ()
    {
        return PPQ(audioProcessor.playheadPositionInPPQ);
    }

    PPQ defaultLatencyInPPQ = 0.0;
    double defaultBPM = 120.0;
    PPQ defaultDisplaySizeInPPQ = 4.0; // 4 beats

    PPQ latencyInPPQ()
    {
        if (audioProcessor.getPlayHead() == nullptr) return defaultLatencyInPPQ;

        auto positionInfo = audioProcessor.getPlayHead()->getPosition();
        if (!positionInfo.hasValue()) return defaultLatencyInPPQ;

        double bpm = positionInfo->getBpm().orFallback(defaultBPM);
        return PPQ(audioProcessor.latencyInSeconds * (bpm / 60.0));
    }

    // Multi-buffer smoothing state
    PPQ lastSmoothedLatencyPPQ = 0.0;        // Current smoothed latency value
    PPQ smoothingTargetLatencyPPQ = 0.0;     // Target we're smoothing towards
    PPQ smoothingStartLatencyPPQ = 0.0;      // Starting point of current smooth
    double smoothingProgress = 1.0;          // 0.0 to 1.0, 1.0 means complete
    double smoothingDurationSeconds = 2.0;   // How long to spread adjustment over
    juce::int64 lastSmoothingUpdateTime = 0; // For timing calculations
    void setSmoothingDurationSeconds(double duration) { smoothingDurationSeconds = duration; }
    PPQ smoothedLatencyInPPQ()
    {
        PPQ targetLatency = latencyInPPQ();
        juce::int64 currentTime = juce::Time::getHighResolutionTicks();
        
        // Check if target has changed significantly or this is first frame
        double targetDifference = std::abs((targetLatency - smoothingTargetLatencyPPQ).toDouble());
        bool targetChanged = targetDifference > 0.01;  // Increased threshold to reduce jitter
        bool isFirstFrame = lastSmoothedLatencyPPQ == 0.0;
        
        if (isFirstFrame || targetChanged)
        {
            if (isFirstFrame)
            {
                // Initialize everything to target on first frame
                lastSmoothedLatencyPPQ = targetLatency;
                smoothingTargetLatencyPPQ = targetLatency;
                smoothingStartLatencyPPQ = targetLatency;
                smoothingProgress = 1.0;
                lastSmoothingUpdateTime = currentTime;
                return targetLatency;
            }
            else
            {
                // Start new smoothing transition
                smoothingStartLatencyPPQ = lastSmoothedLatencyPPQ;
                smoothingTargetLatencyPPQ = targetLatency;
                smoothingProgress = 0.0;
                lastSmoothingUpdateTime = currentTime;
            }
        }
        
        // Calculate time elapsed since last update
        double timeElapsedSeconds = (currentTime - lastSmoothingUpdateTime) / static_cast<double>(juce::Time::getHighResolutionTicksPerSecond());
        lastSmoothingUpdateTime = currentTime;
        
        // Update progress based on time elapsed
        if (smoothingProgress < 1.0)
        {
            double progressIncrement = timeElapsedSeconds / smoothingDurationSeconds;
            smoothingProgress = std::min(1.0, smoothingProgress + progressIncrement);
            
            // Use simple linear interpolation for more predictable behavior
            double totalAdjustment = (smoothingTargetLatencyPPQ - smoothingStartLatencyPPQ).toDouble();
            PPQ currentAdjustment = PPQ(totalAdjustment * smoothingProgress);
            lastSmoothedLatencyPPQ = smoothingStartLatencyPPQ + currentAdjustment;
        }
        else
        {
            // Smoothing complete, use target directly
            lastSmoothedLatencyPPQ = smoothingTargetLatencyPPQ;
        }
        
        return lastSmoothedLatencyPPQ;
    }

    //==============================================================================
    // Prints

    void print(const juce::String &line)
    {
        audioProcessor.debugText += line + "\n";
    }

    std::string gemsToString(std::array<Gem, 7> gems)
    {
        std::string str = "(";
        for (const auto &gem : gems)
        {
            str += std::to_string((int)gem) + ",";
        }
        str += ")";
        return str;
    }

    void printCallback()
    {
        consoleOutput.clear();
        // consoleOutput.moveCaretToEnd();

        consoleOutput.insertTextAtCaret(audioProcessor.debugText);
        audioProcessor.debugText.clear();
    }

    // void printMidiMessages()
    // {
    //     auto midiEventMap = audioProcessor.getMidiEventMap();
    //     for (const auto &item : midiEventMap)
    //     {
    //         int index = item.first;
    //         const std::vector<juce::MidiMessage> &messages = item.second;

    //         std::string str = std::to_string(index) + ": " + midiMessagesToString(messages);
    //         print(str);
    //     }
    // }

    std::string midiMessagesToString(const std::vector<juce::MidiMessage> &messages)
    {
        std::string str = "";
        for (const auto &message : messages)
        {
            str += message.getDescription().toStdString() + " ";
        }
        return str;
    }
};