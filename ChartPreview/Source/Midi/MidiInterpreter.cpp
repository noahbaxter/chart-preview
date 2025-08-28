/*
  ==============================================================================

    MidiInterpreter.cpp
    Created: 15 Jun 2024 3:57:52pm
    Author:  Noah Baxter

  ==============================================================================
*/

#include "MidiInterpreter.h"

MidiInterpreter::MidiInterpreter(juce::ValueTree &state, NoteStateMapArray &noteStateMapArray, GridlineMap &gridlineMap, juce::CriticalSection &gridlineMapLock, juce::CriticalSection &noteStateMapLock)
    : noteStateMapArray(noteStateMapArray),
      gridlineMap(gridlineMap),
      gridlineMapLock(gridlineMapLock),
      noteStateMapLock(noteStateMapLock),
      state(state)
{
}

MidiInterpreter::~MidiInterpreter()
{
}

GridlineMap MidiInterpreter::generateGridlineWindow(PPQ trackWindowStart, PPQ trackWindowEnd)
{
    GridlineMap gridlineWindow;
    
    // Lock the gridlineMap during iteration to prevent crashes
    const juce::ScopedLock lock(gridlineMapLock);
    
    // Simply read from the pre-generated gridlineMap
    for (const auto& gridlineItem : gridlineMap)
    {
        PPQ gridlinePPQ = gridlineItem.first;
        Gridline gridlineType = static_cast<Gridline>(gridlineItem.second);
        
        // Only include gridlines within our window
        if (gridlinePPQ >= trackWindowStart && gridlinePPQ < trackWindowEnd)
        {
            gridlineWindow[gridlinePPQ] = gridlineType;
        }
    }
    
    return gridlineWindow;
}

TrackWindow MidiInterpreter::generateTrackWindow(PPQ trackWindowStart, PPQ trackWindowEnd)
{
    TrackWindow trackWindow;

    const juce::ScopedLock lock(noteStateMapLock);

    for (uint pitch = 0; pitch < 128; pitch++)
    {
        const NoteStateMap& noteStateMap = noteStateMapArray[pitch];
        auto it = noteStateMap.lower_bound(trackWindowStart);
        while (it != noteStateMap.end() && it->first < trackWindowEnd)
        {
            Dynamic dynamic = (Dynamic)it->second;
            // Skip note offs
            if(dynamic == Dynamic::NONE)
            {
                ++it;
                continue;
            }

            PPQ position = it->first;
            
            // If the trackWindow at this position is empty, generate an empty frame
            if (trackWindow[position].empty())
            {
                trackWindow[position] = generateEmptyTrackFrame();
            }

            if (isPart(state, Part::GUITAR))
            {
                addGuitarEventToFrame(trackWindow[position], position, pitch);
            }
            else // if (isPart(state, Part::DRUMS))
            {
                addDrumEventToFrame(trackWindow[position], position, pitch, dynamic);
            }
            ++it;
        }
    }

    return trackWindow;
}

SustainWindow MidiInterpreter::generateSustainWindow(PPQ trackWindowStart, PPQ trackWindowEnd, PPQ latencyBufferEnd)
{
    SustainWindow sustainWindow;
    
    // Only process guitar sustains for now
    if (isPart(state, Part::DRUMS) || isPart(state, Part::REAL_DRUMS))
    {
        return sustainWindow;
    }
    
    // Lock the noteStateMapArray during iteration to prevent crashes
    const juce::ScopedLock lock(noteStateMapLock);
    
    PPQ MIN_SUSTAIN_LENGTH = PPQ(0.25);
    
    for (uint pitch = 0; pitch < 128; pitch++)
    {
        const NoteStateMap& noteStateMap = noteStateMapArray[pitch];
        
        for (auto it = noteStateMap.begin(); it != noteStateMap.end(); ++it)
        {
            PPQ notePPQ = it->first;
            uint velocity = it->second;
            
            // Only process note-on events (velocity > 0)
            if (velocity == 0) continue;
            
            // Find the corresponding note-off (velocity == 0)
            auto nextIt = std::next(it);
            while (nextIt != noteStateMap.end() && nextIt->second != 0)
            {
                ++nextIt;
            }

            // If corresponding note off exists within the map
            PPQ noteOffPPQ = (nextIt != noteStateMap.end()) ? 
                nextIt->first :                                 // Use found note-off PPQ
                std::min(notePPQ + (trackWindowEnd - trackWindowStart), latencyBufferEnd);  // Cap to latency buffer end
            
            PPQ duration = noteOffPPQ - notePPQ;
            
            // Create sustain if it meets criteria
            if (duration >= MIN_SUSTAIN_LENGTH && 
                notePPQ < trackWindowEnd && 
                noteOffPPQ > trackWindowStart) {
                
                uint gemColumn = getGuitarGemColumn(pitch);
                if (gemColumn < LANE_COUNT) {
                    SustainEvent sustain;
                    sustain.startPPQ = notePPQ;
                    sustain.endPPQ = noteOffPPQ;
                    sustain.gemColumn = gemColumn;
                    sustain.gemType = getGuitarGemType(pitch, notePPQ);
                    sustainWindow.push_back(sustain);
                }
            }
        }
    }
    
    return sustainWindow;
}

