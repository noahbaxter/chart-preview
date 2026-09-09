#pragma once

#include <JuceHeader.h>

/**
    Export preferences that belong to the person, not the project.

    The charter tag is the same on every chart someone makes, so typing it per
    project would be typing it forever. These live in one file per machine, so
    a fresh plugin instance in a brand new project already knows who is
    charting and how they like their exports packaged.

    Per-song values (title, difficulty, artwork) deliberately do not live here.

    The file is opened and closed per access rather than held open. A static
    PropertiesFile outlives JUCE's own shutdown and aborts on the way out when
    its destructor tries to save, which cost an afternoon once already.
*/
class ChartSettings
{
public:
    static juce::String charter()                 { return get("charter"); }
    static void setCharter(const juce::String& v) { set("charter", v); }

    static juce::String icon()                    { return get("icon"); }
    static void setIcon(const juce::String& v)    { set("icon", v); }

    /** Whether a cover dropped on a song with no background makes one. */
    static bool autoBackground()
    {
        juce::PropertiesFile file(options());
        return file.getBoolValue("autoBackground", true);
    }

    static void setAutoBackground(bool v)
    {
        juce::PropertiesFile file(options());
        file.setValue("autoBackground", v);
    }

    /**
        Which background template gets made from a cover, as a BackgroundGenerator
        style index. Set once and it holds: picking a cover for a song with no
        background makes one in this style without being asked again.
    */
    static int backgroundStyle()
    {
        juce::PropertiesFile file(options());
        return file.getIntValue("backgroundStyle", 0);
    }

    static void setBackgroundStyle(int v)
    {
        juce::PropertiesFile file(options());
        file.setValue("backgroundStyle", v);
    }

    /** True when exports are packed into a single .sng rather than a folder. */
    static bool packAsSng()
    {
        juce::PropertiesFile file(options());
        return file.getBoolValue("packAsSng", false);
    }

    static void setPackAsSng(bool v)
    {
        juce::PropertiesFile file(options());
        file.setValue("packAsSng", v);
    }

private:
    static juce::PropertiesFile::Options options()
    {
        juce::PropertiesFile::Options opts;
        opts.applicationName     = "Chartchotic";
        opts.filenameSuffix      = "settings";
        opts.folderName          = "Chartchotic";
        opts.osxLibrarySubFolder = "Application Support";
        return opts;
    }

    static juce::String get(const juce::String& key)
    {
        juce::PropertiesFile file(options());
        return file.getValue(key);
    }

    static void set(const juce::String& key, const juce::String& value)
    {
        juce::PropertiesFile file(options());
        file.setValue(key, value);
    }
};
