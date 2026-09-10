#pragma once

#include <set>
#include <vector>
#include <JuceHeader.h>
#include "../Utils/ChartTypes.h"
#include "../Visual/HighwaySlot.h"
#include "../Midi/Processing/NoteProcessor.h"
#include "../Midi/Discovery/TrackDiscovery.h"
#include "../UI/ToolbarComponent.h"

class ChartchoticAudioProcessor;
class InstrumentSession;

class SessionController
{
public:
    SessionController() = default;

    struct Callbacks {
        std::function<void()> resized;
        std::function<void()> loadState;
        std::function<float(float)> computeFarFadeEnd;
        std::function<void(std::function<void(HighwayComponent&)>)> forEachHighway;
        std::function<void(std::function<void(HighwayComponent&)>)> forAllHighways;
    };

    void init(juce::ValueTree& state,
              ToolbarComponent& toolbar,
              ChartchoticAudioProcessor& processor,
              HighwaySlot* slots, int maxSlots, int& activeSlotCount,
              Callbacks callbacks);

    void rebuildFromSession(InstrumentSession& session);
    void rebuildVisibleSlots();

    void forceSingleInstrument();
    void forceSingleDifficulty();
    void applyVisualState();

    // Accessors for toolbar callback wiring
    bool isActive() const { return hasActiveSession; }
    std::set<Part>& getEnabledParts() { return enabledParts; }
    std::set<SkillLevel>& getEnabledDifficulties() { return enabledDifficulties; }
    const std::vector<Part>& getDiscoveredParts() const { return discoveredParts; }

    // Session slot cache entry
    struct SlotData {
        Part part = Part::GUITAR;
        int trackIdx = 0;
        std::shared_ptr<NoteStateMapArray> noteStateMapArray = std::make_shared<NoteStateMapArray>();
        std::shared_ptr<juce::CriticalSection> noteStateMapLock = std::make_shared<juce::CriticalSection>();
        const DiscoFlipState* discoFlipState = nullptr;
        const StrictHatPedalState* strictHatPedalState = nullptr;
    };

private:
    juce::ValueTree* state = nullptr;
    ToolbarComponent* toolbar = nullptr;
    ChartchoticAudioProcessor* processor = nullptr;
    HighwaySlot* slots = nullptr;
    int maxSlots = 0;
    int* activeSlotCount = nullptr;
    Callbacks cb;

    std::vector<SlotData> slotCache;
    std::vector<Part> discoveredParts;
    std::set<Part> enabledParts;
    std::set<SkillLevel> enabledDifficulties;
    bool hasActiveSession = false;

    void updateSessionData(InstrumentSession& session);
    void updateVisibleSlots();

    void saveEnabledParts();
    void restoreEnabledParts();
    void saveEnabledDifficulties();
    void restoreEnabledDifficulties();
};
