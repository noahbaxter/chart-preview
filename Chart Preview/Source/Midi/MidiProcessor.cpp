#include "MidiProcessor.h"


MidiProcessor::MidiProcessor(juce::ValueTree &state)
    : state(state)
{
    juce::PluginHostType hostType;
    if (hostType.isReaper())
    {
        state.setProperty("isReaper", true, nullptr);
    }
}

MidiProcessor::~MidiProcessor()
{
}

void MidiProcessor::process(juce::MidiBuffer &midiMessages, uint startPositionInSamples, uint endPositionInSamples, uint latencyInSamples)
{
    MediaItem_Take *take = fetchCurrentMediaItem(startPositionInSamples);
    fetchAllMidiEvents(take, midiMessages);

    // Erase all events in the map within the range of the current block so changes are reflected
    for (auto &noteStateMap : noteStateMapArray)
    {
        if (isLookaheadMode())
        {
            noteStateMap.erase(noteStateMap.begin(), noteStateMap.end());
        }
        else
        {
            // // OLD - build and maintain all midi events for the project
            // auto lower = noteStateMap.lower_bound(startPositionInSamples);
            // auto upper = noteStateMap.lower_bound(endPositionInSamples);
            // noteStateMap.erase(lower, upper);

            // NEW - build and maintain only the midi events for the current block + adjascent events for each note
            auto lower = noteStateMap.upper_bound(std::max<int>(startPositionInSamples - latencyInSamples, 0));
            if (lower != noteStateMap.begin())
            {
                --lower;    // Keep last event of this note
                noteStateMap.erase(noteStateMap.begin(), lower);
            }
            // Remove all events after the end of the chart window, represented by the amount of latency
            auto upper = noteStateMap.upper_bound((int)(startPositionInSamples + latencyInSamples));
            noteStateMap.erase(upper, noteStateMap.end());
        }
    }

    uint numMessages = 0;
    for (const auto &message : midiMessages)
    {
        uint localMessagePositionInSamples = message.samplePosition;
        uint globalMessagePositionInSamples = startPositionInSamples + localMessagePositionInSamples;
        auto midiMessage = message.getMessage();

        if (midiMessage.isNoteOn() || midiMessage.isNoteOff())
        {
            // Prevent note offs from being at the same position as note ons
            if(midiMessage.isNoteOff() && globalMessagePositionInSamples > 0)
            {
                globalMessagePositionInSamples -= 1;
            }
            
            uint noteNumber = midiMessage.getNoteNumber();
            uint velocity = midiMessage.isNoteOn() ? midiMessage.getVelocity() : 0; // Note off MUST be velocity 0
            noteStateMapArray[noteNumber][globalMessagePositionInSamples] = velocity;
        }

        if (++numMessages >= maxNumMessagesPerBlock)
        {
            break;
        }
    }
}

MediaItem_Take& MidiProcessor::fetchCurrentMediaItem(uint startPositionInSamples)
{
    // Get the track that the VST is hosted on
    TrackEnvelope *track = nullptr;
    int numTracks = CountTracks(nullptr);
    for (int i = 0; i < numTracks; ++i)
    {
        MediaTrack *currentTrack = GetTrack(nullptr, i);
        int numFX = TrackFX_GetCount(currentTrack);
        for (int j = 0; j < numFX; ++j)
        {
            if (TrackFX_GetFXGUID(currentTrack, j) == "") // Assuming pluginGUID is a member variable storing the GUID of the VST
            {
                track = currentTrack;
                break;
            }
        }
        if (track != nullptr)
        {
            break;
        }
    }

    if (track == nullptr)
    {
        // Handle the case when track is not available
        return nullptr;
    }

    // Get the current MediaItem_Take on the track at the current position
    MediaItem_Take* take = nullptr;
    double cursorPosition = GetCursorPosition(); // Get the current cursor position in seconds

    int numItems = CountTrackMediaItems(track);
    for (int i = 0; i < numItems; ++i)
    {
        MediaItem* item = GetTrackMediaItem(track, i);
        if (item != nullptr)
        {
            MediaItem_Take* currentTake = GetActiveTake(item);
            if (currentTake != nullptr)
            {
                double itemStart = GetMediaItemInfo_Value(item, "D_POSITION");
                double itemLength = GetMediaItemInfo_Value(item, "D_LENGTH");
                double itemEnd = itemStart + itemLength;

                if (itemStart <= cursorPosition && itemEnd >= cursorPosition)
                {
                    take = currentTake;
                    break;
                }
            }
        }
    }

    if (take != nullptr)
    {
        // Use the obtained take for further processing
        fetchAllMidiEvents(take, midiMessages);
    }

    return take;
}

void MidiProcessor::fetchAllMidiEvents(MediaItem_Take *take, juce::MidiBuffer &midiMessages)
{
    int bufSize = 0;
    MIDI_GetAllEvts(take, nullptr, &bufSize); // Get the required buffer size

    std::vector<char> buffer(bufSize);
    if (MIDI_GetAllEvts(take, buffer.data(), &bufSize))
    {
        const char *ptr = buffer.data();
        const char *end = ptr + bufSize;

        while (ptr < end)
        {
            int offset = *(int *)ptr;
            ptr += sizeof(int);
            char flag = *ptr++; // flag
            int msglen = *(int *)ptr;
            ptr += sizeof(int);
            const unsigned char *msg = (const unsigned char *)ptr;
            ptr += msglen;

            juce::MidiMessage midiMessage(msg, msglen);
            midiMessages.addEvent(midiMessage, offset);
        }
    }
}