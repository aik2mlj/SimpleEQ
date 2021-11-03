/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

struct MyLookAndFeel : juce::LookAndFeel_V4 {
    // implement DrawRotarySlider
    void drawRotarySlider (juce::Graphics &g,
                           int x, int y,
                           int width, int height,
                           float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider &slider) override;
};

struct RotarySliderWithLabels : juce::Slider {
    RotarySliderWithLabels(juce::RangedAudioParameter &rap, const juce::String &unitSuffix)
    : juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag,
                   juce::Slider::TextEntryBoxPosition::NoTextBox),
                   param(&rap), suffix(unitSuffix) {
        setLookAndFeel(&lnf);
    }

    ~RotarySliderWithLabels() override { setLookAndFeel(nullptr); }

    // show labels
    struct LabelPos {
        float pos;
        juce::String label;
    };
    juce::Array<LabelPos> labels;

    void paint(juce::Graphics &g) override;
    juce::Rectangle<int> getSliderBounds() const;
    static int getTextHeight() { return 14; }
    juce::String getDisplayString() const;
private:
    MyLookAndFeel lnf;

    juce::RangedAudioParameter *param;
    juce::String suffix;
};

struct ResponseCurveComponent : juce::Component,
        juce::AudioProcessorParameter::Listener,
        juce::Timer {
    explicit ResponseCurveComponent(SimpleEQAudioProcessor&);
    ~ResponseCurveComponent() override;

    void parameterValueChanged (int parameterIndex, float newValue) override;
    void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) override {}
    void timerCallback() override;
    void updateChain();

    void paint(juce::Graphics &g) override;
private:
    SimpleEQAudioProcessor &audioProcessor;

    // need an atomic bool to ensure threads security
    juce::Atomic<bool> parametersChanged {false};

    MonoChain monoChain;
};

//==============================================================================
/**
*/
class SimpleEQAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    explicit SimpleEQAudioProcessorEditor(SimpleEQAudioProcessor &);
    ~SimpleEQAudioProcessorEditor() override;

    //==============================================================================
    void paint(juce::Graphics &) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    SimpleEQAudioProcessor &audioProcessor;

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

    std::vector<juce::Component*> getComps();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleEQAudioProcessorEditor)
};
