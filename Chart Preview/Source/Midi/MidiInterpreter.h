/*
  ==============================================================================

    MidiInterpreter.h
    Created: 15 Jun 2024 3:57:52pm
    Author:  Noah Baxter

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class MidiInterpreter
{
	public:
		MidiInterpreter(juce::ComboBox& partMenu, juce::ComboBox& skillMenu);
		~MidiInterpreter();

		std::vector<uint> interpretMidiMessages(const std::vector<juce::MidiMessage> &messages);

		// TODO: should this go here?
		std::map<int, std::vector<juce::MidiMessage>> getFakeMidiMap();

	private:
		juce::ComboBox &partMenu, &skillMenu;

		// std::vector<uint> interpretGuitarMidiMessages(const std::vector<juce::MidiMessage> &messages);
		// std::vector<uint> interpretDrumMidiMessages(const std::vector<juce::MidiMessage> &messages);

		// std::map<uint, std::string>
		// int[] gemNotes = { 60, 64, 67, 72, 76 };
};