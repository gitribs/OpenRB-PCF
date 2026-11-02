#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
constexpr double kEngineRate = 44100.0;
constexpr int kPcfBlock = 64;

// Starting values for a newly inserted effect. Hosts restore project and preset
// state through setStateInformation, which replaces these after construction.
constexpr int kDefaultFreq = 64;
constexpr int kDefaultRes = 9;
constexpr int kDefaultAmount = 109;
constexpr int kDefaultDecay = 21;
constexpr int kDefaultPattern = 5;
constexpr int kDefaultSeqLength = 16;
constexpr uint8_t kSeqVelocities[] = { 0, 60, 120 };
constexpr uint8_t kSeqResolutions[] = { 8, 16, 32 };
constexpr float kUntrustedPlayFloor = 1.0e-4f;

bool bufferHasEnergy (const float* left, const float* right, int n)
{
    for (int i = 0; i < n; ++i)
        if (std::abs (left[i]) > kUntrustedPlayFloor || std::abs (right[i]) > kUntrustedPlayFloor)
            return true;
    return false;
}

juce::String seqPadId (int index)
{
    char id[16];
    std::snprintf (id, sizeof (id), "seq_%02d", index);
    return juce::String (id);
}

bool ratesMatch (double a, double b)
{
    return std::abs (a - b) < 1.0e-6;
}
} // namespace

PcfProcessor::PcfProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout()),
      pcf (orb_pcf_new())
{
}

PcfProcessor::~PcfProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout PcfProcessor::createLayout()
{
    using juce::AudioParameterBool;
    using juce::AudioParameterChoice;
    using juce::AudioParameterInt;
    using juce::ParameterID;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "freq", 1 }, "Frequency", 0, 127, kDefaultFreq));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "res", 1 }, "Resonance", 0, 127, kDefaultRes));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "amount", 1 }, "Amount", 0, 127, kDefaultAmount));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "decay", 1 }, "Decay", 0, 127, kDefaultDecay));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "pattern", 1 }, "Pattern", 0, 54, kDefaultPattern));
    p.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { "mode", 1 }, "Mode", juce::StringArray { "Low-pass", "Band-pass" }, 0));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "seq_enable", 1 }, "Seq Enable", false));
    p.push_back (std::make_unique<AudioParameterInt> (
        ParameterID { "seq_length", 1 }, "Seq Length", 1, 32, kDefaultSeqLength));
    p.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { "seq_res", 1 }, "Seq Grid", juce::StringArray { "8", "16", "32" }, 1));
    for (int i = 0; i < 32; ++i)
        p.push_back (std::make_unique<AudioParameterInt> (
            ParameterID { seqPadId (i), 1 }, "Seq Step " + juce::String (i + 1), 0, 2, 0));
    return { p.begin(), p.end() };
}

void PcfProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostRate = sampleRate > 0.0 ? sampleRate : kEngineRate;
    exact44100 = ratesMatch (hostRate, kEngineRate);
    maxHostBlock = std::max (samplesPerBlock, 1);

    const int extra = 256 + (int) juce::WindowedSincInterpolator::getBaseLatency();
    const int maxEngine = (int) std::ceil ((double) maxHostBlock * kEngineRate / std::min (hostRate, kEngineRate))
                          + extra + kPcfBlock;

    hostFifo.setSize (2, maxHostBlock + extra);
    engineBuf.setSize (2, maxEngine);
    engineFifo.setSize (2, maxEngine + extra);
    monoScratch.setSize (1, maxEngine);
    hostFifo.clear();
    engineBuf.clear();
    engineFifo.clear();
    monoScratch.clear();
    hostFifoFill = 0;
    engineFifoFill = 0;

    for (auto& i : toEngine)
        i.reset();
    for (auto& i : toHost)
        i.reset();

    if (pcf)
        orb_pcf_reset (pcf.get());

    clearPlayClock();
    setLatencySamples (latencySamples());
}

void PcfProcessor::releaseResources() {}

void PcfProcessor::reset()
{
    hostFifo.clear();
    engineBuf.clear();
    engineFifo.clear();
    hostFifoFill = 0;
    engineFifoFill = 0;
    for (auto& i : toEngine)
        i.reset();
    for (auto& i : toHost)
        i.reset();
    if (pcf)
        orb_pcf_reset (pcf.get());
    clearPlayClock();
}

int PcfProcessor::latencySamples() const
{
    const int pcfLatency = (int) std::llround ((double) kPcfBlock * hostRate / kEngineRate);
    if (exact44100)
        return pcfLatency;

    const double base = juce::WindowedSincInterpolator::getBaseLatency();
    const double upHost = base;
    const double downHost = base * hostRate / kEngineRate;
    return pcfLatency + (int) std::llround (upHost + downHost);
}

bool PcfProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out)
        return false;
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

