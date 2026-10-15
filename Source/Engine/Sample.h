#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace vg
{
struct Onset
{
    float pos;      // normalised 0..1
    float strength; // normalised 0..1
};

struct Sample
{
    juce::AudioBuffer<float> data; // 1 or 2 channels
    double rate = 44100.0;
    juce::String name, path;
    bool factory = false;
    std::vector<Onset> onsets; // by position, used by transient slicing

    int numFrames() const noexcept { return data.getNumSamples(); }

    // Energy-rise onset detection on a coarse envelope.
    void detectOnsets()
    {
        onsets.clear();
        const int n = numFrames();
        const int hop = 256;
        const int nh = n / hop;
        if (nh < 8)
            return;

        std::vector<float> env ((size_t) nh);
        float maxEnv = 1.0e-9f;
        for (int h = 0; h < nh; ++h)
        {
            double e = 0.0;
            for (int c = 0; c < data.getNumChannels(); ++c)
            {
                const float* x = data.getReadPointer (c) + h * hop;
                for (int i = 0; i < hop; ++i)
                    e += (double) x[i] * x[i];
            }
            env[(size_t) h] = (float) std::sqrt (e / (hop * data.getNumChannels()));
            maxEnv = std::max (maxEnv, env[(size_t) h]);
        }

        std::vector<float> flux ((size_t) nh, 0.0f);
        float maxFlux = 1.0e-9f;
        for (int h = 1; h < nh; ++h)
        {
            if (env[(size_t) h] < 0.02f * maxEnv)
                continue;
            const float f = std::max (0.0f, std::log (env[(size_t) h] + 1.0e-4f) - std::log (env[(size_t) h - 1] + 1.0e-4f));
            flux[(size_t) h] = f;
            maxFlux = std::max (maxFlux, f);
        }

        int last = -100;
        for (int h = 1; h < nh; ++h)
        {
            const float f = flux[(size_t) h];
            if (f < 0.25f || h - last < 8)
                continue;
            bool peak = true;
            for (int k = std::max (1, h - 4); k <= std::min (nh - 1, h + 4); ++k)
                if (flux[(size_t) k] > f)
                    peak = false;
            if (! peak)
                continue;
            const float pos = (float) (h * hop) / (float) n;
            if (pos < 0.005f)
                continue;
            onsets.push_back ({ pos, std::min (1.0f, f / std::min (maxFlux, 4.0f)) });
            last = h;
        }

        if (onsets.size() > 64) // keep the strongest, back in positional order
        {
            std::sort (onsets.begin(), onsets.end(), [] (const Onset& a, const Onset& b) { return a.strength > b.strength; });
            onsets.resize (64);
            std::sort (onsets.begin(), onsets.end(), [] (const Onset& a, const Onset& b) { return a.pos < b.pos; });
        }
    }
};
} // namespace vg
