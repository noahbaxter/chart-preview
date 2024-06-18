/*
  ==============================================================================

    MidiInterpreter.h
    Created: 15 Jun 2024 3:57:52pm
    Author:  Noah Baxter

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Utils.h"

class MidiInterpreter
{
	public:
		MidiInterpreter(juce::ValueTree &state);
		~MidiInterpreter();

		std::array<Gem, 7>interpretMidiFrame(const std::vector<juce::MidiMessage> &messages);
		std::function<bool(int)> isNoteHeld;

		// TODO: should this go here?
		std::map<int, std::vector<juce::MidiMessage>> getFakeMidiEventMap();

	private:
		juce::ValueTree &state;

		std::array<Gem, 7> interpretGuitarFrame(std::array<Gem, 7> &gems, const std::vector<juce::MidiMessage> &messages);
		std::array<Gem, 7> interpretDrumFrame(std::array<Gem, 7> &gems, const std::vector<juce::MidiMessage> &messages);
};