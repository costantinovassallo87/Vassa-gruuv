#pragma once

#include "Drums.h"
#include "Sample.h"
#include <memory>

namespace vg
{
// Renders the built-in synth drum for a track into a Sample, so every track starts with a sound.
inline std::shared_ptr<Sample> makeFactorySample (int track)
{
    constexpr double sr = 44100.0;
    DrumVoice v;
    v.prepare (sr);
    v.trigger (track, 1.0f, 0.0f, 1.0f);

    std::vector<float> out;
    out.reserve ((size_t) (sr * 3));
    float l, r;
    while (v.isActive() && out.size() < (size_t) (sr * 3))
    {
        v.render (l, r);
        out.push_back (l);
    }
    while (out.size() < 64)
        out.push_back (0.0f);

    float peak = 1.0e-6f;
    for (auto x : out)
        peak = std::max (peak, std::abs (x));
    for (auto& x : out)
        x *= 0.9f / peak;

    auto s = std::make_shared<Sample>();
    s->data.setSize (1, (int) out.size());
    std::copy (out.begin(), out.end(), s->data.getWritePointer (0));
    s->rate = sr;
    s->name = padInfo (track).name;
    s->factory = true;
    s->detectOnsets();
    return s;
}
} // namespace vg