TrackFrame MidiInterpreter::generateEmptyTrackFrame()
{
    TrackFrame frame = {
                   //| Guitar  |   Drums   |   Real Drums
                   //| -------------------------------------
        Gem::NONE, //| Open    |   Kick    |
        Gem::NONE, //| Fret 1  |   Lane 1  |
        Gem::NONE, //| Fret 2  |   Lane 2  |
        Gem::NONE, //| Fret 3  |   Lane 3  |
        Gem::NONE, //| Fret 4  |   Lane 4  |
        Gem::NONE, //| Fret 5  |   Lane 5  |
        Gem::NONE, //| Fret 6  |   2x Kick |
    };

    return frame;
}

void MidiInterpreter::addGuitarEventToFrame(TrackFrame &frame, PPQ position, uint pitch)
{
    uint gemColumn = getGuitarGemColumn(pitch);
    if (gemColumn < LANE_COUNT) {
        Gem gem = getGuitarGemType(pitch, position);
        frame[gemColumn] = gem;
    }
}

void MidiInterpreter::addDrumEventToFrame(TrackFrame &frame, PPQ position, uint pitch, Dynamic dynamic)
{
    uint gemColumn = getDrumGemColumn(pitch);
    if (gemColumn < LANE_COUNT) {
        Gem gem = getDrumGemType(pitch, position, dynamic);
        frame[gemColumn] = gem;
    }
}

uint MidiInterpreter::getGuitarGemColumn(uint pitch)
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

Gem MidiInterpreter::getGuitarGemType(uint pitch, PPQ position)
{
    using Guitar = MidiPitchDefinitions::Guitar;
    SkillLevel skill = (SkillLevel)((int)state.getProperty("skillLevel"));
    
    bool modStrum = (isNoteHeld((int)Guitar::EASY_STRUM, position) && skill == SkillLevel::EASY) ||
                    (isNoteHeld((int)Guitar::MEDIUM_STRUM, position) && skill == SkillLevel::MEDIUM) ||
                    (isNoteHeld((int)Guitar::HARD_STRUM, position) && skill == SkillLevel::HARD) ||
                    (isNoteHeld((int)Guitar::EXPERT_STRUM, position) && skill == SkillLevel::EXPERT);
    bool modTap = isNoteHeld((int)Guitar::TAP, position);
    bool modHOPO = (isNoteHeld((int)Guitar::EASY_HOPO, position) && skill == SkillLevel::EASY) ||
                   (isNoteHeld((int)Guitar::MEDIUM_HOPO, position) && skill == SkillLevel::MEDIUM) ||
                   (isNoteHeld((int)Guitar::HARD_HOPO, position) && skill == SkillLevel::HARD) ||
                   (isNoteHeld((int)Guitar::EXPERT_HOPO, position) && skill == SkillLevel::EXPERT);

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
        uint currentColumn = getGuitarGemColumn(pitch);
        
        // Get valid guitar pitches for current skill level
        std::vector<uint> guitarPitches;
        switch (skill)
        {
            case SkillLevel::EASY:
                guitarPitches = {(uint)Guitar::EASY_GREEN, (uint)Guitar::EASY_RED, (uint)Guitar::EASY_YELLOW, (uint)Guitar::EASY_BLUE, (uint)Guitar::EASY_ORANGE};
                break;
            case SkillLevel::MEDIUM:
                guitarPitches = {(uint)Guitar::MEDIUM_GREEN, (uint)Guitar::MEDIUM_RED, (uint)Guitar::MEDIUM_YELLOW, (uint)Guitar::MEDIUM_BLUE, (uint)Guitar::MEDIUM_ORANGE};
                break;
            case SkillLevel::HARD:
                guitarPitches = {(uint)Guitar::HARD_GREEN, (uint)Guitar::HARD_RED, (uint)Guitar::HARD_YELLOW, (uint)Guitar::HARD_BLUE, (uint)Guitar::HARD_ORANGE};
                break;
            case SkillLevel::EXPERT:
                guitarPitches = {(uint)Guitar::EXPERT_GREEN, (uint)Guitar::EXPERT_RED, (uint)Guitar::EXPERT_YELLOW, (uint)Guitar::EXPERT_BLUE, (uint)Guitar::EXPERT_ORANGE};
                break;
        }
        
        // Check if any other guitar note is held at the same timestamp
        for (uint otherPitch : guitarPitches)
        {
            if (otherPitch != pitch && isNoteHeld(otherPitch, position))
            {
                isPartOfChord = true;
                break;
            }
        }
        
        // Only single notes can be Auto HOPOs
        if (!isPartOfChord && shouldBeAutoHOPO(pitch, position))
        {
            return Gem::HOPO_GHOST;
        }
        
        return Gem::NOTE;
    }
}

