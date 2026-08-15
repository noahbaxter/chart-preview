#pragma once

#include <JuceHeader.h>

/**
    Shows the charter icon a typed name resolves to, and says so when it
    resolves to nothing.

    Deliberately not a browser. Clone Hero's icon repository holds 900-odd
    icons and grows whenever someone adds one, so shipping a copy would mean
    maintaining a copy, and building a picker over the live list would mean
    owning a gallery, its paging, and its cache. A charter sets this field once
    and never touches it again.

    What actually goes wrong is typing it slightly wrong: the game silently
    shows no icon, and you find out from a screenshot weeks later. So the only
    job here is confirming the name resolves, which one request answers.
    Browsing is the repository's own preview page, in a real browser.
*/
class IconPreview : public juce::Component,
                    private juce::Thread,
                    private juce::Timer
{
public:
    enum class State { empty, looking, found, missing, offline };

    IconPreview();
    ~IconPreview() override;

    /** The gallery, for the Browse button. */
    static juce::URL browseUrl();

    /** Looks the name up, after a pause so it is not fetched per keystroke. */
    void setIconName(const juce::String& name);

    void paint(juce::Graphics& g) override;

    /** False while a lookup is pending or running. */
    bool settled() const { return state != State::looking; }

private:
    void run() override;
    void timerCallback() override;

    static juce::URL iconUrl(const juce::String& name);
    static juce::File cacheFile(const juce::String& name);

    juce::String requested;
    juce::String displayed;
    juce::Image icon;
    State state = State::empty;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IconPreview)
};
