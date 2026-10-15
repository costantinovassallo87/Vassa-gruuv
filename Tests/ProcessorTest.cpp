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

vg::PatternGrid grid (VassaGruuvProcessor& p) { return p.pattern.snapshotVelocities(); }

void testGeneration()
{
    VassaGruuvProcessor proc;
    proc.pattern.clear();
    for (int s = 0; s < 16; s += 2)
        proc.pattern.set (vg::Velocity, 3, s, 77); // track 4 (Clap): a marker pattern
    proc.pattern.set (vg::Nudge, 3, 0, 10);
    const auto before = grid (proc)[3];

    proc.setTrackLocked (3, true);
    proc.generateLocal (0, 0.6f);
    check (grid (proc)[3] == before && proc.pattern.get (vg::Nudge, 3, 0) == 10, "a locked track survives Generate untouched");
    int others = 0;
    const auto now = grid (proc);
    for (int t = 0; t < 16; ++t)
        if (t != 3)
            for (auto v : now[(size_t) t]) others += v > 0;
    check (others > 5, "unlocked tracks are generated");
    check (std::abs (proc.apvts.getRawParameterValue ("tempo")->load() - 120.0f) < 0.01f, "tempo is left alone while a track is locked");

    proc.setTrackLocked (3, false);
    proc.generateLocal (4 /* Trap */, 0.6f);
    check (std::abs (proc.apvts.getRawParameterValue ("tempo")->load() - 140.0f) < 0.1f, "tempo follows the genre when nothing is locked");

    const auto kickBefore = grid (proc)[0], snareBefore = grid (proc)[2];
    for (int i = 0; i < 10; ++i)
        proc.regenerateTrack (5 /* Hat C */, 0, 1.0f);
    check (grid (proc)[0] == kickBefore && grid (proc)[2] == snareBefore, "regenerating one track leaves the others alone");

    proc.pattern.clear();
    proc.pattern.set (vg::Velocity, 0, 0, 100);
    proc.pattern.set (vg::Velocity, 0, 4, 100);
    proc.humanize (0, 1.0f, 1.0f);
    bool moved = proc.pattern.get (vg::Nudge, 0, 0) != 0 || proc.pattern.get (vg::Nudge, 0, 4) != 0;
    check (moved && proc.pattern.get (vg::Velocity, 0, 0) > 0 && proc.pattern.get (vg::Velocity, 0, 1) == 0, "humanize jitters timing and velocity of active steps only");
    check (std::abs (proc.pattern.get (vg::Nudge, 0, 0)) <= 25, "humanize stays within +-25 % of a step");
    proc.quantize (-1);
    check (proc.pattern.get (vg::Nudge, 0, 0) == 0 && proc.pattern.get (vg::Nudge, 0, 4) == 0, "quantize clears the nudge");

    // Locks survive a state round trip
    proc.setTrackLocked (7, true);
    juce::MemoryBlock state;
    proc.getStateInformation (state);
    VassaGruuvProcessor other;
    other.setStateInformation (state.getData(), (int) state.getSize());
    check (other.isTrackLocked (7) && ! other.isTrackLocked (6), "locks are saved with the project");
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    testGeneration();

    const auto silent = run (false, false);
    check (silent.mainPeak < 1.0e-6f, "empty pattern is silent");

    const auto mixed = run (false, true);
    check (mixed.mainPeak > 0.05f, "step on track 2 reaches the main mix (peak " + juce::String (mixed.mainPeak, 3) + ")");

    const auto routed = run (true, true);
    check (routed.mainPeak < 1.0e-6f, "routed track leaves the main mix empty");
    check (routed.trackPeak[1] > 0.05f, "routed track 2 plays on its own bus (peak " + juce::String (routed.trackPeak[1], 3) + ")");

    return failures == 0 ? 0 : 1;
}
