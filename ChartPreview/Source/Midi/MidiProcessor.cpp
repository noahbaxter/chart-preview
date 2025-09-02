#include "MidiProcessor.h"

MidiProcessor::MidiProcessor(juce::ValueTree &state) : state(state)
{
}

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
            // Keep 2 events before window to prevent sustain modifier note ons from being deleted
            if (lower != noteStateMap.begin()) --lower;
            if (lower != noteStateMap.begin()) --lower;
            noteStateMap.erase(noteStateMap.begin(), lower);

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
    using Drums = MidiPitchDefinitions::Drums;
    
    // Collect all note messages with their positions
    struct NoteMessage {
        juce::MidiMessage message;
        PPQ position;
        uint pitch;
        bool isSustainedModifier;
    };
    
    std::vector<NoteMessage> noteMessages;
    uint numMessages = 0;
    
    for (const auto message : midiMessages)
    {
        auto midiMessage = message.getMessage();
        if (midiMessage.isNoteOn() || midiMessage.isNoteOff())
        {
            PPQ messagePositionPPQ = startPPQ + calculatePPQSegment(message.samplePosition, bpm, sampleRate);
            uint pitch = midiMessage.getNoteNumber();
            
            // Identify sustained modifier notes for priority processing
            bool isSustainedModifier = MidiUtility::isSustainedModifierPitch(pitch);
            
            noteMessages.push_back({midiMessage, messagePositionPPQ, pitch, isSustainedModifier});
        }
        
        if (++numMessages >= maxNumMessagesPerBlock) break;
    }
    
    // Sort so sustained modifier notes are processed first (for all instruments)
    // This ensures modifiers like tom markers, HOPO/strum markers, star power, etc. 
    // are active before the actual notes that depend on them
    std::sort(noteMessages.begin(), noteMessages.end(), [](const NoteMessage& a, const NoteMessage& b) {
        if (a.isSustainedModifier != b.isSustainedModifier) return a.isSustainedModifier > b.isSustainedModifier; // Modifiers first
        return a.position < b.position; // Then by time
    });
    
    // Process all messages in order
    for (const auto& noteMsg : noteMessages) {
        processNoteMessage(noteMsg.message, noteMsg.position);
    }
}

void MidiProcessor::processNoteMessage(const juce::MidiMessage &midiMessage, PPQ messagePPQ)
{
    uint noteNumber = midiMessage.getNoteNumber();
    uint velocity = midiMessage.isNoteOn() ? midiMessage.getVelocity() : 0;
    
    // Ensure notes that stop and start at the same PPQ are processed in correct order
    if (midiMessage.isNoteOff()) {
        messagePPQ -= PPQ(1); // Smallest possible PPQ unit
    }
    
    // Calculate the final Gem type at MIDI processing time
    Gem gemType = Gem::NONE;
    if (velocity > 0) {
        if (isPart(state, Part::GUITAR)) {
            gemType = getGuitarGemType(noteNumber, messagePPQ);
        } else if (isPart(state, Part::DRUMS)) {
            Dynamic dynamic = (Dynamic)velocity;
            gemType = getDrumGemType(noteNumber, messagePPQ, dynamic);
        }
    }

    const juce::ScopedLock lock(noteStateMapLock);
    noteStateMapArray[noteNumber][messagePPQ] = NoteData(velocity, gemType);
}

uint MidiProcessor::getGuitarGemColumn(uint pitch)
{
    return MidiUtility::getGuitarGemColumn(pitch, state);
}

Gem MidiProcessor::getGuitarGemType(uint pitch, PPQ position)
{
    return MidiUtility::getGuitarGemType(pitch, position, state, noteStateMapArray, noteStateMapLock);
}


uint MidiProcessor::getDrumGemColumn(uint pitch)
{
    return MidiUtility::getDrumGemColumn(pitch, state);
}

Gem MidiProcessor::getDrumGemType(uint pitch, PPQ position, Dynamic dynamic)
{
    return MidiUtility::getDrumGemType(pitch, position, dynamic, state, noteStateMapArray, noteStateMapLock);
}

void MidiProcessor::refreshMidiDisplay()
{
    const juce::ScopedLock lock(noteStateMapLock);
    
    // Iterate through all pitches and all notes
    for (uint pitch = 0; pitch < 128; pitch++)
    {
        NoteStateMap& noteStateMap = noteStateMapArray[pitch];
        
        for (auto& noteEntry : noteStateMap)
        {
            PPQ position = noteEntry.first;
            NoteData& noteData = noteEntry.second;
            
            // Only recalculate for note-on events
            if (noteData.velocity > 0)
            {
                // Recalculate the gem type with current settings
                if (isPart(state, Part::GUITAR)) {
                    noteData.gemType = getGuitarGemType(pitch, position);
                } else if (isPart(state, Part::DRUMS)) {
                    Dynamic dynamic = (Dynamic)noteData.velocity;
                    noteData.gemType = getDrumGemType(pitch, position, dynamic);
                }
            }
        }
    }
}