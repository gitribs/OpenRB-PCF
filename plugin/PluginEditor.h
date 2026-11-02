#pragma once

#include "PluginProcessor.h"

#include <array>
#include <memory>

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

class FoldArrow final : public juce::Button
{
public:
    FoldArrow();
    void setOpen (bool shouldBeOpen);
    bool isOpen() const noexcept { return open; }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    bool open = false;
};

class StepPad final : public juce::Button
{
public:
    StepPad();
    void setLevel (int newLevel);
    int getLevel() const noexcept { return level; }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    int level = 0;
};

class SequencerStrip final : public juce::Component
{
public:
    explicit SequencerStrip (PcfProcessor&);
    ~SequencerStrip() override;
    void resized() override;

private:
    void setupIncDec (juce::Slider& s);

    juce::ToggleButton enable { "Enabled" };
    juce::Slider length, grid;
    juce::Label lengthN, gridN;
    std::array<StepPad, 32> pads;

    juce::AudioProcessorValueTreeState::ButtonAttachment enableA;
    juce::AudioProcessorValueTreeState::SliderAttachment lengthA, gridA;
    std::array<std::unique_ptr<juce::ParameterAttachment>, 32> padA;
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
    void setSeqOpen (bool open);

    PcfLookAndFeel lnf;

    juce::Slider freq, res, amount, decay, pattern;
    juce::ToggleButton lowPass { "Low-pass" }, bandPass { "Band-pass" };
    juce::Label title, freqN, resN, amountN, decayN, patternN, modeN;
    FoldArrow fold;
    std::unique_ptr<SequencerStrip> seqStrip;

    juce::AudioProcessorValueTreeState::SliderAttachment freqA, resA, amountA, decayA, patternA;
    std::unique_ptr<juce::ParameterAttachment> modeA;
    std::unique_ptr<juce::ParameterAttachment> seqEnableWatch;
    bool ignoreModeCallbacks = false;
    bool seqOpen = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PcfEditor)
};
