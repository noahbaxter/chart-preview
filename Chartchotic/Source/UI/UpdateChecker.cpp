#include "UpdateChecker.h"

#ifdef CHARTCHOTIC_VERSION_STRING
  #define CHARTCHOTIC_VERSION CHARTCHOTIC_VERSION_STRING
#elif defined(JucePlugin_VersionString)
  #define CHARTCHOTIC_VERSION JucePlugin_VersionString
#else
  #define CHARTCHOTIC_VERSION "dev"
#endif

UpdateChecker::UpdateChecker() : Thread("UpdateChecker") {}

UpdateChecker::~UpdateChecker()
{
    cancelCallbacks();
    stopThread(5000);
}

void UpdateChecker::checkForUpdates()
{
    if (checking.load())
        return;

    checking.store(true);
    startThread(juce::Thread::Priority::low);
}

UpdateChecker::UpdateInfo UpdateChecker::getLatestUpdateInfo() const
{
    juce::ScopedLock sl(lock);
    return lastResult;
}

bool UpdateChecker::isChecking() const
{
    return checking.load();
}

void UpdateChecker::run()
{
    UpdateInfo result;

    juce::String channel(CHARTCHOTIC_BUILD_CHANNEL);
    if (channel == "DEV")
        result = checkDevChannel();
    else if (channel == "BETA")
        result = checkBetaChannel();
    else
        result = checkReleaseChannel();

    {
        juce::ScopedLock sl(lock);
        lastResult = result;
    }

    checking.store(false);

    if (callbacksEnabled->load() && onUpdateCheckComplete)
    {
        auto callback = onUpdateCheckComplete;
        auto enabled = callbacksEnabled;  // shared_ptr copy, outlives this
        juce::MessageManager::callAsync([callback, result, enabled]()
        {
            if (enabled->load() && callback)
                callback(result);
        });
    }
}

UpdateChecker::UpdateInfo UpdateChecker::checkReleaseChannel()
{
    UpdateInfo info;

    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withHttpRequestCmd("GET")
                       .withExtraHeaders("Accept: application/vnd.github+json\r\nUser-Agent: Chartchotic-UpdateChecker");

    // Only check /releases/latest — this skips prereleases by design
    auto stream = juce::URL(juce::String(GITHUB_API_BASE) + "/releases/latest")
                      .createInputStream(options);

    if (stream == nullptr)
        return info;

    auto json = juce::JSON::parse(stream->readEntireStreamAsString());
    if (!json.isObject())
        return info;

    auto tagName = json.getProperty("tag_name", "").toString();
    if (tagName.isEmpty())
        return info;

    juce::String remoteVersion = tagName.startsWith("v") ? tagName.substring(1) : tagName;
    juce::String localVersion(CHARTCHOTIC_VERSION);

    bool shouldUpdate;
#ifdef DEBUG
    // Debug: show banner if version differs at all
    shouldUpdate = (remoteVersion != localVersion);
#else
    // Release: only show if remote is strictly newer
    shouldUpdate = isNewerVersion(remoteVersion, localVersion);
#endif

    if (shouldUpdate)
    {
        info.available = true;
        info.version = tagName;
        info.releaseNotes = json.getProperty("body", "").toString();
        info.displayMessage = "Chartchotic " + tagName + " is available.";
        info.downloadUrl = "https://github.com/noahbaxter/chartchotic/releases/latest";
    }

    return info;
}

