#include "SessionController.h"
#include "../PluginProcessor.h"
#include "../Midi/InstrumentSession.h"
#include "../Visual/Geometry/PositionMath.h"

void SessionController::init(juce::ValueTree& st,
                              ToolbarComponent& tb,
                              ChartchoticAudioProcessor& proc,
                              HighwaySlot* sl, int maxSl, int& activeCount,
                              Callbacks callbacks)
{
    state = &st;
    toolbar = &tb;
    processor = &proc;
    slots = sl;
    maxSlots = maxSl;
    activeSlotCount = &activeCount;
    cb = std::move(callbacks);
}

void SessionController::rebuildFromSession(InstrumentSession& session)
{
    updateSessionData(session);
    rebuildVisibleSlots();
}

void SessionController::updateSessionData(InstrumentSession& session)
{
    slotCache.clear();
    size_t previousDiscoveredCount = discoveredParts.size();
    discoveredParts.clear();
    hasActiveSession = !session.isEmpty();

    if (!hasActiveSession)
    {
        enabledParts.clear();
        toolbar->setDiscoveredParts(discoveredParts);
        toolbar->setEnabledParts(enabledParts);
        return;
    }

    const auto& tracks = session.getTracks();

    for (int i = 0; i < (int)tracks.size(); ++i)
    {
        Part part = tracks[i].part;
        if (!isHighwayRenderable(part))
            continue;

        SlotData data;
        data.part = part;
        data.trackIdx = i;

        const auto& notes = session.getNotes(i);

        if (!notes.empty())
        {
            const juce::ScopedLock lock(*data.noteStateMapLock);
            for (auto& map : *data.noteStateMapArray)
                map.clear();

            NoteProcessor noteProcessor;
            noteProcessor.processAllNotes(notes, *data.noteStateMapArray);
        }

        const auto& discoFlip = session.getDiscoFlipState(i);
        data.discoFlipState = discoFlip.hasRegions() ? &discoFlip : nullptr;

        if (std::find(discoveredParts.begin(), discoveredParts.end(), part) == discoveredParts.end())
            discoveredParts.push_back(part);

        slotCache.push_back(std::move(data));
    }

    // Sort discovered parts: Guitar, guitar alts, Bass, Keys, Drums, Vocals
    std::sort(discoveredParts.begin(), discoveredParts.end(),
              [](Part a, Part b) { return getPartSortOrder(a) < getPartSortOrder(b); });

    // First discovery: restore from state, or enable all parts
    if (enabledParts.empty())
    {
        restoreEnabledParts();

        // Remove any restored parts that aren't in current discovery
        for (auto it = enabledParts.begin(); it != enabledParts.end(); )
        {
            if (std::find(discoveredParts.begin(), discoveredParts.end(), *it) == discoveredParts.end())
                it = enabledParts.erase(it);
            else
                ++it;
        }

        // If still empty (no saved state or all stale), enable everything
        if (enabledParts.empty())
        {
            for (auto p : discoveredParts)
                enabledParts.insert(p);
        }
    }
    else
    {
        // Were all parts enabled before re-discovery? ("All" mode)
        bool wasAllEnabled = enabledParts.size() >= previousDiscoveredCount
                             && previousDiscoveredCount > 0;

        // Remove enabled parts that are no longer discovered
        for (auto it = enabledParts.begin(); it != enabledParts.end(); )
        {
            if (std::find(discoveredParts.begin(), discoveredParts.end(), *it) == discoveredParts.end())
                it = enabledParts.erase(it);
            else
                ++it;
        }

        // If user had all parts enabled, auto-enable newly discovered parts
        if (wasAllEnabled)
        {
            for (auto p : discoveredParts)
                enabledParts.insert(p);
        }
    }

    // First discovery: restore from state, or default to current skill level
    if (enabledDifficulties.empty())
    {
        restoreEnabledDifficulties();

        if (enabledDifficulties.empty())
        {
            SkillLevel current = (SkillLevel)((int)state->getProperty("skillLevel"));
            enabledDifficulties.insert(current);
        }
    }

    // Enable multi-select mode for difficulty in any session mode
    toolbar->enableMultiDifficultyMode(true);

    toolbar->setDiscoveredParts(discoveredParts);
    toolbar->setEnabledParts(enabledParts);
    toolbar->setEnabledDifficulties(enabledDifficulties);
}

