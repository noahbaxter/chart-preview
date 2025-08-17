#include "MidiProcessor.h"

void MidiProcessor::process(juce::MidiBuffer &midiMessages,
                            const juce::AudioPlayHead::PositionInfo &positionInfo,
                            uint blockSizeInSamples,
                            uint latencyInSamples,
                            double sampleRate)
{
    auto ppqPosition = positionInfo.getPpqPosition();
    auto bpm = positionInfo.getBpm();
    auto timeSig = positionInfo.getTimeSignature();

    if (!ppqPosition.hasValue() || !bpm.hasValue() || !timeSig.hasValue()) return;

    PPQ startPPQ = *ppqPosition;
    PPQ endPPQ = buildTimingMap(midiMessages,
                                blockSizeInSamples,
                                startPPQ,
                                *bpm,
                                timeSig->numerator,
                                timeSig->denominator,
                                sampleRate);
    
    // Use stable host BPM for latency calculation (for audio processing cleanup)
    PPQ latencyPPQ = calculatePPQSegment(latencyInSamples, *bpm, sampleRate);

    cleanupOldEvents(startPPQ, endPPQ, latencyPPQ);
    buildGridLineMap(startPPQ, endPPQ);
    buildNoteStateMap(midiMessages, startPPQ, sampleRate);
    
    // Update last processed PPQ for cleanup tracking
    lastProcessedPPQ = std::max(endPPQ, lastProcessedPPQ);
}

//================================================================================
// Calculate the total PPQ length of the buffer
//================================================================================

PPQ MidiProcessor::buildTimingMap(juce::MidiBuffer &midiMessages,
                                  uint bufferSizeInSamples,
                                  PPQ startPPQ,
                                  double initialBpm,
                                  uint initialTimeSignatureNumerator,
                                  uint initialTimeSignatureDenominator,
                                  double sampleRate)
{
    // Clear previous timing maps for this frame
    tempoChanges.clear();
    timeSignatureChanges.clear();
    
    // Set initial values
    tempoChanges[startPPQ] = initialBpm;
    timeSignatureChanges[startPPQ] = {initialTimeSignatureNumerator, initialTimeSignatureDenominator};

    uint currentSample = 0;
    PPQ currentPPQ = startPPQ;
    double currentBpm = initialBpm;

    // Process MIDI messages to find tempo and time signature changes
    // We'll use the accurate PPQ positioning system to place these events
    for (const auto message : midiMessages)
    {
        auto midiMessage = message.getMessage();
        
        // Update currentPPQ for all messages to maintain accurate timing
        currentPPQ += calculatePPQSegment(message.samplePosition - currentSample, currentBpm, sampleRate);
        
        if (midiMessage.isMetaEvent())
        {
            // Tempo change
            if (midiMessage.getMetaEventType() == 0x51)
            {
                auto data = midiMessage.getMetaEventData();
                if (midiMessage.getMetaEventLength() >= 3)
                {
                    uint microsecondsPerQuarter = (data[0] << 16) | (data[1] << 8) | data[2];
                    currentBpm = 60000000.0 / microsecondsPerQuarter;
                    tempoChanges[currentPPQ] = currentBpm;
                }
            }
            // Time signature change
            else if (midiMessage.getMetaEventType() == 0x58)
            {
                auto data = midiMessage.getMetaEventData();
                if (midiMessage.getMetaEventLength() >= 4)
                {
                    uint numerator = data[0];
                    uint denominator = 1 << data[1]; // 2^denominator
                    
                    // Store time signature change at the calculated PPQ position
                    timeSignatureChanges[currentPPQ] = {numerator, denominator};
                }
            }
        }

        currentSample = message.samplePosition;
    }

    // Calculate accurate end PPQ position using the timing maps
    PPQ endPPQ = calculatePPQPosition(startPPQ, bufferSizeInSamples, sampleRate);
    return endPPQ;
}

PPQ MidiProcessor::calculatePPQSegment(uint samples, double bpm, double sampleRate)
{
    // Calculate the PPQ segment for a given number of samples at a fixed BPM
    double timeInSeconds = samples / sampleRate;
    double beatsPerSecond = bpm / 60.0;
    return PPQ(timeInSeconds * beatsPerSecond);
}

