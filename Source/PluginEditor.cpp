/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginEditor.h"

//==============================================================================
void MyLookAndFeel::drawRotarySlider(juce::Graphics &g, int x, int y, int width, int height,
                                     float sliderPosProportional, float rotaryStartAngle,
                                     float rotaryEndAngle, juce::Slider &slider) {
    using namespace juce;

    auto bounds = Rectangle<float>(x, y, width, height);
    g.setColour(Colour(97u, 18u, 167u)); // choose a colour you like
    g.fillEllipse(bounds);

    g.setColour(Colour(255u, 154u, 1u)); // choose a colour you like
    g.drawEllipse(bounds, 1.f);

    if (auto *rswl = dynamic_cast<RotarySliderWithLabels *>(&slider)) {
        auto center = bounds.getCentre();

        Path p;

        // draw the pointing bar
        Rectangle<float> r;
        r.setLeft(center.getX() - 2);
        r.setRight(center.getX() + 2);
        r.setTop(bounds.getY());
        r.setBottom(center.getY() - rswl->getTextHeight() * 1.5);
        p.addRoundedRectangle(r, 2.f);

        jassert(rotaryStartAngle < rotaryEndAngle);
        auto sliderAngRad = jmap(sliderPosProportional, 0.f, 1.f, rotaryStartAngle, rotaryEndAngle);
        p.applyTransform(AffineTransform().rotated(sliderAngRad, center.getX(), center.getY()));

        g.fillPath(p);

        // show text at the center
        g.setFont(rswl->getTextHeight());
        auto text = rswl->getDisplayString();
        auto strWidth = g.getCurrentFont().getStringWidth(text);

        r.setSize(strWidth + 4,
                  rswl->getTextHeight() + 2); // a little bit bigger than the bounding box of text
        r.setCentre(center);

        g.setColour(Colours::black);
        g.fillRect(r);

        g.setColour(Colours::white);
        g.drawFittedText(text, r.toNearestInt(), juce::Justification::centred, 1);
    }
}

//==============================================================================
void RotarySliderWithLabels::paint(juce::Graphics &g) {
    using namespace juce;
    // 7'o clock to 5'o clock
    auto startAng = degreesToRadians(180.f + 45.f);
    auto endAng = degreesToRadians(180.f - 45.f) + MathConstants<float>::twoPi;

    auto range = getRange();

    auto sliderBounds = getSliderBounds();

    // boarders for debug
    //    g.setColour(Colours::red);
    //    g.drawRect(getLocalBounds());
    //    g.setColour(Colours::yellow);
    //    g.drawRect(sliderBounds);

    getLookAndFeel().drawRotarySlider(g, sliderBounds.getX(), sliderBounds.getY(),
                                      sliderBounds.getWidth(), sliderBounds.getHeight(),
                                      jmap(getValue(), range.getStart(), range.getEnd(), 0.0, 1.0),
                                      startAng, endAng, *this);

    // display labels
    auto center = sliderBounds.toFloat().getCentre();
    auto radius = sliderBounds.getWidth() * 0.5f;

    g.setColour(Colour(0u, 172u, 1u));
    g.setFont(getTextHeight());

    auto numChoices = labels.size();
    for (int i = 0; i < numChoices; ++i) {
        auto pos = labels[i].pos;
        jassert(0.f <= pos && pos <= 1.f);

        auto ang = jmap(pos, 0.f, 1.f, startAng, endAng);

        auto labelCenter =
            center.getPointOnCircumference(radius + getTextHeight() * 0.5f + 1.f, ang);

        Rectangle<float> r;
        auto str = labels[i].label;
        r.setSize(g.getCurrentFont().getStringWidth(str), getTextHeight());
        r.setCentre(labelCenter);
        r.setY(r.getY() + getTextHeight()); // shift down a bit

        g.drawFittedText(str, r.toNearestInt(), Justification::centred, 1);
    }
}

juce::Rectangle<int> RotarySliderWithLabels::getSliderBounds() const {
    // resize into a smaller square
    auto bounds = getLocalBounds();
    auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    size -= getTextHeight() * 2;
    juce::Rectangle<int> r;
    r.setSize(size, size);
    r.setCentre(bounds.getCentreX(), 0);
    r.setY(2); // a little gap to the top
    return r;
}

