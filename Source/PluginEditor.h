/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

enum FFTOrder {
    order2048 = 11,
    order4096,
    order8192
};

// lấy dữ liệu block từ fifo để biến đổi dữ liệu âm thanh thành dữ liệu FFT
template<typename BlockType>
struct FFTDataGenerator
{
    /**
     produces the FFT data from an audio buffer.
     */
    void produceFFTDataForRendering(const juce::AudioBuffer<float>& audioData, const float negativeInfinity)
    {
        const auto fftSize = getFFTSize();

        fftData.assign(fftData.size(), 0);
        auto* readIndex = audioData.getReadPointer(0);
        std::copy(readIndex, readIndex + fftSize, fftData.begin());

        // first apply a windowing function to our data
        window->multiplyWithWindowingTable(fftData.data(), fftSize);       // [1]

        // then render our FFT data..
        forwardFFT->performFrequencyOnlyForwardTransform(fftData.data());  // [2]

        int numBins = (int)fftSize / 2;

        //normalize the fft values.
        for (int i = 0; i < numBins; ++i)
        {
            auto v = fftData[i];
            //            fftData[i] /= (float) numBins;
            if (!std::isinf(v) && !std::isnan(v))
            {
                v /= float(numBins);
            }
            else
            {
                v = 0.f;
            }
            fftData[i] = v;
        }

        //convert them to decibels
        for (int i = 0; i < numBins; ++i)
        {
            fftData[i] = juce::Decibels::gainToDecibels(fftData[i], negativeInfinity);
        }

        fftDataFifo.push(fftData);
    }

    void changeOrder(FFTOrder newOrder)
    {
        //when you change order, recreate the window, forwardFFT, fifo, fftData
        //also reset the fifoIndex
        //things that need recreating should be created on the heap via std::make_unique<>

        order = newOrder;
        auto fftSize = getFFTSize();

        forwardFFT = std::make_unique<juce::dsp::FFT>(order);
        window = std::make_unique<juce::dsp::WindowingFunction<float>>(fftSize, juce::dsp::WindowingFunction<float>::blackmanHarris);

        fftData.clear();
        fftData.resize(fftSize * 2, 0);

        fftDataFifo.prepare(fftData.size());
    }
    //==============================================================================
    int getFFTSize() const { return 1 << order; }
    int getNumAvailableFFTDataBlocks() const { return fftDataFifo.getNumAvailableForReading(); }
    //==============================================================================
    bool getFFTData(BlockType& fftData) { return fftDataFifo.pull(fftData); }
private:
    FFTOrder order;
    BlockType fftData;
    std::unique_ptr<juce::dsp::FFT> forwardFFT;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;

    Fifo<BlockType> fftDataFifo;
};

// Biến đổi FFT data thành path
template<typename PathType>
struct AnalyzerPathGenerator
{
    /*
     converts 'renderData[]' into a juce::Path
     */
    void generatePath(const std::vector<float>& renderData,
        juce::Rectangle<float> fftBounds,
        int fftSize,
        float binWidth,
        float negativeInfinity)
    {
        auto top = fftBounds.getY();
        auto bottom = fftBounds.getHeight();
        auto width = fftBounds.getWidth();

        int numBins = (int)fftSize / 2;

        PathType p;
        p.preallocateSpace(3 * (int)fftBounds.getWidth());

        auto map = [bottom, top, negativeInfinity](float v)
        {
            return juce::jmap(v,
                negativeInfinity, 0.f,
                float(bottom), top);
        };

        auto y = map(renderData[0]);

        //        jassert( !std::isnan(y) && !std::isinf(y) );
        if (std::isnan(y) || std::isinf(y))
            y = bottom;

        p.startNewSubPath(0, y);

        const int pathResolution = 2; //you can draw line-to's every 'pathResolution' pixels.

        for (int binNum = 1; binNum < numBins; binNum += pathResolution)
        {
            y = map(renderData[binNum]);

            //            jassert( !std::isnan(y) && !std::isinf(y) );

            if (!std::isnan(y) && !std::isinf(y))
            {
                auto binFreq = binNum * binWidth;
                auto normalizedBinX = juce::mapFromLog10(binFreq, 20.f, 20000.f);
                int binX = std::floor(normalizedBinX * width);
                p.lineTo(binX, y);
            }
        }

        pathFifo.push(p);
    }

    int getNumPathsAvailable() const
    {
        return pathFifo.getNumAvailableForReading();
    }

    bool getPath(PathType& path)
    {
        return pathFifo.pull(path);
    }
private:
    Fifo<PathType> pathFifo;
};


struct LookAndFeel : juce::LookAndFeel_V4 {
    // tạo 1 bound hay nền cho cái rotary sliders
    // Ở đây là tạo vòng tròn
    void drawRotarySlider(juce::Graphics&,
        int x, int y, int width, int height,
        float sliderPosProportional,
        float rotaryStartAngle,
        float rotaryEndAngle,
        juce::Slider&) override;

    void drawToggleButton(juce::Graphics& g,
                          juce::ToggleButton& toggleButton,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

};

// Vẽ các con sliders có nhãn ở 2 bên với hậu tố (hoặc nhãn)
struct RotarySliderWithLabels : juce::Slider 
{
    RotarySliderWithLabels(juce::RangedAudioParameter& rap, const juce::String& unitSuffix) :
    juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag,
                 juce::Slider::TextEntryBoxPosition::NoTextBox),
    param(&rap),
    suffix(unitSuffix)
    {
        setLookAndFeel(&lnf);
    }

