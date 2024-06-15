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
    juce::AudioPlayHead::CurrentPositionInfo positionInfo;
    if (getPlayHead() == nullptr || !getPlayHead()->getCurrentPosition(positionInfo))
    {
        return;
    }

    playheadPositionInSamples = positionInfo.timeInSamples;
    uint blockSizeInSamples = buffer.getNumSamples();

    // Prevents events being erased while scrubbing through timeline while playback is stopped
    if (positionInfo.isPlaying)
    {
        midiProcessor.process(midiMessages, playheadPositionInSamples, blockSizeInSamples);

        // if (true)
        // {
        //     print(std::to_string(blockSizeInSamples) + ": " + std::to_string(playheadPositionInSamples) + " --- " + std::to_string(playheadPositionInSamples + blockSizeInSamples));

        //     juce::MidiBuffer::Iterator it(midiMessages);

        //     int positionInSamples;
        //     juce::MidiMessage message;
        //     while (it.getNextEvent(message, positionInSamples))
        //     {
        //         print("*****   " + std::to_string(playheadPositionInSamples + positionInSamples));
        //     }

        // }
    }
    else {
        midiProcessor.lastProcessedSample = playheadPositionInSamples;
    }
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
