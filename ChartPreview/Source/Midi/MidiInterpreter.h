/*
  ==============================================================================

    MidiInterpreter.h
    Created: 15 Jun 2024 3:57:52pm
    Author:  Noah Baxter

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Utils/Utils.h"

class MidiInterpreter
{
	public:
		MidiInterpreter(juce::ValueTree &state, NoteStateMapArray &noteStateMapArray, GridlineMap &gridlineMap, juce::CriticalSection &gridlineMapLock, juce::CriticalSection &noteStateMapLock);
		~MidiInterpreter();

		NoteStateMapArray &noteStateMapArray;
		GridlineMap &gridlineMap;
		juce::CriticalSection &gridlineMapLock;
		juce::CriticalSection &noteStateMapLock;

		bool isNoteHeld(uint pitch, PPQ position)
		{
			const juce::ScopedLock lock(noteStateMapLock);
			auto &noteStateMap = noteStateMapArray[pitch];
			auto it = noteStateMap.upper_bound(position);
			if (it == noteStateMap.begin())
			{
				return false;
			}
			else
			{
				--it;
				return it->second;
			}
		}

		GridlineMap generateGridlineWindow(PPQ trackWindowStart, PPQ trackWindowEnd);
		TrackWindow generateTrackWindow(PPQ trackWindowStart, PPQ trackWindowEnd);
		SustainWindow generateSustainWindow(PPQ trackWindowStart, PPQ trackWindowEnd, PPQ latencyBufferEnd);
		TrackFrame generateEmptyTrackFrame();
		
		static Gem getDrumGlyph(bool cymbal, bool dynamicsEnabled, Dynamic dynamic)
		{
			if (dynamicsEnabled)
			{
				switch (dynamic)
				{
				case Dynamic::GHOST:
					return cymbal ? Gem::CYM_GHOST : Gem::HOPO_GHOST;
					break;
				case Dynamic::ACCENT:
					return cymbal ? Gem::CYM_ACCENT : Gem::TAP_ACCENT;
					break;
				default:
					return cymbal ? Gem::CYM : Gem::NOTE;
					break;
				}
			}
			else
			{
				return cymbal ? Gem::CYM : Gem::NOTE;
			}
		}

	private:
		juce::ValueTree &state;

		uint getGuitarGemColumn(uint pitch);
		Gem getGuitarGemType(uint pitch, PPQ position);
		void addGuitarEventToFrame(TrackFrame &frame, PPQ position, uint pitch);
		
		uint getDrumGemColumn(uint pitch);
		Gem getDrumGemType(uint pitch, PPQ position, Dynamic dynamic);
		void addDrumEventToFrame(TrackFrame &frame, PPQ position, uint pitch, Dynamic dynamic);
};