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

// Offline, instant "AI": probabilistic style templates + mutation. No network needed.
namespace PatternGenerator
{
    juce::StringArray styleNames();
    GeneratedGroove generate (int styleIndex, float density, juce::Random& rng);
    void mutate (PatternGrid& grid, float amount, juce::Random& rng);
}
} // namespace vg
