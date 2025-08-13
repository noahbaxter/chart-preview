#include "MidiProcessor.h"

void MidiProcessor::process(juce::MidiBuffer& midiMessages,
                            const juce::AudioPlayHead::PositionInfo& positionInfo,
                            uint blockSizeInSamples,
                            uint latencyInSamples,
                            double sampleRate)
{
    auto ppqPosition = positionInfo.getPpqPosition();
    auto bpm = positionInfo.getBpm();
    
    if (!ppqPosition.hasValue() || !bpm.hasValue())
        return;
        
    PPQ startPPQ = *ppqPosition;
    // Initial estimate for end and latency in PPQ
    PPQ endPPQ = startPPQ + estimatePPQFromSamples(blockSizeInSamples, *bpm, sampleRate);
    PPQ latencyPPQ = estimatePPQFromSamples(latencyInSamples, *bpm, sampleRate);

    // Erase events in PPQ range
    if (endPPQ >= lastProcessedPPQ)
    {
        for (auto &noteStateMap : noteStateMapArray)
        {
            auto lower = noteStateMap.upper_bound(PPQ(startPPQ - latencyPPQ));
            if (lower != noteStateMap.begin())
            {
                --lower;
                noteStateMap.erase(noteStateMap.begin(), lower);
            }

            auto upper = noteStateMap.upper_bound(PPQ(startPPQ + latencyPPQ));
            noteStateMap.erase(upper, noteStateMap.end());
        }
    }

    uint numMessages = 0;
    for (const auto message : midiMessages)
    {
        // For each message, we need to estimate its PPQ position
        uint localSamplePos = message.samplePosition;
        PPQ estimatedMessagePPQ = startPPQ + estimatePPQFromSamples(localSamplePos, *bpm, sampleRate);

        auto midiMessage = message.getMessage();
        if (midiMessage.isNoteOn() || midiMessage.isNoteOff())
        {
            if (midiMessage.isNoteOff() && estimatedMessagePPQ > 0.0)
            {
                estimatedMessagePPQ -= 1;
            }

            uint noteNumber = midiMessage.getNoteNumber();
            uint velocity = midiMessage.isNoteOn() ? midiMessage.getVelocity() : 0;
            noteStateMapArray[noteNumber][PPQ(estimatedMessagePPQ)] = velocity;
        }

        if (++numMessages >= maxNumMessagesPerBlock)
            break;
    }

    lastProcessedPPQ = std::max(endPPQ, lastProcessedPPQ);
}

PPQ MidiProcessor::estimatePPQFromSamples(uint samples, double bpm, double sampleRate)
{
    // Use current BPM as best estimate - this will be slightly inaccurate with tempo automation
    double timeInSeconds = samples / sampleRate;
    double beatsPerSecond = bpm / 60.0;
    return PPQ(timeInSeconds * beatsPerSecond);
}