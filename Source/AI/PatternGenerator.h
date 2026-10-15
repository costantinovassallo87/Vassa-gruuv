#pragma once

#include "Engine/Pattern.h"
#include <optional>

namespace vg
{
struct GeneratedGroove
{
    PatternGrid steps {};
    std::optional<double> bpm;
    std::optional<float> swing; // 0..1
    juce::String description;
};

using TrackRow = std::array<uint8_t, numSteps>;

// Offline generator: per-genre pools of pattern variants, assembled per track. No network needed.
namespace PatternGenerator
{
    // All genre names followed by "???" (a hybrid that mixes every genre, track by track).
    juce::StringArray styleNames();
    int hybridIndex();

    GeneratedGroove generate (int styleIndex, float density, juce::Random& rng);

    // A fresh pattern for a single track, in the given genre. Falls back to a generic role-based pattern
    // when the genre has no part for that track.
    TrackRow generateTrack (int styleIndex, int track, float density, juce::Random& rng);

    void mutate (PatternGrid& grid, float amount, juce::Random& rng);
}
} // namespace vg