juce::String RotarySliderWithLabels::getDisplayString() const {
    if (auto *choiceParam = dynamic_cast<juce::AudioParameterChoice *>(param))
        return choiceParam->getCurrentChoiceName();

    juce::String str;
    bool addK = false; // display in khz?

    if (auto *floatParam = dynamic_cast<juce::AudioParameterFloat *>(param)) {
        float val = getValue();
        if (val > 999.f) { // display in khz
            val /= 1000.f;
            addK = true;
        }
        str = juce::String(val, (addK ? 2 : 0)); // format into 2 decimals if displaying in khz
    } else {
        jassertfalse;
    }

    // add suffix
    if (suffix.isNotEmpty()) {
        str << " ";
        if (addK) str << "k";
        str << suffix;
    }

    return str;
}

//==============================================================================
ResponseCurveComponent::ResponseCurveComponent(SimpleEQAudioProcessor &p) : audioProcessor(p) {
    // add listener
    const auto &params = audioProcessor.getParameters();
    for (auto param : params) { param->addListener(this); }

    // ensure to display the proper params
    updateChain();

    // start timer
    startTimerHz(60);
}

ResponseCurveComponent::~ResponseCurveComponent() {
    // remove listener
    const auto &params = audioProcessor.getParameters();
    for (auto param : params) { param->removeListener(this); }
}

void ResponseCurveComponent::parameterValueChanged(int parameterIndex, float newValue) {
    parametersChanged.set(true);
}

void ResponseCurveComponent::timerCallback() {
    if (parametersChanged.compareAndSetBool(false, true)) {
        updateChain();
        // signal a repaint
        repaint();
    }
}

void ResponseCurveComponent::updateChain() {
    // update the mono-chain
    auto chainSettings = getChainSettings(audioProcessor.apvts);
    // peak filter
    auto peakCoefficients = makePeakFilter(chainSettings, audioProcessor.getSampleRate());
    updateCoefficients(monoChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
    // cut filters
    auto lowCutCoefficients = makeLowCutFilter(chainSettings, audioProcessor.getSampleRate());
    auto highCutCoefficients = makeHighCutFilter(chainSettings, audioProcessor.getSampleRate());
    updateCutFilter(monoChain.get<ChainPositions::LowCut>(), lowCutCoefficients,
                    chainSettings.lowCutSlope);
    updateCutFilter(monoChain.get<ChainPositions::HighCut>(), highCutCoefficients,
                    chainSettings.highCutSlope);
}

void ResponseCurveComponent::paint(juce::Graphics &g) {
    using namespace juce;
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll(Colours::black);
    g.drawImage(background, getLocalBounds().toFloat());

    auto responseArea = getAnalysisArea();

    auto w = responseArea.getWidth();

    auto &lowCut = monoChain.get<ChainPositions::LowCut>();
    auto &peak = monoChain.get<ChainPositions::Peak>();
    auto &highCut = monoChain.get<ChainPositions::HighCut>();

    auto sampleRate = audioProcessor.getSampleRate();

    // calculate the corresponding height of each frequency chop
    std::vector<double> mags;
    mags.resize(w);
    for (int i = 0; i < w; ++i) {
        double mag = 1.f;
        auto freq = mapToLog10(double(i) / double(w), 20.0, 20000.0);

        if (!monoChain.isBypassed<ChainPositions::Peak>())
            mag *= peak.coefficients->getMagnitudeForFrequency(freq, sampleRate);

        if (!lowCut.isBypassed<0>())
            mag *= lowCut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!lowCut.isBypassed<1>())
            mag *= lowCut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!lowCut.isBypassed<2>())
            mag *= lowCut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!lowCut.isBypassed<3>())
            mag *= lowCut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);

        if (!highCut.isBypassed<0>())
            mag *= highCut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!highCut.isBypassed<1>())
            mag *= highCut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!highCut.isBypassed<2>())
            mag *= highCut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!highCut.isBypassed<3>())
            mag *= highCut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);

        mags[i] = Decibels::gainToDecibels(mag);
    }

    // begin painting
    Path responseCurve;

    const double outputMin = responseArea.getBottom();
    const double outputMax = responseArea.getY();
    auto map = [outputMin, outputMax](double input) { // this is a lambda function!
        return jmap(input, -24.0, 24.0, outputMin, outputMax);
    };

    responseCurve.startNewSubPath(responseArea.getX(),
                                  map(mags.front())); // start from the first point.
    for (size_t i = 1; i < mags.size(); ++i) {
        responseCurve.lineTo(responseArea.getX() + i, map(mags[i])); // set the curve
    }

    // set colour && beatify
    g.setColour(Colours::orange);
    g.drawRoundedRectangle(getRenderArea().toFloat(), 4.f, 1.f); // boarder
    g.setColour(Colours::white);
    g.strokePath(responseCurve, PathStrokeType(2.f)); // the curve
}