OrbPcfParams PcfProcessor::readParams (bool bypass) const
{
    auto asInt = [this] (const char* id, int fallback) -> int
    {
        if (auto* v = apvts.getRawParameterValue (id))
            return juce::roundToInt (v->load());
        return fallback;
    };

    OrbPcfParams params {};
    params.frequency = (uint8_t) juce::jlimit (0, 127, asInt ("freq", kDefaultFreq));
    params.resonance = (uint8_t) juce::jlimit (0, 127, asInt ("res", kDefaultRes));
    params.amount = (uint8_t) juce::jlimit (0, 127, asInt ("amount", kDefaultAmount));
    params.decay = (uint8_t) juce::jlimit (0, 127, asInt ("decay", kDefaultDecay));
    params.pattern = (uint8_t) juce::jlimit (0, 54, asInt ("pattern", kDefaultPattern));
    params.mode = asInt ("mode", 0) == 0 ? 1 : 0;
    params.bypass = bypass ? 1 : 0;
    params.seq_enable = asInt ("seq_enable", 0) != 0 ? 1 : 0;
    params.seq_length = (uint8_t) juce::jlimit (1, 32, asInt ("seq_length", kDefaultSeqLength));
    params.seq_resolution = kSeqResolutions[juce::jlimit (0, 2, asInt ("seq_res", 1))];
    for (int i = 0; i < 32; ++i)
    {
        const auto id = seqPadId (i);
        const int level = juce::jlimit (0, 2, asInt (id.toRawUTF8(), 0));
        params.seq_steps[i] = kSeqVelocities[level];
    }
    return params;
}

void PcfProcessor::clearPlayClock()
{
    havePlayOrigin = false;
    playOrigin = 0.0;
    haveLastRawPpq = false;
    lastRawPpq = 0.0;
    haveLastSamples = false;
    lastSamples = 0;
    sawTrustedPlay = false;
    latchedUntrustedPlay = false;
}

void PcfProcessor::latchUntrustedPlay (OrbPcfPlayhead& head, const float* left, const float* right, int nHost)
{
    if (head.playing != 0)
        sawTrustedPlay = true;

    if (sawTrustedPlay)
        return;

    // Hosts such as Reason can omit kPlaying and stall PPQ on the first Play
    // after insert. Hold until audio actually arrives, then free-run from 0 so
    // the pattern starts with the sound. A later trusted transport snap replaces this.
    if (!latchedUntrustedPlay && nHost > 0 && bufferHasEnergy (left, right, nHost))
    {
        latchedUntrustedPlay = true;
        havePlayOrigin = true;
        playOrigin = 0.0;
        haveLastRawPpq = false;
        lastRawPpq = 0.0;
        haveLastSamples = false;
        lastSamples = 0;
        if (pcf)
            orb_pcf_restart_clock (pcf.get());
        head.ppq = 0.0;
    }

    if (latchedUntrustedPlay)
        head.playing = 1;
}

double PcfProcessor::startingQuarter (double rawPpq, bool hasPpq, juce::int64 samples, bool hasSamples, double bpm) const
{
    const bool canDerive = hasSamples && hostRate > 0.0 && bpm > 0.0;
    const double derived = canDerive ? (double) samples * bpm / (hostRate * 60.0) : 0.0;
    if (!hasPpq)
        return canDerive ? derived : 0.0;
    // A playhead left at 0 while the sample counter is already into the song.
    if (canDerive && std::abs (rawPpq) < 1.0e-6 && derived > 0.25)
        return derived;
    return rawPpq;
}

OrbPcfPlayhead PcfProcessor::readPlayhead (int nHost)
{
    OrbPcfPlayhead head { 120.0, 0.0, 0 };
    if (auto* ph = getPlayHead())
    {
        if (auto info = ph->getPosition())
        {
            head.bpm = info->getBpm().orFallback (120.0);
            if (!(head.bpm > 0.0) || !std::isfinite (head.bpm))
                head.bpm = 120.0;

            const auto rawOpt = info->getPpqPosition();
            const bool hasPpq = rawOpt.hasValue() && std::isfinite (*rawOpt);
            const double rawPpq = hasPpq ? *rawOpt : 0.0;

            const auto sampleOpt = info->getTimeInSamples();
            const bool hasSamples = sampleOpt.hasValue();
            const juce::int64 samples = hasSamples ? *sampleOpt : 0;

            const bool ppqMoving = hasPpq && haveLastRawPpq && std::abs (rawPpq - lastRawPpq) > 1.0e-9;
            const bool samplesMoving = hasSamples && haveLastSamples && samples != lastSamples;
            head.playing = (info->getIsPlaying() || info->getIsRecording() || ppqMoving || samplesMoving) ? 1 : 0;

            const double advance = (hostRate > 0.0 && nHost > 0)
                                       ? (double) nHost * head.bpm / (hostRate * 60.0)
                                       : 0.0;

            if (!havePlayOrigin)
            {
                playOrigin = startingQuarter (rawPpq, hasPpq, samples, hasSamples, head.bpm);
                havePlayOrigin = true;
            }

            double reported = playOrigin;
            if (head.playing != 0 && advance > 0.0)
            {
                const bool haveDelta = hasPpq && haveLastRawPpq;
                const double delta = haveDelta ? rawPpq - lastRawPpq : 0.0;
                const bool agrees = haveDelta && std::abs (delta - advance) < std::max (1.0e-4, advance * 0.5);
                const bool jumped = haveDelta && std::abs (delta) > std::max (1.0, advance * 4.0);
                if (agrees || jumped)
                    reported = rawPpq;
                playOrigin = reported + advance;
            }
            else if (head.playing == 0 && hasPpq && haveLastRawPpq
                     && std::abs (rawPpq - lastRawPpq) > std::max (1.0, advance * 4.0))
            {
                playOrigin = rawPpq;
                reported = rawPpq;
            }

            head.ppq = std::isfinite (reported) ? reported : 0.0;

            if (hasPpq)
            {
                lastRawPpq = rawPpq;
                haveLastRawPpq = true;
            }
            if (hasSamples)
            {
                lastSamples = samples;
                haveLastSamples = true;
            }
        }
    }
    return head;
}

