#include "IconPreview.h"
#include "../UI/Theme.h"

namespace
{
    // The official repository, where the game itself gets these.
    const char* const kTreeApi =
        "https://gitlab.com/api/v4/projects/clonehero%2Fsources/repository/tree?path=public/icons&per_page=100&page=";
    const char* const kRawBase = "https://gitlab.com/clonehero/sources/-/raw/master/public/icons/";
    const char* const kGalleryUrl = "https://clonehero.gitlab.io/sources";

    // Long enough that typing a name does not fire a request per letter.
    constexpr int kDebounceMs = 600;
    constexpr int kTimeoutMs = 5000;
    constexpr int kMaxIndexPages = 20;

    // The index is a list of filenames, and filenames change when someone adds
    // an icon, which is rare. A week old is fine; a stale name resolving is a
    // better failure than a lookup that needs the network every time.
    constexpr double kIndexTtlDays = 7.0;

    /**
        Beside the settings file. userApplicationDataDirectory is ~/Library on
        macOS, not ~/Library/Application Support, so the extra hop is what
        keeps this next to ChartSettings rather than one level above it.
    */
    juce::File iconCacheDirectory()
    {
        auto root = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
       #if JUCE_MAC
        root = root.getChildFile("Application Support");
       #endif
        return root.getChildFile("Chartchotic").getChildFile("icons");
    }

    juce::File indexFile() { return iconCacheDirectory().getChildFile("index.txt"); }

    juce::URL::InputStreamOptions streamOptions()
    {
        return juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                 .withConnectionTimeoutMs(kTimeoutMs);
    }

    /**
        Every icon filename in the repository.

        A filename index rather than a single guessed URL, because the name in
        song.ini and the name on disk routinely disagree in case: 40 of the 287
        names the repository itself documents differ from their own file only
        by case (its `sources.txt` says `a2z`, the file is `A2Z.png`). Five
        icons are .jpg too. Guessing "<name>.png" reports "no such icon" for
        roughly one name in seven that exists perfectly well.
    */
    juce::StringArray fetchIndex()
    {
        juce::StringArray names;

        for (int page = 1; page <= kMaxIndexPages; ++page)
        {
            auto stream = juce::URL(juce::String(kTreeApi) + juce::String(page))
                            .createInputStream(streamOptions());
            if (stream == nullptr) break;

            auto parsed = juce::JSON::parse(stream->readEntireStreamAsString());
            auto* entries = parsed.getArray();
            if (entries == nullptr || entries->isEmpty()) break;

            for (const auto& entry : *entries)
                names.add(entry.getProperty("name", {}).toString());

            if (entries->size() < 100) break;   // last page
        }

        return names;
    }

    juce::StringArray loadIndex()
    {
        auto file = indexFile();
        const bool fresh = file.existsAsFile()
                        && (juce::Time::getCurrentTime() - file.getLastModificationTime()).inDays() < kIndexTtlDays;

        if (fresh)
        {
            juce::StringArray cached;
            cached.addLines(file.loadFileAsString());
            cached.removeEmptyStrings();
            if (!cached.isEmpty()) return cached;
        }

        auto fetched = fetchIndex();
        if (!fetched.isEmpty())
        {
            file.getParentDirectory().createDirectory();
            file.replaceWithText(fetched.joinIntoString("\n"), false, false, "\n");
            return fetched;
        }

        // Network is down and the cache is stale: stale beats nothing.
        juce::StringArray stale;
        stale.addLines(file.loadFileAsString());
        stale.removeEmptyStrings();
        return stale;
    }

    /** The filename a typed name resolves to, exact case preferred. */
    juce::String resolve(const juce::StringArray& index, const juce::String& typed)
    {
        for (const auto& file : index)
            if (file.upToLastOccurrenceOf(".", false, false) == typed)
                return file;

        for (const auto& file : index)
            if (file.upToLastOccurrenceOf(".", false, false).equalsIgnoreCase(typed))
                return file;

        return {};
    }
}

