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

const juce::String kWithPrint =
    kModloader
    + "\nopenrb-print 1\n"
      "ppq: 192\n"
      "0 freq 64\n"
      "768 freq 40\n"
      "1536 decay 90\n";
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
        expectEqual (rec.enabled, 1, "missing enabled defaults on");
    }

    {
        PcfRecall rec;
        expectString (parseRecall (kBp, rec), {}, "parse bp");
        expectEqual (rec.frequency, 10, "bp frequency");
        expectEqual (rec.mode, 1, "bp mode");
        expectEqual (rec.enabled, 1, "bp enabled default");
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
        expectEqual (paramInt (proc.apvts, "enabled"), 1, "default enabled on");
        expectString (proc.importRecall (kBp), {}, "import bp");
        expectEqual (paramInt (proc.apvts, "freq"), 10, "apply freq");
        expectEqual (paramInt (proc.apvts, "res"), 20, "apply res");
        expectEqual (paramInt (proc.apvts, "amount"), 30, "apply amount");
        expectEqual (paramInt (proc.apvts, "decay"), 40, "apply decay");
        expectEqual (paramInt (proc.apvts, "pattern"), 7, "apply pattern");
        expectEqual (paramInt (proc.apvts, "mode"), 1, "apply mode bp");
        expectEqual (paramInt (proc.apvts, "seq_enable"), 0, "seq_enable default");
        expectEqual (paramInt (proc.apvts, "enabled"), 1, "apply enabled default");
        auto* enabled = proc.apvts.getParameter ("enabled");
        expect (enabled != nullptr, "enabled param exists");
        expect (enabled->convertTo0to1 (0.0f) == 0.0f, "bool convert 0");
        expect (enabled->convertTo0to1 (1.0f) == 1.0f, "bool convert 1");
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
        expectEqual (paramInt (proc.apvts, "enabled"), 1, "enabled default does not clear seq");
    }

    {
        PcfProcessor proc;
        const int freq = paramInt (proc.apvts, "freq");
        expectString (proc.importRecall ("openrb-distort 1\namount: 1\nshape: 2\n"),
                      "not OpenRB PCF data",
                      "cross plugin");
        expectEqual (paramInt (proc.apvts, "freq"), freq, "no change on fail");
    }

    {
        PcfRecall rec;
        expectString (parseRecall (kWithPrint, rec), {}, "recall with print appendix");
        expectEqual (rec.frequency, 64, "appendix recall frequency");
        expectEqual (rec.decay, 21, "appendix recall decay");
        expectEqual (rec.mode, 0, "appendix recall mode");

        PcfPrint printed;
        expectString (parsePrint (kWithPrint, printed), {}, "parse print appendix");
        expectEqual (printed.ppq, 192, "print ppq");
        expectEqual ((int) printed.events.size(), 3, "print event count");
        expectEqual (printed.events[0].tick, 0, "print tick 0");
        expectString (juce::String (printed.events[0].id), "freq", "print id 0 freq");
        expectEqual (printed.events[0].value, 64, "print freq 64");
        expectEqual (printed.events[1].tick, 768, "print tick freq 40");
        expectString (juce::String (printed.events[1].id), "freq", "print id 1 freq");
        expectEqual (printed.events[1].value, 40, "print freq 40");
        expectEqual (printed.events[2].tick, 1536, "print tick decay");
        expectString (juce::String (printed.events[2].id), "decay", "print id decay");
        expectEqual (printed.events[2].value, 90, "print decay 90");
    }

    {
        PcfPrint printed;
        expectString (parsePrint (kModloader, printed), {}, "recall-only print");
        expectEqual ((int) printed.events.size(), 0, "recall-only empty events");
        expectEqual (printed.ppq, 192, "recall-only ppq");
    }

    {
        PcfPrint printed;
        const juce::String unknown =
            kModloader
            + "\nopenrb-print 1\n"
              "ppq: 192\n"
              "0 freq 64\n"
              "0 frequency 12\n"
              "0 enabled 1\n"
              "768 freq 40\n"
              "100 banana 3\n";
        expectString (parsePrint (unknown, printed), {}, "unknown print params");
        expectEqual ((int) printed.events.size(), 3, "keep enabled, skip frequency and banana");
        expectString (juce::String (printed.events[0].id), "freq", "kept freq start");
        expectString (juce::String (printed.events[1].id), "enabled", "kept enabled");
        expectEqual (printed.events[1].value, 1, "enabled value 1");
        expectString (juce::String (printed.events[2].id), "freq", "kept freq move");
        expectEqual (printed.events[2].tick, 768, "kept freq tick");
    }

    {
        PcfRecall rec;
        PcfPrint printed;
        const juce::String v2 =
            kModloader
            + "\nopenrb-print 2\n"
              "ppq: 192\n"
              "0 freq 64\n";
        expectString (parseRecall (v2, rec), {}, "recall still ok on print v2");
        expectEqual (rec.frequency, 64, "print v2 frequency snapshot");
        expectString (parsePrint (v2, printed), {}, "print v2 ignored");
        expectEqual ((int) printed.events.size(), 0, "print v2 empty");
    }

    {
        PcfPrint printed;
        const juce::String scaled =
            kModloader
            + "\nopenrb-print 1\n"
              "ppq: 96\n"
              "0 freq 64\n"
              "384 freq 40\n";
        expectString (parsePrint (scaled, printed), {}, "scale ppq");
        expectEqual (printed.ppq, 192, "scaled ppq is 192");
        expectEqual ((int) printed.events.size(), 2, "scaled events");
        expectEqual (printed.events[1].tick, 768, "tick scaled 96 to 192");
    }

    {
        PcfPrint printed;
        const juce::String clamped =
            kModloader
            + "\nopenrb-print 1\n"
              "ppq: 192\n"
              "0 freq 200\n"
              "0 pattern 99\n"
              "0 mode 9\n"
              "768 pattern -4\n"
              "1536 mode 0\n";
        expectString (parsePrint (clamped, printed), {}, "clamp print");
        expectEqual ((int) printed.events.size(), 5, "clamp print count");
        expectEqual (printed.events[0].value, 127, "clamp print freq");
        expectEqual (printed.events[1].value, 54, "clamp print pattern");
        expectEqual (printed.events[2].value, 1, "clamp print mode");
        expectEqual (printed.events[3].value, 0, "clamp print pattern low");
        expectEqual (printed.events[4].value, 0, "print mode lp");
    }

    {
        PcfProcessor proc;
        expectString (proc.importRecall (kWithPrint), {}, "import with print");
        expect (proc.isPrintArmed(), "import arms print");
        expectEqual (paramInt (proc.apvts, "freq"), 64, "import print freq tick 0");
        expectEqual (paramInt (proc.apvts, "decay"), 21, "import print decay snapshot");
        expectEqual (paramInt (proc.apvts, "mode"), 0, "import print mode");
        expectEqual (paramInt (proc.apvts, "seq_enable"), 0, "import print leaves seq");
        const auto events = proc.copyPrintEvents();
        expectEqual ((int) events.size(), 3, "import stored print events");
        expect (proc.hasPrintTake(), "import loaded print take");
        expectEqual (events[0].tick, 0, "stored tick 0");
        expectString (juce::String (events[0].id), "freq", "stored freq");
        expectEqual (events[2].value, 90, "stored decay value");

        expectString (proc.importRecall (kModloader), {}, "import recall-only after print");
        expectEqual ((int) proc.copyPrintEvents().size(), 0, "recall-only clears print");
        expect (! proc.hasPrintTake(), "recall-only unloads print");
        expect (! proc.isPrintArmed(), "recall-only disarms print");
        expectEqual (paramInt (proc.apvts, "freq"), 64, "recall-only freq stays");
        expectEqual (paramInt (proc.apvts, "enabled"), 1, "recall-only enabled stays on");
    }

    {
        PcfRecall rec;
        const juce::String off =
            "openrb-pcf 1\n"
            "frequency: 64\n"
            "resonance: 9\n"
            "amount: 109\n"
            "decay: 21\n"
            "pattern: 5\n"
            "mode: lp\n"
            "enabled: 0\n";
        expectString (parseRecall (off, rec), {}, "parse enabled 0");
        expectEqual (rec.enabled, 0, "snapshot enabled 0");

        PcfProcessor proc;
        auto* seq = proc.apvts.getParameter ("seq_enable");
        expect (seq != nullptr, "seq_enable exists for enabled-off import");
        if (seq != nullptr)
        {
            seq->beginChangeGesture();
            seq->setValueNotifyingHost (seq->convertTo0to1 (1.0f));
            seq->endChangeGesture();
        }
        expectString (proc.importRecall (off), {}, "import enabled 0");
        expectEqual (paramInt (proc.apvts, "enabled"), 0, "apply enabled off");
        expectEqual (paramInt (proc.apvts, "seq_enable"), 1, "enabled off leaves seq on");
        expectString (proc.importRecall (kModloader), {}, "import missing enabled");
        expectEqual (paramInt (proc.apvts, "enabled"), 1, "missing enabled restores on");
        expectEqual (paramInt (proc.apvts, "seq_enable"), 1, "missing enabled leaves seq");
    }

    {
        PcfRecall rec;
        const juce::String clamped =
            "openrb-pcf 1\n"
            "frequency: 10\n"
            "resonance: 20\n"
            "amount: 30\n"
            "decay: 40\n"
            "pattern: 7\n"
            "mode: lp\n"
            "enabled: 5\n";
        expectString (parseRecall (clamped, rec), {}, "clamp enabled");
        expectEqual (rec.enabled, 1, "enabled 5 clamps to 1");
    }

    {
        PcfRecall rec;
        expectString (parseRecall (
                          "openrb-pcf 1\n"
                          "frequency: 1\n"
                          "resonance: 1\n"
                          "amount: 1\n"
                          "decay: 1\n"
                          "pattern: 0\n"
                          "mode: lp\n"
                          "enabled: banana\n",
                          rec),
                      "invalid enabled",
                      "bad enabled");
    }

    {
        const juce::String tick0 =
            "openrb-pcf 1\n"
            "frequency: 64\n"
            "resonance: 9\n"
            "amount: 109\n"
            "decay: 21\n"
            "pattern: 5\n"
            "mode: lp\n"
            "enabled: 1\n"
            "\n"
            "openrb-print 1\n"
            "ppq: 192\n"
            "0 enabled 0\n";
        PcfPrint printed;
        expectString (parsePrint (tick0, printed), {}, "parse tick-0 enabled");
        expectEqual ((int) printed.events.size(), 1, "tick-0 enabled event");
        expectString (juce::String (printed.events[0].id), "enabled", "tick-0 id");
        expectEqual (printed.events[0].tick, 0, "tick-0");
        expectEqual (printed.events[0].value, 0, "tick-0 value off");

        PcfProcessor proc;
        auto* seq = proc.apvts.getParameter ("seq_enable");
        if (seq != nullptr)
        {
            seq->beginChangeGesture();
            seq->setValueNotifyingHost (seq->convertTo0to1 (1.0f));
            seq->endChangeGesture();
        }
        expectString (proc.importRecall (tick0), {}, "import tick-0 enabled");
        expect (proc.hasPrintTake(), "tick-0 enable loads print");
        expect (proc.isPrintArmed(), "tick-0 enable arms print");
        const auto events = proc.copyPrintEvents();
        expectEqual ((int) events.size(), 1, "stored tick-0 enable");
        expectString (juce::String (events[0].id), "enabled", "stored enable id");
        // Always-lane: seed writes tick 0 even though nothing moves later.
        expectEqual (paramInt (proc.apvts, "enabled"), 0, "tick-0 enable is a moving lane");
        expectEqual (paramInt (proc.apvts, "seq_enable"), 1, "print enable leaves seq");
    }

    if (gFails != 0)
    {
        std::cerr << gFails << " failure(s)\n";
        return 1;
    }

    std::cout << "All recall import tests passed\n";
    return 0;
}
