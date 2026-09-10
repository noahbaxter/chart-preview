/*
  ==============================================================================

    MidiInterpreter.h
    Created: 15 Jun 2024 3:57:52pm
    Author:  Noah Baxter

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../Utils/ChartTypes.h"
#include "../Utils/MidiTypes.h"
#include "../Utils/InstrumentMapper.h"
#include "../DiscoFlipState.h"
#include "../StrictHatPedalState.h"
#include "PartWindow.h"
#include "TrackResolver.h"

class MidiInterpreter
{
	public:
		MidiInterpreter(juce::ValueTree &state, NoteStateMapArray &noteStateMapArray, juce::CriticalSection &noteStateMapLock);
		MidiInterpreter(Part part, juce::ValueTree &state, NoteStateMapArray &noteStateMapArray, juce::CriticalSection &noteStateMapLock);

		Part instrumentPart = Part::GUITAR;
		~MidiInterpreter();

		void setDiscoFlipState(const DiscoFlipState* flipState) { discoFlip = flipState; }
		const DiscoFlipState* getDiscoFlipState() const { return discoFlip; }

		void setStrictHatPedalState(const StrictHatPedalState* s) { strictHatPedal = s; }
		const StrictHatPedalState* getStrictHatPedalState() const { return strictHatPedal; }

		const juce::ValueTree& getState() const { return state; }

		NoteStateMapArray &noteStateMapArray;
		juce::CriticalSection &noteStateMapLock;

		// Resolve all 4 difficulties in one pass, one lock
		PartWindow resolveAllDifficulties(PPQ windowStart, PPQ windowEnd, PPQ latencyEnd);

	private:
		juce::ValueTree &state;
		const DiscoFlipState* discoFlip = nullptr;
		const StrictHatPedalState* strictHatPedal = nullptr;
};
