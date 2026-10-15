#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <cstdint>

namespace vg
{
constexpr int numTracks = 16;
constexpr int numSteps = 16;

// Per-step values. Velocity doubles as the on/off flag of a step (0 = off).
enum Lane { Velocity = 0, Length, Pitch, Repeat, Nudge, Chance, Slice, numLanes };

struct LaneInfo
{
    const char* name;
    int min, max, def;
    const char* unit;
    bool bipolar;
};

inline const LaneInfo& laneInfo (int lane)
{
    static const LaneInfo table[numLanes] = {
        { "Velocity", 0,   127, 100, "",    false },
        { "Length",   10,  400, 100, " %",  false },
        { "Pitch",    -24, 24,  0,   " st", true  },
        { "Repeat",   1,   8,   1,   "x",   false },
        { "Nudge",    -50, 50,  0,   " %",  true  },
        { "Chance",   0,   100, 100, " %",  false },
        { "Slice",    0,   32,  0,   "",    false }, // 0 = auto (step order)
    };
    return table[juce::jlimit (0, (int) numLanes - 1, lane)];
}

using PatternGrid = std::array<std::array<uint8_t, numSteps>, numTracks>;

// Step data shared between the UI/AI threads (writers) and the audio thread (reader).
class Pattern
{
public:
    Pattern() { clear(); }

    int get (int lane, int track, int step) const noexcept { return cells[index (lane, track, step)].load (std::memory_order_relaxed); }

    void set (int lane, int track, int step, int v) noexcept
    {
        const auto& li = laneInfo (lane);
        cells[index (lane, track, step)].store ((int16_t) juce::jlimit (li.min, li.max, v), std::memory_order_relaxed);
    }

    bool isNonDefault (int lane, int track, int step) const noexcept { return get (lane, track, step) != resetValue (lane); }

    static int resetValue (int lane) noexcept { return lane == Velocity ? 0 : laneInfo (lane).def; }

    void clearTrackMods (int track) noexcept
    {
        for (int l = 1; l < numLanes; ++l)
            for (int s = 0; s < numSteps; ++s)
                set (l, track, s, resetValue (l));
    }

    void clearMods() noexcept
    {
        for (int l = 1; l < numLanes; ++l)
            for (int t = 0; t < numTracks; ++t)
                for (int s = 0; s < numSteps; ++s)
                    set (l, t, s, resetValue (l));
    }

    void clear() noexcept
    {
        for (int l = 0; l < numLanes; ++l)
            for (int t = 0; t < numTracks; ++t)
                for (int s = 0; s < numSteps; ++s)
                    set (l, t, s, resetValue (l));
    }

    bool trackHasSteps (int track) const noexcept
    {
        for (int s = 0; s < numSteps; ++s)
            if (get (Velocity, track, s) > 0)
                return true;
        return false;
    }

    PatternGrid snapshotVelocities() const
    {
        PatternGrid g {};
        for (int t = 0; t < numTracks; ++t)
            for (int s = 0; s < numSteps; ++s)
                g[(size_t) t][(size_t) s] = (uint8_t) get (Velocity, t, s);
        return g;
    }

    void assignVelocities (const PatternGrid& g) noexcept
    {
        for (int t = 0; t < numTracks; ++t)
            for (int s = 0; s < numSteps; ++s)
                set (Velocity, t, s, g[(size_t) t][(size_t) s]);
    }

    // 4 hex digits per value (value + 1000), lane-major.
    juce::String toHex() const
    {
        juce::String out;
        out.preallocateBytes ((size_t) hexLength() + 8);
        for (int l = 0; l < numLanes; ++l)
            for (int t = 0; t < numTracks; ++t)
                for (int s = 0; s < numSteps; ++s)
                    out << juce::String::toHexString (get (l, t, s) + 1000).paddedLeft ('0', 4);
        return out;
    }

    bool fromHex (const juce::String& hex)
    {
        if (hex.length() != hexLength())
            return false;
        int i = 0;
        for (int l = 0; l < numLanes; ++l)
            for (int t = 0; t < numTracks; ++t)
                for (int s = 0; s < numSteps; ++s, ++i)
                    set (l, t, s, hex.substring (i * 4, i * 4 + 4).getHexValue32() - 1000);
        return true;
    }

private:
    static constexpr int hexLength() { return numLanes * numTracks * numSteps * 4; }
    static size_t index (int lane, int track, int step) noexcept { return (size_t) ((lane * numTracks + track) * numSteps + step); }

    std::array<std::atomic<int16_t>, (size_t) (numLanes * numTracks * numSteps)> cells;
};
} // namespace vg
