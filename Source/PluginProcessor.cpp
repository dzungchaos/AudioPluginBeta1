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

    // ph???i prepare filter 
    // passing 1 process spec obj v??o c??i chain
    juce::dsp::ProcessSpec spec;

    spec.maximumBlockSize = samplesPerBlock;    // s??? l?????ng sample ch??i c??ng 1 l??c/tdiem
    spec.numChannels = 1;                       // mono c?? 1 channel
    spec.sampleRate = sampleRate;           

    // pass to chain
    leftChain.prepare(spec);
    rightChain.prepare(spec);

    updateFilters();

    leftChannelFifo.prepare(samplesPerBlock);
    rightChannelFifo.prepare(samplesPerBlock);

    osc.initialise([](float x) { return std::sin(x); });

    spec.numChannels = getTotalNumOutputChannels();
    osc.prepare(spec);
    osc.setFrequency(100);
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
    
    updateFilters();

    // t???o 1 block ????? extract channel (left, right) t??? c??i buffer
    juce::dsp::AudioBlock<float> block(buffer);

    //buffer.clear();

    //for (int i = 0; i < buffer.getNumSamples(); ++i) {
    //    buffer.setSample(0, i, osc.processSample(0));
    //}

    //juce::dsp::ProcessContextReplacing<float> stereoContext(block);
    //osc.process(stereoContext);

    // t???o c??c block ?????i di???n cho c??c k??nh
    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);

    // t???o 1 context ????? ch???a 2 con k??nh kia
    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);

    // th??m m???y c??i context v???a t???o v??o mono filter chain
    leftChain.process(leftContext);
    rightChain.process(rightContext);

    // trong qu?? tr??nh x??? l?? kh???i th?? c???n update li??n t???c
    leftChannelFifo.update(buffer);
    rightChannelFifo.update(buffer);
}

//==============================================================================
bool AudioPluginBetaAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AudioPluginBetaAudioProcessor::createEditor()
{
    return new AudioPluginBetaAudioProcessorEditor (*this);
    // return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void AudioPluginBetaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void AudioPluginBetaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.

    // C?? th??? tr??ch xu???t d??? li???u trong treestate d??ng help function
    // Vi???c c???n l??m l?? ki???m tra xem treestate c?? valid tr?????c khi copy kh??ng
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if (tree.isValid()) {
        apvts.replaceState(tree);
        updateFilters();
    }


}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts) {
    ChainSettings settings;

    // t???o 1 ?????i t?????ng settings v?? thi???t l???p th??ng s??? b???ng
    // GetRaw -> load() ????? ?????m b???o ????n v???
    settings.lowCutFreq = apvts.getRawParameterValue("LowCut Freq")->load();
    settings.highCutFreq = apvts.getRawParameterValue("HighCut Freq")->load();
    settings.peakFreq = apvts.getRawParameterValue("Peak Freq")->load();
    settings.peakGainInDecibels = apvts.getRawParameterValue("Peak Gain")->load();
    settings.peakQuality = apvts.getRawParameterValue("Peak Quality")->load();
    settings.lowCutSlope = static_cast<Slope>(apvts.getRawParameterValue("LowCut Slope")->load());
    settings.highCutSlope = static_cast<Slope>(apvts.getRawParameterValue("HighCut Slope")->load());

    settings.lowCutBypassed = apvts.getRawParameterValue("LowCut Bypassed")->load() > 0.5f;
    settings.peakBypassed = apvts.getRawParameterValue("Peak Bypassed")->load() > 0.5f;
    settings.highCutBypassed = apvts.getRawParameterValue("HighCut Bypassed")->load() > 0.5f;
    
    return settings;
}

Coefficents makePeakFilter(const ChainSettings& chainSettings, double sampleRate) {
    return juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        sampleRate,
        chainSettings.peakFreq,
        chainSettings.peakQuality,
        juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels));
}

