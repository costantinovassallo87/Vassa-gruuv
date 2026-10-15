// Headless test of the real processor: sequencer -> sampler -> main mix / per-track buses.
#include "PluginProcessor.h"

namespace
{
int failures = 0;
void check (bool ok, const juce::String& what)
{
    std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

struct Result { float mainPeak = 0, trackPeak[4] {}; };

Result run (bool routeTrack1, bool addStep)
{
    VassaGruuvProcessor proc;
    if (routeTrack1)
        proc.getBus (false, 2)->enable (true); // bus 0 = main, bus 1 = Track 1, bus 2 = Track 2

    proc.pattern.clear();
    if (addStep)
        proc.pattern.set (vg::Velocity, 1, 0, 127); // track 2, step 1
    if (auto* p = proc.apvts.getParameter ("run"))
        p->setValueNotifyingHost (1.0f);
    if (auto* p = proc.apvts.getParameter ("reverb"))
        p->setValueNotifyingHost (0.0f);

    const int channels = proc.getTotalNumOutputChannels();
    proc.setRateAndBufferSizeDetails (44100.0, 512);
    proc.prepareToPlay (44100.0, 512);

    juce::AudioBuffer<float> buf (channels, 512);
    juce::MidiBuffer midi;
    Result r;
    for (int b = 0; b < 100; ++b) // ~1.1 s at 120 bpm = 2 beats
    {
        buf.clear();
        proc.processBlock (buf, midi);
        r.mainPeak = std::max (r.mainPeak, buf.getMagnitude (0, 0, 512));
        r.mainPeak = std::max (r.mainPeak, buf.getMagnitude (1, 0, 512));
        if (routeTrack1)
            r.trackPeak[1] = std::max (r.trackPeak[1], std::max (buf.getMagnitude (2, 0, 512), buf.getMagnitude (3, 0, 512)));
    }
    return r;
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI init;

    const auto silent = run (false, false);
    check (silent.mainPeak < 1.0e-6f, "empty pattern is silent");

    const auto mixed = run (false, true);
    check (mixed.mainPeak > 0.05f, "step on track 2 reaches the main mix (peak " + juce::String (mixed.mainPeak, 3) + ")");

    const auto routed = run (true, true);
    check (routed.mainPeak < 1.0e-6f, "routed track leaves the main mix empty");
    check (routed.trackPeak[1] > 0.05f, "routed track 2 plays on its own bus (peak " + juce::String (routed.trackPeak[1], 3) + ")");

    return failures == 0 ? 0 : 1;
}
