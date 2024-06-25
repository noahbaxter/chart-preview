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
		MidiInterpreter(juce::ValueTree &state, NoteStateMapArray &noteStateMapArray);
		~MidiInterpreter();

		NoteStateMapArray &noteStateMapArray;
		bool isNoteHeld(uint pitch, uint position)
		{
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

		TrackWindow generateTrackWindow(uint trackWindowStart, uint trackWindowEnd);
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

		void addGuitarEventToFrame(TrackFrame &frame, uint position, uint pitch);
		void addDrumEventToFrame(TrackFrame &frame, uint position, uint pitch, Dynamic dynamic);
};