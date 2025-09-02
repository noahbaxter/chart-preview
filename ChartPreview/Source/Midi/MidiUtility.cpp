#include "MidiUtility.h"

uint MidiUtility::getGuitarGemColumn(uint pitch, juce::ValueTree &state)
{
    using Guitar = MidiPitchDefinitions::Guitar;
    SkillLevel skill = (SkillLevel)((int)state.getProperty("skillLevel"));
    Guitar note = (Guitar)pitch;
    
    // Open notes - column 0
    if ((note == Guitar::EASY_OPEN && skill == SkillLevel::EASY) ||
        (note == Guitar::MEDIUM_OPEN && skill == SkillLevel::MEDIUM) ||
        (note == Guitar::HARD_OPEN && skill == SkillLevel::HARD) ||
        (note == Guitar::EXPERT_OPEN && skill == SkillLevel::EXPERT))
    {
        return 0;
    }
    // Green notes - column 1
    else if ((note == Guitar::EASY_GREEN && skill == SkillLevel::EASY) ||
             (note == Guitar::MEDIUM_GREEN && skill == SkillLevel::MEDIUM) ||
             (note == Guitar::HARD_GREEN && skill == SkillLevel::HARD) ||
             (note == Guitar::EXPERT_GREEN && skill == SkillLevel::EXPERT))
    {
        return 1;
    }
    // Red notes - column 2
    else if ((note == Guitar::EASY_RED && skill == SkillLevel::EASY) ||
             (note == Guitar::MEDIUM_RED && skill == SkillLevel::MEDIUM) ||
             (note == Guitar::HARD_RED && skill == SkillLevel::HARD) ||
             (note == Guitar::EXPERT_RED && skill == SkillLevel::EXPERT))
    {
        return 2;
    }
    // Yellow notes - column 3
    else if ((note == Guitar::EASY_YELLOW && skill == SkillLevel::EASY) ||
             (note == Guitar::MEDIUM_YELLOW && skill == SkillLevel::MEDIUM) ||
             (note == Guitar::HARD_YELLOW && skill == SkillLevel::HARD) ||
             (note == Guitar::EXPERT_YELLOW && skill == SkillLevel::EXPERT))
    {
        return 3;
    }
    // Blue notes - column 4
    else if ((note == Guitar::EASY_BLUE && skill == SkillLevel::EASY) ||
             (note == Guitar::MEDIUM_BLUE && skill == SkillLevel::MEDIUM) ||
             (note == Guitar::HARD_BLUE && skill == SkillLevel::HARD) ||
             (note == Guitar::EXPERT_BLUE && skill == SkillLevel::EXPERT))
    {
        return 4;
    }
    // Orange notes - column 5
    else if ((note == Guitar::EASY_ORANGE && skill == SkillLevel::EASY) ||
             (note == Guitar::MEDIUM_ORANGE && skill == SkillLevel::MEDIUM) ||
             (note == Guitar::HARD_ORANGE && skill == SkillLevel::HARD) ||
             (note == Guitar::EXPERT_ORANGE && skill == SkillLevel::EXPERT))
    {
        return 5;
    }
    
    return LANE_COUNT; // Invalid/unknown pitch
}

