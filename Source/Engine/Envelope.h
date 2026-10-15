#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <cmath>

namespace vg
{
// Breakpoint ("line") envelopes, Cubase-sampler style. Each track has one per kind.
enum EnvKind { EnvAmp = 0, EnvPitch, EnvFilter, numEnvKinds };

constexpr int maxEnvPoints = 8;

struct EnvInfo
{
    const char* name;
    float min, max; // level range
    const char* unit;
    bool bipolar;
};

inline const EnvInfo& envInfo (int kind)
{
    static const EnvInfo t[numEnvKinds] = {
        { "Amp",    0.0f,   1.0f,  "",    false },
        { "Pitch",  -24.0f, 24.0f, " st", true  },
        { "Filter", -48.0f, 48.0f, " st", true  },
    };
    return t[juce::jlimit (0, (int) numEnvKinds - 1, kind)];
}

struct EnvPoint
{
    float timeMs = 0.0f;
    float level = 0.0f;
};

// Points are sorted by time; the first point is always at 0 ms.
// While a note is held the envelope stops at the sustain point; on release it plays on through the remaining points.
struct EnvShape
{
    int n = 1;
    int sustain = 0;
    EnvPoint p[maxEnvPoints];

    // Keeps the shape well-formed after any edit.
    void sanitize (int kind)
    {
        const auto& info = envInfo (kind);
        n = juce::jlimit (1, maxEnvPoints, n);
        sustain = juce::jlimit (0, n - 1, sustain);
        p[0].timeMs = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            p[i].level = juce::jlimit (info.min, info.max, p[i].level);
            if (i > 0)
                p[i].timeMs = juce::jmax (p[i].timeMs, p[i - 1].timeMs + 1.0f);
        }
    }

    juce::String toString() const
    {
        juce::String s (n);
        s << "," << sustain;
        for (int i = 0; i < n; ++i)
            s << ";" << p[i].timeMs << ":" << p[i].level;
        return s;
    }

    static EnvShape fromString (const juce::String& text, int kind)
    {
        auto parts = juce::StringArray::fromTokens (text, ";", "");
        auto head = juce::StringArray::fromTokens (parts[0], ",", "");
        EnvShape e;
        e.n = juce::jlimit (1, maxEnvPoints, juce::jmin (head[0].getIntValue(), parts.size() - 1));
        e.sustain = head[1].getIntValue();
        for (int i = 0; i < e.n; ++i)
        {
            auto tl = juce::StringArray::fromTokens (parts[i + 1], ":", "");
            e.p[i] = { tl[0].getFloatValue(), tl[1].getFloatValue() };
        }
        e.sanitize (kind);
        return e;
    }
};

inline EnvShape defaultEnv (int kind)
{
    EnvShape e;
    if (kind == EnvAmp)
    {
        e.n = 2;
        e.p[0] = { 0.0f, 1.0f };
        e.p[1] = { 50.0f, 0.0f }; // release tail
    }
    return e;
}

// Runtime envelope generator.
class LineEnv
{
public:
    float level = 0.0f;
    bool finished = true; // reached its last point (or has a single point)

    void start (const EnvShape& s, double sampleRate)
    {
        shape = s;
        sr = sampleRate;
        seg = 0;
        segPos = 0.0;
        released = false;
        level = startLevel = shape.p[0].level;
        finished = shape.n <= 1;
    }

    void release()
    {
        if (released)
            return;
        released = true;
        if (seg < shape.sustain)
        {
            seg = shape.sustain;
            segPos = 0.0;
            startLevel = level;
        }
    }

    float tick()
    {
        if (seg >= shape.n - 1)
        {
            finished = true;
            return level;
        }
        if (seg == shape.sustain && ! released)
        {
            level = shape.p[seg].level;
            return level;
        }

        const double dur = juce::jmax (1.0, (double) (shape.p[seg + 1].timeMs - shape.p[seg].timeMs) * 0.001 * sr);
        segPos += 1.0;
        const float f = (float) juce::jmin (1.0, segPos / dur);
        level = startLevel + (shape.p[seg + 1].level - startLevel) * f;
        if (segPos >= dur)
        {
            ++seg;
            segPos = 0.0;
            startLevel = level = shape.p[seg].level;
        }
        return level;
    }

private:
    EnvShape shape;
    double sr = 44100.0, segPos = 0.0;
    float startLevel = 0.0f;
    int seg = 0;
    bool released = false;
};
} // namespace vg
