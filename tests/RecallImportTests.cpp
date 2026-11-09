#include "PluginProcessor.h"
#include "RecallImport.h"

#include <iostream>

namespace
{
int gFails = 0;

void expect (bool cond, const char* msg)
{
    if (! cond)
    {
        std::cerr << "FAIL: " << msg << '\n';
        ++gFails;
    }
}

void expectEqual (int a, int b, const char* msg)
{
    if (a != b)
    {
        std::cerr << "FAIL: " << msg << " (" << a << " != " << b << ")\n";
        ++gFails;
    }
}

void expectString (const juce::String& a, const juce::String& b, const char* msg)
{
    if (a != b)
    {
        std::cerr << "FAIL: " << msg << " (\"" << a << "\" != \"" << b << "\")\n";
        ++gFails;
    }
}

int paramInt (juce::AudioProcessorValueTreeState& apvts, const char* id)
{
    if (auto* v = apvts.getRawParameterValue (id))
        return juce::roundToInt (v->load());
    return -999;
}

const juce::String kModloader =
    "openrb-pcf 1\n"
    "frequency: 64\n"
    "resonance: 9\n"
    "amount: 109\n"
    "decay: 21\n"
    "pattern: 5\n"
    "mode: lp\n";

const juce::String kBp =
    "openrb-pcf 1\n"
    "frequency: 10\n"
    "resonance: 20\n"
    "amount: 30\n"
    "decay: 40\n"
    "pattern: 7\n"
    "mode: bp\n";
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    {
        PcfRecall rec;
        expectString (parseRecall (kModloader, rec), {}, "parse modloader");
        expectEqual (rec.frequency, 64, "modloader frequency");
        expectEqual (rec.resonance, 9, "modloader resonance");
        expectEqual (rec.amount, 109, "modloader amount");
        expectEqual (rec.decay, 21, "modloader decay");
        expectEqual (rec.pattern, 5, "modloader pattern");
        expectEqual (rec.mode, 0, "modloader mode lp");
    }

    {
        PcfRecall rec;
        expectString (parseRecall (kBp, rec), {}, "parse bp");
        expectEqual (rec.frequency, 10, "bp frequency");
        expectEqual (rec.mode, 1, "bp mode");
    }

    {
        PcfRecall rec;
        expectString (parseRecall ("openrb-delay 1\nmix: 22\n", rec),
                      "not OpenRB PCF data",
                      "wrong header");
    }

    {
        PcfRecall rec;
        expectString (parseRecall ("openrb-pcf 2\nfrequency: 1\n", rec),
                      "unsupported recall version",
                      "bad version");
    }

    {
        PcfRecall rec;
        const juce::String missing =
            "openrb-pcf 1\n"
            "frequency: 10\n"
            "resonance: 20\n"
            "amount: 30\n"
            "pattern: 7\n"
            "mode: lp\n";
        expectString (parseRecall (missing, rec), "missing decay", "missing key");
    }

    {
        PcfRecall rec;
        const juce::String banana =
            "openrb-pcf 1\n"
            "frequency: 10\n"
            "resonance: 20\n"
            "amount: 30\n"
            "decay: 40\n"
            "pattern: 7\n"
            "mode: banana\n";
        expectString (parseRecall (banana, rec), "invalid mode", "mode banana");
    }

    {
        PcfRecall rec;
        const juce::String clamped =
            "openrb-pcf 1\n"
            "frequency: 200\n"
            "resonance: -3\n"
            "amount: 30\n"
            "decay: 40\n"
            "pattern: 99\n"
            "mode: lp\n"
            "unknown: 1\n";
        expectString (parseRecall (clamped, rec), {}, "clamp parse");
        expectEqual (rec.frequency, 127, "clamp frequency");
        expectEqual (rec.resonance, 0, "clamp resonance");
        expectEqual (rec.pattern, 54, "clamp pattern");
    }

    {
        PcfProcessor proc;
        expectString (proc.importRecall (kBp), {}, "import bp");
        expectEqual (paramInt (proc.apvts, "freq"), 10, "apply freq");
        expectEqual (paramInt (proc.apvts, "res"), 20, "apply res");
        expectEqual (paramInt (proc.apvts, "amount"), 30, "apply amount");
        expectEqual (paramInt (proc.apvts, "decay"), 40, "apply decay");
        expectEqual (paramInt (proc.apvts, "pattern"), 7, "apply pattern");
        expectEqual (paramInt (proc.apvts, "mode"), 1, "apply mode bp");
        expectEqual (paramInt (proc.apvts, "seq_enable"), 0, "seq_enable default");
    }

    {
        PcfProcessor proc;
        auto* seq = proc.apvts.getParameter ("seq_enable");
        auto* step = proc.apvts.getParameter ("seq_00");
        expect (seq != nullptr && step != nullptr, "seq params exist");
        if (seq != nullptr)
        {
            seq->beginChangeGesture();
            seq->setValueNotifyingHost (seq->convertTo0to1 (1.0f));
            seq->endChangeGesture();
        }
        if (step != nullptr)
        {
            step->beginChangeGesture();
            step->setValueNotifyingHost (step->convertTo0to1 (2.0f));
            step->endChangeGesture();
        }
        const int length = paramInt (proc.apvts, "seq_length");
        const int grid = paramInt (proc.apvts, "seq_res");
        expectString (proc.importRecall (kModloader), {}, "import with seq on");
        expectEqual (paramInt (proc.apvts, "freq"), 64, "modloader apply freq");
        expectEqual (paramInt (proc.apvts, "mode"), 0, "modloader apply lp");
        expectEqual (paramInt (proc.apvts, "seq_enable"), 1, "seq_enable kept");
        expectEqual (paramInt (proc.apvts, "seq_00"), 2, "seq_00 kept");
        expectEqual (paramInt (proc.apvts, "seq_length"), length, "seq_length kept");
        expectEqual (paramInt (proc.apvts, "seq_res"), grid, "seq_res kept");
    }

    {
        PcfProcessor proc;
        const int freq = paramInt (proc.apvts, "freq");
        expectString (proc.importRecall ("openrb-distort 1\namount: 1\nshape: 2\n"),
                      "not OpenRB PCF data",
                      "cross plugin");
        expectEqual (paramInt (proc.apvts, "freq"), freq, "no change on fail");
    }

    if (gFails != 0)
    {
        std::cerr << gFails << " failure(s)\n";
        return 1;
    }

    std::cout << "All recall import tests passed\n";
    return 0;
}
