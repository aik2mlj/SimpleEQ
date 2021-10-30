/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

enum Slope {
    Slope12,
    Slope24,
    Slope36,
    Slope48
};

struct ChainSettings {
    float peakFreq {0}, peakGainInDecibels {0}, peakQuality {1.f};
    float lowCutFreq {0}, highCutFreq {0};
    Slope lowCutSlope {Slope::Slope12}, highCutSlope {Slope::Slope12};
};

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts);

//==============================================================================
/**
*/
class SimpleEQAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    SimpleEQAudioProcessor();
    ~SimpleEQAudioProcessor() override;

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

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    // apvts is a member.
    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "Parameters", createParameterLayout() };

private:
    using Filter = juce::dsp::IIR::Filter<float>;

    using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>; // 4 filters for different slopes

    using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter>;

    MonoChain leftChain, rightChain;

    enum ChainPositions {
        LowCut,
        Peak,
        HighCut
    };

    using Coefficients = Filter::CoefficientsPtr;
    static void updateCoefficients(Coefficients &old, const Coefficients &replacements);
    void updatePeakFilter(const ChainSettings &chainSettings);

    template<typename ChainType, typename CoefficientType>
    void updateCutFilter(ChainType &cut,
                         const CoefficientType &cutCoefficients,
                         const Slope &slope) {
        cut.template setBypassed<0>(true);
        cut.template setBypassed<1>(true);
        cut.template setBypassed<2>(true);
        cut.template setBypassed<3>(true);
        switch (slope) {
            case Slope48:
                *cut.template get<3>().coefficients = *cutCoefficients[3];
                cut.template setBypassed<3>(false);
            case Slope36:
                *cut.template get<2>().coefficients = *cutCoefficients[2];
                cut.template setBypassed<2>(false);
            case Slope24:
                *cut.template get<1>().coefficients = *cutCoefficients[1];
                cut.template setBypassed<1>(false);
            case Slope12:
                *cut.template get<0>().coefficients = *cutCoefficients[0];
                cut.template setBypassed<0>(false);
                break;
        }
    }

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleEQAudioProcessor)
};
