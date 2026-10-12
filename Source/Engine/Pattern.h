#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <cstdint>

namespace vg
{
constexpr int numPads = 16;
constexpr int numSteps = 16;

using PatternGrid = std::array<std::array<uint8_t, numSteps>, numPads>;

// Step grid shared between the UI/AI threads (writers) and the audio thread (reader).
// 0 = step off, 1..127 = velocity.
class Pattern
{
public:
    Pattern() { clear(); }

    uint8_t get (int pad, int step) const noexcept { return cells[(size_t) pad][(size_t) step].load (std::memory_order_relaxed); }
    void set (int pad, int step, uint8_t v) noexcept { cells[(size_t) pad][(size_t) step].store (v, std::memory_order_relaxed); }

    void clear() noexcept
    {
        for (auto& row : cells)
            for (auto& c : row)
                c.store (0, std::memory_order_relaxed);
    }

    PatternGrid snapshot() const
    {
        PatternGrid g {};
        for (int p = 0; p < numPads; ++p)
            for (int s = 0; s < numSteps; ++s)
                g[(size_t) p][(size_t) s] = get (p, s);
        return g;
    }

    void assign (const PatternGrid& g) noexcept
    {
        for (int p = 0; p < numPads; ++p)
            for (int s = 0; s < numSteps; ++s)
                set (p, s, juce::jmin<uint8_t> (127, g[(size_t) p][(size_t) s]));
    }

    juce::String toHex() const
    {
        static const char* digits = "0123456789abcdef";
        juce::String out;
        for (int p = 0; p < numPads; ++p)
            for (int s = 0; s < numSteps; ++s)
            {
                const auto v = get (p, s);
                out << juce::String::charToString (digits[v >> 4]) << juce::String::charToString (digits[v & 15]);
            }
        return out;
    }

    bool fromHex (const juce::String& hex)
    {
        if (hex.length() != numPads * numSteps * 2)
            return false;
        for (int i = 0; i < numPads * numSteps; ++i)
            set (i / numSteps, i % numSteps, (uint8_t) juce::jmin (127, hex.substring (i * 2, i * 2 + 2).getHexValue32()));
        return true;
    }

private:
    std::array<std::array<std::atomic<uint8_t>, numSteps>, numPads> cells;
};
} // namespace vg
