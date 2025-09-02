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
            Dynamic dynamic = (Dynamic)it->second.velocity;
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
                addGuitarEventToFrame(trackWindow[position], position, pitch, it->second.gemType);
            }
            else // if (isPart(state, Part::DRUMS))
            {
                addDrumEventToFrame(trackWindow[position], position, pitch, it->second.gemType);
            }
            ++it;
        }
    }

    return trackWindow;
}

SustainWindow MidiInterpreter::generateSustainWindow(PPQ trackWindowStart, PPQ trackWindowEnd, PPQ latencyBufferEnd)
{
    SustainWindow sustainWindow;
    
    // Lock the noteStateMapArray during iteration to prevent crashes
    const juce::ScopedLock lock(noteStateMapLock);
    
    using Drums = MidiPitchDefinitions::Drums;
    using Guitar = MidiPitchDefinitions::Guitar;
    
    for (uint pitch = 0; pitch < 128; pitch++)
    {
        const NoteStateMap& noteStateMap = noteStateMapArray[pitch];
        
        for (auto it = noteStateMap.begin(); it != noteStateMap.end(); ++it)
        {
            PPQ notePPQ = it->first;
            uint velocity = it->second.velocity;
            
            // Only process note-on events (velocity > 0)
            if (velocity == 0) continue;
            
            // Find the corresponding note-off (velocity == 0)
            auto nextIt = std::next(it);
            while (nextIt != noteStateMap.end() && nextIt->second.velocity != 0)
            {
                ++nextIt;
            }

            // If corresponding note off exists within the map
            PPQ noteOffPPQ = (nextIt != noteStateMap.end()) ? 
                nextIt->first :     // Use found note-off PPQ
                latencyBufferEnd;   // Extend to latency buffer end if no note-off found
            
            // Check if sustain overlaps with current track window (any part)
            if (notePPQ < trackWindowEnd && noteOffPPQ > trackWindowStart) {
                
                // Lanes
                if (pitch == (uint)Drums::LANE_1 || pitch == (uint)Drums::LANE_2 ||
                    pitch == (uint)Guitar::LANE_1 || pitch == (uint)Guitar::LANE_2) {
                    // Use lane detection logic to determine which columns to create lanes for
                    auto lanes = MidiUtility::detectLanes(pitch, notePPQ, noteOffPPQ, 
                                                          velocity, state, noteStateMapArray, noteStateMapLock);
                    
                    // Add detected lanes to window
                    for (const auto& lane : lanes) {
                        sustainWindow.push_back(lane);
                    }
                }
                // Sustains (guitar only)
                else if (isPart(state, Part::GUITAR)) {
                    PPQ duration = noteOffPPQ - notePPQ;
                    if (duration >= MIN_SUSTAIN_LENGTH) {
                        uint gemColumn = MidiUtility::getGuitarGemColumn(pitch, state);
                        if (gemColumn < LANE_COUNT) {
                            SustainEvent sustain;
                            sustain.startPPQ = notePPQ;
                            sustain.endPPQ = noteOffPPQ;
                            sustain.gemColumn = gemColumn;
                            sustain.sustainType = SustainType::SUSTAIN;
                            sustain.gemType = it->second.gemType;
                            sustainWindow.push_back(sustain);
                        }
                    }
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

void MidiInterpreter::addGuitarEventToFrame(TrackFrame &frame, PPQ position, uint pitch, Gem gemType)
{
    uint gemColumn = MidiUtility::getGuitarGemColumn(pitch, state);
    if (gemColumn < LANE_COUNT) {
        frame[gemColumn] = gemType;
    }
}

void MidiInterpreter::addDrumEventToFrame(TrackFrame &frame, PPQ position, uint pitch, Gem gemType)
{
    uint gemColumn = MidiUtility::getDrumGemColumn(pitch, state);
    if (gemColumn < LANE_COUNT) {
        frame[gemColumn] = gemType;
    }
}


