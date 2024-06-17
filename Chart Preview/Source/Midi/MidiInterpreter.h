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
		MidiInterpreter(juce::ComboBox& partMenu, juce::ComboBox& skillMenu);
		~MidiInterpreter();

		std::vector<uint> interpretMidiFrameOLD(const std::vector<juce::MidiMessage> &messages);
		std::vector<ChartEvents> interpretMidiFrame(const std::vector<juce::MidiMessage> &messages);

		// TODO: should this go here?
		std::map<int, std::vector<juce::MidiMessage>> getFakeMidiMap();

	private:
		juce::ComboBox &partMenu, &skillMenu;

		std::vector<uint> interpretGuitar(const std::vector<juce::MidiMessage> &messages);
		std::vector<uint> interpretDrum(const std::vector<juce::MidiMessage> &messages);

};