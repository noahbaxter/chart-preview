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
    // Calculate end PPQ position using host BPM
    PPQ endPPQ = startPPQ + calculatePPQSegment(blockSizeInSamples, *bpm, sampleRate);
    
    buildGridlineMap(startPPQ, endPPQ, timeSig->numerator, timeSig->denominator);
    
    // Use stable host BPM for latency calculation (for audio processing cleanup)
    PPQ latencyPPQ = calculatePPQSegment(latencyInSamples, *bpm, sampleRate);

    cleanupOldEvents(startPPQ, endPPQ, latencyPPQ); // Placeholder for visual window bounds
    processMidiMessages(midiMessages, startPPQ, sampleRate, *bpm);
    
    // Update last processed PPQ for cleanup tracking
    lastProcessedPPQ = std::max(endPPQ, lastProcessedPPQ);
}

//================================================================================
// GRIDLINES
//================================================================================

void MidiProcessor::buildGridlineMap(PPQ startPPQ, PPQ endPPQ, uint initialTimeSignatureNumerator, uint initialTimeSignatureDenominator)
{
    // Check if time signature has changed
    if (initialTimeSignatureNumerator != lastTimeSignatureNumerator || 
        initialTimeSignatureDenominator != lastTimeSignatureDenominator)
    {
        lastTimeSignatureChangePPQ = startPPQ;
        lastTimeSignatureNumerator = initialTimeSignatureNumerator;
        lastTimeSignatureDenominator = initialTimeSignatureDenominator;
    }
    
    // Place gridlines for any boundaries that fall within this buffer
    double measureLength = static_cast<double>(initialTimeSignatureNumerator) * (4.0 / initialTimeSignatureDenominator);
    double relativeStart = (startPPQ - lastTimeSignatureChangePPQ).toDouble();
    double relativeEnd = (endPPQ - lastTimeSignatureChangePPQ).toDouble();
    
    const juce::ScopedLock lock(gridlineMapLock);
    
    // Find first half-beat boundary at or after relativeStart
    double firstHalfBeat = std::ceil(relativeStart / 0.5) * 0.5;
    
    // Place gridlines for all half-beat boundaries in this buffer
    for (double relativePPQ = firstHalfBeat; relativePPQ <= relativeEnd; relativePPQ += 0.5)
    {
        PPQ gridlinePPQ = lastTimeSignatureChangePPQ + PPQ(relativePPQ);
        
        // Determine gridline type based on position relative to time signature start
        if (std::abs(std::fmod(relativePPQ, measureLength)) < 0.001)
        {
            gridlineMap[gridlinePPQ] = Gridline::MEASURE;
        }
        else if (std::abs(std::fmod(relativePPQ, 1.0)) < 0.001)
        {
            gridlineMap[gridlinePPQ] = Gridline::BEAT;
        }
        else
        {
            gridlineMap[gridlinePPQ] = Gridline::HALF_BEAT;
        }
    }
}

PPQ MidiProcessor::calculatePPQSegment(uint samples, double bpm, double sampleRate)
{
    // Calculate the PPQ segment for a given number of samples at a fixed BPM
    double timeInSeconds = samples / sampleRate;
    double beatsPerSecond = bpm / 60.0;
    return PPQ(timeInSeconds * beatsPerSecond);
}


//================================================================================
// CLEANUP
//================================================================================

void MidiProcessor::cleanupOldEvents(PPQ startPPQ, PPQ endPPQ, PPQ latencyPPQ)
{
    // Calculate conservative cleanup bounds that never delete events still in the visual range
    // Use the maximum of audio latency and visual window bounds to ensure we don't delete visible events
    PPQ conservativeStartPPQ = startPPQ - latencyPPQ;
    PPQ conservativeEndPPQ = startPPQ + latencyPPQ;
    
    // Get current visual window bounds to clamp cleanup
    PPQ currentVisualStart = PPQ(0.0);
    PPQ currentVisualEnd = PPQ(0.0);
    {
        const juce::ScopedLock lock(visualWindowLock);
        currentVisualStart = visualWindowStartPPQ;
        currentVisualEnd = visualWindowEndPPQ;
    }
    
    // If visual window bounds are available, clamp cleanup to never go beyond them
    if (currentVisualStart > PPQ(0.0) && currentVisualEnd > PPQ(0.0))
    {
        // Never delete events that could still be in the visual window
        PPQ oldStartPPQ = conservativeStartPPQ;
        PPQ oldEndPPQ = conservativeEndPPQ;
        
        conservativeStartPPQ = std::min(conservativeStartPPQ, currentVisualStart);
        conservativeEndPPQ = std::max(conservativeEndPPQ, currentVisualEnd);
    }
    
    // Erase notes in PPQ range
    {
        const juce::ScopedLock lock(noteStateMapLock);
        for (auto &noteStateMap : noteStateMapArray)
        {
            auto lower = noteStateMap.upper_bound(conservativeStartPPQ);
            if (lower != noteStateMap.begin())
            {
                --lower;
                noteStateMap.erase(noteStateMap.begin(), lower);
            }

            auto upper = noteStateMap.upper_bound(conservativeEndPPQ);
            noteStateMap.erase(upper, noteStateMap.end());
        }
    }

    // Erase gridlines in PPQ range
    {
        const juce::ScopedLock lock(gridlineMapLock);
        auto lower = gridlineMap.upper_bound(conservativeStartPPQ);
        if (lower != gridlineMap.begin())
        {
            --lower;
            gridlineMap.erase(gridlineMap.begin(), lower);
        }

        auto upper = gridlineMap.upper_bound(conservativeEndPPQ);
        gridlineMap.erase(upper, gridlineMap.end());
    }
}


//================================================================================
// NOTE STATE MAP
//================================================================================

void MidiProcessor::processMidiMessages(juce::MidiBuffer &midiMessages, PPQ startPPQ, double sampleRate, double bpm)
{
    uint numMessages = 0;
    for (const auto message : midiMessages)
    {
        // Calculate PPQ position for each message using host BPM
        PPQ messagePositionPPQ = startPPQ + calculatePPQSegment(message.samplePosition, bpm, sampleRate);

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
    // if (midiMessage.isNoteOff() && messagePPQ > 0.0)
    // {
    //     messagePPQ -= 1;
    // }
    uint noteNumber = midiMessage.getNoteNumber();
    uint velocity = midiMessage.isNoteOn() ? midiMessage.getVelocity() : 0;

    const juce::ScopedLock lock(noteStateMapLock);
    noteStateMapArray[noteNumber][messagePPQ] = velocity;
}