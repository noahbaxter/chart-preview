/*
  ==============================================================================

    Logger.h
    Centralized debug logging with category-based filtering

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <map>
#include <functional>

namespace DebugTools {

enum class LogCategory {
    ReaperAPI,       // REAPER API calls and responses
    MidiPipeline,    // MIDI pipeline processing
    MidiComparison,  // MIDI buffer vs timeline comparison
    Performance,     // Performance metrics and timing
    Cache,           // Cache operations
    General          // General debug messages
};

/**
 * Centralized logger with category-based filtering.
 * All debug logging should go through this instead of raw print() calls.
 */
class Logger {
public:
    Logger(std::function<void(const juce::String&)> printFunc = nullptr)
        : print(printFunc)
    {
        // Default: all categories disabled
        for (int i = 0; i <= (int)LogCategory::General; i++) {
            flags[(LogCategory)i] = false;
        }
    }

    // Log a message if the category is enabled
    void log(LogCategory category, const juce::String& message) {
        if (print && isEnabled(category)) {
            print(getCategoryPrefix(category) + message);
        }
    }

    // Enable/disable a specific category
    void enable(LogCategory category, bool enabled = true) {
        flags[category] = enabled;
    }

    // Check if a category is enabled
    bool isEnabled(LogCategory category) const {
        auto it = flags.find(category);
        return it != flags.end() && it->second;
    }

    // Enable all categories
    void enableAll() {
        for (auto& pair : flags) {
            pair.second = true;
        }
    }

    // Disable all categories
    void disableAll() {
        for (auto& pair : flags) {
            pair.second = false;
        }
    }

    // Set the print callback
    void setPrintCallback(std::function<void(const juce::String&)> printFunc) {
        print = printFunc;
    }

private:
    std::function<void(const juce::String&)> print;
    std::map<LogCategory, bool> flags;

    juce::String getCategoryPrefix(LogCategory category) const {
        switch (category) {
            case LogCategory::ReaperAPI:      return "[REAPER] ";
            case LogCategory::MidiPipeline:   return "[PIPELINE] ";
            case LogCategory::MidiComparison: return "[COMPARE] ";
            case LogCategory::Performance:    return "[PERF] ";
            case LogCategory::Cache:          return "[CACHE] ";
            case LogCategory::General:        return "[DEBUG] ";
            default:                          return "[LOG] ";
        }
    }
};

} // namespace DebugTools