void SessionController::forceSingleInstrument()
{
    if (enabledParts.size() <= 1) return;
    Part first = discoveredParts.empty() ? *enabledParts.begin() : discoveredParts[0];
    for (auto p : discoveredParts)
        if (enabledParts.count(p)) { first = p; break; }
    enabledParts.clear();
    enabledParts.insert(first);
    toolbar->setEnabledParts(enabledParts);
}

void SessionController::forceSingleDifficulty()
{
    if (enabledDifficulties.size() <= 1) return;
    SkillLevel first = *enabledDifficulties.begin();
    enabledDifficulties.clear();
    enabledDifficulties.insert(first);
    toolbar->setEnabledDifficulties(enabledDifficulties);
    state->setProperty("skillLevel", (int)first, nullptr);
}

void SessionController::rebuildVisibleSlots()
{
    updateVisibleSlots();
}

void SessionController::updateVisibleSlots()
{
    // Hide all pooled slots
    for (int i = 0; i < maxSlots; i++)
    {
        slots[i].active = false;
        slots[i].highway->setVisible(false);
    }

    if (!hasActiveSession || slotCache.empty())
    {
        if (processor->isReaperHost)
        {
            // REAPER with no instrument tracks — show nothing, editor draws the hint
            *activeSlotCount = 0;
            toolbar->setMultiInstrumentMode(false);
            cb.resized();
            return;
        }

        // Non-REAPER fallback: single default slot with manual instrument selection
        auto& slot = slots[0];
        slot.part = getPartFromState(*state);
        slot.skillLevel = (SkillLevel)(int)state->getProperty("skillLevel");
        slot.active = true;
        slot.ownedState.reset();
        slot.interpreter = std::make_unique<MidiInterpreter>(
            *state, processor->getNoteStateMapArray(), processor->getNoteStateMapLock());
        slot.noteStateMapArray = std::make_shared<NoteStateMapArray>();
        slot.noteStateMapLock = std::make_shared<juce::CriticalSection>();
        slot.highway->setActivePart(slot.part);
        slot.highway->setVisible(true);
        *activeSlotCount = 1;

        toolbar->setMultiInstrumentMode(false);
        cb.resized();
        cb.loadState();
        return;
    }

    // Difficulty order for consistent grid layout (E/M/H/X = top-left → bottom-right)
    static const SkillLevel diffOrder[] = { SkillLevel::EASY, SkillLevel::MEDIUM,
                                             SkillLevel::HARD, SkillLevel::EXPERT };

    // Assign slots from session cache (up to maxSlots)
    int slotIdx = 0;
    for (auto& cached : slotCache)
    {
        if (enabledParts.count(cached.part) == 0)
            continue;

        for (auto level : diffOrder)
        {
            if (enabledDifficulties.count(level) == 0)
                continue;
            if (slotIdx >= maxSlots)
                break;

            auto& slot = slots[slotIdx];
            slot.part = cached.part;
            slot.skillLevel = level;
            slot.active = true;

            // Per-slot state with baked skillLevel for interpreter
            slot.ownedState = std::make_unique<juce::ValueTree>(state->createCopy());
            slot.ownedState->setProperty("part", (int)cached.part, nullptr);
            slot.ownedState->setProperty("skillLevel", (int)level, nullptr);

            // All difficulties share the same raw note data
            slot.noteStateMapArray = cached.noteStateMapArray;
            slot.noteStateMapLock = cached.noteStateMapLock;

            slot.interpreter = std::make_unique<MidiInterpreter>(
                cached.part, *slot.ownedState, *slot.noteStateMapArray, *slot.noteStateMapLock);

            if (cached.discoFlipState)
                slot.interpreter->setDiscoFlipState(cached.discoFlipState);

            slot.highway->setActivePart(cached.part);
            slotIdx++;
        }
    }

    *activeSlotCount = slotIdx;

    // Multi-slot labels
    bool multiInst = enabledParts.size() > 1;
    bool multiDiff = enabledDifficulties.size() > 1;
    for (int i = 0; i < *activeSlotCount; i++)
    {
        slots[i].highway->showPartLabel = multiInst;
        slots[i].highway->showDifficultyLabel = multiDiff;
        slots[i].highway->displaySkillLevel = slots[i].skillLevel;
    }
    toolbar->setMultiInstrumentMode(*activeSlotCount > 1);

    saveEnabledParts();
    saveEnabledDifficulties();

    // Layout only -- no loadState, no rebuildTrack
    cb.resized();

    // Apply visual settings directly (NOT via loadState which triggers disk I/O + full rebuild)
    applyVisualState();

    // Show highways only after data is ready (avoids blank/stale frame flash)
    for (int i = 0; i < *activeSlotCount; i++)
        slots[i].highway->setVisible(true);
}