    ~RotarySliderWithLabels() {
        setLookAndFeel(nullptr);
    }

    struct LabelPos {
        float pos;
        juce::String label;
    };

    juce::Array<LabelPos> labels;

    void paint(juce::Graphics& g) override;
    // Shrink elipse để thành vòng tròn
    juce::Rectangle<int> getSliderBounds() const;

    int getTextHeight() const { return 14; }

    juce::String getDisplayString() const;

private:
    // Thằng lookandfeel là thằng khởi tạo thằng này nên cần có lnf
    LookAndFeel lnf;

    // này nó derives mọi thằng audio param khác
    juce::RangedAudioParameter* param;

    // hậu tố (đơn vị, k, ...)
    juce::String suffix;
};

struct PathProducer {
    PathProducer(SingleChannelSampleFifo < AudioPluginBetaAudioProcessor::BlockType >& scsf) :
        leftChannelFifo(&scsf) {
        leftChannelFFTDataGenerator.changeOrder(FFTOrder::order2048);
        monoBuffer.setSize(1, leftChannelFFTDataGenerator.getFFTSize());
    }

    /*
    Khi còn buffer để lấy từ scsf
        nếu pull được thì
            gửi nó đến fft data gen
    Bao gồm cả gen path trong này
    */
    void process(juce::Rectangle<float> fftBounds, double sampleRate);
    juce::Path getPath() { return leftChannelFFTPath; }

private:
    SingleChannelSampleFifo < AudioPluginBetaAudioProcessor::BlockType >* leftChannelFifo;

    juce::AudioBuffer<float> monoBuffer;

    FFTDataGenerator<std::vector<float>> leftChannelFFTDataGenerator;

    AnalyzerPathGenerator<juce::Path> pathProducer;

    juce::Path leftChannelFFTPath;
};

// Lớp vẽ đường cong phản hồi
// kế thừa listener: 
// kế thùa timer: 
// lấy dữ liệu của scsf luôn (2 kênh)
struct ResponseCurveComponent : juce::Component,
    juce::AudioProcessorParameter::Listener,
    juce::Timer {
    ResponseCurveComponent(AudioPluginBetaAudioProcessor&);
    ~ResponseCurveComponent();

    // Nếu param thay đổi, set atomic flag thành true
    void parameterValueChanged(int parameterIndex, float newValue) override;

    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override {  }
    
    // xử lý khi biến tgian thay đổi
    void timerCallback() override;

    void paint(juce::Graphics& g) override;

    // gọi cái này để tạo lưới trước khi vẽ
    void resized() override;

    void toggleAnalysisEnablement(bool enabled) {
        shouldShowFFTAnalysis = enabled;
    }
private:
    AudioPluginBetaAudioProcessor& audioProcessor;

    // kiểm tra xem param có thay đổi?
    juce::Atomic<bool> parameterChanged{ false };

    MonoChain monoChain;

    // dùng này để gọn và để nó tự lưu mỗi khi khởi động gui
    void updateChain();

    // background của cái response curve grid
    juce::Image background;

    // Tạo vùng để render hình ảnh
    juce::Rectangle<int> getRenderArea();

    // Tạo vùng để lũ phân tích hiển thị
    // response curve cũng được vẽ trong này
    juce::Rectangle<int> getAnalysisArea();
    
    PathProducer leftPathProducer, rightPathProducer;

    bool shouldShowFFTAnalysis = true;
};

//==============================================================================
struct PowerButton : juce::ToggleButton { };
struct AnalyzerButton : juce::ToggleButton {
    
    void resized() override {
        auto bounds = getLocalBounds();
        auto insetRect = bounds.reduced(4);

        randomPath.clear();

        juce::Random r;

        randomPath.startNewSubPath(insetRect.getX(),
            insetRect.getY() + insetRect.getHeight() * r.nextFloat());

        for (auto x = insetRect.getX() + 1; x < insetRect.getRight(); x += 2) {
            randomPath.lineTo(x, insetRect.getY() + insetRect.getHeight() * r.nextFloat());
        }
        
    }

    juce::Path randomPath;

};

/**
*/
class AudioPluginBetaAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    AudioPluginBetaAudioProcessorEditor (AudioPluginBetaAudioProcessor&);
    ~AudioPluginBetaAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    AudioPluginBetaAudioProcessor& audioProcessor;

    RotarySliderWithLabels peakFreqSlider,
                       peakGainSlider,
                       peakQualitySlider,
                       lowCutFreqSlider,
                       highCutFreqSlider,
                       lowCutSlopeSlider,
                       highCutSlopeSlider;

    ResponseCurveComponent responseCurveComponent;

    using APVTS = juce::AudioProcessorValueTreeState;
    using Attachment = APVTS::SliderAttachment;

    Attachment peakFreqSliderAttachment,
               peakGainSliderAttachment,
               peakQualitySliderAttachment,
               lowCutFreqSliderAttachment,
               highCutFreqSliderAttachment,
               lowCutSlopeSliderAttachment,
               highCutSlopeSliderAttachment;

    PowerButton lowCutBypassButton, highCutBypassButton, peakBypassButton;
    AnalyzerButton analyzerEnableButton;
    
    using ButtonAttachment = APVTS::ButtonAttachment;
    ButtonAttachment lowCutBypassButtonAttachment, 
                     highCutBypassButtonAttachment, 
                     peakBypassButtonAttachment, 
                     analyzerEnableButtonAttachment;

    std::vector<juce::Component*> getComps();

    LookAndFeel lnf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginBetaAudioProcessorEditor)
};
