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
                       ), 
       midiProcessor(state)
#endif
{
    initializeDefaultState();
}

ChartPreviewAudioProcessor::~ChartPreviewAudioProcessor()
{
}

void ChartPreviewAudioProcessor::initializeDefaultState()
{
    state = juce::ValueTree("state");
    
    state.setProperty("skillLevel", (int)SkillLevel::EXPERT, nullptr);
    state.setProperty("part", (int)Part::DRUMS, nullptr);
    state.setProperty("drumType", (int)DrumType::PRO, nullptr);
    state.setProperty("framerate", 3, nullptr); // 60 FPS
    state.setProperty("latency", 2, nullptr);   // 500 ms
    state.setProperty("autoHopo", (int)HopoMode::OFF, nullptr);
    state.setProperty("starPower", 1, nullptr);
    state.setProperty("kick2x", 1, nullptr);
    state.setProperty("dynamics", 1, nullptr);
    state.setProperty("dynamicZoom", 0, nullptr);
    state.setProperty("zoomPPQ", 2.5, nullptr);
    state.setProperty("zoomTime", 1.0, nullptr);
}

void ChartPreviewAudioProcessor::setLatencyInSeconds(float latencyInSeconds)
{
    this->latencyInSeconds = latencyInSeconds;
    double sampleRate = getSampleRate();
    if (sampleRate > 0.0)
    {
        this->latencyInSamples = (uint)(latencyInSeconds * sampleRate);
        setLatencySamples(this->latencyInSamples);
    }
}

// MANUAL OVERRIDES
void ChartPreviewAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    setLatencyInSeconds(latencyInSeconds);
}

void ChartPreviewAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

void ChartPreviewAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    // Can't process if there is no playhead
    if (getPlayHead() == nullptr)
        return;

    auto positionInfo = getPlayHead()->getPosition();
    if (!positionInfo.hasValue())
        return;

    // Get playhead position
    playheadPositionInSamples = static_cast<uint>(positionInfo->getTimeInSamples().orFallback(0));
    playheadPositionInPPQ = positionInfo->getPpqPosition().orFallback(0.0);
    isPlaying = positionInfo->getIsPlaying();

    if (isPlaying)
    {
        midiProcessor.process(midiMessages,
                              *positionInfo,
                              buffer.getNumSamples(),
                              latencyInSamples,
                              getSampleRate());
    }
}
//==============================================
// Stock JUCE
juce::AudioProcessorEditor *ChartPreviewAudioProcessor::createEditor()
{
    return new ChartPreviewAudioProcessorEditor(*this, state);
}

bool ChartPreviewAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

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

void ChartPreviewAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    std::unique_ptr<juce::XmlElement> xml(state.createXml());

    if (xml != nullptr)
    {
        copyXmlToBinary(*xml, destData);
    }
}

void ChartPreviewAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr)
    {
        state = juce::ValueTree::fromXml(*xml);
    }
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ChartPreviewAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
#if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChartPreviewAudioProcessor();
}

