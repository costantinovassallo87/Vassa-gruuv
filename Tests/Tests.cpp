#include "AI/ClaudeClient.h"
#include "AI/PatternGenerator.h"
#include "Engine/Drums.h"

using namespace vg;

struct VassaTests : public juce::UnitTest
{
    VassaTests() : juce::UnitTest ("VassaGruuv", "vg") {}

    void runTest() override
    {
        beginTest ("Pattern hex round trip");
        {
            Pattern a, b;
            a.set (3, 5, 99);
            a.set (15, 15, 127);
            expect (b.fromHex (a.toHex()));
            expectEquals ((int) b.get (3, 5), 99);
            expectEquals ((int) b.get (15, 15), 127);
            expectEquals ((int) b.get (0, 0), 0);
            expect (! b.fromHex ("zz"));
        }

        beginTest ("Generator: every style has a downbeat kick and valid velocities");
        {
            juce::Random rng (1234);
            for (int s = 0; s < PatternGenerator::styleNames().size(); ++s)
            {
                const auto g = PatternGenerator::generate (s, 0.5f, rng);
                expect (g.steps[0][0] > 0, "style " + juce::String (s));
                expect (g.bpm.has_value());
                for (auto& row : g.steps)
                    for (auto v : row)
                        expect (v <= 127);
            }
        }

        beginTest ("Mutate keeps velocities in range");
        {
            juce::Random rng (7);
            auto g = PatternGenerator::generate (0, 1.0f, rng);
            for (int i = 0; i < 50; ++i)
                PatternGenerator::mutate (g.steps, 1.0f, rng);
            for (auto& row : g.steps)
                for (auto v : row)
                    expect (v <= 127);
        }

        beginTest ("parseGrooveJson accepts fenced / chatty replies");
        {
            GeneratedGroove g;
            juce::String err;
            const juce::String reply = "Sure!\n```json\n{\"bpm\": 128, \"swing\": 0.2, \"description\": \"test\", "
                                       "\"pattern\": [[127,0,0,0,100,0,0,0,127,0,0,0,100,0,0,0],[0,1,2]]}\n```";
            expect (parseGrooveJson (reply, g, err), err);
            expectEquals ((int) g.steps[0][0], 127);
            expectEquals ((int) g.steps[0][4], 100);
            expectEquals ((int) g.steps[1][2], 2);
            expectEquals ((int) g.steps[1][3], 0);
            expect (g.bpm.has_value() && *g.bpm == 128.0);
            expectEquals (g.description, juce::String ("test"));

            expect (! parseGrooveJson ("no json here", g, err));
            expect (! parseGrooveJson ("{\"bpm\": 100}", g, err));
        }

        beginTest ("Every pad renders finite, non-silent audio that decays");
        {
            for (int pad = 0; pad < numPads; ++pad)
            {
                DrumVoice v;
                v.prepare (48000.0);
                v.trigger (pad, 1.0f, 0.0f, 1.0f, nullptr);
                float peak = 0.0f, l, r;
                bool finite = true;
                for (int i = 0; i < 48000 * 6 && v.isActive(); ++i)
                {
                    v.render (l, r);
                    finite &= std::isfinite (l) && std::isfinite (r);
                    peak = juce::jmax (peak, std::abs (l));
                }
                expect (finite, padInfo (pad).name);
                expect (peak > 0.05f, juce::String (padInfo (pad).name) + " is silent");
                expect (peak < 2.0f, juce::String (padInfo (pad).name) + " is too loud");
                expect (! v.isActive(), juce::String (padInfo (pad).name) + " never ends");
            }
        }

        beginTest ("Choke silences a voice quickly");
        {
            DrumVoice v;
            v.prepare (48000.0);
            v.trigger (6, 1.0f, 0.0f, 1.0f, nullptr); // open hat
            float l, r;
            for (int i = 0; i < 100; ++i) v.render (l, r);
            v.choke();
            for (int i = 0; i < 4800; ++i) v.render (l, r);
            expect (! v.isActive());
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