IconPreview::IconPreview()
    : juce::Thread("IconPreview")
{
    setInterceptsMouseClicks(false, false);
}

IconPreview::~IconPreview()
{
    stopTimer();
    stopThread(kTimeoutMs);
}

juce::URL IconPreview::browseUrl()
{
    return juce::URL(kGalleryUrl);
}

void IconPreview::setIconName(const juce::String& name)
{
    auto trimmed = name.trim();
    {
        const juce::ScopedLock lock(requestLock);
        if (trimmed == requested) return;
        requested = trimmed;
    }

    if (trimmed.isEmpty())
    {
        stopTimer();
        state = State::empty;
        icon = {};
        displayed = {};
        repaint();
        return;
    }

    state = State::looking;
    repaint();
    startTimer(kDebounceMs);
}

void IconPreview::timerCallback()
{
    stopTimer();
    // A lookup already running is for an older name, so it is abandoned rather
    // than allowed to land after this one.
    stopThread(kTimeoutMs);
    startThread(juce::Thread::Priority::low);
}

void IconPreview::run()
{
    juce::String name;
    {
        const juce::ScopedLock lock(requestLock);
        name = requested;
    }
    if (name.isEmpty()) return;

    auto index = loadIndex();
    if (threadShouldExit()) return;

    const bool haveIndex = !index.isEmpty();
    const auto filename = resolve(index, name);

    juce::Image loaded;
    if (filename.isNotEmpty())
    {
        auto cached = iconCacheDirectory().getChildFile(filename);
        if (cached.existsAsFile())
            loaded = juce::ImageFileFormat::loadFrom(cached);

        if (!loaded.isValid())
        {
            auto url = juce::URL(juce::String(kRawBase) + juce::URL::addEscapeChars(filename, false));
            if (auto stream = url.createInputStream(streamOptions()))
            {
                juce::MemoryBlock data;
                stream->readIntoMemoryBlock(data);
                loaded = juce::ImageFileFormat::loadFrom(data.getData(), data.getSize());
                if (loaded.isValid())
                {
                    cached.getParentDirectory().createDirectory();
                    cached.replaceWithData(data.getData(), data.getSize());
                }
            }
        }
    }

    if (threadShouldExit()) return;

    juce::Component::SafePointer<IconPreview> safe(this);
    juce::MessageManager::callAsync([safe, name, filename, loaded, haveIndex]() mutable
    {
        if (safe == nullptr) return;
        // A name typed since this started wins; this result is stale.
        if (safe->requested != name) return;

        safe->displayed = filename;
        safe->icon = loaded;
        safe->state = !haveIndex        ? State::offline
                    : filename.isEmpty() ? State::missing
                    : loaded.isValid()   ? State::found
                                         : State::offline;
        safe->repaint();
    });
}

void IconPreview::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    auto box = bounds.removeFromLeft(getHeight());

    g.setColour(juce::Colour(Theme::darkBgLighter));
    g.fillRect(box);
    if (icon.isValid())
        g.drawImage(icon, box.reduced(2).toFloat(), juce::RectanglePlacement::centred);
    g.setColour(juce::Colours::white.withAlpha(Theme::borderAlpha));
    g.drawRect(box);

    juce::String message;
    juce::Colour colour = juce::Colour(Theme::textDim);
    switch (state)
    {
        case State::empty:   message = "no icon"; break;
        case State::looking: message = "checking"; break;
        case State::missing: message = "no such icon"; colour = juce::Colour(Theme::coral); break;
        // Worth distinguishing: a name that cannot be checked is not a name
        // that is wrong, and refusing to export over a dropped connection
        // would be its own bug.
        case State::offline: message = "cannot check"; break;
        case State::found:
            colour = juce::Colour(Theme::green);
            // The resolved filename, since it is often cased differently from
            // what was typed and that is worth seeing.
            message = displayed;
            break;
    }

    g.setColour(colour);
    g.setFont(Theme::getUIFont((float)getHeight() * 0.34f));
    g.drawText(message, bounds.reduced(6, 0), juce::Justification::centredLeft, true);
}
