#pragma once

#include "Pattern.h"
#include <cmath>
#include <cstdint>

namespace vg
{
struct Trig
{
    int pos = 0;       // sample offset inside the block
    int track = 0;
    float vel = 1.0f;
    float pitch = 0.0f; // semitones
    int slice = 0;
    int gate = -1;     // samples; -1 = held until a noteOff trig
    int step = 0;
    bool noteOff = false;
};

inline uint32_t hash32 (uint64_t x)
{
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return (uint32_t) x;
}

constexpr double stepPpq = 0.25; // one 16th note

// Collects all triggers (including repeats / nudge / chance) that start inside [ppq0, ppq1).
// Chance is a deterministic hash of (absolute step, track) so a trig gets the same verdict in every block.
inline int collectTrigs (const Pattern& pat, double ppq0, double ppq1, double bpm, float swing, double sr, Trig* out, int cap)
{
    const double ppqPerSample = bpm / 60.0 / sr;
    int count = 0;

    for (long long k = (long long) std::floor (ppq0 / stepPpq) - 2;; ++k)
    {
        const double base = (double) k * stepPpq + ((k & 1) ? (double) swing * stepPpq / 3.0 : 0.0);
        if (base - 0.5 * stepPpq >= ppq1)
            break;

        const int step = (int) (((k % numSteps) + numSteps) % numSteps);
        for (int t = 0; t < numTracks; ++t)
        {
            const int vel = pat.get (Velocity, t, step);
            if (vel <= 0)
                continue;

            const int chance = pat.get (Chance, t, step);
            if (chance < 100 && (int) (hash32 ((uint64_t) (k + 1000000) * 64u + (uint64_t) t) % 100u) >= chance)
                continue;

            const int repeats = pat.get (Repeat, t, step);
            const double nudged = base + (double) pat.get (Nudge, t, step) / 100.0 * stepPpq;
            const double lenPpq = (double) pat.get (Length, t, step) / 100.0 * stepPpq / repeats;

            for (int r = 0; r < repeats; ++r)
            {
                const double tt = nudged + r * stepPpq / repeats;
                if (tt < ppq0 || tt >= ppq1 || count >= cap)
                    continue;

                Trig& e = out[count++];
                e.pos = (int) ((tt - ppq0) / ppqPerSample);
                e.track = t;
                e.vel = (float) vel / 127.0f;
                e.pitch = (float) pat.get (Pitch, t, step);
                e.slice = pat.get (Slice, t, step) == 0 ? step : pat.get (Slice, t, step) - 1;
                e.gate = juce::jmax (1, (int) (lenPpq / ppqPerSample));
                e.step = step;
                e.noteOff = false;
            }
        }
    }
    return count;
}
} // namespace vg
