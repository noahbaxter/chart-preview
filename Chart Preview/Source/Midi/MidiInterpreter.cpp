/*
  ==============================================================================

    MidiInterpreter.cpp
    Created: 15 Jun 2024 3:57:52pm
    Author:  Noah Baxter

  ==============================================================================
*/

#include "MidiInterpreter.h"

MidiInterpreter::MidiInterpreter(juce::ValueTree &state, NoteStateMapArray &noteStateMapArray)
    : state(state),
      noteStateMapArray(noteStateMapArray)
{
}

MidiInterpreter::~MidiInterpreter()
{
}

TrackWindow MidiInterpreter::generateTrackWindow(uint trackWindowStart, uint trackWindowEnd)
{
    TrackWindow trackWindow;

    Part part = (Part)((int)state.getProperty("part"));
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

            uint position = it->first;
            // If the trackWindow at this position is empty, generate an empty frame
            if (trackWindow[position].empty())
            {
                trackWindow[position] = generateEmptyTrackFrame();
            }

            if (part == Part::GUITAR)
            {
                addGuitarEventToFrame(trackWindow[position], position, pitch);
            }
            else if (part == Part::DRUMS)
            {
                addDrumEventToFrame(trackWindow[position], position, pitch, dynamic);
            }
            ++it;
        }
    }

    return trackWindow;
}

TrackFrame MidiInterpreter::generateEmptyTrackFrame()
{
    TrackFrame frame = {
                   //| Guitar  |   Drums   |   Real Drums
                   //| -------------------------------------
        Gem::NONE, //| Open    |   Kick    |
        Gem::NONE, //| Fret 1  |   Lane 1  |
        Gem::NOTE, //| Fret 2  |   Lane 2  |
        Gem::NONE, //| Fret 3  |   Lane 3  |
        Gem::NOTE, //| Fret 4  |   Lane 4  |
        Gem::NONE, //| Fret 5  |   Lane 5  |
        Gem::NONE, //| Fret 6  |   2x Kick |
    };

    return frame;
}

void MidiInterpreter::addGuitarEventToFrame(TrackFrame &frame, uint position, uint pitch)
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

    Gem gem;
    if(modStrum)
    {
        gem = Gem::NOTE;
    }
    else if(modTap)
    {
        gem = Gem::TAP_ACCENT;
    }
    else if(modHOPO)
    {
        gem = Gem::HOPO_GHOST;
    }
    else
    {
        gem = Gem::NOTE;
    }

    Guitar note = (Guitar)pitch;

    if ((note == Guitar::EASY_OPEN && skill == SkillLevel::EASY) ||
        (note == Guitar::MEDIUM_OPEN && skill == SkillLevel::MEDIUM) ||
        (note == Guitar::HARD_OPEN && skill == SkillLevel::HARD) ||
        (note == Guitar::EXPERT_OPEN && skill == SkillLevel::EXPERT))
    {
        frame[0] = gem;
    }
    else if ((note == Guitar::EASY_GREEN && skill == SkillLevel::EASY) ||
             (note == Guitar::MEDIUM_GREEN && skill == SkillLevel::MEDIUM) ||
             (note == Guitar::HARD_GREEN && skill == SkillLevel::HARD) ||
             (note == Guitar::EXPERT_GREEN && skill == SkillLevel::EXPERT))
    {
        frame[1] = gem;
    }
    else if ((note == Guitar::EASY_RED && skill == SkillLevel::EASY) ||
             (note == Guitar::MEDIUM_RED && skill == SkillLevel::MEDIUM) ||
             (note == Guitar::HARD_RED && skill == SkillLevel::HARD) ||
             (note == Guitar::EXPERT_RED && skill == SkillLevel::EXPERT))
    {
        frame[2] = gem;
    }
    else if ((note == Guitar::EASY_YELLOW && skill == SkillLevel::EASY) ||
             (note == Guitar::MEDIUM_YELLOW && skill == SkillLevel::MEDIUM) ||
             (note == Guitar::HARD_YELLOW && skill == SkillLevel::HARD) ||
             (note == Guitar::EXPERT_YELLOW && skill == SkillLevel::EXPERT))
    {
        frame[3] = gem;
    }
    else if ((note == Guitar::EASY_BLUE && skill == SkillLevel::EASY) ||
             (note == Guitar::MEDIUM_BLUE && skill == SkillLevel::MEDIUM) ||
             (note == Guitar::HARD_BLUE && skill == SkillLevel::HARD) ||
             (note == Guitar::EXPERT_BLUE && skill == SkillLevel::EXPERT))
    {
        frame[4] = gem;
    }
    else if ((note == Guitar::EASY_ORANGE && skill == SkillLevel::EASY) ||
             (note == Guitar::MEDIUM_ORANGE && skill == SkillLevel::MEDIUM) ||
             (note == Guitar::HARD_ORANGE && skill == SkillLevel::HARD) ||
             (note == Guitar::EXPERT_ORANGE && skill == SkillLevel::EXPERT))
    {
        frame[5] = gem;
    }
}

