/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginBetaAudioProcessor::AudioPluginBetaAudioProcessor()
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

AudioPluginBetaAudioProcessor::~AudioPluginBetaAudioProcessor()
{
}

//==============================================================================
const juce::String AudioPluginBetaAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AudioPluginBetaAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginBetaAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginBetaAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AudioPluginBetaAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AudioPluginBetaAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int AudioPluginBetaAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AudioPluginBetaAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String AudioPluginBetaAudioProcessor::getProgramName (int index)
{
    return {};
}

void AudioPluginBetaAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void AudioPluginBetaAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..

    // phải prepare filter 
    // passing 1 process spec obj vào cái chain
    juce::dsp::ProcessSpec spec;

    spec.maximumBlockSize = samplesPerBlock;    // số lượng sample chơi cùng 1 lúc/tdiem
    spec.numChannels = 1;                       // mono có 1 channel
    spec.sampleRate = sampleRate;           

    // pass to chain
    leftChain.prepare(spec);
    rightChain.prepare(spec);

    auto chainSettings = getChainSettings(apvts);

    auto peakCoefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        sampleRate, 
        chainSettings.peakFreq, 
        chainSettings.peakQuality, 
        juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels));

    *leftChain.get<ChainPositions::Peak>().coefficients = *peakCoefficients;
    *rightChain.get<ChainPositions::Peak>().coefficients = *peakCoefficients;

    auto cutCoefficents = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowCutFreq, 
                                                                                                      sampleRate, 
                                                                                                      2 * (chainSettings.lowCutSlope + 1));
    // lọc ở loa trái
    auto& leftLowCut = leftChain.get<ChainPositions::lowCut>();

    leftLowCut.setBypassed<0>(true);
    leftLowCut.setBypassed<1>(true);
    leftLowCut.setBypassed<2>(true);
    leftLowCut.setBypassed<3>(true);

    // Quá trình tạo option cho lọc thông cao
    switch (chainSettings.lowCutSlope) {
    case Slope_12:
        *leftLowCut.get<0>().coefficients = *cutCoefficents[0];
        leftLowCut.setBypassed<0>(false);
        break;
    case Slope_24:
        *leftLowCut.get<0>().coefficients = *cutCoefficents[0];
        leftLowCut.setBypassed<0>(false);
        *leftLowCut.get<1>().coefficients = *cutCoefficents[1];
        leftLowCut.setBypassed<1>(false);
        break;
    case Slope_36:
        *leftLowCut.get<0>().coefficients = *cutCoefficents[0];
        leftLowCut.setBypassed<0>(false);
        *leftLowCut.get<1>().coefficients = *cutCoefficents[1];
        leftLowCut.setBypassed<1>(false);
        *leftLowCut.get<2>().coefficients = *cutCoefficents[2];
        leftLowCut.setBypassed<2>(false);
        break;
    case Slope_48:
        *leftLowCut.get<0>().coefficients = *cutCoefficents[0];
        leftLowCut.setBypassed<0>(false);
        *leftLowCut.get<1>().coefficients = *cutCoefficents[1];
        leftLowCut.setBypassed<1>(false);
        *leftLowCut.get<2>().coefficients = *cutCoefficents[2];
        leftLowCut.setBypassed<2>(false);
        *leftLowCut.get<3>().coefficients = *cutCoefficents[3];
        leftLowCut.setBypassed<3>(false);
        break;
    default:
        break;
    }


    // thông cao ở tai phải
    auto& rightLowCut = rightChain.get<ChainPositions::lowCut>();

    rightLowCut.setBypassed<0>(true);
    rightLowCut.setBypassed<1>(true);
    rightLowCut.setBypassed<2>(true);
    rightLowCut.setBypassed<3>(true);

    // Quá trình tạo option cho lọc thông cao
    switch (chainSettings.lowCutSlope) {
    case Slope_12:
        *rightLowCut.get<0>().coefficients = *cutCoefficents[0];
        rightLowCut.setBypassed<0>(false);
        break;
    case Slope_24:
        *rightLowCut.get<0>().coefficients = *cutCoefficents[0];
        rightLowCut.setBypassed<0>(false);
        *rightLowCut.get<1>().coefficients = *cutCoefficents[1];
        rightLowCut.setBypassed<1>(false);
        break;
    case Slope_36:
        *rightLowCut.get<0>().coefficients = *cutCoefficents[0];
        rightLowCut.setBypassed<0>(false);
        *rightLowCut.get<1>().coefficients = *cutCoefficents[1];
        rightLowCut.setBypassed<1>(false);
        *rightLowCut.get<2>().coefficients = *cutCoefficents[2];
        rightLowCut.setBypassed<2>(false);
        break;
    case Slope_48:
        *rightLowCut.get<0>().coefficients = *cutCoefficents[0];
        rightLowCut.setBypassed<0>(false);
        *rightLowCut.get<1>().coefficients = *cutCoefficents[1];
        rightLowCut.setBypassed<1>(false);
        *rightLowCut.get<2>().coefficients = *cutCoefficents[2];
        rightLowCut.setBypassed<2>(false);
        *rightLowCut.get<3>().coefficients = *cutCoefficents[3];
        rightLowCut.setBypassed<3>(false);
        break;
    default:
        break;
    }
}

void AudioPluginBetaAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AudioPluginBetaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void AudioPluginBetaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    auto chainSettings = getChainSettings(apvts);

    auto peakCoefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        getSampleRate(),
        chainSettings.peakFreq,
        chainSettings.peakQuality,
        juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels));

    *leftChain.get<ChainPositions::Peak>().coefficients = *peakCoefficients;
    *rightChain.get<ChainPositions::Peak>().coefficients = *peakCoefficients;

    auto cutCoefficents = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowCutFreq,
        getSampleRate(),
        2 * (chainSettings.lowCutSlope + 1));
    // lọc ở loa trái
    auto& leftLowCut = leftChain.get<ChainPositions::lowCut>();

    leftLowCut.setBypassed<0>(true);
    leftLowCut.setBypassed<1>(true);
    leftLowCut.setBypassed<2>(true);
    leftLowCut.setBypassed<3>(true);

    // Quá trình tạo option cho lọc thông cao
    switch (chainSettings.lowCutSlope) {
    case Slope_12:
        *leftLowCut.get<0>().coefficients = *cutCoefficents[0];
        leftLowCut.setBypassed<0>(false);
        break;
    case Slope_24:
        *leftLowCut.get<0>().coefficients = *cutCoefficents[0];
        leftLowCut.setBypassed<0>(false);
        *leftLowCut.get<1>().coefficients = *cutCoefficents[1];
        leftLowCut.setBypassed<1>(false);
        break;
    case Slope_36:
        *leftLowCut.get<0>().coefficients = *cutCoefficents[0];
        leftLowCut.setBypassed<0>(false);
        *leftLowCut.get<1>().coefficients = *cutCoefficents[1];
        leftLowCut.setBypassed<1>(false);
        *leftLowCut.get<2>().coefficients = *cutCoefficents[2];
        leftLowCut.setBypassed<2>(false);
        break;
    case Slope_48:
        *leftLowCut.get<0>().coefficients = *cutCoefficents[0];
        leftLowCut.setBypassed<0>(false);
        *leftLowCut.get<1>().coefficients = *cutCoefficents[1];
        leftLowCut.setBypassed<1>(false);
        *leftLowCut.get<2>().coefficients = *cutCoefficents[2];
        leftLowCut.setBypassed<2>(false);
        *leftLowCut.get<3>().coefficients = *cutCoefficents[3];
        leftLowCut.setBypassed<3>(false);
        break;
    default:
        break;
    }


    // thông cao ở tai phải
    auto& rightLowCut = rightChain.get<ChainPositions::lowCut>();

    rightLowCut.setBypassed<0>(true);
    rightLowCut.setBypassed<1>(true);
    rightLowCut.setBypassed<2>(true);
    rightLowCut.setBypassed<3>(true);

    // Quá trình tạo option cho lọc thông cao
    switch (chainSettings.lowCutSlope) {
    case Slope_12:
        *rightLowCut.get<0>().coefficients = *cutCoefficents[0];
        rightLowCut.setBypassed<0>(false);
        break;
    case Slope_24:
        *rightLowCut.get<0>().coefficients = *cutCoefficents[0];
        rightLowCut.setBypassed<0>(false);
        *rightLowCut.get<1>().coefficients = *cutCoefficents[1];
        rightLowCut.setBypassed<1>(false);
        break;
    case Slope_36:
        *rightLowCut.get<0>().coefficients = *cutCoefficents[0];
        rightLowCut.setBypassed<0>(false);
        *rightLowCut.get<1>().coefficients = *cutCoefficents[1];
        rightLowCut.setBypassed<1>(false);
        *rightLowCut.get<2>().coefficients = *cutCoefficents[2];
        rightLowCut.setBypassed<2>(false);
        break;
    case Slope_48:
        *rightLowCut.get<0>().coefficients = *cutCoefficents[0];
        rightLowCut.setBypassed<0>(false);
        *rightLowCut.get<1>().coefficients = *cutCoefficents[1];
        rightLowCut.setBypassed<1>(false);
        *rightLowCut.get<2>().coefficients = *cutCoefficents[2];
        rightLowCut.setBypassed<2>(false);
        *rightLowCut.get<3>().coefficients = *cutCoefficents[3];
        rightLowCut.setBypassed<3>(false);
        break;
    default:
        break;
    }

    // tạo 1 block để extract channel (left, right) từ cái buffer
    juce::dsp::AudioBlock<float> block(buffer);

    // tạo các block đại diện cho các kênh
    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);

    // tạo 1 context để chứa 2 con kênh kia
    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);

    // thêm mấy cái context vừa tạo vào mono filter chain
    leftChain.process(leftContext);
    rightChain.process(rightContext);

}

