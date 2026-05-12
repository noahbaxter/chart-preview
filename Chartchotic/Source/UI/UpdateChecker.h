#pragma once

#include <JuceHeader.h>

#ifndef CHARTCHOTIC_BUILD_CHANNEL
  #define CHARTCHOTIC_BUILD_CHANNEL "RELEASE"
#endif

class UpdateChecker : private juce::Thread
{
public:
    struct UpdateInfo
    {
        bool available = false;
        juce::String version;
        juce::String downloadUrl;
        juce::String releaseNotes;
        juce::String channelLabel;
        juce::String displayMessage;
    };

    UpdateChecker();
    ~UpdateChecker() override;

    void checkForUpdates();
    UpdateInfo getLatestUpdateInfo() const;
    bool isChecking() const;

    std::function<void(const UpdateInfo&)> onUpdateCheckComplete;

    // Must be called before destruction to prevent async callbacks on dead objects
    void cancelCallbacks() { callbacksEnabled->store(false); }

private:
    void run() override;

    UpdateInfo checkReleaseChannel();
    UpdateInfo checkBetaChannel();
    UpdateInfo checkDevChannel();

    bool isNewerVersion(const juce::String& remote, const juce::String& local) const;

    mutable juce::CriticalSection lock;
    UpdateInfo lastResult;
    std::atomic<bool> checking{false};
    std::shared_ptr<std::atomic<bool>> callbacksEnabled = std::make_shared<std::atomic<bool>>(true);

    static constexpr const char* GITHUB_API_BASE = "https://api.github.com/repos/noahbaxter/chartchotic";

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UpdateChecker)
};