uint MidiUtility::getDrumGemColumn(uint pitch, juce::ValueTree &state)
{
    using Drums = MidiPitchDefinitions::Drums;
    SkillLevel skill = (SkillLevel)((int)state.getProperty("skillLevel"));
    bool kick2xEnabled = (bool)state.getProperty("kick2x");
    
    Drums note = (Drums)pitch;
    if ((note == Drums::EASY_KICK && skill == SkillLevel::EASY) ||
        (note == Drums::MEDIUM_KICK && skill == SkillLevel::MEDIUM) ||
        (note == Drums::HARD_KICK && skill == SkillLevel::HARD) ||
        (note == Drums::EXPERT_KICK && skill == SkillLevel::EXPERT))
    {
        return 0;
    }
    else if ((note == Drums::EASY_RED && skill == SkillLevel::EASY) ||
             (note == Drums::MEDIUM_RED && skill == SkillLevel::MEDIUM) ||
             (note == Drums::HARD_RED && skill == SkillLevel::HARD) ||
             (note == Drums::EXPERT_RED && skill == SkillLevel::EXPERT))
    {
        return 1;
    }
    else if ((note == Drums::EASY_YELLOW && skill == SkillLevel::EASY) ||
             (note == Drums::MEDIUM_YELLOW && skill == SkillLevel::MEDIUM) ||
             (note == Drums::HARD_YELLOW && skill == SkillLevel::HARD) ||
             (note == Drums::EXPERT_YELLOW && skill == SkillLevel::EXPERT))
    {
        return 2;
    }
    else if ((note == Drums::EASY_BLUE && skill == SkillLevel::EASY) ||
             (note == Drums::MEDIUM_BLUE && skill == SkillLevel::MEDIUM) ||
             (note == Drums::HARD_BLUE && skill == SkillLevel::HARD) ||
             (note == Drums::EXPERT_BLUE && skill == SkillLevel::EXPERT))
    {
        return 3;
    }
    else if ((note == Drums::EASY_GREEN && skill == SkillLevel::EASY) ||
             (note == Drums::MEDIUM_GREEN && skill == SkillLevel::MEDIUM) ||
             (note == Drums::HARD_GREEN && skill == SkillLevel::HARD) ||
             (note == Drums::EXPERT_GREEN && skill == SkillLevel::EXPERT))
    {
        return 4;
    }
    else if (kick2xEnabled && note == Drums::EXPERT_KICK_2X && skill == SkillLevel::EXPERT)
    {
        return 6;
    }

    return LANE_COUNT; // Invalid/unknown pitch
}

bool MidiUtility::isNoteHeld(uint pitch, PPQ position, NoteStateMapArray &noteStateMapArray, juce::CriticalSection &noteStateMapLock)
{
    const juce::ScopedLock lock(noteStateMapLock);
    auto &noteStateMap = noteStateMapArray[pitch];
    auto it = noteStateMap.upper_bound(position);
    if (it == noteStateMap.begin())
    {
        return false;
    }
    else
    {
        --it;
        return it->second.velocity > 0;
    }
}

bool MidiUtility::isNoteHeldWithTolerance(uint pitch, PPQ position, NoteStateMapArray &noteStateMapArray, juce::CriticalSection &noteStateMapLock)
{
    const juce::ScopedLock lock(noteStateMapLock);
    auto &noteStateMap = noteStateMapArray[pitch];
    
    PPQ startRange = position - CHORD_TOLERANCE;
    PPQ endRange = position + CHORD_TOLERANCE;
    
    auto lower = noteStateMap.lower_bound(startRange);
    auto upper = noteStateMap.upper_bound(endRange);
    
    for (auto it = lower; it != upper; ++it)
    {
        if (it->second.velocity > 0)
        {
            return true;
        }
    }
    
    return false;
}

std::vector<uint> MidiUtility::getGuitarPitchesForSkill(SkillLevel skill)
{
    using Guitar = MidiPitchDefinitions::Guitar;
    switch (skill)
    {
        case SkillLevel::EASY:
            return {(uint)Guitar::EASY_OPEN, (uint)Guitar::EASY_GREEN, (uint)Guitar::EASY_RED, (uint)Guitar::EASY_YELLOW, (uint)Guitar::EASY_BLUE, (uint)Guitar::EASY_ORANGE};
        case SkillLevel::MEDIUM:
            return {(uint)Guitar::MEDIUM_OPEN, (uint)Guitar::MEDIUM_GREEN, (uint)Guitar::MEDIUM_RED, (uint)Guitar::MEDIUM_YELLOW, (uint)Guitar::MEDIUM_BLUE, (uint)Guitar::MEDIUM_ORANGE};
        case SkillLevel::HARD:
            return {(uint)Guitar::HARD_OPEN, (uint)Guitar::HARD_GREEN, (uint)Guitar::HARD_RED, (uint)Guitar::HARD_YELLOW, (uint)Guitar::HARD_BLUE, (uint)Guitar::HARD_ORANGE};
        case SkillLevel::EXPERT:
            return {(uint)Guitar::EXPERT_OPEN, (uint)Guitar::EXPERT_GREEN, (uint)Guitar::EXPERT_RED, (uint)Guitar::EXPERT_YELLOW, (uint)Guitar::EXPERT_BLUE, (uint)Guitar::EXPERT_ORANGE};
    }
    return {}; // Empty vector for invalid skill level
}

