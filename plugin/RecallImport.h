#pragma once

#include <JuceHeader.h>

struct PcfRecall
{
    int frequency = 0;
    int resonance = 0;
    int amount = 0;
    int decay = 0;
    int pattern = 0;
    int mode = 0;
};

// empty = success; otherwise a short reason
juce::String parseRecall (const juce::String& text, PcfRecall& out);
void applyRecall (juce::AudioProcessorValueTreeState& apvts, const PcfRecall& values);
