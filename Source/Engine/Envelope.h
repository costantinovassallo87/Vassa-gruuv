#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <cmath>

namespace vg
{
// Every track has three ADSR envelopes: amplitude, pitch and filter (the last two scaled by an amount in semitones).
enum EnvKind { EnvAmp = 0, EnvPitch, EnvFilter, numEnvKinds };

inline const char* envName (int kind)
{
    static const char* n[numEnvKinds] = { "Amp", "Pitch", "Filter" };
    return n[juce::jlimit (0, (int) numEnvKinds - 1, kind)];
}

// Level (0..1) of a linear ADSR at time tMs. releaseAtMs < 0 means the note is never released (held / one-shot).
// This is the reference shape: the voice generator below and the waveform display both follow it.
inline float adsrLevel (float tMs, float a, float d, float s, float r, float releaseAtMs)
{
    auto held = [&] (float t)
    {
        if (t < a)
            return t / a;
        if (t < a + d)
            return 1.0f + (s - 1.0f) * (t - a) / d;
        return s;
    };

    if (releaseAtMs < 0.0f || tMs < releaseAtMs)
        return held (tMs);

    const float f = (tMs - releaseAtMs) / juce::jmax (r, 1.0f);
    return f >= 1.0f ? 0.0f : held (releaseAtMs) * (1.0f - f);
}

// Runtime generator, linear segments.
class Adsr
{
public:
    enum State { Idle, Att, Dec, Sus, Rel };
    State st = Idle;
    float level = 0.0f;

    void start (float aMs, float dMs, float s, float rMs, double sr)
    {
        sus = s;
        aInc = aMs <= 0.5f ? 1.0f : 1.0f / (aMs * 0.001f * (float) sr);
        dDec = (1.0f - s) / (juce::jmax (dMs, 1.0f) * 0.001f * (float) sr);
        rLen = juce::jmax (rMs, 1.0f) * 0.001f * (float) sr;
        if (aMs <= 0.5f)
        {
            level = 1.0f;
            st = Dec;
        }
        else
        {
            level = 0.0f;
            st = Att;
        }
    }

    void release()
    {
        if (st == Idle || st == Rel)
            return;
        st = Rel;
        rDec = level / rLen;
    }

    float tick()
    {
        switch (st)
        {
            case Att: level += aInc; if (level >= 1.0f) { level = 1.0f; st = Dec; } break;
            case Dec:
                level -= dDec;
                if (level <= sus)
                {
                    level = sus;
                    st = sus <= 1.0e-4f ? Idle : Sus;
                }
                break;
            case Rel: level -= rDec; if (level <= 0.0f) { level = 0.0f; st = Idle; } break;
            case Idle:
            case Sus: break;
        }
        return level;
    }

private:
    float sus = 1.0f, aInc = 1.0f, dDec = 0.0f, rDec = 0.0f, rLen = 1.0f;
};
} // namespace vg
