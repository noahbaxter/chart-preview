#include "GemCalculator.h"
#include "InstrumentMapper.h"
#include "MidiConstants.h"

Gem GemCalculator::getGuitarGemType(uint pitch, PPQ position, juce::ValueTree& state,
                                    NoteStateMapArray& noteStateMapArray,
                                    juce::CriticalSection& noteStateMapLock)
{
    using Guitar = MidiPitchDefinitions::Guitar;
    SkillLevel skill = (SkillLevel)((int)state.getProperty("skillLevel"));
    std::vector<uint> guitarPitches = InstrumentMapper::getGuitarPitchesForSkill(skill);
    uint currentColumn = InstrumentMapper::getGuitarColumn(pitch, skill);

    // First, determine if this note is part of a chord
    bool isPartOfChord = false;
    for (uint otherPitch : guitarPitches)
    {
        if (otherPitch != pitch)
        {
            const juce::ScopedLock lock(noteStateMapLock);
            auto& noteStateMap = noteStateMapArray[otherPitch];

            PPQ startRange = position - MIDI_CHORD_TOLERANCE;
            PPQ endRange = position + MIDI_CHORD_TOLERANCE;

            auto lower = noteStateMap.lower_bound(startRange);
            auto upper = noteStateMap.upper_bound(endRange);

            for (auto it = lower; it != upper; ++it)
            {
                if (it->second.velocity > 0)
                {
                    isPartOfChord = true;
                    break;
                }
            }

            if (isPartOfChord) break;
        }
    }

    // Check for explicit modifiers
    bool modStrum = false;
    bool modTap = false;
    bool modHOPO = false;

    {
        const juce::ScopedLock lock(noteStateMapLock);

        auto checkModifier = [&](uint modPitch) -> bool {
            auto& noteStateMap = noteStateMapArray[modPitch];
            auto it = noteStateMap.upper_bound(position);
            if (it == noteStateMap.begin())
                return false;
            --it;
            return it->second.velocity > 0;
        };

        modStrum = (checkModifier((int)Guitar::EASY_STRUM) && skill == SkillLevel::EASY) ||
                   (checkModifier((int)Guitar::MEDIUM_STRUM) && skill == SkillLevel::MEDIUM) ||
                   (checkModifier((int)Guitar::HARD_STRUM) && skill == SkillLevel::HARD) ||
                   (checkModifier((int)Guitar::EXPERT_STRUM) && skill == SkillLevel::EXPERT);

        modTap = checkModifier((int)Guitar::TAP);

        modHOPO = (checkModifier((int)Guitar::EASY_HOPO) && skill == SkillLevel::EASY) ||
                  (checkModifier((int)Guitar::MEDIUM_HOPO) && skill == SkillLevel::MEDIUM) ||
                  (checkModifier((int)Guitar::HARD_HOPO) && skill == SkillLevel::HARD) ||
                  (checkModifier((int)Guitar::EXPERT_HOPO) && skill == SkillLevel::EXPERT);
    }

    if (modStrum)
    {
        return Gem::NOTE;
    }
    else if (modTap)
    {
        return Gem::TAP_ACCENT;
    }
    else if (modHOPO)
    {
        return Gem::HOPO_GHOST;
    }
    else // No explicit modifiers
    {
        // Chords are ALWAYS strums unless forced (cannot be auto-HOPO)
        if (isPartOfChord)
        {
            return Gem::NOTE;
        }
        // Only single notes can be Auto HOPOs
        else if (shouldBeAutoHOPO(pitch, position, state, noteStateMapArray, noteStateMapLock))
        {
            return Gem::HOPO_GHOST;
        }

        return Gem::NOTE;
    }
}

Gem GemCalculator::getDrumGemType(uint pitch, PPQ position, Dynamic dynamic,
                                  juce::ValueTree& state,
                                  NoteStateMapArray& noteStateMapArray,
                                  juce::CriticalSection& noteStateMapLock)
{
    using Drums = MidiPitchDefinitions::Drums;
    bool dynamicsEnabled = (bool)state.getProperty("dynamics");
    bool isProDrums = (DrumType)((int)state.getProperty("drumType")) == DrumType::PRO;

    Drums note = (Drums)pitch;
    SkillLevel skill = (SkillLevel)((int)state.getProperty("skillLevel"));

    // Determine if this should be a cymbal (pro drums only)
    bool cymbal = false;
    if (isProDrums)
    {
        const juce::ScopedLock lock(noteStateMapLock);

        auto checkModifier = [&](uint modPitch) -> bool {
            auto& noteStateMap = noteStateMapArray[modPitch];
            auto it = noteStateMap.upper_bound(position);
            if (it == noteStateMap.begin())
                return false;
            --it;
            return it->second.velocity > 0;
        };

        if ((note == Drums::EASY_YELLOW || note == Drums::MEDIUM_YELLOW ||
             note == Drums::HARD_YELLOW || note == Drums::EXPERT_YELLOW))
        {
            cymbal = !checkModifier((uint)Drums::TOM_YELLOW);
        }
        else if ((note == Drums::EASY_BLUE || note == Drums::MEDIUM_BLUE ||
                  note == Drums::HARD_BLUE || note == Drums::EXPERT_BLUE))
        {
            cymbal = !checkModifier((uint)Drums::TOM_BLUE);
        }
        else if ((note == Drums::EASY_GREEN || note == Drums::MEDIUM_GREEN ||
                  note == Drums::HARD_GREEN || note == Drums::EXPERT_GREEN))
        {
            cymbal = !checkModifier((uint)Drums::TOM_GREEN);
        }
    }

    // Kicks can't have dynamics
    bool canHaveDynamics = dynamicsEnabled && !InstrumentMapper::isDrumKick(pitch);

    return getDrumGlyph(cymbal, canHaveDynamics, dynamic);
}

