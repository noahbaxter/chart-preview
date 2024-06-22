/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ChartPreviewAudioProcessor::ChartPreviewAudioProcessor()
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

ChartPreviewAudioProcessor::~ChartPreviewAudioProcessor()
{
}

//==============================================================================
const juce::String ChartPreviewAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ChartPreviewAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool ChartPreviewAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool ChartPreviewAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double ChartPreviewAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ChartPreviewAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int ChartPreviewAudioProcessor::getCurrentProgram()
{
    return 0;
}

void ChartPreviewAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String ChartPreviewAudioProcessor::getProgramName (int index)
{
    return {};
}

void ChartPreviewAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void ChartPreviewAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    setLatencyInSeconds(latencyInSeconds);
}

void ChartPreviewAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ChartPreviewAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void ChartPreviewAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::AudioPlayHead::CurrentPositionInfo positionInfo;
    // Can't process if there is no playhead
    if (getPlayHead() == nullptr || !getPlayHead()->getCurrentPosition(positionInfo))
    {
        return;
    }

    playheadPositionInSamples = positionInfo.timeInSamples;
    uint blockSizeInSamples = buffer.getNumSamples();

    // Prevents events being erased while scrubbing through timeline while playback is stopped
    isPlaying = positionInfo.isPlaying;
    if (isPlaying)
    {
        midiProcessor.process(midiMessages, playheadPositionInSamples, blockSizeInSamples, latencyInSamples);
    }
    else {
        midiProcessor.lastProcessedSample = playheadPositionInSamples;
    }
}

//==============================================================================
bool ChartPreviewAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* ChartPreviewAudioProcessor::createEditor()
{
    return new ChartPreviewAudioProcessorEditor (*this);
}

//==============================================================================
void ChartPreviewAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void ChartPreviewAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChartPreviewAudioProcessor();
}