bool MidiUtility::isDrumKick(uint pitch)
{
    using Drums = MidiPitchDefinitions::Drums;
    Drums note = (Drums)pitch;
    return (note == Drums::EASY_KICK || 
            note == Drums::MEDIUM_KICK || 
            note == Drums::HARD_KICK || 
            note == Drums::EXPERT_KICK || 
            note == Drums::EXPERT_KICK_2X);
}

bool MidiUtility::isWithinChordTolerance(PPQ position1, PPQ position2)
{
    PPQ diff = (position1 > position2) ? (position1 - position2) : (position2 - position1);
    return diff <= CHORD_TOLERANCE;
}

bool MidiUtility::isSustainedModifierPitch(uint pitch)
{
    using Guitar = MidiPitchDefinitions::Guitar;
    using Drums = MidiPitchDefinitions::Drums;
    
    // Guitar modifiers (all sustained)
    if (pitch == (uint)Guitar::SP ||
        pitch == (uint)Guitar::TAP ||
        pitch == (uint)Guitar::EXPERT_STRUM || pitch == (uint)Guitar::EXPERT_HOPO ||
        pitch == (uint)Guitar::HARD_STRUM || pitch == (uint)Guitar::HARD_HOPO ||
        pitch == (uint)Guitar::MEDIUM_STRUM || pitch == (uint)Guitar::MEDIUM_HOPO ||
        pitch == (uint)Guitar::EASY_STRUM || pitch == (uint)Guitar::EASY_HOPO ||
        pitch == (uint)Guitar::LANE_1 || pitch == (uint)Guitar::LANE_2)
    {
        return true;
    }
    
    // Drum modifiers (all sustained)
    if (pitch == (uint)Drums::SP ||
        pitch == (uint)Drums::TOM_GREEN || pitch == (uint)Drums::TOM_BLUE || pitch == (uint)Drums::TOM_YELLOW ||
        pitch == (uint)Drums::LANE_1 || pitch == (uint)Drums::LANE_2)
    {
        return true;
    }
    
    return false;
}

Gem MidiUtility::getGuitarGemType(uint pitch, PPQ position, juce::ValueTree &state, NoteStateMapArray &noteStateMapArray, juce::CriticalSection &noteStateMapLock)
{
    using Guitar = MidiPitchDefinitions::Guitar;
    SkillLevel skill = (SkillLevel)((int)state.getProperty("skillLevel"));
    
    bool modStrum = (isNoteHeld((int)Guitar::EASY_STRUM, position, noteStateMapArray, noteStateMapLock) && skill == SkillLevel::EASY) ||
                    (isNoteHeld((int)Guitar::MEDIUM_STRUM, position, noteStateMapArray, noteStateMapLock) && skill == SkillLevel::MEDIUM) ||
                    (isNoteHeld((int)Guitar::HARD_STRUM, position, noteStateMapArray, noteStateMapLock) && skill == SkillLevel::HARD) ||
                    (isNoteHeld((int)Guitar::EXPERT_STRUM, position, noteStateMapArray, noteStateMapLock) && skill == SkillLevel::EXPERT);
    bool modTap = isNoteHeld((int)Guitar::TAP, position, noteStateMapArray, noteStateMapLock);
    bool modHOPO = (isNoteHeld((int)Guitar::EASY_HOPO, position, noteStateMapArray, noteStateMapLock) && skill == SkillLevel::EASY) ||
                   (isNoteHeld((int)Guitar::MEDIUM_HOPO, position, noteStateMapArray, noteStateMapLock) && skill == SkillLevel::MEDIUM) ||
                   (isNoteHeld((int)Guitar::HARD_HOPO, position, noteStateMapArray, noteStateMapLock) && skill == SkillLevel::HARD) ||
                   (isNoteHeld((int)Guitar::EXPERT_HOPO, position, noteStateMapArray, noteStateMapLock) && skill == SkillLevel::EXPERT);

    // Check for explicit modifiers first
    if(modStrum)
    {
        return Gem::NOTE;
    }
    else if(modTap)
    {
        return Gem::TAP_ACCENT;
    }
    else if(modHOPO)
    {
        return Gem::HOPO_GHOST;
    }
    else
    {
        // No explicit modifiers - check for Auto HOPO
        // First check if this note is part of a chord (multiple notes at same timestamp)
        bool isPartOfChord = false;
        uint currentColumn = getGuitarGemColumn(pitch, state);
        
        // Get valid guitar pitches for current skill level using helper function
        std::vector<uint> guitarPitches = getGuitarPitchesForSkill(skill);
        
        // Check if any other guitar note is held within chord tolerance
        for (uint otherPitch : guitarPitches)
        {
            if (otherPitch != pitch && isNoteHeldWithTolerance(otherPitch, position, noteStateMapArray, noteStateMapLock))
            {
                isPartOfChord = true;
                break;
            }
        }
        
        // Only single notes can be Auto HOPOs
        if (!isPartOfChord && shouldBeAutoHOPO(pitch, position, state, noteStateMapArray, noteStateMapLock))
        {
            return Gem::HOPO_GHOST;
        }
        
        return Gem::NOTE;
    }
}

