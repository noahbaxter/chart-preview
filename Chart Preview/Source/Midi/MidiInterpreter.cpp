/*
  ==============================================================================

    MidiInterpreter.cpp
    Created: 15 Jun 2024 3:57:52pm
    Author:  Noah Baxter

  ==============================================================================
*/

#include "MidiInterpreter.h"

MidiInterpreter::MidiInterpreter(juce::ComboBox& partMenu, juce::ComboBox& skillMenu)
    : partMenu(partMenu), skillMenu(skillMenu)
{
}

MidiInterpreter::~MidiInterpreter()
{
}


std::vector<uint> MidiInterpreter::interpretMidiMessages(const std::vector<juce::MidiMessage>& messages)
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

    int instrument = partMenu.getSelectedId();
    int difficulty = skillMenu.getSelectedId();

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
            if (note == 60 && difficulty == 1 ||
                note == 72 && difficulty == 2 ||
                note == 84 && difficulty == 3 ||
                note == 96 && difficulty == 4)
            {
                gems[0] = 1;
            }
            else if (note == 61 && difficulty == 1 ||
                     note == 73 && difficulty == 2 ||
                     note == 85 && difficulty == 3 ||
                     note == 97 && difficulty == 4)
            {
                gems[1] = 1;
                dyns[1] = dynamic;
            }
            else if (note == 62 && difficulty == 1 ||
                     note == 74 && difficulty == 2 ||
                     note == 86 && difficulty == 3 ||
                     note == 98 && difficulty == 4)
            {
                gems[2] = 1;
                dyns[2] = dynamic;
            }
            else if (note == 63 && difficulty == 1 ||
                     note == 75 && difficulty == 2 ||
                     note == 87 && difficulty == 3 ||
                     note == 99 && difficulty == 4)
            {
                gems[3] = 1;
                dyns[3] = dynamic;
            }
            else if (note == 64 && difficulty == 1 ||
                     note == 76 && difficulty == 2 ||
                     note == 88 && difficulty == 3 ||
                     note == 100 && difficulty == 4)
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

std::map<int, std::vector<juce::MidiMessage>> MidiInterpreter::getFakeMidiMap()
{
    std::map<int, std::vector<juce::MidiMessage>> fakeMidiMap;

    int density = 8;
    int displaySizeInSamples = 22050;
    for (int i = 0; i <= density; i++)
    {
        int vel = (i < density / 4) ? 1 : (i >= 3 * density / 4) ? 127
                                                                 : 64;

        fakeMidiMap[i * displaySizeInSamples / density].push_back(juce::MidiMessage::noteOn(1, 96, static_cast<float>(vel) / 127.f));

        fakeMidiMap[i * displaySizeInSamples / density].push_back(juce::MidiMessage::noteOn(1, 97, static_cast<float>(vel) / 127.f));

        fakeMidiMap[i * displaySizeInSamples / density].push_back(juce::MidiMessage::noteOn(1, 110, static_cast<float>(vel) / 127.f));
        fakeMidiMap[i * displaySizeInSamples / density].push_back(juce::MidiMessage::noteOn(1, 98, static_cast<float>(vel) / 127.f));

        // fakeMidiMap[i * displaySizeInSamples / density].push_back(juce::MidiMessage::noteOn(1, 111, static_cast<float>(vel) / 127.f));
        fakeMidiMap[i * displaySizeInSamples / density].push_back(juce::MidiMessage::noteOn(1, 99, static_cast<float>(vel) / 127.f));

        fakeMidiMap[i * displaySizeInSamples / density].push_back(juce::MidiMessage::noteOn(1, 100, static_cast<float>(vel) / 127.f));
        fakeMidiMap[i * displaySizeInSamples / density].push_back(juce::MidiMessage::noteOn(1, 112, static_cast<float>(vel) / 127.f));
    }
    
    return fakeMidiMap;
}