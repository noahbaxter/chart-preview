/*
  ==============================================================================

    MidiInterpreter.h
    Created: 15 Jun 2024 3:57:52pm
    Author:  Noah Baxter

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../Utils/Utils.h"
#include "../Utils/MidiTypes.h"
#include "../Utils/ChordAnalyzer.h"
#include "../Utils/InstrumentMapper.h"
#include "../Utils/GemCalculator.h"
#include "../Utils/LaneDetector.h"

class MidiInterpreter
{
	public:
		MidiInterpreter(juce::ValueTree &state, NoteStateMapArray &noteStateMapArray, juce::CriticalSection &noteStateMapLock);
		~MidiInterpreter();

		NoteStateMapArray &noteStateMapArray;
		juce::CriticalSection &noteStateMapLock;

		bool isNoteHeld(uint pitch, PPQ position)
		{
			return ChordAnalyzer::isNoteHeld(pitch, position, noteStateMapArray, noteStateMapLock);
		}

		TrackWindow generateTrackWindow(PPQ trackWindowStart, PPQ trackWindowEnd);
		SustainWindow generateSustainWindow(PPQ trackWindowStart, PPQ trackWindowEnd, PPQ latencyBufferEnd);
		TrackFrame generateEmptyTrackFrame();

	private:
		juce::ValueTree &state;

		void addGuitarEventToFrame(TrackFrame &frame, PPQ position, uint pitch, Gem gemType);
		void addDrumEventToFrame(TrackFrame &frame, PPQ position, uint pitch, Gem gemType);

		// Helper functions for testing
		TrackWindow generateFakeTrackWindow(PPQ trackWindowStartPPQ, PPQ trackWindowEndPPQ);
		SustainWindow generateFakeSustains(PPQ trackWindowStartPPQ, PPQ trackWindowEndPPQ);
};