Gem MidiUtility::getDrumGemType(uint pitch, PPQ position, Dynamic dynamic, juce::ValueTree &state, NoteStateMapArray &noteStateMapArray, juce::CriticalSection &noteStateMapLock)
{
    using Drums = MidiPitchDefinitions::Drums;
    bool dynamicsEnabled = (bool)state.getProperty("dynamics");
    bool isProDrums = (DrumType)((int)state.getProperty("drumType")) == DrumType::PRO;
    
    Drums note = (Drums)pitch;
    
    // Determine if this should be a cymbal (pro drums only)
    bool cymbal = false;
    if (isProDrums) {
        if ((note == Drums::EASY_YELLOW || note == Drums::MEDIUM_YELLOW || 
             note == Drums::HARD_YELLOW || note == Drums::EXPERT_YELLOW)) {
            cymbal = !isNoteHeld((uint)Drums::TOM_YELLOW, position, noteStateMapArray, noteStateMapLock);
        }
        else if ((note == Drums::EASY_BLUE || note == Drums::MEDIUM_BLUE || 
                  note == Drums::HARD_BLUE || note == Drums::EXPERT_BLUE)) {
            cymbal = !isNoteHeld((uint)Drums::TOM_BLUE, position, noteStateMapArray, noteStateMapLock);
        }
        else if ((note == Drums::EASY_GREEN || note == Drums::MEDIUM_GREEN || 
                  note == Drums::HARD_GREEN || note == Drums::EXPERT_GREEN)) {
            cymbal = !isNoteHeld((int)Drums::TOM_GREEN, position, noteStateMapArray, noteStateMapLock);
        }
    }
    
    // Kicks can't have dynamics - use helper function
    bool canHaveDynamics = dynamicsEnabled && !isDrumKick(pitch);
    
    return getDrumGlyph(cymbal, canHaveDynamics, dynamic);
}

bool MidiUtility::shouldBeAutoHOPO(uint pitch, PPQ position, juce::ValueTree &state, NoteStateMapArray &noteStateMapArray, juce::CriticalSection &noteStateMapLock)
{
    // Check if Auto HOPOs are enabled
    HopoMode hopoMode = (HopoMode)((int)state.getProperty("autoHopo", 1)); // Default Off
    if (hopoMode == HopoMode::OFF) return false;
    
    // Maximum distance from previous note to qualify as auto HOPO
    PPQ threshold = PPQ(0.0);
    switch (hopoMode)
    {
        case HopoMode::SIXTEENTH:
            threshold = TICK_120_SIXTEENTH;
            break;
        case HopoMode::DOT_SIXTEENTH:
            threshold = TICK_160_SIXTEENTH_DOT;
            break;
        case HopoMode::CLASSIC_170:
            threshold = TICK_170;
            break;
        case HopoMode::EIGHTH:
            threshold = TICK_240_EIGTH;
            break;
        default:
            return false;
    }

    threshold += PPQ(0.01); // Small buffer to account for rounding errors
    
    using Guitar = MidiPitchDefinitions::Guitar;
    SkillLevel skill = (SkillLevel)((int)state.getProperty("skillLevel"));
    
    // Get valid guitar pitches for current skill level using helper function
    std::vector<uint> guitarPitches = getGuitarPitchesForSkill(skill);
    
    // Check if this pitch is a valid guitar note
    bool isGuitarNote = std::find(guitarPitches.begin(), guitarPitches.end(), pitch) != guitarPitches.end();
    if (!isGuitarNote) return false;
    
    uint currentColumn = getGuitarGemColumn(pitch, state);
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
                uint noteColumn = getGuitarGemColumn(searchPitch, state);
                
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