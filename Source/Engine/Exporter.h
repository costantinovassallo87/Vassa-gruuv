#pragma once

#include "Sampler.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include "Scheduler.h"
#include <array>

namespace vg
{
struct ExportInput
{
    const Pattern* pattern = nullptr;
    std::array<TrackParams, numTracks> params;
    std::array<std::shared_ptr<const Sample>, numTracks> samples;
    std::array<juce::String, numTracks> names;
    double bpm = 120.0;
    float swing = 0.0f;
    double sampleRate = 44100.0;
};

// Renders the pattern offline, one stereo WAV per audible track with steps. The reverb tail is folded back onto
// the start so each stem loops seamlessly. Returns the written files.
juce::Array<juce::File> exportStems (const ExportInput& in, int loops, const juce::File& dir, const juce::String& baseName);
} // namespace vg