void SessionController::applyVisualState()
{
    // Propagate render toggle flags from state to active highways
    bool gems = !state->hasProperty("showGems") || (bool)(*state)["showGems"];
    bool bars = !state->hasProperty("showBars") || (bool)(*state)["showBars"];
    bool sustains = !state->hasProperty("showSustains") || (bool)(*state)["showSustains"];
    bool lanes = !state->hasProperty("showLanes") || (bool)(*state)["showLanes"];
    bool gridlines = !state->hasProperty("showGridlines") || (bool)(*state)["showGridlines"];
    bool track = !state->hasProperty("showTrack") || (bool)(*state)["showTrack"];
    bool laneSeps = !state->hasProperty("showLaneSeparators") || (bool)(*state)["showLaneSeparators"];
    bool strikeline = !state->hasProperty("showStrikeline") || (bool)(*state)["showStrikeline"];
    bool hwy = !state->hasProperty("showHighway") || (bool)(*state)["showHighway"];
    bool stretch = state->hasProperty("stretchToFill") && (bool)(*state)["stretchToFill"];

    float userLength = state->hasProperty("highwayLength")
        ? juce::jlimit(FAR_FADE_MIN, FAR_FADE_MAX, (float)(*state)["highwayLength"])
        : FAR_FADE_DEFAULT;
    float scaledLength = cb.computeFarFadeEnd(userLength);

    cb.forEachHighway([=](auto& hw) {
        hw.getSceneRenderer().showGems = gems;
        hw.getSceneRenderer().showBars = bars;
        hw.getSceneRenderer().showSustains = sustains;
        hw.getSceneRenderer().showLanes = lanes;
        hw.getSceneRenderer().showGridlines = gridlines;
        hw.getSceneRenderer().showTrack = track;
        hw.getSceneRenderer().showLaneSeparators = laneSeps;
        hw.getSceneRenderer().showStrikeline = strikeline;
        hw.showHighway = hwy;
        hw.stretchToFill = stretch;
        hw.getSceneRenderer().farFadeEnd = scaledLength;
    });

    // Texture settings (all pooled highways, not just active)
    if (state->hasProperty("textureScale"))
    {
        float ts = (float)(*state)["textureScale"];
        cb.forAllHighways([ts](auto& hw) { hw.getTrackRenderer().textureScale = ts; });
    }
    if (state->hasProperty("textureOpacity"))
    {
        float to = juce::jlimit(0.05f, 1.0f, (float)(*state)["textureOpacity"]);
        cb.forAllHighways([to](auto& hw) { hw.getTrackRenderer().textureOpacity = to; });
    }

    // Point overlays at cache -- rebuildTrack handles this via the cache path
    cb.forEachHighway([](auto& hw) { hw.rebuildTrack(); });
}

void SessionController::saveEnabledParts()
{
    if (state == nullptr) return;
    juce::String s;
    for (auto p : enabledParts)
    {
        if (s.isNotEmpty()) s += ",";
        s += juce::String((int)p);
    }
    state->setProperty("enabledParts", s, nullptr);
}

void SessionController::restoreEnabledParts()
{
    if (state == nullptr || !state->hasProperty("enabledParts")) return;
    juce::String s = state->getProperty("enabledParts").toString();
    if (s.isEmpty()) return;

    juce::StringArray tokens;
    tokens.addTokens(s, ",", "");
    enabledParts.clear();
    for (auto& t : tokens)
        enabledParts.insert((Part)t.getIntValue());
}

void SessionController::saveEnabledDifficulties()
{
    if (state == nullptr) return;
    juce::String s;
    for (auto d : enabledDifficulties)
    {
        if (s.isNotEmpty()) s += ",";
        s += juce::String((int)d);
    }
    state->setProperty("enabledDifficulties", s, nullptr);
}

void SessionController::restoreEnabledDifficulties()
{
    if (state == nullptr || !state->hasProperty("enabledDifficulties")) return;
    juce::String s = state->getProperty("enabledDifficulties").toString();
    if (s.isEmpty()) return;

    juce::StringArray tokens;
    tokens.addTokens(s, ",", "");
    enabledDifficulties.clear();
    for (auto& t : tokens)
        enabledDifficulties.insert((SkillLevel)t.getIntValue());
}
