#include "LaneDetector.h"
#include "InstrumentMapper.h"

std::vector<SustainEvent> LaneDetector::detectLanes(uint laneType, PPQ startPPQ, PPQ endPPQ,
                                                    uint laneVelocity, juce::ValueTree& state,
                                                    NoteStateMapArray& noteStateMapArray,
                                                    juce::CriticalSection& noteStateMapLock)
{
    using Drums = MidiPitchDefinitions::Drums;
    std::vector<SustainEvent> lanes;

    // Check if lane applies to current skill level based on velocity
    SkillLevel skill = (SkillLevel)((int)state.getProperty("skillLevel"));
    bool appliesToSkill = (skill == SkillLevel::EXPERT) ||
                          (skill == SkillLevel::HARD && laneVelocity >= 41 && laneVelocity <= 50);

    if (!appliesToSkill) return lanes;

    // Get pitches for current instrument and skill level
    std::vector<uint> instrumentPitches;
    if (isPart(state, Part::GUITAR))
    {
        instrumentPitches = InstrumentMapper::getGuitarPitchesForSkill(skill);
    }
    else if (isPart(state, Part::DRUMS) || isPart(state, Part::REAL_DRUMS))
    {
        instrumentPitches = InstrumentMapper::getDrumPitchesForSkill(skill);
    }
    else
    {
        return lanes; // Unknown instrument
    }

    // Find first notes after lane start to determine column(s)
    std::vector<uint> laneColumns;
    const juce::ScopedLock lock(noteStateMapLock);

    // Collect all notes in the lane timeframe and sort by time
    std::vector<std::pair<PPQ, uint>> noteEvents; // (time, pitch)

    for (uint pitch : instrumentPitches)
    {
        const auto& noteStateMap = noteStateMapArray[pitch];
        auto it = noteStateMap.lower_bound(startPPQ);

        while (it != noteStateMap.end() && it->first <= endPPQ)
        {
            if (it->second.velocity > 0)
            { // Note-on event
                uint column = isPart(state, Part::GUITAR) ? InstrumentMapper::getGuitarColumn(pitch, skill)
                                                          : InstrumentMapper::getDrumColumn(pitch, skill, (bool)state.getProperty("kick2x"));
                if (column < LANE_COUNT)
                { // Valid column
                    noteEvents.push_back({it->first, pitch});
                }
            }
            ++it;
        }
    }

    // Sort by time to get chronological order
    std::sort(noteEvents.begin(), noteEvents.end());

    // Take first 1 or 2 notes depending on lane type
    uint maxNotes = (laneType == (uint)Drums::LANE_2) ? 2 : 1;
    for (size_t i = 0; i < noteEvents.size() && laneColumns.size() < maxNotes; ++i)
    {
        uint column = isPart(state, Part::GUITAR) ? InstrumentMapper::getGuitarColumn(noteEvents[i].second, skill)
                                                   : InstrumentMapper::getDrumColumn(noteEvents[i].second, skill, (bool)state.getProperty("kick2x"));
        laneColumns.push_back(column);
    }

    // Create lane sustain events
    for (uint column : laneColumns)
    {
        SustainEvent lane;
        lane.startPPQ = startPPQ;
        lane.endPPQ = endPPQ;
        lane.gemColumn = column;
        lane.sustainType = SustainType::LANE;
        lane.gemType = Gem::NOTE; // Placeholder gem type
        lanes.push_back(lane);
    }

    return lanes;
}