bool GemCalculator::shouldBeAutoHOPO(uint pitch, PPQ position, juce::ValueTree& state,
                                     NoteStateMapArray& noteStateMapArray,
                                     juce::CriticalSection& noteStateMapLock)
{
    // Check if Auto HOPOs are enabled
    HopoMode hopoMode = (HopoMode)((int)state.getProperty("autoHopo", 1)); // Default Off
    if (hopoMode == HopoMode::OFF) return false;

    // Maximum distance from previous note to qualify as auto HOPO
    PPQ threshold = PPQ(0.0);
    switch (hopoMode)
    {
        case HopoMode::SIXTEENTH:
            threshold = MIDI_HOPO_SIXTEENTH;
            break;
        case HopoMode::DOT_SIXTEENTH:
            threshold = MIDI_HOPO_SIXTEENTH_DOT;
            break;
        case HopoMode::CLASSIC_170:
            threshold = MIDI_HOPO_CLASSIC_170;
            break;
        case HopoMode::EIGHTH:
            threshold = MIDI_HOPO_EIGHTH;
            break;
        default:
            return false;
    }

    threshold += MIDI_HOPO_THRESHOLD_BUFFER; // Small buffer to account for rounding errors

    using Guitar = MidiPitchDefinitions::Guitar;
    SkillLevel skill = (SkillLevel)((int)state.getProperty("skillLevel"));

    // Get valid guitar pitches for current skill level
    std::vector<uint> guitarPitches = InstrumentMapper::getGuitarPitchesForSkill(skill);

    // Check if this pitch is a valid guitar note
    bool isGuitarNote = std::find(guitarPitches.begin(), guitarPitches.end(), pitch) != guitarPitches.end();
    if (!isGuitarNote) return false;

    uint currentColumn = InstrumentMapper::getGuitarColumn(pitch, skill);
    if (currentColumn >= LANE_COUNT) return false;

    // Look for the most recent note before this position
    PPQ searchStart = position - threshold;
    PPQ mostRecentNotePPQ = PPQ(-1000.0); // Very old timestamp
    uint mostRecentNoteColumn = LANE_COUNT; // Invalid column
    bool foundChord = false;

    const juce::ScopedLock lock(noteStateMapLock);

    // Search all guitar pitches for recent notes
    for (uint searchPitch : guitarPitches)
    {
        const NoteStateMap& noteStateMap = noteStateMapArray[searchPitch];

        // Find notes in the time window before current position
        auto it = noteStateMap.lower_bound(searchStart);
        while (it != noteStateMap.end() && it->first < position)
        {
            if (it->second.velocity > 0) // Note ON event
            {
                PPQ notePPQ = it->first;
                uint noteColumn = InstrumentMapper::getGuitarColumn(searchPitch, skill);

                // Check if this is more recent than our current most recent
                if (notePPQ > mostRecentNotePPQ)
                {
                    mostRecentNotePPQ = notePPQ;
                    mostRecentNoteColumn = noteColumn;
                    foundChord = false; // Reset chord flag for new timestamp
                }
                else if (notePPQ == mostRecentNotePPQ && noteColumn != mostRecentNoteColumn)
                {
                    // Multiple notes at same timestamp = chord
                    foundChord = true;
                }
            }
            ++it;
        }
    }

    // Auto HOPO rules:
    // 1. Must be within threshold of previous note
    // 2. Previous note must be single note (not chord)
    // 3. Current note must be single note (checked in calling code)
    // 4. Must be different column than previous note

    if (mostRecentNotePPQ <= searchStart) return false; // No recent note found
    if (foundChord) return false; // Previous note was part of a chord
    if (mostRecentNoteColumn == currentColumn) return false; // Same color as previous

    return true;
}

Gem GemCalculator::getDrumGlyph(bool cymbal, bool dynamicsEnabled, Dynamic dynamic)
{
    if (dynamicsEnabled)
    {
        switch (dynamic)
        {
            case Dynamic::GHOST:
                return cymbal ? Gem::CYM_GHOST : Gem::HOPO_GHOST;
                break;
            case Dynamic::ACCENT:
                return cymbal ? Gem::CYM_ACCENT : Gem::TAP_ACCENT;
                break;
            default:
                return cymbal ? Gem::CYM : Gem::NOTE;
                break;
        }
    }
    else
    {
        return cymbal ? Gem::CYM : Gem::NOTE;
    }
}
