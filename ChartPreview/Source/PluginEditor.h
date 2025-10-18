/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Midi/MidiInterpreter.h"
#include "Visual/Renderers/HighwayRenderer.h"
#include "Visual/Managers/GridlineGenerator.h"
#include "Utils/Utils.h"
#include "Utils/TimeConverter.h"

//==============================================================================
/**
*/
class ChartPreviewAudioProcessorEditor  :
    public juce::AudioProcessorEditor,
    private juce::ComboBox::Listener,
    private juce::Slider::Listener,
    private juce::ToggleButton::Listener,
    private juce::TextEditor::Listener,
    private juce::Timer
{
public:
    ChartPreviewAudioProcessorEditor (ChartPreviewAudioProcessor&, juce::ValueTree &state);
    ~ChartPreviewAudioProcessorEditor() override;

    //==============================================================================
    void timerCallback() override
    {
        printCallback();

        // Handle debounced track changes
        if (pendingTrackChange >= 0)
        {
            trackChangeDebounceCounter++;
            // Wait for 10 frames (~166ms at 60fps) of no new changes before applying
            if (trackChangeDebounceCounter >= 10)
            {
                // Apply the track change
                state.setProperty("reaperTrack", pendingTrackChange, nullptr);
                audioProcessor.invalidateReaperCache();
                pendingTrackChange = -1;
                trackChangeDebounceCounter = 0;
                repaint();
            }
        }

        bool isReaperMode = audioProcessor.isReaperHost && audioProcessor.getReaperMidiProvider().isReaperApiAvailable();

        // Track position changes for render logic
        if (auto* playHead = audioProcessor.getPlayHead()) {
            auto positionInfo = playHead->getPosition();
            if (positionInfo.hasValue()) {
                PPQ currentPosition = PPQ(positionInfo->getPpqPosition().orFallback(0.0));
                bool isCurrentlyPlaying = positionInfo->getIsPlaying();

                // Update tracked position and playing state
                lastKnownPosition = currentPosition;
                lastPlayingState = isCurrentlyPlaying;

                // In REAPER mode, throttled cache invalidation while paused to pick up MIDI edits in real-time
                // Throttled to ~20 Hz (every 3 frames at 60 FPS) to keep responsiveness without overwhelming the host
                if (isReaperMode && !isCurrentlyPlaying)
                {
                    paused_frameCounterSinceLastInvalidation++;
                    if (paused_frameCounterSinceLastInvalidation >= 3)
                    {
                        paused_frameCounterSinceLastInvalidation = 0;
                        audioProcessor.invalidateReaperCache();
                    }
                }
                else
                {
                    paused_frameCounterSinceLastInvalidation = 0;  // Reset when playing
                }
            }
        }

        repaint();
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
        if (slider == &chartSpeedSlider)
        {
            state.setProperty("speedTime", slider->getValue(), nullptr);
            updateDisplaySizeFromSpeedSlider();

            // In REAPER mode, invalidate cache to immediately fetch new data window
            if (audioProcessor.isReaperHost && audioProcessor.getReaperMidiProvider().isReaperApiAvailable())
            {
                audioProcessor.invalidateReaperCache();
            }

            repaint();
        }
    }

    void buttonClicked(juce::Button * button) override
    {
        if (button == &hitIndicatorsToggle)
        {
            bool buttonState = button->getToggleState();
            state.setProperty("hitIndicators", buttonState ? 1 : 0, nullptr);
        }
        else if (button == &starPowerToggle)
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
        else if (button == &clearLogsButton)
        {
            audioProcessor.clearDebugText();
            consoleOutput.clear();
        }
        audioProcessor.refreshMidiDisplay();
    }

    void textEditorReturnKeyPressed(juce::TextEditor& editor) override
    {
        if (&editor == &reaperTrackInput)
        {
            applyTrackNumberChange();
            // Deselect the text box (unfocus)
            reaperTrackInput.giveAwayKeyboardFocus();
        }
        else if (&editor == &latencyOffsetInput)
        {
            applyLatencyOffsetChange();
            latencyOffsetInput.giveAwayKeyboardFocus();
        }
    }

    void textEditorFocusLost(juce::TextEditor& editor) override
    {
        if (&editor == &reaperTrackInput)
        {
            applyTrackNumberChange();
        }
        else if (&editor == &latencyOffsetInput)
        {
            applyLatencyOffsetChange();
        }
    }

    void textEditorEscapeKeyPressed(juce::TextEditor& editor) override
    {
        if (&editor == &reaperTrackInput)
        {
            // Restore previous value and unfocus
            reaperTrackInput.setText(juce::String((int)state["reaperTrack"]), false);
            reaperTrackInput.giveAwayKeyboardFocus();
        }
        else if (&editor == &latencyOffsetInput)
        {
            // Restore previous value and unfocus
            latencyOffsetInput.setText(juce::String((int)state["latencyOffsetMs"]), false);
            latencyOffsetInput.giveAwayKeyboardFocus();
        }
    }

    void applyTrackNumberChange()
    {
        int trackValue = reaperTrackInput.getText().getIntValue();
        if (trackValue >= 1 && trackValue <= 999)
        {
            pendingTrackChange = trackValue;
            trackChangeDebounceCounter = 0;
            repaint();
        }
        else
        {
            // Invalid value, restore previous
            reaperTrackInput.setText(juce::String((int)state["reaperTrack"]), false);
        }
    }

    void applyLatencyOffsetChange()
    {
        int offsetValue = latencyOffsetInput.getText().getIntValue();

        // Get pipeline type to determine valid range
        bool isReaperMode = audioProcessor.isReaperHost && audioProcessor.getReaperMidiProvider().isReaperApiAvailable();
        int minValue = isReaperMode ? -2000 : 0;
        int maxValue = 2000;

        if (offsetValue >= minValue && offsetValue <= maxValue)
        {
            state.setProperty("latencyOffsetMs", offsetValue, nullptr);
            audioProcessor.refreshMidiDisplay();

            // In REAPER mode, invalidate cache to immediately apply offset
            if (isReaperMode)
            {
                audioProcessor.invalidateReaperCache();
            }

            repaint();
        }
        else
        {
            // Invalid value, restore previous
            latencyOffsetInput.setText(juce::String((int)state["latencyOffsetMs"]), false);
        }
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        // Handle arrow keys for track number when text box has focus
        if (reaperTrackInput.hasKeyboardFocus(true))
        {
            int currentTrack = reaperTrackInput.getText().getIntValue();

            if (key == juce::KeyPress::upKey)
            {
                if (currentTrack < 999)
                {
                    reaperTrackInput.setText(juce::String(currentTrack + 1), false);
                    applyTrackNumberChange();
                }
                return true;
            }
            else if (key == juce::KeyPress::downKey)
            {
                if (currentTrack > 1)
                {
                    reaperTrackInput.setText(juce::String(currentTrack - 1), false);
                    applyTrackNumberChange();
                }
                return true;
            }
        }

        // Handle arrow keys for latency offset when text box has focus
        if (latencyOffsetInput.hasKeyboardFocus(true))
        {
            int currentOffset = latencyOffsetInput.getText().getIntValue();

            // Get pipeline type to determine valid range
            bool isReaperMode = audioProcessor.isReaperHost && audioProcessor.getReaperMidiProvider().isReaperApiAvailable();
            int minValue = isReaperMode ? -2000 : 0;
            int maxValue = 2000;

            if (key == juce::KeyPress::upKey)
            {
                if (currentOffset < maxValue)
                {
                    int newValue = currentOffset + 10;  // Increment by 10ms
                    if (newValue > maxValue) newValue = maxValue;
                    latencyOffsetInput.setText(juce::String(newValue), false);
                    applyLatencyOffsetChange();
                }
                return true;
            }
            else if (key == juce::KeyPress::downKey)
            {
                if (currentOffset > minValue)
                {
                    int newValue = currentOffset - 10;  // Decrement by 10ms
                    if (newValue < minValue) newValue = minValue;
                    latencyOffsetInput.setText(juce::String(newValue), false);
                    applyLatencyOffsetChange();
                }
                return true;
            }
        }

        return false;
    }

    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        // Ignore scroll on text input fields
        if (reaperTrackInput.isMouseOver(true) || latencyOffsetInput.isMouseOver(true))
            return;

        // Get the current playhead position
        if (auto* playHead = audioProcessor.getPlayHead())
        {
            auto positionInfo = playHead->getPosition();
            if (positionInfo.hasValue())
            {
                double currentPPQ = positionInfo->getPpqPosition().orFallback(0.0);
                double jumpBeats = event.mods.isShiftDown() ? SCROLL_SHIFT_BEATS : SCROLL_NORMAL_BEATS;

                // Note: when shift is held, deltaY might be in deltaX instead
                double wheelDelta = wheel.deltaY != 0.0 ? wheel.deltaY : wheel.deltaX;
                double jumpAmount = wheelDelta * jumpBeats;

                double newPPQ = currentPPQ + jumpAmount;
                newPPQ = std::max(0.0, newPPQ);  // Clamp to 0

                audioProcessor.requestTimelinePositionChange(PPQ(newPPQ));
            }
        }
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
    std::unique_ptr<juce::Drawable> reaperLogo;

    juce::Label chartSpeedLabel;
    juce::Label versionLabel;
    juce::Label reaperTrackLabel;
    juce::Label latencyOffsetLabel;
    juce::ComboBox skillMenu, partMenu, drumTypeMenu, framerateMenu, latencyMenu, autoHopoMenu;

    // Custom TextEditor that passes arrow keys to parent
    class TrackNumberEditor : public juce::TextEditor
    {
    public:
        bool keyPressed(const juce::KeyPress& key) override
        {
            // Let parent handle arrow keys for track navigation
            if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
            {
                if (auto* parent = getParentComponent())
                    return parent->keyPressed(key);
            }
            return juce::TextEditor::keyPressed(key);
        }
    };

    TrackNumberEditor reaperTrackInput;
    TrackNumberEditor latencyOffsetInput;
    juce::ToggleButton hitIndicatorsToggle, starPowerToggle, kick2xToggle, dynamicsToggle;
    juce::Slider chartSpeedSlider;

    juce::TextEditor consoleOutput;
    juce::ToggleButton debugToggle;
    juce::TextButton clearLogsButton;

    //==============================================================================

    void initAssets();
    void initMenus();
    void loadState();
    void updateDisplaySizeFromSpeedSlider();
    void applyLatencySetting(int latencyValue);

    void paintReaperMode(juce::Graphics& g);
    void paintStandardMode(juce::Graphics& g);

    float latencyInSeconds = 0.0;
    
    // Resize constraints
    juce::ComponentBoundsConstrainer constrainer;

    // Position tracking for cursor vs playhead separation
    PPQ lastKnownPosition = 0.0;
    bool lastPlayingState = false;

    PPQ displaySizeInPPQ = 1.5; // Only used for MIDI window fetching
    double displayWindowTimeSeconds = 1.0; // Actual render window time in seconds

    // Track change debouncing
    int pendingTrackChange = -1;  // -1 means no pending change
    int trackChangeDebounceCounter = 0;

    // Cache invalidation throttling (for REAPER MIDI edit detection while paused)
    int paused_frameCounterSinceLastInvalidation = 0;  // Throttle to ~20 Hz (every 3 frames at 60 FPS)

    // Scroll wheel timeline control
    static constexpr double SCROLL_NORMAL_BEATS = 2.0;   // Normal scroll: quarter note
    static constexpr double SCROLL_SHIFT_BEATS = 0.5;     // Shift+scroll: full beat

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChartPreviewAudioProcessorEditor)

    // Latency tracking
    PPQ defaultLatencyInPPQ = 0.0;
    double defaultBPM = 120.0;

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
        if (!audioProcessor.debugText.isEmpty())
        {
            consoleOutput.moveCaretToEnd();
            consoleOutput.insertTextAtCaret(audioProcessor.debugText);
            audioProcessor.debugText.clear();
        }
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