void ResponseCurveComponent::resized() {
    // create a background image for the
    using namespace juce;
    background = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);

    Graphics g(background);

    Array<float> freqs{20,   /*30, 40, */ 50,       100,   200,  /*300, 400, */ 500, 1000,
                       2000, /*3000, 4000, */ 5000, 10000, 20000};

    auto renderArea = getAnalysisArea();
    auto left = renderArea.getX();
    auto right = renderArea.getRight();
    auto top = renderArea.getY();
    auto bottom = renderArea.getBottom();
    auto width = renderArea.getWidth();

    Array<float> xs;
    for (auto f : freqs) {
        auto normX = mapFromLog10(f, 20.f, 20000.f);
        xs.add(left + width * normX);
    }

    g.setColour(Colours::dimgrey);
    for (auto x : xs) { g.drawVerticalLine(x, top, bottom); }

    Array<float> gain{-24, -12, 0, 12, 24};

    for (auto gDb : gain) {
        auto y = jmap(gDb, -24.f, 24.f, float(bottom), float(top));
        g.setColour(gDb == 0.f ? Colour(0u, 172u, 1u) : Colours::darkgrey);
        g.drawHorizontalLine(y, left, right);
    }

    g.drawRect(getAnalysisArea());

    // label freqs
    g.setColour(Colours::lightgrey);
    const int fontHeight = 10;
    g.setFont(fontHeight);
    for (int i = 0; i < freqs.size(); ++i) {
        auto f = freqs[i];
        auto x = xs[i];

        bool addK = false;
        String str;
        if (f > 999.f) {
            addK = true;
            f /= 1000.f;
        }

        str << f;
        if (addK) str << "k";
        str << "Hz";

        auto textWidth = g.getCurrentFont().getStringWidth(str);
        Rectangle<int> r;
        r.setSize(textWidth, fontHeight);
        r.setCentre(x, 0);
        r.setY(1);

        g.drawFittedText(str, r, juce::Justification::centred, 1);
    }

    // label gains
    for (auto gDb : gain) {
        auto y = jmap(gDb, -24.f, 24.f, float(bottom), float(top));
        String str;
        if (gDb > 0) str << "+";
        str << gDb;

        auto textWidth = g.getCurrentFont().getStringWidth(str);

        // right side labels
        Rectangle<int> r;
        r.setSize(textWidth, fontHeight);
        r.setX(getWidth() - textWidth);
        r.setCentre(r.getCentreX(), y);

        g.setColour(gDb == 0.f ? Colour(0u, 172u, 1u) : Colours::lightgrey);
        g.drawFittedText(str, r, juce::Justification::centred, 1);

        // left side labels
        str.clear();
        str << (gDb - 24.f);
        r.setX(getX());
        textWidth = g.getCurrentFont().getStringWidth(str);
        r.setSize(textWidth, fontHeight);
        g.setColour(Colours::lightgrey);
        g.drawFittedText(str, r, juce::Justification::centred, 1);
    }
}

juce::Rectangle<int> ResponseCurveComponent::getRenderArea() {
    auto bounds = getLocalBounds();

    bounds.removeFromTop(12);
    bounds.removeFromBottom(2);
    bounds.removeFromLeft(20);
    bounds.removeFromRight(20);
    return bounds;
}

juce::Rectangle<int> ResponseCurveComponent::getAnalysisArea() {
    auto bounds = getRenderArea();
    bounds.removeFromTop(4);
    bounds.removeFromBottom(4);
    return bounds;
}

