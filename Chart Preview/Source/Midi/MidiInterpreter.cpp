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

std::vector<uint> MidiInterpreter::interpretMidiFrameOLD(const std::vector<juce::MidiMessage> &messages)
{
    // Guitar/Bass  [green, red, yellow, blue, orange]
    // Drums        [kick, red, yellow, blue, green]

    // 0 - none
    // 1 - normal
    // 2 - hopo
    // 3 - tap
    // 4 - strum

    // Drums
    // [kick, red, yellow, blue, green]
    // 0 - none
    // 1 - drum ghost
    // 2 - drum normal
    // 3 - drum accent
    // 4 - cymbal ghost
    // 5 - cymbal normal
    // 6 - cymbal accent

    int instrument = (int)state.getProperty("part");
    int difficulty = (int)state.getProperty("skillLevel");

    std::vector<uint> gems = { 0, 0, 0, 0, 0 };
    std::vector<uint> mods = { 0, 0, 0, 0, 0 };
    std::vector<uint> dyns = { 0, 0, 0, 0, 0 };
    for (const auto &message : messages)
    {
        if(!message.isNoteOn())
        {
            continue;
        }

        uint note = message.getNoteNumber();

        // Base note
        if (note >= 60 && note <= 100)
        {
            uint velocity = message.getVelocity();
            uint dynamic = (velocity == 1) ? 0 : (velocity == 127) ? 2 : 1;
            if ((note == 60 && difficulty == 1) ||
                (note == 72 && difficulty == 2) ||
                (note == 84 && difficulty == 3) ||
                (note == 96 && difficulty == 4))
            {
                gems[0] = 1;
            }
            else if ((note == 61 && difficulty == 1) ||
                     (note == 73 && difficulty == 2) ||
                     (note == 85 && difficulty == 3) ||
                     (note == 97 && difficulty == 4))
            {
                gems[1] = 1;
                dyns[1] = dynamic;
            }
            else if ((note == 62 && difficulty == 1) ||
                     (note == 74 && difficulty == 2) ||
                     (note == 86 && difficulty == 3) ||
                     (note == 98 && difficulty == 4))
            {
                gems[2] = 1;
                dyns[2] = dynamic;
            }
            else if ((note == 63 && difficulty == 1) ||
                     (note == 75 && difficulty == 2) ||
                     (note == 87 && difficulty == 3) ||
                     (note == 99 && difficulty == 4))
            {
                gems[3] = 1;
                dyns[3] = dynamic;
            }
            else if ((note == 64 && difficulty == 1) ||
                     (note == 76 && difficulty == 2) ||
                     (note == 88 && difficulty == 3) ||
                     (note == 100 && difficulty == 4))
            {
                gems[4] = 1;
                dyns[4] = dynamic;
            }
        }

        // Drum modifiers
        if (instrument == 2)
        {
            if (note == 110) { mods[2] = 1; }
            else if (note == 111) { mods[3] = 1; }
            else if (note == 112) { mods[4] = 1; }
        }

    }

    // TODO: overdrive, fills, rolls, etc.
    for (int i = 0; i < gems.size(); i++)
    {
        if (gems[i] > 0 )
        {
            gems[i] += dyns[i];

            // yellow, blue, green only can be cymbals
            if (i > 1 && mods[i] == 0)
            {
                gems[i] += 3;
            }
        }
    }

    return gems;
}
std::array<Gem,7> MidiInterpreter::interpretMidiFrame(const std::vector<juce::MidiMessage> &messages, std::array<bool,128> &noteStateMap)
{
    std::array<Gem,7> gems = {
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
        interpretGuitarFrame(messages, noteStateMap, gems);
    }
    else if (part == Part::DRUMS)
    {
        interpretDrumFrame(messages, gems);
    }

    return gems;
}
std::array<Gem,7> MidiInterpreter::interpretGuitarFrame(const std::vector<juce::MidiMessage> &messages, std::array<bool,128> &noteStateMap, std::array<Gem,7> &gems)
{
    using Guitar = MidiPitchDefinitions::Guitar;

    SkillLevel skill = (SkillLevel)((int)state.getProperty("skillLevel"));

    Gem gem = Gem::NOTE;    
    if ((noteStateMap[(int)Guitar::EASY_HOPO] && skill == SkillLevel::EASY) ||
        (noteStateMap[(int)Guitar::MEDIUM_HOPO] && skill == SkillLevel::MEDIUM) ||
        (noteStateMap[(int)Guitar::HARD_HOPO] && skill == SkillLevel::HARD) ||
        (noteStateMap[(int)Guitar::EXPERT_HOPO] && skill == SkillLevel::EXPERT))
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

std::array<Gem,7> MidiInterpreter::interpretDrumFrame(const std::vector<juce::MidiMessage> &messages, std::array<Gem,7> &gems)
{
    using Drums = MidiPitchDefinitions::Drums;

    SkillLevel skill = (SkillLevel)((int)state.getProperty("skillLevel"));
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
        else if (note == Drums::EXPERT_KICK_2X && skill == SkillLevel::EXPERT)
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
        gems[i] = (Gem)((int)gems[i] + dynamics[i]);

        // only yellow, blue, and green can be cymbals
        if (i > 1 && modifier[i] == 0)
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