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
        // Erase notes in PPQ range
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

        // Erase gridlines in PPQ range
        {
            const juce::ScopedLock lock(gridlineMapLock);
            auto lower = gridlineMap.upper_bound(PPQ(startPPQ - latencyPPQ));
            if (lower != gridlineMap.begin())
            {
                --lower;
                gridlineMap.erase(gridlineMap.begin(), lower);
            }

            auto upper = gridlineMap.upper_bound(PPQ(startPPQ + latencyPPQ));
            gridlineMap.erase(upper, gridlineMap.end());
        }
    }

    uint numMessages = 0;
    PPQ lastTimeSignatureChangePPQ = 0.0;

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
        else if (midiMessage.isMetaEvent())
        {
            // Time signature changes
            if (midiMessage.getMetaEventType() == 0x58)
            {
                auto data = midiMessage.getMetaEventData();
                if (midiMessage.getMetaEventLength() >= 4)
                {
                    uint numerator = data[0];
                    uint denominator = 1 << data[1]; // 2^denominator
                    // data[2] = MIDI clocks per metronome click
                    // data[3] = 32nd notes per quarter note

                    {
                        const juce::ScopedLock lock(gridlineMapLock);
                        generateGridLines(lastTimeSignatureChangePPQ, estimatedMessagePPQ, numerator, denominator);
                    }
                    lastTimeSignatureChangePPQ = estimatedMessagePPQ;
                }
            }

            if (++numMessages >= maxNumMessagesPerBlock)
                break;
        }
    }

    lastProcessedPPQ = std::max(endPPQ, lastProcessedPPQ);
    
    // Generate gridlines for any ungrided space
    if (lastTimeSignatureChangePPQ < lastProcessedPPQ || gridlineMap.empty())
    {
        const juce::ScopedLock lock(gridlineMapLock);
        auto timeSig = positionInfo.getTimeSignature();
        if (timeSig.hasValue())
        {
            generateGridLines(lastTimeSignatureChangePPQ, lastProcessedPPQ, timeSig->numerator, timeSig->denominator);
        }
        else
        {
            // Default to 4/4 if no time signature info
            generateGridLines(lastTimeSignatureChangePPQ, lastProcessedPPQ, 4, 4);
        }
    }
}

PPQ MidiProcessor::estimatePPQFromSamples(uint samples, double bpm, double sampleRate)
{
    // Use current BPM as best estimate - this will be slightly inaccurate with tempo automation
    double timeInSeconds = samples / sampleRate;
    double beatsPerSecond = bpm / 60.0;
    return PPQ(timeInSeconds * beatsPerSecond);
}

void MidiProcessor::generateGridLines(PPQ startPPQ, PPQ endPPQ, uint timeSignatureNumerator, uint timeSignatureDenominator)
{
    // Prevent overlapping gridlines
    if (!gridlineMap.empty()) {
        auto lastGridlineIt = gridlineMap.rbegin();
        PPQ lastGridlinePPQ = lastGridlineIt->first;
        startPPQ = lastGridlinePPQ;
    }

    // Quarter notes always equal 1.0 PPQ regardless of time signature
    const double ppqPerQuarterNote = 1.0;
    
    // Calculate PPQ per beat based on time signature
    // For 4/4: beat = quarter note = 1.0 PPQ
    // For 3/8: beat = eighth note = 0.5 PPQ  
    // For 6/8: beat = eighth note = 0.5 PPQ
    double ppqPerBeat = ppqPerQuarterNote * (4.0 / timeSignatureDenominator);
    
    // Place gridlines at most every half beat
    PPQ ppqPerHalfBeat = PPQ(ppqPerBeat * 0.5);
    
    // Start from the time signature change point
    PPQ currentPPQ = startPPQ;
    
    while (currentPPQ <= endPPQ)
    {
        // Calculate position relative to the start of this time signature block
        double beatsFromTimeSigStart = (currentPPQ.toDouble() - startPPQ.toDouble()) / ppqPerBeat;
        
        // Convert to a scaled integer representation (1000 = 1 beat)
        int64_t scaledBeatsFromStart = static_cast<int64_t>(beatsFromTimeSigStart * 1000.0);
        int64_t scaledTimeSigNumerator = timeSignatureNumerator * 1000;

        Gridline gridlineType;
        
        // Measure: every timeSignatureNumerator beats from the start
        if (scaledBeatsFromStart >= 0 && scaledBeatsFromStart % scaledTimeSigNumerator == 0)
        {
            gridlineType = Gridline::MEASURE;
        }
        // Beat: every whole beat from the start
        else if (scaledBeatsFromStart >= 0 && scaledBeatsFromStart % 1000 == 0)
        {
            gridlineType = Gridline::BEAT;
        }
        // Half-beat: every half beat
        else // if (scaledBeatsFromStart >= 0 && scaledBeatsFromStart % 500 == 0)
        {
            gridlineType = Gridline::HALF_BEAT;
        }

        gridlineMap[currentPPQ] = gridlineType;

        // Move to the next half-beat position based on the current time signature
        currentPPQ += ppqPerHalfBeat;
    }
}