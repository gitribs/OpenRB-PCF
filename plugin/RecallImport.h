#pragma once

#include <JuceHeader.h>

#include <vector>

struct PcfRecall
{
    int frequency = 0;
    int resonance = 0;
    int amount = 0;
    int decay = 0;
    int pattern = 0;
    int mode = 0;
    int enabled = 1;
};

struct PcfPrintEvent
{
    int tick = 0;
    const char* id = "";
    int value = 0;
};

struct PcfPrint
{
    int ppq = 192;
    std::vector<PcfPrintEvent> events;
};

// empty = success; otherwise a short reason
juce::String parseRecall (const juce::String& text, PcfRecall& out);
void applyRecall (juce::AudioProcessorValueTreeState& apvts, const PcfRecall& values);

// empty = success. Missing or non-v1 print is success with no events.
juce::String parsePrint (const juce::String& text, PcfPrint& out);
