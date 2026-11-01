#pragma once

#include "PluginProcessor.h"

class PcfLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    PcfLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawTickBox (juce::Graphics&, juce::Component&, float x, float y, float w, float h,
                      bool ticked, bool isEnabled, bool shouldDrawButtonAsHighlighted,
                      bool shouldDrawButtonAsDown) override;
    juce::Button* createSliderButton (juce::Slider&, bool isIncrement) override;
    juce::Font getLabelFont (juce::Label&) override;
};

class PcfEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PcfEditor (PcfProcessor&);
    ~PcfEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void setupKnob (juce::Slider& s);
    void setupPattern();
    void setupModeButtons (PcfProcessor& p);

    PcfLookAndFeel lnf;

    juce::Slider freq, res, amount, decay, pattern;
    juce::ToggleButton lowPass { "Low-pass" }, bandPass { "Band-pass" };
    juce::Label title, freqN, resN, amountN, decayN, patternN, modeN;

    juce::AudioProcessorValueTreeState::SliderAttachment freqA, resA, amountA, decayA, patternA;
    std::unique_ptr<juce::ParameterAttachment> modeA;
    bool ignoreModeCallbacks = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PcfEditor)
};
