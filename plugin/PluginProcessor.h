#pragma once

#include <JuceHeader.h>
#include "orb_pcf.h"

#include <memory>

class PcfEditor;

class PcfProcessor final : public juce::AudioProcessor
{
public:
    PcfProcessor();
    ~PcfProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // empty = success; otherwise a short reason for the editor to show
    juce::String importRecall (const juce::String& text);

    juce::AudioProcessorValueTreeState apvts;

private:
    struct OrbPcfDeleter
    {
        void operator() (OrbPcf* p) const noexcept { orb_pcf_free (p); }
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void processBuffer (juce::AudioBuffer<float>& buffer, bool bypass);
    OrbPcfParams readParams (bool bypass) const;
    OrbPcfPlayhead readPlayhead (int nHost);
    double startingQuarter (double rawPpq, bool hasPpq, juce::int64 samples, bool hasSamples, double bpm) const;
    void clearPlayClock();
    void latchUntrustedPlay (OrbPcfPlayhead& head, const float* left, const float* right, int nHost);
    int latencySamples() const;
    int hostToEngine (const float* left, const float* right, int nHost);
    void engineToHost (float* left, float* right, int nHost, int nEngine);
    void shiftFifo (juce::AudioBuffer<float>& fifo, int& fill, int used);

    std::unique_ptr<OrbPcf, OrbPcfDeleter> pcf;
    double hostRate = 44100.0;
    bool exact44100 = true;
    int maxHostBlock = 0;

    juce::WindowedSincInterpolator toEngine[2];
    juce::WindowedSincInterpolator toHost[2];
    juce::AudioBuffer<float> hostFifo;
    juce::AudioBuffer<float> engineBuf;
    juce::AudioBuffer<float> engineFifo;
    juce::AudioBuffer<float> monoScratch;
    int hostFifoFill = 0;
    int engineFifoFill = 0;

    // Quarter-note position at the start of the next buffer when the host playhead is stalled.
    bool havePlayOrigin = false;
    double playOrigin = 0.0;
    bool haveLastRawPpq = false;
    double lastRawPpq = 0.0;
    bool haveLastSamples = false;
    juce::int64 lastSamples = 0;
    bool sawTrustedPlay = false;
    bool latchedUntrustedPlay = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PcfProcessor)
};