bool MidiInterpreter::shouldBeAutoHOPO(uint pitch, PPQ position)
{
    // Check if Auto HOPOs are enabled
    HopoMode hopoMode = (HopoMode)((int)state.getProperty("autoHopo", 1)); // Default Off
    if (hopoMode == HopoMode::OFF) return false;
    
    // Get threshold based on HOPO mode using resolution-based calculations
    // Standard MIDI resolution is 480 ticks per quarter note (PPQ)
    const double MIDI_RESOLUTION = 480.0;
    PPQ threshold = PPQ(0.0);
    
    switch (hopoMode)
    {
        case HopoMode::GH12_SIXTEENTH:
            // 1/16th note = 120 ticks at 480 resolution
            threshold = PPQ(120.0 / MIDI_RESOLUTION); 
            break;
        case HopoMode::CLASSIC_170:
            // 170 ticks at 480 resolution
            threshold = PPQ(170.0 / MIDI_RESOLUTION);
            break;
        case HopoMode::MODERN_FORMULA:
            // (resolution/3) + 1 ticks = ((480/3)+1)/480 beats
            threshold = PPQ(((MIDI_RESOLUTION / 3.0) + 1.0) / MIDI_RESOLUTION);
            break;
        default:
            return false;
    }
    
    using Guitar = MidiPitchDefinitions::Guitar;
    SkillLevel skill = (SkillLevel)((int)state.getProperty("skillLevel"));
    
    // Get valid guitar pitches for current skill level
    std::vector<uint> guitarPitches;
    switch (skill)
    {
        case SkillLevel::EASY:
            guitarPitches = {(uint)Guitar::EASY_GREEN, (uint)Guitar::EASY_RED, (uint)Guitar::EASY_YELLOW, (uint)Guitar::EASY_BLUE, (uint)Guitar::EASY_ORANGE};
            break;
        case SkillLevel::MEDIUM:
            guitarPitches = {(uint)Guitar::MEDIUM_GREEN, (uint)Guitar::MEDIUM_RED, (uint)Guitar::MEDIUM_YELLOW, (uint)Guitar::MEDIUM_BLUE, (uint)Guitar::MEDIUM_ORANGE};
            break;
        case SkillLevel::HARD:
            guitarPitches = {(uint)Guitar::HARD_GREEN, (uint)Guitar::HARD_RED, (uint)Guitar::HARD_YELLOW, (uint)Guitar::HARD_BLUE, (uint)Guitar::HARD_ORANGE};
            break;
        case SkillLevel::EXPERT:
            guitarPitches = {(uint)Guitar::EXPERT_GREEN, (uint)Guitar::EXPERT_RED, (uint)Guitar::EXPERT_YELLOW, (uint)Guitar::EXPERT_BLUE, (uint)Guitar::EXPERT_ORANGE};
            break;
    }
    
    // Check if this pitch is a valid guitar note
    bool isGuitarNote = std::find(guitarPitches.begin(), guitarPitches.end(), pitch) != guitarPitches.end();
    if (!isGuitarNote) return false;
    
    uint currentColumn = getGuitarGemColumn(pitch);
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
            if (it->second) // Note ON event
            {
                PPQ notePPQ = it->first;
                uint noteColumn = getGuitarGemColumn(searchPitch);
                
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

uint MidiInterpreter::getDrumGemColumn(uint pitch)
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

Gem MidiInterpreter::getDrumGemType(uint pitch, PPQ position, Dynamic dynamic)
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
            cymbal = !isNoteHeld((uint)Drums::TOM_YELLOW, position);
        }
        else if ((note == Drums::EASY_BLUE || note == Drums::MEDIUM_BLUE || 
                  note == Drums::HARD_BLUE || note == Drums::EXPERT_BLUE)) {
            cymbal = !isNoteHeld((uint)Drums::TOM_BLUE, position);
        }
        else if ((note == Drums::EASY_GREEN || note == Drums::MEDIUM_GREEN || 
                  note == Drums::HARD_GREEN || note == Drums::EXPERT_GREEN)) {
            cymbal = !isNoteHeld((int)Drums::TOM_GREEN, position);
        }
    }
    
    // Kicks can't have dynamics
    bool canHaveDynamics = dynamicsEnabled && 
        !(note == Drums::EASY_KICK || 
          note == Drums::MEDIUM_KICK || 
          note == Drums::HARD_KICK || 
          note == Drums::EXPERT_KICK || 
          note == Drums::EXPERT_KICK_2X);
    
    return getDrumGlyph(cymbal, canHaveDynamics, dynamic);
}