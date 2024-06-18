/*
  ==============================================================================

    MidiInterpreter.cpp
    Created: 15 Jun 2024 3:57:52pm
    Author:  Noah Baxter

  ==============================================================================
*/

#include "MidiInterpreter.h"

MidiInterpreter::MidiInterpreter(juce::ValueTree &state)
    : state(state)
{
}

MidiInterpreter::~MidiInterpreter()
{
}

std::array<Gem, 7> MidiInterpreter::interpretMidiFrame(const std::vector<juce::MidiMessage> &messages)
{
    std::array<Gem, 7> gems = {
                    //| Guitar  |   Drums   |   Real Drums
                    //| -------------------------------------
        Gem::NONE,  //| Open    |   Kick    |
        Gem::NONE,  //| Fret 1  |   Lane 1  |
        Gem::NONE,  //| Fret 2  |   Lane 2  |
        Gem::NONE,  //| Fret 3  |   Lane 3  |
        Gem::NONE,  //| Fret 4  |   Lane 4  |
        Gem::NONE,  //| Fret 5  |   Lane 5  |
        Gem::NONE,  //| Fret 6  |   2x Kick |
    };

    Part part = (Part)((int)state.getProperty("part"));
    if (part == Part::GUITAR)
    {
        interpretGuitarFrame(gems, messages);
    }
    else if (part == Part::DRUMS)
    {
        interpretDrumFrame(gems, messages);
    }

    return gems;
}
std::array<Gem, 7> MidiInterpreter::interpretGuitarFrame(std::array<Gem, 7> &gems, const std::vector<juce::MidiMessage> &messages)
{
    using Guitar = MidiPitchDefinitions::Guitar;

    SkillLevel skill = (SkillLevel)((int)state.getProperty("skillLevel"));

    Gem gem = Gem::NOTE;    
    if ((isNoteHeld((int)Guitar::EASY_HOPO) && skill == SkillLevel::EASY) ||
        (isNoteHeld((int)Guitar::MEDIUM_HOPO) && skill == SkillLevel::MEDIUM) ||
        (isNoteHeld((int)Guitar::HARD_HOPO) && skill == SkillLevel::HARD) ||
        (isNoteHeld((int)Guitar::EXPERT_HOPO) && skill == SkillLevel::EXPERT))
    {
        gem = Gem::HOPO_GHOST;
    }

    for (const auto &message : messages)
    {
        Guitar note = (Guitar)message.getNoteNumber();

        if ((note == Guitar::EASY_OPEN && skill == SkillLevel::EASY) ||
            (note == Guitar::MEDIUM_OPEN && skill == SkillLevel::MEDIUM) ||
            (note == Guitar::HARD_OPEN && skill == SkillLevel::HARD) ||
            (note == Guitar::EXPERT_OPEN && skill == SkillLevel::EXPERT))
        {
            gems[0] = gem;
        }
        else if ((note == Guitar::EASY_GREEN && skill == SkillLevel::EASY) ||
                 (note == Guitar::MEDIUM_GREEN && skill == SkillLevel::MEDIUM) ||
                 (note == Guitar::HARD_GREEN && skill == SkillLevel::HARD) ||
                 (note == Guitar::EXPERT_GREEN && skill == SkillLevel::EXPERT))
        {
            gems[1] = gem;
        }
        else if ((note == Guitar::EASY_RED && skill == SkillLevel::EASY) ||
                 (note == Guitar::MEDIUM_RED && skill == SkillLevel::MEDIUM) ||
                 (note == Guitar::HARD_RED && skill == SkillLevel::HARD) ||
                 (note == Guitar::EXPERT_RED && skill == SkillLevel::EXPERT))
        {
            gems[2] = gem;
        }
        else if ((note == Guitar::EASY_YELLOW && skill == SkillLevel::EASY) ||
                 (note == Guitar::MEDIUM_YELLOW && skill == SkillLevel::MEDIUM) ||
                 (note == Guitar::HARD_YELLOW && skill == SkillLevel::HARD) ||
                 (note == Guitar::EXPERT_YELLOW && skill == SkillLevel::EXPERT))
        {
            gems[3] = gem;
        }
        else if ((note == Guitar::EASY_BLUE && skill == SkillLevel::EASY) ||
                 (note == Guitar::MEDIUM_BLUE && skill == SkillLevel::MEDIUM) ||
                 (note == Guitar::HARD_BLUE && skill == SkillLevel::HARD) ||
                 (note == Guitar::EXPERT_BLUE && skill == SkillLevel::EXPERT))
        {
            gems[4] = gem;
        }
        else if ((note == Guitar::EASY_ORANGE && skill == SkillLevel::EASY) ||
                 (note == Guitar::MEDIUM_ORANGE && skill == SkillLevel::MEDIUM) ||
                 (note == Guitar::HARD_ORANGE && skill == SkillLevel::HARD) ||
                 (note == Guitar::EXPERT_ORANGE && skill == SkillLevel::EXPERT))
        {
            gems[5] = gem;
        }
    }
}

