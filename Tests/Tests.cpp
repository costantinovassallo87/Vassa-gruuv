#include "AI/ClaudeClient.h"
#include "AI/PatternGenerator.h"
#include "Engine/Exporter.h"
#include "Engine/Factory.h"
#include "Engine/Sampler.h"
#include "Engine/Scheduler.h"

using namespace vg;

namespace
{
std::shared_ptr<const Sample> makeSine (int frames = 44100, double rate = 44100.0)
{
    auto s = std::make_shared<Sample>();
    s->data.setSize (1, frames);
    for (int i = 0; i < frames; ++i)
        s->data.setSample (0, i, 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * 220.0f * (float) i / (float) rate));
    s->rate = rate;
    s->name = "sine";
    return s;
}

// Renders one voice for n samples, returns peak and checks finiteness.
struct Rendered { float peak = 0; bool finite = true; bool stillActive = false; std::vector<float> l; };

Rendered renderVoice (const TrackParams& p, std::shared_ptr<const Sample> s, int n, int gate = -1, int slice = 0, float pitch = 0)
{
    SamplerVoice v;
    v.start (p, std::move (s), 1.0f, pitch, slice, gate, 44100.0);
    Rendered r;
    std::vector<float> L ((size_t) n, 0.0f), R ((size_t) n, 0.0f);
    v.render (L.data(), R.data(), n);
    for (int i = 0; i < n; ++i)
    {
        r.finite &= std::isfinite (L[(size_t) i]) && std::isfinite (R[(size_t) i]);
        r.peak = std::max (r.peak, std::abs (L[(size_t) i]));
    }
    r.stillActive = v.active;
    r.l = std::move (L);
    return r;
}

float peakRange (const std::vector<float>& x, int a, int b)
{
    float m = 0;
    for (int i = a; i < b && i < (int) x.size(); ++i)
        m = std::max (m, std::abs (x[(size_t) i]));
    return m;
}
} // namespace

struct VassaTests : public juce::UnitTest
{
    VassaTests() : juce::UnitTest ("VassaGruuv", "vg") {}