void MidiInterpreter::addDrumEventToFrame(TrackFrame &frame, uint position, uint pitch, Dynamic dynamic)
{
    using Drums = MidiPitchDefinitions::Drums;
    SkillLevel skill = (SkillLevel)((int)state.getProperty("skillLevel"));
    bool dynamicsEnabled = (bool)state.getProperty("dynamics");
    bool kick2xEnabled = (bool)state.getProperty("kick2x");
    bool isProDrums = (DrumType)((int)state.getProperty("drumType")) == DrumType::PRO;

    Drums note = (Drums)pitch;
    if ((note == Drums::EASY_KICK && skill == SkillLevel::EASY) ||
        (note == Drums::MEDIUM_KICK && skill == SkillLevel::MEDIUM) ||
        (note == Drums::HARD_KICK && skill == SkillLevel::HARD) ||
        (note == Drums::EXPERT_KICK && skill == SkillLevel::EXPERT))
    {
        // Kicks can't be cymbals, and can't have dynamics
        frame[0] = getDrumGlyph(false, false, dynamic);
    }
    else if ((note == Drums::EASY_RED && skill == SkillLevel::EASY) ||
             (note == Drums::MEDIUM_RED && skill == SkillLevel::MEDIUM) ||
             (note == Drums::HARD_RED && skill == SkillLevel::HARD) ||
             (note == Drums::EXPERT_RED && skill == SkillLevel::EXPERT))
    {
        // Snares can't be cymbals
        frame[1] = getDrumGlyph(false, dynamicsEnabled, dynamic);
    }
    else if ((note == Drums::EASY_YELLOW && skill == SkillLevel::EASY) ||
             (note == Drums::MEDIUM_YELLOW && skill == SkillLevel::MEDIUM) ||
             (note == Drums::HARD_YELLOW && skill == SkillLevel::HARD) ||
             (note == Drums::EXPERT_YELLOW && skill == SkillLevel::EXPERT))
    {
        bool cymbal = isProDrums && !isNoteHeld((uint)Drums::TOM_YELLOW, position);
        frame[2] = getDrumGlyph(cymbal, dynamicsEnabled, dynamic);
    }
    else if ((note == Drums::EASY_BLUE && skill == SkillLevel::EASY) ||
             (note == Drums::MEDIUM_BLUE && skill == SkillLevel::MEDIUM) ||
             (note == Drums::HARD_BLUE && skill == SkillLevel::HARD) ||
             (note == Drums::EXPERT_BLUE && skill == SkillLevel::EXPERT))
    {
        bool cymbal = isProDrums && !isNoteHeld((uint)Drums::TOM_BLUE, position);
        frame[3] = getDrumGlyph(cymbal, dynamicsEnabled, dynamic);
    }
    else if ((note == Drums::EASY_GREEN && skill == SkillLevel::EASY) ||
             (note == Drums::MEDIUM_GREEN && skill == SkillLevel::MEDIUM) ||
             (note == Drums::HARD_GREEN && skill == SkillLevel::HARD) ||
             (note == Drums::EXPERT_GREEN && skill == SkillLevel::EXPERT))
    {
        bool cymbal = isProDrums && !isNoteHeld((int)Drums::TOM_GREEN, position);
        frame[4] = getDrumGlyph(cymbal, dynamicsEnabled, dynamic);
    }
    else if (kick2xEnabled && note == Drums::EXPERT_KICK_2X && skill == SkillLevel::EXPERT)
    {
        // Kicks can't be cymbals, and can't have dynamics
        frame[0] = getDrumGlyph(false, false, dynamic);
    }
}