std::array<Gem, 7> MidiInterpreter::interpretDrumFrame(std::array<Gem, 7> &gems, const std::vector<juce::MidiMessage> &messages)
{
    using Drums = MidiPitchDefinitions::Drums;

    SkillLevel skill = (SkillLevel)((int)state.getProperty("skillLevel"));
    bool dynamicsEnabled = (bool)state.getProperty("dynamics");
    bool kick2xEnabled = (bool)state.getProperty("kick2x");
    bool isProDrums = (DrumType)((int)state.getProperty("drumType")) == DrumType::PRO;

    std::array<int, 7> modifier = {0, 0, 0, 0, 0, 0, 0};
    std::array<int, 7> dynamics = {0, 0, 0, 0, 0, 0, 0};
    for (const auto &message : messages)
    {
        Drums note = (Drums)message.getNoteNumber();
        uint velocity = message.getVelocity();
        int dynamic = (velocity == 1) ? -1 : (velocity == 127) ? 1 : 0;

        if ((note == Drums::EASY_KICK && skill == SkillLevel::EASY) ||
            (note == Drums::MEDIUM_KICK && skill == SkillLevel::MEDIUM) ||
            (note == Drums::HARD_KICK && skill == SkillLevel::HARD) ||
            (note == Drums::EXPERT_KICK && skill == SkillLevel::EXPERT))
        {
            gems[0] = Gem::NOTE;
        }
        else if ((note == Drums::EASY_RED && skill == SkillLevel::EASY) ||
                 (note == Drums::MEDIUM_RED && skill == SkillLevel::MEDIUM) ||
                 (note == Drums::HARD_RED && skill == SkillLevel::HARD) ||
                 (note == Drums::EXPERT_RED && skill == SkillLevel::EXPERT))
        {
            gems[1] = Gem::NOTE;
            dynamics[1] = dynamic;
        }
        else if ((note == Drums::EASY_YELLOW && skill == SkillLevel::EASY) ||
                 (note == Drums::MEDIUM_YELLOW && skill == SkillLevel::MEDIUM) ||
                 (note == Drums::HARD_YELLOW && skill == SkillLevel::HARD) ||
                 (note == Drums::EXPERT_YELLOW && skill == SkillLevel::EXPERT))
        {
            gems[2] = Gem::NOTE;
            dynamics[2] = dynamic;
        }
        else if ((note == Drums::EASY_BLUE && skill == SkillLevel::EASY) ||
                 (note == Drums::MEDIUM_BLUE && skill == SkillLevel::MEDIUM) ||
                 (note == Drums::HARD_BLUE && skill == SkillLevel::HARD) ||
                 (note == Drums::EXPERT_BLUE && skill == SkillLevel::EXPERT))
        {
            gems[3] = Gem::NOTE;
            dynamics[3] = dynamic;
        }
        else if ((note == Drums::EASY_GREEN && skill == SkillLevel::EASY) ||
                 (note == Drums::MEDIUM_GREEN && skill == SkillLevel::MEDIUM) ||
                 (note == Drums::HARD_GREEN && skill == SkillLevel::HARD) ||
                 (note == Drums::EXPERT_GREEN && skill == SkillLevel::EXPERT))
        {
            gems[4] = Gem::NOTE;
            dynamics[4] = dynamic;
        }
        else if (kick2xEnabled && note == Drums::EXPERT_KICK_2X && skill == SkillLevel::EXPERT)
        {
            gems[6] = Gem::NOTE;
        }

        // Pro Drum Modifiers
        else if (isProDrums)
        {
            if (note == Drums::TOM_YELLOW)
            {
                modifier[2] = 1;
            }
            else if (note == Drums::TOM_BLUE)
            {
                modifier[3] = 1;
            }
            else if (note == Drums::TOM_GREEN)
            {
                modifier[4] = 1;
            }
        }
    }

    // Adjust gems based on modifiers
    for (int i = 1; i <= 4; i++)
    {
        if (gems[i] == Gem::NONE)
        {
            continue;
        }

        if (dynamicsEnabled)
        {
            gems[i] = (Gem)((int)gems[i] + dynamics[i]);
        }

        // only yellow, blue, and green can be cymbals
        if (isProDrums && i > 1 && modifier[i] == 0)
        {
            gems[i] = (Gem)((int)gems[i] + 3);
        }
    }
}

std::map<int, std::vector<juce::MidiMessage>> MidiInterpreter::getFakeMidiEventMap()
{
    std::map<int, std::vector<juce::MidiMessage>> fakeMidiEventMap;

    int density = 8;
    int displaySizeInSamples = 22050;
    for (int i = 0; i <= density; i++)
    {
        int vel = (i < density / 4) ? 1 : (i >= 3 * density / 4) ? 127
                                                                 : 64;

        fakeMidiEventMap[i * displaySizeInSamples / density].push_back(juce::MidiMessage::noteOn(1, 96, static_cast<float>(vel) / 127.f));

        fakeMidiEventMap[i * displaySizeInSamples / density].push_back(juce::MidiMessage::noteOn(1, 97, static_cast<float>(vel) / 127.f));

        fakeMidiEventMap[i * displaySizeInSamples / density].push_back(juce::MidiMessage::noteOn(1, 110, static_cast<float>(vel) / 127.f));
        fakeMidiEventMap[i * displaySizeInSamples / density].push_back(juce::MidiMessage::noteOn(1, 98, static_cast<float>(vel) / 127.f));

        // fakeMidiEventMap[i * displaySizeInSamples / density].push_back(juce::MidiMessage::noteOn(1, 111, static_cast<float>(vel) / 127.f));
        fakeMidiEventMap[i * displaySizeInSamples / density].push_back(juce::MidiMessage::noteOn(1, 99, static_cast<float>(vel) / 127.f));

        fakeMidiEventMap[i * displaySizeInSamples / density].push_back(juce::MidiMessage::noteOn(1, 100, static_cast<float>(vel) / 127.f));
        fakeMidiEventMap[i * displaySizeInSamples / density].push_back(juce::MidiMessage::noteOn(1, 112, static_cast<float>(vel) / 127.f));
    }
    
    return fakeMidiEventMap;
}