PPQ MidiProcessor::calculatePPQPosition(PPQ startPPQ, uint sampleOffset, double sampleRate)
{
    // MUST build the timing map first
    if (tempoChanges.empty())
    {
        // Fallback to default BPM calculation
        double defaultBPM = 120.0;
        return startPPQ + calculatePPQSegment(sampleOffset, defaultBPM, sampleRate);
    }

    if (sampleOffset == 0) return startPPQ;

    // Find the tempo that applies at the start of this frame
    auto tempoIt = tempoChanges.upper_bound(startPPQ);
    if (tempoIt != tempoChanges.begin())
        --tempoIt;

    double currentBpm = tempoIt->second;
    PPQ currentPPQ = startPPQ;
    uint remainingSamples = sampleOffset;

    // Iterate through tempo changes to calculate accurate PPQ position
    auto nextTempoIt = tempoIt;
    ++nextTempoIt;

    while (remainingSamples > 0 && nextTempoIt != tempoChanges.end())
    {
        // Calculate how many samples we can process with current tempo
        PPQ tempoChangePPQ = nextTempoIt->first;
        PPQ ppqToTempoChange = tempoChangePPQ - currentPPQ;

        // Convert PPQ to samples at current tempo
        uint samplesAtCurrentTempo = static_cast<uint>((ppqToTempoChange.toDouble() * 60.0 / currentBpm) * sampleRate);

        if (samplesAtCurrentTempo <= remainingSamples)
        {
            // We can process up to the tempo change
            currentPPQ = tempoChangePPQ;
            remainingSamples -= samplesAtCurrentTempo;
            currentBpm = nextTempoIt->second;
            ++nextTempoIt;
        }
        else
        {
            // We can't reach the next tempo change, process remaining samples
            PPQ ppqForRemainingSamples = calculatePPQSegment(remainingSamples, currentBpm, sampleRate);
            currentPPQ += ppqForRemainingSamples;
            uint samplesProcessed = remainingSamples;
            remainingSamples = 0;
        }
    }

    // Process any remaining samples with the final tempo
    if (remainingSamples > 0)
    {
        PPQ ppqForRemainingSamples = calculatePPQSegment(remainingSamples, currentBpm, sampleRate);
        currentPPQ += ppqForRemainingSamples;
    }

    return currentPPQ;
}

//================================================================================
// CLEANUP
//================================================================================

void MidiProcessor::cleanupOldEvents(PPQ startPPQ, PPQ endPPQ, PPQ latencyPPQ)
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

//================================================================================
// GRIDLINES
//================================================================================

void MidiProcessor::buildGridLineMap(PPQ startPPQ, PPQ endPPQ)
{
    const juce::ScopedLock lock(gridlineMapLock);
    
    // Find the time signature that applies at the start
    auto timeSigIt = timeSignatureChanges.upper_bound(startPPQ);
    if (timeSigIt != timeSignatureChanges.begin())
        --timeSigIt;
    
    uint currentNumerator = 4;  // Default
    uint currentDenominator = 4; // Default
    
    if (timeSigIt != timeSignatureChanges.end())
    {
        currentNumerator = timeSigIt->second.first;
        currentDenominator = timeSigIt->second.second;
    }
    
    // Generate gridlines for each time signature section
    PPQ currentStart = startPPQ;
    auto nextTimeSigIt = timeSigIt;
    ++nextTimeSigIt;
    
    while (currentStart < endPPQ)
    {
        PPQ currentEnd = endPPQ;
        
        // Check if there's a time signature change before the end
        if (nextTimeSigIt != timeSignatureChanges.end() && nextTimeSigIt->first < endPPQ)
        {
            currentEnd = nextTimeSigIt->first;
        }
        
        // Generate gridlines for this section
        generateGridLines(currentStart, currentEnd, currentNumerator, currentDenominator);
        
        // Move to next section
        currentStart = currentEnd;
        
        if (nextTimeSigIt != timeSignatureChanges.end() && nextTimeSigIt->first < endPPQ)
        {
            currentNumerator = nextTimeSigIt->second.first;
            currentDenominator = nextTimeSigIt->second.second;
            ++nextTimeSigIt;
        }
    }
}

void MidiProcessor::generateGridLines(PPQ startPPQ, PPQ endPPQ, uint timeSignatureNumerator, uint timeSignatureDenominator)
{
    // Prevent overlapping gridlines
    if (!gridlineMap.empty())
    {
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

//================================================================================
// NOTE STATE MAP
//================================================================================

void MidiProcessor::buildNoteStateMap(juce::MidiBuffer &midiMessages, PPQ startPPQ, double sampleRate)
{
    uint numMessages = 0;
    for (const auto message : midiMessages)
    {
        // Use timing maps to calculate accurate PPQ position for each message
        PPQ messagePositionPPQ = calculatePPQPosition(startPPQ, message.samplePosition, sampleRate);

        auto midiMessage = message.getMessage();
        if (midiMessage.isNoteOn() || midiMessage.isNoteOff())
        {
            processNoteMessage(midiMessage, messagePositionPPQ);
        }

        if (++numMessages >= maxNumMessagesPerBlock)
            break;
    }
}

void MidiProcessor::processNoteMessage(const juce::MidiMessage &midiMessage, PPQ messagePPQ)
{
    if (midiMessage.isNoteOff() && messagePPQ > 0.0)
    {
        messagePPQ -= 1;
    }

    uint noteNumber = midiMessage.getNoteNumber();
    uint velocity = midiMessage.isNoteOn() ? midiMessage.getVelocity() : 0;
    noteStateMapArray[noteNumber][messagePPQ] = velocity;
}