/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

enum Slope {
    Slope_12,
    Slope_24,
    Slope_36,
    Slope_48
};

struct ChainSettings {
    float peakFreq{ 0 }, peakGainInDecibels{ 0 }, peakQuality{ 1.f };
    float lowCutFreq{ 0 }, highCutFreq{ 0 };
    Slope lowCutSlope{ Slope::Slope_12 }, highCutSlope{ Slope::Slope_12 };
};

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts);

//==============================================================================
/**
*/
class AudioPluginBetaAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    AudioPluginBetaAudioProcessor();
    ~AudioPluginBetaAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // khởi tạo layout chứa các param
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    // Biến này giúp định vị các nút trên gui và thực hiện các chức năng của nút
    // Cần cung cấp một danh sách các Param (trên)
    juce::AudioProcessorValueTreeState apvts {*this, nullptr, "Parameters", createParameterLayout()};

private:
    using Filter = juce::dsp::IIR::Filter<float>;

    using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;

    using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter>;

    MonoChain leftChain, rightChain;
    
    enum ChainPositions {
        lowCut,
        Peak,
        HighCut
    };

    void updatePeakFilter(const ChainSettings& chainSettings);
    using Coefficents = Filter::CoefficientsPtr;
    static void updateCoefficents(Coefficents& old, const Coefficents& replacements);

    template<int Index, typename ChainType, typename CoefficentType>
    void update(ChainType& chain, const CoefficentType& coefficents) {
        updateCoefficents(chain.template get<Index>().coefficients, coefficents[Index]);
        chain.template setBypassed<Index>(false);
    }

    template<typename ChainType, typename CoefficentType>
    void updateCutFilter(ChainType& chain,
        const CoefficentType& coefficents,
        const Slope& slope) {

        // K can thiet dung 'template'
        chain.template setBypassed<0>(true);
        chain.template setBypassed<1>(true);
        chain.template setBypassed<2>(true);
        chain.template setBypassed<3>(true);

        // Quá trình tạo option cho lọc thông cao
        switch (slope) {
        case Slope_48:
            update<3>(chain, coefficents);
        case Slope_36:
            update<2>(chain, coefficents);
        case Slope_24:
            update<1>(chain, coefficents);
        case Slope_12:
            update<0>(chain, coefficents);
        }
    }
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginBetaAudioProcessor)
};
