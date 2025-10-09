/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ReaperVST3.h"

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
    debugText = "Plugin loaded at " + juce::Time::getCurrentTime().toString(true, true) + "\n";
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
// VST2 REAPER Integration
class ChartPreviewVST2Extensions : public juce::VST2ClientExtensions
{
public:
    ChartPreviewVST2Extensions(ChartPreviewAudioProcessor* proc) : processor(proc) {}

    juce::pointer_sized_int handleVstPluginCanDo(juce::int32 index, juce::pointer_sized_int value, void* ptr, float opt) override
    {
        // Check if REAPER is asking about specific capabilities
        if (ptr != nullptr)
        {
            juce::String capability = juce::String::fromUTF8((const char*)ptr);

            // Advertise that we support REAPER extensions
            if (capability == "reaper_vst_extensions")
                return 1; // Yes, we support this
            // Support Cockos VST extensions (main capability)
            else if (capability == "hasCockosExtensions")
                return 0xbeef0000; // Magic return value for Cockos extensions
            // REAPER-specific capabilities
            else if (capability == "hasCockosNoScrollUI")
                return 1; // We support this
            else if (capability == "hasCockosSampleAccurateAutomation")
                return 1; // We support this
            else if (capability == "hasCockosEmbeddedUI")
                return 0; // We don't support embedded UI
            else if (capability == "wantsChannelCountNotifications")
                return 1; // We want channel count notifications
        }

        return 0; // Return 0 for "don't know"
    }

    juce::pointer_sized_int handleVstManufacturerSpecific(juce::int32 index, juce::pointer_sized_int value, void* ptr, float) override
    {
        // Handle REAPER custom plugin name query (effGetEffectName with 0x50)
        if (index == 0x2d && value == 0x50 && ptr)
        {
            // Provide a custom name for this plugin instance
            *(const char**)ptr = "Chart Preview (VST2)";
            return 0xf00d; // Magic return value indicating we handled the name query
        }

        return 0;
    }

    // This is called once by the VST plug-in wrapper after its constructor
    void handleVstHostCallbackAvailable(std::function<VstHostCallbackType>&& callback) override
    {
        // Store the callback for later use
        hostCallback = std::move(callback);

        // Try to get REAPER API through direct handshake
        tryGetReaperApi();
    }

    void tryGetReaperApi()
    {
        if (!hostCallback)
            return;

        // REAPER's VST2 extension: Call audioMaster(NULL, 0xdeadbeef, 0xdeadf00d, 0, "FunctionName", 0.0)
        // to get each function directly. Test by trying to get GetPlayState.
        auto testResult = hostCallback(0xdeadbeef, 0xdeadf00d, 0, (void*)"GetPlayState", 0.0);

        if (testResult != 0)
        {
            processor->isReaperHost = true;

            // Store callback in static member so our wrapper function can access it
            staticCallback = &hostCallback;

            // Create a static function that can be used as a function pointer
            static auto reaperApiWrapper = [](const char* funcname) -> void* {
                if (!ChartPreviewVST2Extensions::staticCallback)
                    return nullptr;

                auto& callback = *ChartPreviewVST2Extensions::staticCallback;
                auto result = callback(0xdeadbeef, 0xdeadf00d, 0, (void*)funcname, 0.0);
                return (void*)result;
            };

            processor->reaperGetFunc = reaperApiWrapper;

            // Initialize the REAPER MIDI provider
            processor->reaperMidiProvider.initialize(processor->reaperGetFunc);
            processor->debugText += "✅ REAPER API connected - MIDI timeline access ready\n";
        }
    }

private:
    ChartPreviewAudioProcessor* processor;
    std::function<VstHostCallbackType> hostCallback;

    // Static storage for the callback so our lambda can access it
    static std::function<VstHostCallbackType>* staticCallback;
};

// Define static member
std::function<ChartPreviewVST2Extensions::VstHostCallbackType>* ChartPreviewVST2Extensions::staticCallback = nullptr;

juce::VST2ClientExtensions* ChartPreviewAudioProcessor::getVST2ClientExtensions()
{
    // Create VST2 extensions instance on demand
    if (!vst2Extensions)
        vst2Extensions = std::make_unique<ChartPreviewVST2Extensions>(this);

    return vst2Extensions.get();
}

bool ChartPreviewAudioProcessor::attemptReaperConnection()
{
    if (!isReaperHost || !reaperGetFunc)
        return false;

    // Test the connection by getting a simple REAPER function
    auto GetPlayState = (int(*)())reaperGetFunc("GetPlayState");
    if (GetPlayState)
    {
        DBG("Successfully connected to REAPER API via VST2!");
        return true;
    }

    return false;
}

void* ChartPreviewAudioProcessor::getReaperApi(const char* funcname)
{
    if (reaperGetFunc)
        return reaperGetFunc(funcname);
    return nullptr;
}


std::string ChartPreviewAudioProcessor::getHostInfo()
{
    // Try to get host information
    juce::String hostName = getPlayHead() ? "Unknown Host" : "No PlayHead";

    // JUCE doesn't provide a direct way to get host name in VST2, but we can infer it
    // from various clues or wait for the REAPER-specific handshake
    return hostName.toStdString();
}

//==============================================================================
// VST3 REAPER Integration
#if JucePlugin_Build_VST3
class ChartPreviewVST3Extensions : public juce::VST3ClientExtensions
{
public:
    ChartPreviewVST3Extensions(ChartPreviewAudioProcessor* proc) : processor(proc) {}

    // Called by JUCE when the host provides an IHostApplication
    void setIHostApplication(Steinberg::FUnknown* host) override
    {
        if (!host)
            return;

        // Use FUnknownPtr for automatic COM handling (from reference project)
        auto reaper = FUnknownPtr<IReaperHostApplication>(host);
        if (reaper)
        {
            processor->isReaperHost = true;

            // Create a wrapper function to call getReaperApi
            // Store reaper interface in static variable for the function pointer
            static FUnknownPtr<IReaperHostApplication> staticReaper = reaper;
            static auto reaperApiWrapper = [](const char* funcname) -> void* {
                if (!staticReaper)
                    return nullptr;
                return staticReaper->getReaperApi(funcname);
            };

            processor->reaperGetFunc = reaperApiWrapper;

            // Initialize the REAPER MIDI provider
            processor->reaperMidiProvider.initialize(processor->reaperGetFunc);
            processor->debugText += "✅ REAPER API connected via VST3 - MIDI timeline access ready\n";
        }
    }

private:
    ChartPreviewAudioProcessor* processor;
};
#endif

juce::VST3ClientExtensions* ChartPreviewAudioProcessor::getVST3ClientExtensions()
{
#if JucePlugin_Build_VST3
    // Create VST3 extensions instance on demand
    if (!vst3Extensions)
        vst3Extensions = std::make_unique<ChartPreviewVST3Extensions>(this);

    return vst3Extensions.get();
#else
    return nullptr;
#endif
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChartPreviewAudioProcessor();
}