void AudioPluginBetaAudioProcessor::updatePeakFilter(const ChainSettings& chainSettings) {
    auto peakCoefficients = makePeakFilter(chainSettings, getSampleRate());

    leftChain.setBypassed<ChainPositions::Peak>(chainSettings.peakBypassed);
    rightChain.setBypassed<ChainPositions::Peak>(chainSettings.peakBypassed);

    updateCoefficents(leftChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
    updateCoefficents(rightChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
}

void updateCoefficents(Coefficents& old, const Coefficents& replacements) {
    *old = *replacements;
}

void AudioPluginBetaAudioProcessor::updateLowCutFilters(const ChainSettings& chainSettings) {
    auto cutCoefficents = makeLowCutFilter(chainSettings, getSampleRate());
    // l???c ??? loa tr??i + ph???i
    auto& leftLowCut = leftChain.get<ChainPositions::lowCut>();
    auto& rightLowCut = rightChain.get<ChainPositions::lowCut>();

    leftChain.setBypassed<ChainPositions::lowCut>(chainSettings.lowCutBypassed);
    rightChain.setBypassed<ChainPositions::lowCut>(chainSettings.lowCutBypassed);

    updateCutFilter(leftLowCut, cutCoefficents, chainSettings.lowCutSlope);
    updateCutFilter(rightLowCut, cutCoefficents, chainSettings.lowCutSlope);  
}

void AudioPluginBetaAudioProcessor::updateHighCutFilters(const ChainSettings& chainSettings) {
    auto highCutCoefficents = makeHighCutFilter(chainSettings, getSampleRate());
    // l???c th??ng th???p ??? loa tr??i + ph???i
    auto& leftHighCut = leftChain.get<ChainPositions::HighCut>();
    auto& rightHighCut = rightChain.get<ChainPositions::HighCut>();

    leftChain.setBypassed<ChainPositions::HighCut>(chainSettings.highCutBypassed);
    rightChain.setBypassed<ChainPositions::HighCut>(chainSettings.highCutBypassed);

    updateCutFilter(leftHighCut, highCutCoefficents, chainSettings.highCutSlope);
    updateCutFilter(rightHighCut, highCutCoefficents, chainSettings.highCutSlope);    
}

void AudioPluginBetaAudioProcessor::updateFilters() {
    auto chainSettings = getChainSettings(apvts);

    updateLowCutFilters(chainSettings);
    updatePeakFilter(chainSettings);
    updateHighCutFilters(chainSettings);
}

juce::AudioProcessorValueTreeState::ParameterLayout
    AudioPluginBetaAudioProcessor::createParameterLayout () 
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // th??m lowcut param: Hz
    layout.add(std::make_unique<juce::AudioParameterFloat>("LowCut Freq", "LowCut Freq", juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f), 20.f));
    // th??m highcut param: Hz
    layout.add(std::make_unique<juce::AudioParameterFloat>("HighCut Freq", "HighCut Freq", juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f), 20000.f));
    // th??m peak param: Hz
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Freq", "Peak Freq", juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f), 750.f));
    // th??m peak gain param: dB
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Gain", "Peak Gain", juce::NormalisableRange<float>(-24.f, 24.f, 0.5f, 1.f), 0.0f));
    // th??m peak quality param: narrow or wide
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Quality", "Peak Quality", juce::NormalisableRange<float>(0.1f, 10.f, 0.05f, 1.f), 1.f));
    
    // l???c th??ng 12dB/oct, 24dB/oct, 36dB/oct, 48dB/oct
    juce::StringArray stringArray;
    for (int i = 0; i < 4; i++) {
        juce::String str;
        str << (12 + i * 12);
        str << ("dB/Oct");
        stringArray.add(str);
    }

    // t???o choice cho ch??? ????? l???c th??ng
    layout.add(std::make_unique<juce::AudioParameterChoice>("LowCut Slope", "LowCut Slope", stringArray, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>("HighCut Slope", "HighCut Slope", stringArray, 0));

    layout.add(std::make_unique<juce::AudioParameterBool>("LowCut Bypassed", "LowCut Bypassed", false));
    layout.add(std::make_unique<juce::AudioParameterBool>("Peak Bypassed", "Peak Bypassed", false));
    layout.add(std::make_unique<juce::AudioParameterBool>("HighCut Bypassed", "HighCut Bypassed", false));
    layout.add(std::make_unique<juce::AudioParameterBool>("Analyzer Enabled", "Analyzer Enabled", true));

    

    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginBetaAudioProcessor();
}