//==============================================================================
bool AudioPluginBetaAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AudioPluginBetaAudioProcessor::createEditor()
{
    // return new AudioPluginBetaAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void AudioPluginBetaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void AudioPluginBetaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts) {
    ChainSettings settings;

    // tạo 1 đối tượng settings và thiết lập thông số bằng
    // GetRaw -> load() để đảm bảo đơn vị
    settings.lowCutFreq = apvts.getRawParameterValue("LowCut Freq")->load();
    settings.highCutFreq = apvts.getRawParameterValue("HighCut Freq")->load();
    settings.peakFreq = apvts.getRawParameterValue("Peak Freq")->load();
    settings.peakGainInDecibels = apvts.getRawParameterValue("Peak Gain")->load();
    settings.peakQuality = apvts.getRawParameterValue("Peak Quality")->load();
    settings.lowCutSlope = static_cast<Slope>(apvts.getRawParameterValue("LowCut Slope")->load());
    settings.highCutSlope = static_cast<Slope>(apvts.getRawParameterValue("HighCut Slope")->load());

    return settings;
}

juce::AudioProcessorValueTreeState::ParameterLayout
    AudioPluginBetaAudioProcessor::createParameterLayout () 
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // thêm lowcut param: Hz
    layout.add(std::make_unique<juce::AudioParameterFloat>("LowCut Freq", "LowCut Freq", juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f), 20.f));
    // thêm highcut param: Hz
    layout.add(std::make_unique<juce::AudioParameterFloat>("HighCut Freq", "HighCut Freq", juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f), 20000.f));
    // thêm peak param: Hz
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Freq", "Peak Freq", juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f), 750.f));
    // thêm peak gain param: dB
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Gain", "Peak Gain", juce::NormalisableRange<float>(-24.f, 24.f, 0.5f, 1.f), 0.0f));
    // thêm peak quality param: narrow or wide
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Quality", "Peak Quality", juce::NormalisableRange<float>(0.1f, 10.f, 0.05f, 1.f), 1.f));
    
    // lọc thông 12dB/oct, 24dB/oct, 36dB/oct, 48dB/oct
    juce::StringArray stringArray;
    for (int i = 0; i < 4; i++) {
        juce::String str;
        str << (12 + i * 12);
        str << ("dB/Oct");
        stringArray.add(str);
    }

    // tạo choice cho chế độ lọc thông
    layout.add(std::make_unique<juce::AudioParameterChoice>("LowCut Slope", "LowCut Slope", stringArray, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>("HighCut Slope", "HighCut Slope", stringArray, 0));

    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginBetaAudioProcessor();
}