UpdateChecker::UpdateInfo UpdateChecker::checkBetaChannel()
{
    UpdateInfo info;

    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withHttpRequestCmd("GET")
                       .withExtraHeaders("Accept: application/vnd.github+json\r\nUser-Agent: Chartchotic-UpdateChecker");

    juce::String localVersion(CHARTCHOTIC_VERSION);
    auto localBase = localVersion.upToFirstOccurrenceOf("-", false, false);

    auto stableStream = juce::URL(juce::String(GITHUB_API_BASE) + "/releases/latest")
                            .createInputStream(options);
    if (stableStream != nullptr)
    {
        auto json = juce::JSON::parse(stableStream->readEntireStreamAsString());
        if (json.isObject())
        {
            auto tagName = json.getProperty("tag_name", "").toString();
            auto remoteVersion = tagName.startsWith("v") ? tagName.substring(1) : tagName;

            if (!remoteVersion.isEmpty() && isNewerVersion(remoteVersion, localBase))
            {
                info.available = true;
                info.version = tagName;
                info.releaseNotes = json.getProperty("body", "").toString();
                info.channelLabel = "Stable Release";
                info.displayMessage = "Chartchotic " + tagName + " is available.";
                info.downloadUrl = "https://github.com/noahbaxter/chartchotic/releases/latest";
                return info;
            }
        }
    }

    auto betaStream = juce::URL(juce::String(GITHUB_API_BASE) + "/releases/tags/beta-latest")
                          .createInputStream(options);
    if (betaStream == nullptr)
        return info;

    auto json = juce::JSON::parse(betaStream->readEntireStreamAsString());
    if (!json.isObject())
        return info;

    auto title = json.getProperty("name", "").toString();
    auto openParen = title.indexOfChar('(');
    auto closeParen = title.indexOfChar(')');
    if (openParen < 0 || closeParen <= openParen)
        return info;

    auto remoteFullVersion = title.substring(openParen + 1, closeParen);

    auto setBetaUpdate = [&]()
    {
        info.available = true;
        info.version = remoteFullVersion;
        info.channelLabel = "Beta Update";
        info.displayMessage = "A new beta build is available.";
        info.downloadUrl = json.getProperty("html_url", "").toString();
    };

#ifdef DEBUG
    if (remoteFullVersion != localVersion)
        setBetaUpdate();
#else
    auto remoteBase = remoteFullVersion.upToFirstOccurrenceOf("-", false, false);

    if (remoteBase != localBase)
    {
        if (isNewerVersion(remoteBase, localBase))
            setBetaUpdate();
        return info;
    }

    auto localBetaSuffix = localVersion.fromFirstOccurrenceOf("-beta.", false, false);
    auto remoteBetaSuffix = remoteFullVersion.fromFirstOccurrenceOf("-beta.", false, false);

    if (localBetaSuffix.isEmpty() || remoteBetaSuffix.isEmpty())
        return info;

    if (remoteBetaSuffix.getIntValue() > localBetaSuffix.getIntValue())
        setBetaUpdate();
#endif

    return info;
}

UpdateChecker::UpdateInfo UpdateChecker::checkDevChannel()
{
    UpdateInfo info;
    juce::URL url(juce::String(GITHUB_API_BASE) + "/releases/tags/dev-latest");

    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withHttpRequestCmd("GET")
                       .withExtraHeaders("Accept: application/vnd.github+json\r\nUser-Agent: Chartchotic-UpdateChecker");

    auto stream = url.createInputStream(options);
    if (stream == nullptr)
        return info;

    auto response = stream->readEntireStreamAsString();
    auto json = juce::JSON::parse(response);

    if (!json.isObject())
        return info;

    // Extract version from title like "Dev Build (0.9.5-dev.20260226123456.abc1234)"
    auto title = json.getProperty("name", "").toString();
    auto openParen = title.indexOfChar('(');
    auto closeParen = title.indexOfChar(')');
    if (openParen < 0 || closeParen <= openParen)
        return info;

    auto remoteFullVersion = title.substring(openParen + 1, closeParen);
    juce::String localVersion(CHARTCHOTIC_VERSION);

#ifdef DEBUG
    // Debug: show banner if version string differs at all
    if (remoteFullVersion != localVersion)
    {
        info.available = true;
        info.version = remoteFullVersion;
        info.downloadUrl = json.getProperty("html_url", "").toString();
    }
#else
    // Split into base semver and dev suffix: "0.9.5-dev.20260226123456.abc1234"
    auto remoteBase = remoteFullVersion.upToFirstOccurrenceOf("-", false, false);
    auto localBase = localVersion.upToFirstOccurrenceOf("-", false, false);

    // If base versions differ, use semver comparison
    if (remoteBase != localBase)
    {
        if (isNewerVersion(remoteBase, localBase))
        {
            info.available = true;
            info.version = remoteFullVersion;
            info.downloadUrl = json.getProperty("html_url", "").toString();
        }
        return info;
    }

    // Same base version — compare dev timestamps if both have them
    // Local builds without a timestamp (e.g. "0.9.5") are local dev builds, skip update
    auto localDevSuffix = localVersion.fromFirstOccurrenceOf("-dev.", false, false);
    if (localDevSuffix.isEmpty())
        return info;

    auto remoteDevSuffix = remoteFullVersion.fromFirstOccurrenceOf("-dev.", false, false);
    if (remoteDevSuffix.isEmpty())
        return info;

    // Timestamps are YYYYMMDDHHMMSS — lexicographic comparison works
    if (remoteDevSuffix > localDevSuffix)
    {
        info.available = true;
        info.version = remoteFullVersion;
        info.downloadUrl = json.getProperty("html_url", "").toString();
    }
#endif

    return info;
}

bool UpdateChecker::isNewerVersion(const juce::String& remote, const juce::String& local) const
{
    auto remoteParts = juce::StringArray::fromTokens(remote, ".", "");
    auto localParts = juce::StringArray::fromTokens(local, ".", "");

    for (int i = 0; i < std::max(remoteParts.size(), localParts.size()); ++i)
    {
        int r = (i < remoteParts.size()) ? remoteParts[i].getIntValue() : 0;
        int l = (i < localParts.size()) ? localParts[i].getIntValue() : 0;

        if (r > l) return true;
        if (r < l) return false;
    }

    return false;
}
