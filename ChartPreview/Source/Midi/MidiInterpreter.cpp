/*
  ==============================================================================

    MidiInterpreter.cpp
    Created: 15 Jun 2024 3:57:52pm
    Author:  Noah Baxter

  ==============================================================================
*/

#include "MidiInterpreter.h"
#include "Utils/MidiConstants.h"

MidiInterpreter::MidiInterpreter(juce::ValueTree &state, NoteStateMapArray &noteStateMapArray, juce::CriticalSection &noteStateMapLock)
    : noteStateMapArray(noteStateMapArray),
      noteStateMapLock(noteStateMapLock),
      state(state)
{
}

MidiInterpreter::~MidiInterpreter()
{
}

TrackWindow MidiInterpreter::generateTrackWindow(PPQ trackWindowStart, PPQ trackWindowEnd)
{
    // return generateFakeTrackWindow(trackWindowStart, trackWindowEnd);

    TrackWindow trackWindow;

    const juce::ScopedLock lock(noteStateMapLock);

    for (uint pitch = MIDI_PITCH_MIN; pitch < MIDI_PITCH_COUNT; pitch++)
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

TrackWindow MidiInterpreter::generateFakeTrackWindow(PPQ trackWindowStart, PPQ trackWindowEnd)
{
    TrackWindow fakeTrackWindow;

    std::vector<uint> lanes = {1, 2, 3, 4, 5};
    // std::vector<uint> lanes = {0};

    uint numNotes = DEBUG_FAKE_TRACK_WINDOW_NOTE_COUNT;

    for (uint lane : lanes) {
        for (uint n = 0; n < numNotes; n++)
        {
            PPQ position = trackWindowStart + (trackWindowEnd - trackWindowStart) * ((double)n / (double)numNotes);
            if (fakeTrackWindow[position].empty())
            {
                fakeTrackWindow[position] = generateEmptyTrackFrame();
            }
            fakeTrackWindow[position][lane] = GemWrapper(Gem::NOTE, false);
        }
    }

    return fakeTrackWindow;
}

SustainWindow MidiInterpreter::generateSustainWindow(PPQ trackWindowStart, PPQ trackWindowEnd, PPQ latencyBufferEnd)
{
    // return generateFakeSustains(trackWindowStart, trackWindowEnd);

    SustainWindow sustainWindow;

    // Lock the noteStateMapArray during iteration to prevent crashes
    const juce::ScopedLock lock(noteStateMapLock);

    using Drums = MidiPitchDefinitions::Drums;
    using Guitar = MidiPitchDefinitions::Guitar;

    for (uint pitch = MIDI_PITCH_MIN; pitch < MIDI_PITCH_COUNT; pitch++)
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
                    // Extend lane start time slightly earlier for better visibility
                    PPQ extendedStartPPQ = notePPQ - MIDI_LANE_EXTENSION_TIME;
                    
                    // Use lane detection logic to determine which columns to create lanes for
                    auto lanes = LaneDetector::detectLanes(pitch, extendedStartPPQ, noteOffPPQ,
                                                           velocity, state, noteStateMapArray, noteStateMapLock);
                    
                    // Add detected lanes to window
                    for (const auto& lane : lanes) {
                        sustainWindow.push_back(lane);
                    }
                }
                // Sustains (guitar only)
                else if (isPart(state, Part::GUITAR)) {
                    PPQ duration = noteOffPPQ - notePPQ;
                    if (duration >= MIDI_MIN_SUSTAIN_LENGTH) {
                        uint gemColumn = InstrumentMapper::getGuitarColumn(pitch, (SkillLevel)((int)state.getProperty("skillLevel")));
                        if (gemColumn < LANE_COUNT) {
                            // Check if star power is held at the start of this sustain
                            bool isSpHeld = isNoteHeld(static_cast<uint>(Guitar::SP), notePPQ);

                            SustainEvent sustain;
                            sustain.startPPQ = notePPQ;
                            sustain.endPPQ = noteOffPPQ;
                            sustain.gemColumn = gemColumn;
                            sustain.sustainType = SustainType::SUSTAIN;
                            sustain.gemType = GemWrapper(it->second.gemType, isSpHeld);
                            sustainWindow.push_back(sustain);
                        }
                    }
                }
            }
        }
    }
    
    return sustainWindow;
}

SustainWindow MidiInterpreter::generateFakeSustains(PPQ trackWindowStartPPQ, PPQ trackWindowEndPPQ)
{
    SustainWindow fakeSustainWindow;

    // std::vector<uint> lanes = {1, 3, 5};
    // std::vector<uint> lanes = {2, 4};
    std::vector<uint> lanes = {1, 2, 3, 4, 5};
    // std::vector<uint> lanes = {0};

    for (uint lane : lanes) {
        SustainEvent sustain;
        sustain.startPPQ = trackWindowStartPPQ;
        sustain.endPPQ = trackWindowEndPPQ;
        sustain.gemColumn = lane;
        sustain.gemType = GemWrapper(Gem::NOTE, false);
        sustain.sustainType = SustainType::LANE;
        fakeSustainWindow.push_back(sustain);
    }

    return fakeSustainWindow;
}

TrackFrame MidiInterpreter::generateEmptyTrackFrame()
{
    TrackFrame frame = {
                           //| Guitar  |   Drums   |   Real Drums
                           //| -------------------------------------
        GemWrapper(), //| Open    |   Kick    |
        GemWrapper(), //| Fret 1  |   Lane 1  |
        GemWrapper(), //| Fret 2  |   Lane 2  |
        GemWrapper(), //| Fret 3  |   Lane 3  |
        GemWrapper(), //| Fret 4  |   Lane 4  |
        GemWrapper(), //| Fret 5  |   Lane 5  |
        GemWrapper(), //| Fret 6  |   2x Kick |
    };

    return frame;
}

void MidiInterpreter::addGuitarEventToFrame(TrackFrame &frame, PPQ position, uint pitch, Gem gemType)
{
    uint gemColumn = InstrumentMapper::getGuitarColumn(pitch, (SkillLevel)((int)state.getProperty("skillLevel")));
    if (gemColumn < LANE_COUNT) {
        // Check if star power is held at this position (MIDI pitch 116)
        using Guitar = MidiPitchDefinitions::Guitar;
        bool isSpHeld = isNoteHeld(static_cast<uint>(Guitar::SP), position);
        frame[gemColumn] = GemWrapper(gemType, isSpHeld);
    }
}

void MidiInterpreter::addDrumEventToFrame(TrackFrame &frame, PPQ position, uint pitch, Gem gemType)
{
    uint gemColumn = InstrumentMapper::getDrumColumn(pitch, (SkillLevel)((int)state.getProperty("skillLevel")), (bool)state.getProperty("kick2x"));
    if (gemColumn < LANE_COUNT) {
        // Check if star power is held at this position (MIDI pitch 116)
        using Drums = MidiPitchDefinitions::Drums;
        bool isSpHeld = isNoteHeld(static_cast<uint>(Drums::SP), position);
        frame[gemColumn] = GemWrapper(gemType, isSpHeld);
    }
}


