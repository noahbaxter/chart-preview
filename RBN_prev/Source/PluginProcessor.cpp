/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
RBN_prevAudioProcessor::RBN_prevAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    print("START");
    // setAcceptsMidi(true);
}

RBN_prevAudioProcessor::~RBN_prevAudioProcessor()
{
}

//==============================================================================
const juce::String RBN_prevAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool RBN_prevAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool RBN_prevAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool RBN_prevAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double RBN_prevAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int RBN_prevAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int RBN_prevAudioProcessor::getCurrentProgram()
{
    return 0;
}

void RBN_prevAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String RBN_prevAudioProcessor::getProgramName (int index)
{
    return {};
}

void RBN_prevAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void RBN_prevAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
}

void RBN_prevAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool RBN_prevAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

int maxLoops = 5;
int currentLoop = 0;

void RBN_prevAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    midiProcessor.process(midiMessages, 
                          getCurrentPlayheadPositionInSamples(),
                          buffer.getNumSamples());

    

    // Read in current position
    juce::AudioPlayHead::CurrentPositionInfo positionInfo;
    if (getPlayHead() != nullptr && getPlayHead()->getCurrentPosition(positionInfo))
    {
        double measureStart = positionInfo.ppqPosition - fmod(positionInfo.ppqPosition, positionInfo.timeSigDenominator);

        // print("Measure start: " + juce::String(measureStart) + " PPQ: " + juce::String(positionInfo.ppqPosition) + " time sig: " + juce::String(positionInfo.timeSigNumerator) + "/" + juce::String(positionInfo.timeSigDenominator));

        // double ppqPosition = positionInfo.ppqPosition;

        // number of measures we're displaying

        // DBG("Current position: "
        //     + juce::String(positionInfo.timeInSamples) + " samples, "
        //     + juce::String(positionInfo.timeInSeconds) + " seconds, "
        //     + juce::String(positionInfo.ppqPosition) + " PPQ, "
        //     + juce::String(positionInfo.bpm) + " BPM, "
        //     + juce::String(positionInfo.timeSigNumerator) + "/"
        //     + juce::String(positionInfo.timeSigDenominator) + " time signature, "
        //     + juce::String(positionInfo.ppqPositionOfLastBarStart) + " PPQ of last bar start");
    }

    // if (!midiMessages.isEmpty())
    // {
    //     DBG("Received MIDI data");
    // }

    // // juce::MidiBuffer processedMidi;
    // juce::MidiMessage message;
    // int samplePosition;
    // // Iterate over the MIDI buffer
    // for (juce::MidiBuffer::Iterator i(midiMessages); i.getNextEvent(message, samplePosition);)
    // {
    //     DBG("MIDI message: " << message.getDescription() << " at sample " << samplePosition);

    //     // If the event is in the future (relative to the lookahead), add it to the lookahead buffer
    //     // if (samplePosition >= lookaheadSamples)
    //     // {
    //     //     lookaheadBuffer.addEvent(message, samplePosition - lookaheadSamples);
    //     // }
    //     // else // Otherwise, add it to the processed MIDI buffer
    //     // {
    //     //     processedMidi.addEvent(message, samplePosition);
    //     // }
    // }

    // // Add events from the lookahead buffer to the processed MIDI buffer
    // for (juce::MidiBuffer::Iterator i(lookaheadBuffer); i.getNextEvent(message, samplePosition);)
    // {
    //     processedMidi.addEvent(message, samplePosition);
    // }

    // // Clear the lookahead buffer and swap it with the MIDI messages for the next block
    // lookaheadBuffer.clear();
    // lookaheadBuffer.swapWith(midiMessages);

    // // Swap the processed MIDI buffer with the MIDI messages for this block
    // midiMessages.swapWith(processedMidi);
}

//==============================================================================
bool RBN_prevAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* RBN_prevAudioProcessor::createEditor()
{
    return new RBN_prevAudioProcessorEditor (*this);
}

//==============================================================================
void RBN_prevAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void RBN_prevAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RBN_prevAudioProcessor();
}
