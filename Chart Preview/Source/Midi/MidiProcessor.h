#pragma once

#include <JuceHeader.h>
#include "../Utils.h"

#include "../../ThirdParty/reaper-sdk/sdk/reaper_plugin_functions.h"

class MidiProcessor
{
    public:
        MidiProcessor(juce::ValueTree &state);
        ~MidiProcessor();

        void process(juce::MidiBuffer &midiMessages,
                     uint startPositionInSamples,
                     uint endPositionInSamples,
                     uint latencyInSamples);

        MediaItem_Take& fetchCurrentMediaItem(uint startPositionInSamples)
        void fetchAllMidiEvents(MediaItem_Take *take, juce::MidiBuffer &midiMessages);

        NoteStateMapArray noteStateMapArray;

    private:
        juce::ValueTree &state;

        // The maximum number of messages to process per block
        const uint maxNumMessagesPerBlock = 256;

        bool isLookaheadMode()
        {
            return (bool)state.getProperty("isReaper");
        }

        // reaper sdk
        void (*MIDI_GetAllEvts)(MediaItem_Take *take, char *buf, int *buf_sz);
};