    void runTest() override
    {
        beginTest ("Pattern hex round trip (all lanes)");
        {
            Pattern a, b;
            a.set (Velocity, 3, 5, 99);
            a.set (Nudge, 3, 5, -25);
            a.set (Repeat, 15, 15, 4);
            a.set (Length, 0, 0, 400);
            expect (b.fromHex (a.toHex()));
            expectEquals (b.get (Velocity, 3, 5), 99);
            expectEquals (b.get (Nudge, 3, 5), -25);
            expectEquals (b.get (Repeat, 15, 15), 4);
            expectEquals (b.get (Length, 0, 0), 400);
            expectEquals (b.get (Chance, 7, 7), 100);
            expect (! b.fromHex ("zz"));
            a.set (Pitch, 0, 0, 999);
            expectEquals (a.get (Pitch, 0, 0), 24); // clamped
        }

        beginTest ("Generator + mutate stay valid");
        {
            juce::Random rng (1234);
            for (int s = 0; s < PatternGenerator::styleNames().size(); ++s)
            {
                auto g = PatternGenerator::generate (s, 0.5f, rng);
                expect (g.steps[0][0] > 0, "style " + juce::String (s));
                for (int i = 0; i < 20; ++i)
                    PatternGenerator::mutate (g.steps, 1.0f, rng);
                for (auto& row : g.steps)
                    for (auto v : row)
                        expect (v <= 127);
            }
        }

        beginTest ("parseGrooveJson accepts fenced / chatty replies");
        {
            GeneratedGroove g;
            juce::String err;
            const juce::String reply = "Sure!\n```json\n{\"bpm\": 128, \"swing\": 0.2, \"description\": \"test\", "
                                       "\"pattern\": [[127,0,0,0,100,0,0,0,127,0,0,0,100,0,0,0],[0,1,2]]}\n```";
            expect (parseGrooveJson (reply, g, err), err);
            expectEquals ((int) g.steps[0][4], 100);
            expectEquals ((int) g.steps[1][2], 2);
            expect (g.bpm.has_value() && std::abs (*g.bpm - 128.0) < 1.0e-6);
            expect (! parseGrooveJson ("no json here", g, err));
            expect (! parseGrooveJson ("{\"bpm\": 100}", g, err));
        }

        beginTest ("Factory samples are audible, finite and have a tail");
        {
            for (int t = 0; t < numTracks; ++t)
            {
                auto s = makeFactorySample (t);
                expect (s->numFrames() >= 64);
                float peak = 0;
                for (int i = 0; i < s->numFrames(); ++i)
                {
                    const float x = s->data.getSample (0, i);
                    expect (std::isfinite (x));
                    peak = std::max (peak, std::abs (x));
                }
                expect (peak > 0.5f, "factory " + juce::String (t));
            }
        }

        beginTest ("Scheduler: plain steps, repeat, nudge, chance");
        {
            Pattern pat;
            Trig trigs[256];
            const double bpm = 120.0, sr = 48000.0;

            pat.set (Velocity, 0, 0, 100);
            pat.set (Velocity, 0, 4, 100);
            int n = collectTrigs (pat, 0.0, 4.0, bpm, 0.0f, sr, trigs, 256);
            expectEquals (n, 2);

            pat.set (Repeat, 0, 4, 4);
            n = collectTrigs (pat, 0.0, 4.0, bpm, 0.0f, sr, trigs, 256);
            expectEquals (n, 5); // 1 + 4 ratchets

            // Block splitting must not drop or duplicate triggers
            int total = 0;
            for (double p = 0.0; p < 4.0; p += 0.07)
                total += collectTrigs (pat, p, juce::jmin (4.0, p + 0.07), bpm, 0.0f, sr, trigs, 256);
            expectEquals (total, 5);

            pat.set (Nudge, 0, 0, 50);
            n = collectTrigs (pat, 0.0, 0.25, bpm, 0.0f, sr, trigs, 256);
            expectEquals (n, 1);
            expectEquals (trigs[0].pos, (int) (0.125 / (bpm / 60.0 / sr))); // +50% of a 16th

            pat.clear();
            pat.set (Velocity, 1, 2, 100);
            pat.set (Chance, 1, 2, 0);
            n = collectTrigs (pat, 0.0, 4.0, bpm, 0.0f, sr, trigs, 256);
            expectEquals (n, 0);

            pat.set (Chance, 1, 2, 50);
            int hits = 0;
            for (int bar = 0; bar < 200; ++bar)
                hits += collectTrigs (pat, bar * 4.0, bar * 4.0 + 4.0, bpm, 0.0f, sr, trigs, 256);
            expect (hits > 60 && hits < 140, "chance 50% gave " + juce::String (hits) + "/200");

            pat.clear();
            pat.set (Velocity, 2, 1, 100);
            pat.set (Pitch, 2, 1, 7);
            pat.set (Slice, 2, 1, 3);
            pat.set (Length, 2, 1, 50);
            n = collectTrigs (pat, 0.0, 4.0, bpm, 0.0f, sr, trigs, 256);
            expectEquals (n, 1);
            expectEquals ((int) trigs[0].pitch, 7);
            expectEquals (trigs[0].slice, 2);
            expectEquals (trigs[0].gate, (int) (0.5 * 0.25 / (bpm / 60.0 / sr)));

            pat.set (Slice, 2, 1, 0);
            n = collectTrigs (pat, 0.0, 4.0, bpm, 0.0f, sr, trigs, 256);
            expectEquals (trigs[0].slice, 1); // auto = step order

            pat.clear();
            pat.set (Velocity, 0, 1, 100);
            n = collectTrigs (pat, 0.0, 4.0, bpm, 0.0f, sr, trigs, 256);
            const int straight = trigs[0].pos;
            n = collectTrigs (pat, 0.0, 4.0, bpm, 1.0f, sr, trigs, 256);
            expect (trigs[0].pos > straight, "swing delays odd steps");
        }

        beginTest ("Sampler: one-shot plays to the end, classic honours the gate, loop sustains");
        {
            auto sine = makeSine();
            TrackParams p;
            p.v[TPMode] = ModeOneShot;
            auto oneShot = renderVoice (p, sine, 22050);
            expect (oneShot.finite && oneShot.peak > 0.3f);
            expect (oneShot.stillActive, "one-shot still playing at 0.5 s");
            expect (! renderVoice (p, sine, 60000).stillActive, "one-shot ended after sample");

            p.v[TPMode] = ModeClassic;
            p.env[EnvAmp] = EnvShape();
            p.env[EnvAmp].n = 2;
            p.env[EnvAmp].p[0] = { 0.0f, 1.0f };
            p.env[EnvAmp].p[1] = { 20.0f, 0.0f };
            auto gated = renderVoice (p, sine, 30000, 4410); // gate 0.1 s
            expect (! gated.stillActive, "classic released after gate");
            expect (peakRange (gated.l, 10000, 30000) < 0.01f, "silent after the release");

            p.v[TPLoop] = 1.0f;
            p.v[TPLoopStart] = 0.1f;
            p.v[TPLoopEnd] = 0.2f;
            auto looped = renderVoice (p, sine, 100000, -1);
            expect (looped.stillActive, "loop keeps the voice alive past the sample end");
            expect (peakRange (looped.l, 90000, 100000) > 0.1f, "loop still audible");
            expect (looped.finite);
        }

        beginTest ("Sampler: start/end, reverse, transpose, filter");
        {
            auto sine = makeSine();
            TrackParams p;
            p.v[TPMode] = ModeOneShot;
            p.v[TPStart] = 0.25f;
            p.v[TPEnd] = 0.5f;
            auto r = renderVoice (p, sine, 30000);
            expect (! r.stillActive, "region of 0.25 s ends");
            expect (peakRange (r.l, 12000, 30000) < 0.001f);

            p = TrackParams();
            p.v[TPMode] = ModeOneShot;
            p.v[TPTranspose] = 12.0f;
            auto up = renderVoice (p, sine, 30000);
            expect (! up.stillActive, "octave up plays twice as fast");

            p.v[TPTranspose] = 0.0f;
            p.v[TPReverse] = 1.0f;
            expect (renderVoice (p, sine, 2000).peak > 0.1f);

            p.v[TPReverse] = 0.0f;
            p.v[TPFType] = 1.0f; // LP
            p.v[TPFCut] = 100.0f;
            auto dark = renderVoice (p, sine, 8000);
            p.v[TPFType] = 0.0f;
            auto clean = renderVoice (p, sine, 8000);
            expect (dark.peak < clean.peak * 0.5f, "lowpass at 100 Hz attenuates a 220 Hz sine");
            expect (dark.finite);
        }

        beginTest ("Line envelopes: shape, sustain/release, serialisation");
        {
            EnvShape e;
            e.n = 3;
            e.sustain = 1;
            e.p[0] = { 0.0f, 0.0f };
            e.p[1] = { 10.0f, 1.0f };
            e.p[2] = { 30.0f, 0.0f };

            LineEnv env;
            env.start (e, 1000.0); // 1 sample = 1 ms
            float v = 0;
            for (int i = 0; i < 10; ++i) v = env.tick();
            expect (std::abs (v - 1.0f) < 0.01f, "attack reaches the sustain point");
            for (int i = 0; i < 50; ++i) v = env.tick();
            expect (std::abs (v - 1.0f) < 0.01f, "holds while the note is held");
            env.release();
            for (int i = 0; i < 40; ++i) v = env.tick();
            expect (v < 0.01f && env.finished, "release plays the remaining points");

            env.start (e, 1000.0);
            for (int i = 0; i < 4; ++i) env.tick();
            env.release(); // released before reaching the sustain point
            for (int i = 0; i < 40; ++i) v = env.tick();
            expect (v < 0.01f, "early release still decays");

            auto round = EnvShape::fromString (e.toString(), EnvAmp);
            expectEquals (round.n, 3);
            expectEquals (round.sustain, 1);
            expect (std::abs (round.p[2].timeMs - 30.0f) < 1.0e-3f);

            EnvShape bad;
            bad.n = 3;
            bad.p[0] = { 50.0f, 9.0f };
            bad.p[1] = { 10.0f, -3.0f };
            bad.p[2] = { 5.0f, 0.5f };
            bad.sanitize (EnvAmp);
            expect (bad.p[0].timeMs == 0.0f && bad.p[1].timeMs > bad.p[0].timeMs && bad.p[2].timeMs > bad.p[1].timeMs);
            expect (bad.p[0].level <= 1.0f && bad.p[1].level >= 0.0f);
        }

        beginTest ("Amp / pitch / filter envelopes affect the voice");
        {
            auto sine = makeSine();
            TrackParams p;
            p.v[TPMode] = ModeClassic;
            p.env[EnvAmp].n = 2;
            p.env[EnvAmp].p[0] = { 0.0f, 1.0f };
            p.env[EnvAmp].p[1] = { 200.0f, 0.0f };
            p.env[EnvAmp].sustain = 0;
            auto fadeOut = renderVoice (p, sine, 22050, 3000); // released after ~68 ms, tail 200 ms
            expect (peakRange (fadeOut.l, 0, 2000) > 0.3f);
            expect (peakRange (fadeOut.l, 14000, 22050) < 0.01f, "amp envelope tail silences the voice after the gate");

            // Pitch envelope: +12 st at the start makes the sample end sooner than flat
            p = TrackParams();
            p.v[TPMode] = ModeOneShot;
            p.env[EnvPitch].n = 2;
            p.env[EnvPitch].p[0] = { 0.0f, 24.0f };
            p.env[EnvPitch].p[1] = { 100.0f, 0.0f };
            p.env[EnvPitch].sustain = 1;
            auto sweep = renderVoice (p, sine, 44100 + 4000);
            expect (! sweep.stillActive && sweep.finite);

            // Filter envelope: closed filter opened by the envelope is louder than closed without it
            p = TrackParams();
            p.v[TPMode] = ModeOneShot;
            p.v[TPFType] = 1.0f;
            p.v[TPFCut] = 50.0f;
            auto closed = renderVoice (p, sine, 6000);
            p.env[EnvFilter].n = 1;
            p.env[EnvFilter].p[0] = { 0.0f, 48.0f };
            auto opened = renderVoice (p, sine, 6000);
            expect (opened.peak > closed.peak * 3.0f, "filter envelope opens the cutoff");
        }

        beginTest ("Slice mode picks the right region");
        {
            auto s = std::make_shared<Sample>();
            s->data.setSize (1, 40000);
            s->rate = 44100.0;
            s->data.clear();
            for (int i = 20000; i < 40000; ++i) // only the second half has sound
                s->data.setSample (0, i, 0.5f * std::sin (0.1f * (float) i));
            TrackParams p;
            p.v[TPMode] = ModeSlice;
            p.v[TPSMode] = 0;
            p.v[TPSCount] = 4;
            expectEquals (numSlices (*s, p), 4);
            expect (renderVoice (p, s, 4000, -1, 0).peak < 0.001f, "slice 0 is silent");
            expect (renderVoice (p, s, 4000, -1, 3).peak > 0.2f, "slice 3 is loud");
            expect (renderVoice (p, s, 4000, -1, 7).peak > 0.2f, "slice index wraps (7 -> 3)");
        }

        beginTest ("Transient detection finds hits");
        {
            auto s = std::make_shared<Sample>();
            s->data.setSize (1, 44100 * 2);
            s->data.clear();
            for (int hit : { 0, 11025, 22050, 33075 })
                for (int i = 0; i < 4000; ++i)
                    s->data.setSample (0, hit + i, 0.8f * std::exp (-(float) i / 800.0f) * std::sin (0.3f * (float) i));
            s->detectOnsets();
            expect (s->onsets.size() >= 2, "found " + juce::String ((int) s->onsets.size()) + " onsets");
            TrackParams p;
            p.v[TPSMode] = 1;
            p.v[TPSSens] = 1.0f;
            expect (numSlices (*s, p) >= 3);
        }

        beginTest ("Exporter writes one looping stem per active track");
        {
            Pattern pat;
            ExportInput in;
            in.pattern = &pat;
            in.bpm = 120.0;
            in.sampleRate = 44100.0;
            for (int t = 0; t < numTracks; ++t)
            {
                in.samples[(size_t) t] = makeFactorySample (t);
                in.names[(size_t) t] = padInfo (t).name;
            }
            for (int s = 0; s < 16; s += 4)
                pat.set (Velocity, 0, s, 110);
            pat.set (Velocity, 2, 4, 110);
            pat.set (Velocity, 5, 0, 110);
            in.params[5].v[TPMute] = 1.0f; // muted: no file

            const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vg_test_export");
            dir.deleteRecursively();
            const auto files = exportStems (in, 2, dir, "Test");
            expectEquals (files.size(), 2);

            juce::AudioFormatManager fm;
            fm.registerBasicFormats();
            for (const auto& f : files)
            {
                std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
                expect (r != nullptr);
                if (r == nullptr)
                    continue;
                expectEquals ((int) r->lengthInSamples, (int) std::ceil (8.0 / (120.0 / 60.0 / 44100.0))); // 2 bars
                juce::AudioBuffer<float> b (2, (int) r->lengthInSamples);
                r->read (&b, 0, (int) r->lengthInSamples, 0, true, true);
                expect (b.getMagnitude (0, b.getNumSamples()) > 0.2f, f.getFileName());
            }
            dir.deleteRecursively();
        }
    }
};

static VassaTests vassaTests;

int main()
{
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests();

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult (i)->failures;
    return failures == 0 ? 0 : 1;
}
