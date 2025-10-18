#include "ChordAnalyzer.h"
#include "MidiTypes.h"
#include "MidiConstants.h"
#include "InstrumentMapper.h"

bool ChordAnalyzer::isNoteHeld(uint pitch, PPQ position,
                               NoteStateMapArray& noteStateMapArray,
                               juce::CriticalSection& noteStateMapLock)
{
    const juce::ScopedLock lock(noteStateMapLock);
    auto& noteStateMap = noteStateMapArray[pitch];
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

bool ChordAnalyzer::isNoteHeldWithTolerance(uint pitch, PPQ position,
                                            NoteStateMapArray& noteStateMapArray,
                                            juce::CriticalSection& noteStateMapLock)
{
    const juce::ScopedLock lock(noteStateMapLock);
    auto& noteStateMap = noteStateMapArray[pitch];

    PPQ startRange = position - MIDI_CHORD_TOLERANCE;
    PPQ endRange = position + MIDI_CHORD_TOLERANCE;

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

bool ChordAnalyzer::isWithinChordTolerance(PPQ position1, PPQ position2)
{
    PPQ diff = (position1 > position2) ? (position1 - position2) : (position2 - position1);
    return diff <= MIDI_CHORD_TOLERANCE;
}

void ChordAnalyzer::fixChordHOPOs(const std::vector<PPQ>& positions, SkillLevel skill,
                                  NoteStateMapArray& noteStateMapArray,
                                  juce::CriticalSection& noteStateMapLock)
{
    using Guitar = MidiPitchDefinitions::Guitar;
    std::vector<uint> guitarPitches = InstrumentMapper::getGuitarPitchesForSkill(skill);

    for (PPQ position : positions)
    {
        // Count notes at this position
        int noteCount = 0;
        {
            const juce::ScopedLock lock(noteStateMapLock);

            for (uint guitarPitch : guitarPitches)
            {
                if (isNoteHeldWithTolerance(guitarPitch, position, noteStateMapArray, noteStateMapLock))
                {
                    noteCount++;
                }
            }
        }

        // If chord (2+ notes), fix any AUTO-HOPOs (but preserve forced HOPOs)
        if (noteCount >= 2)
        {
            PPQ searchStart = position - MIDI_CHORD_TOLERANCE;
            PPQ searchEnd = position + MIDI_CHORD_TOLERANCE;

            // Check if there's a forced HOPO modifier at this position
            bool hasForcedHOPO = false;
            {
                const juce::ScopedLock lock(noteStateMapLock);

                auto checkModifier = [&](uint modPitch) -> bool {
                    return isNoteHeld(modPitch, position, noteStateMapArray, noteStateMapLock);
                };

                hasForcedHOPO = (checkModifier((int)Guitar::EASY_HOPO) && skill == SkillLevel::EASY) ||
                                (checkModifier((int)Guitar::MEDIUM_HOPO) && skill == SkillLevel::MEDIUM) ||
                                (checkModifier((int)Guitar::HARD_HOPO) && skill == SkillLevel::HARD) ||
                                (checkModifier((int)Guitar::EXPERT_HOPO) && skill == SkillLevel::EXPERT);
            }

            // Only convert HOPOs to strums if they're NOT forced by a modifier
            if (!hasForcedHOPO)
            {
                const juce::ScopedLock lock(noteStateMapLock);

                for (uint guitarPitch : guitarPitches)
                {
                    auto& noteStateMap = noteStateMapArray[guitarPitch];
                    auto lower = noteStateMap.lower_bound(searchStart);
                    auto upper = noteStateMap.upper_bound(searchEnd);

                    for (auto it = lower; it != upper; ++it)
                    {
                        if (it->second.velocity > 0 && it->second.gemType == Gem::HOPO_GHOST)
                        {
                            it->second.gemType = Gem::NOTE;
                        }
                    }
                }
            }
        }
    }
}