//==============================================================================
SimpleEQAudioProcessorEditor::SimpleEQAudioProcessorEditor(SimpleEQAudioProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p),
      peakFreqSlider(*audioProcessor.apvts.getParameter("Peak Freq"), "Hz"),
      peakGainSlider(*audioProcessor.apvts.getParameter("Peak Gain"), "dB"),
      peakQualitySlider(*audioProcessor.apvts.getParameter("Peak Quality"), ""),
      lowCutFreqSlider(*audioProcessor.apvts.getParameter("LowCut Freq"), "Hz"),
      highCutFreqSlider(*audioProcessor.apvts.getParameter("HighCut Freq"), "Hz"),
      lowCutSlopeSlider(*audioProcessor.apvts.getParameter("LowCut Slope"), "dB/Oct"),
      highCutSlopeSlider(*audioProcessor.apvts.getParameter("HighCut Slope"), "db/Oct"),

      responseCurveComponent(audioProcessor),
      peakFreqSliderAttachment(audioProcessor.apvts, "Peak Freq", peakFreqSlider),
      peakGainSliderAttachment(audioProcessor.apvts, "Peak Gain", peakGainSlider),
      peakQualitySliderAttachment(audioProcessor.apvts, "Peak Quality", peakQualitySlider),
      lowCutFreqSliderAttachment(audioProcessor.apvts, "LowCut Freq", lowCutFreqSlider),
      highCutFreqSliderAttachment(audioProcessor.apvts, "HighCut Freq", highCutFreqSlider),
      lowCutSlopeSliderAttachment(audioProcessor.apvts, "LowCut Slope", lowCutSlopeSlider),
      highCutSlopeSliderAttachment(audioProcessor.apvts, "HighCut Slope", highCutSlopeSlider) {
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.

    // add labels
    peakFreqSlider.labels.add({0.f, "20Hz"});
    peakFreqSlider.labels.add({1.f, "20kHz"});

    peakGainSlider.labels.add({0.f, "-24dB"});
    peakGainSlider.labels.add({1.f, "+24dB"});

    peakQualitySlider.labels.add({0.f, "0.1"});
    peakQualitySlider.labels.add({1.f, "10.0"});

    lowCutFreqSlider.labels.add({0.f, "20Hz"});
    lowCutFreqSlider.labels.add({1.f, "20kHz"});

    highCutFreqSlider.labels.add({0.f, "20Hz"});
    highCutFreqSlider.labels.add({1.f, "20kHz"});

    lowCutSlopeSlider.labels.add({0.f, "12"});
    lowCutSlopeSlider.labels.add({1.f, "48"});

    highCutSlopeSlider.labels.add({0.f, "12"});
    highCutSlopeSlider.labels.add({1.f, "48"});

    // make components visible
    for (auto *comp : getComps()) { addAndMakeVisible(comp); }

    setSize(600, 480);
}

SimpleEQAudioProcessorEditor::~SimpleEQAudioProcessorEditor() {}

//==============================================================================
void SimpleEQAudioProcessorEditor::paint(juce::Graphics &g) {
    using namespace juce;
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll(Colours::black);
}

void SimpleEQAudioProcessorEditor::resized() {
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..

    auto bounds = getLocalBounds();
    float hRatio = 25.f / 100.f; // JUCE_LIVE_CONSTANT(33) / 100.f; // change lively
    auto responseArea = bounds.removeFromTop(bounds.getHeight() * hRatio);

    responseCurveComponent.setBounds(responseArea);

    bounds.removeFromTop(5); // give some gaps

    auto lowCutArea = bounds.removeFromLeft(bounds.getWidth() * 0.33);
    auto highCutArea = bounds.removeFromRight(bounds.getWidth() * 0.5);

    lowCutFreqSlider.setBounds(lowCutArea.removeFromTop(lowCutArea.getHeight() * 0.5));
    lowCutSlopeSlider.setBounds(lowCutArea);
    highCutFreqSlider.setBounds(highCutArea.removeFromTop(highCutArea.getHeight() * 0.5));
    highCutSlopeSlider.setBounds(highCutArea);

    peakFreqSlider.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.33));
    peakGainSlider.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.5));
    peakQualitySlider.setBounds(bounds);
}

std::vector<juce::Component *> SimpleEQAudioProcessorEditor::getComps() {
    return {&responseCurveComponent, &peakFreqSlider,    &peakGainSlider,    &peakQualitySlider,
            &lowCutFreqSlider,       &highCutFreqSlider, &lowCutSlopeSlider, &highCutSlopeSlider};
}