void PcfProcessor::shiftFifo (juce::AudioBuffer<float>& fifo, int& fill, int used)
{
    if (used <= 0)
        return;
    if (used >= fill)
    {
        fill = 0;
        return;
    }
    const int remain = fill - used;
    for (int ch = 0; ch < fifo.getNumChannels(); ++ch)
        std::memmove (fifo.getWritePointer (ch), fifo.getReadPointer (ch) + used, (size_t) remain * sizeof (float));
    fill = remain;
}

int PcfProcessor::hostToEngine (const float* left, const float* right, int nHost)
{
    const int cap = hostFifo.getNumSamples();
    const int take = juce::jmin (nHost, cap - hostFifoFill);
    if (take > 0)
    {
        std::memcpy (hostFifo.getWritePointer (0) + hostFifoFill, left, (size_t) take * sizeof (float));
        std::memcpy (hostFifo.getWritePointer (1) + hostFifoFill, right, (size_t) take * sizeof (float));
        hostFifoFill += take;
    }

    const double speed = hostRate / kEngineRate;
    int nEngine = (int) std::floor ((double) hostFifoFill / speed);
    nEngine = juce::jlimit (0, engineBuf.getNumSamples(), nEngine);
    if (nEngine <= 0)
        return 0;

    int used = 0;
    for (int ch = 0; ch < 2; ++ch)
        used = toEngine[ch].process (speed, hostFifo.getReadPointer (ch), engineBuf.getWritePointer (ch), nEngine);

    shiftFifo (hostFifo, hostFifoFill, used);
    return nEngine;
}

void PcfProcessor::engineToHost (float* left, float* right, int nHost, int nEngine)
{
    const int cap = engineFifo.getNumSamples();
    const int take = juce::jmin (nEngine, cap - engineFifoFill);
    if (take > 0)
    {
        std::memcpy (engineFifo.getWritePointer (0) + engineFifoFill, engineBuf.getReadPointer (0),
                     (size_t) take * sizeof (float));
        std::memcpy (engineFifo.getWritePointer (1) + engineFifoFill, engineBuf.getReadPointer (1),
                     (size_t) take * sizeof (float));
        engineFifoFill += take;
    }

    const double speed = kEngineRate / hostRate;
    float* outs[2] = { left, right };
    int used = 0;
    for (int ch = 0; ch < 2; ++ch)
        used = toHost[ch].process (speed, engineFifo.getReadPointer (ch), outs[ch], nHost, engineFifoFill, 0);

    shiftFifo (engineFifo, engineFifoFill, juce::jmin (used, engineFifoFill));
}

void PcfProcessor::processBuffer (juce::AudioBuffer<float>& buffer, bool bypass)
{
    juce::ScopedNoDenormals noDenormals;

    const int nHost = buffer.getNumSamples();
    const int nCh = buffer.getNumChannels();
    if (nHost <= 0 || nCh <= 0 || pcf == nullptr)
        return;

    for (int ch = nCh; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, nHost);

    float* left = buffer.getWritePointer (0);
    float* right = nCh > 1 ? buffer.getWritePointer (1) : monoScratch.getWritePointer (0);
    if (nCh == 1)
        std::memcpy (right, left, (size_t) nHost * sizeof (float));

    const auto params = readParams (bypass);
    auto head = readPlayhead (nHost);
    latchUntrustedPlay (head, left, right, nHost);

    if (exact44100)
    {
        orb_pcf_process (pcf.get(), left, right, nHost, &params, &head);
        if (nCh == 1)
            std::memcpy (buffer.getWritePointer (0), left, (size_t) nHost * sizeof (float));
        return;
    }

    const int nEngine = hostToEngine (left, right, nHost);
    if (nEngine > 0)
        orb_pcf_process (pcf.get(), engineBuf.getWritePointer (0), engineBuf.getWritePointer (1), nEngine, &params, &head);
    engineToHost (left, right, nHost, nEngine);
    if (nCh == 1)
        std::memcpy (buffer.getWritePointer (0), left, (size_t) nHost * sizeof (float));
}

void PcfProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);
    processBuffer (buffer, false);
}

void PcfProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);
    processBuffer (buffer, true);
}

juce::AudioProcessorEditor* PcfProcessor::createEditor()
{
    return new PcfEditor (*this);
}

void PcfProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PcfProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PcfProcessor();
}
