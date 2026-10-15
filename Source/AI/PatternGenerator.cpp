#include "PatternGenerator.h"
#include <array>

namespace vg::PatternGenerator
{
namespace
{
// Each lane is 16 digits, one per step: 0 = never, 9 = always, 1..8 = probability (scaled by density).
struct Lane { int pad; const char* steps; };

struct Style
{
    const char* name;
    double bpm;
    float swing;
    std::array<Lane, 8> lanes;
};

const std::array<Style, 6>& styles()
{
    static const std::array<Style, 6> s = { {
        { "House", 124, 0.0f, { { { 0, "9000900090009000" }, { 3, "0000900000009000" }, { 5, "6060606060606060" },
                                  { 6, "0090009000900090" }, { 11, "3535353535353535" }, { 4, "0000000200000002" },
                                  { 12, "0000200000020000" }, { -1, "" } } } },
        { "Techno", 132, 0.0f, { { { 0, "9000900090009000" }, { 6, "0090009000900090" }, { 5, "5656565656565656" },
                                    { 3, "0000800000008000" }, { 4, "0004000300040003" }, { 13, "0020000200200010" },
                                    { 10, "1000000000000000" }, { -1, "" } } } },
        { "Hip-Hop", 90, 0.5f, { { { 0, "9000002000900200" }, { 2, "0000900000009000" }, { 5, "9090909090909090" },
                                    { 6, "0000000000000030" }, { 4, "0000000000020000" }, { 3, "0000300000003000" },
                                    { -1, "" }, { -1, "" } } } },
        { "Trap", 140, 0.0f, { { { 0, "9000000200900000" }, { 2, "0000000090000000" }, { 3, "0000000090000000" },
                                  { 5, "9989889989988998" }, { 6, "0000002000000020" }, { 1, "0000000000000200" },
                                  { 11, "0000000000000000" }, { -1, "" } } } },
        { "Drum & Bass", 174, 0.0f, { { { 0, "9000000000900000" }, { 2, "0000900000009000" }, { 5, "9090909090909090" },
                                         { 1, "0000000020000200" }, { 6, "0000000000000010" }, { 4, "0200000000020000" },
                                         { -1, "" }, { -1, "" } } } },
        { "Funk", 104, 0.3f, { { { 0, "9002003009020030" }, { 2, "0000900200009002" }, { 5, "9999999999999999" },
                                  { 6, "0000000000200000" }, { 9, "0000000000000300" }, { 4, "0003000000030000" },
                                  { 11, "0505050505050505" }, { -1, "" } } } },
    } };
    return s;
}
} // namespace

juce::StringArray styleNames()
{
    juce::StringArray names;
    for (auto& s : styles())
        names.add (s.name);
    return names;
}

GeneratedGroove generate (int styleIndex, float density, juce::Random& rng)
{
    const auto& st = styles()[(size_t) juce::jlimit (0, (int) styles().size() - 1, styleIndex)];
    density = juce::jlimit (0.0f, 1.0f, density);

    GeneratedGroove g;
    for (const auto& lane : st.lanes)
    {
        if (lane.pad < 0)
            continue;

        for (int s = 0; s < numSteps; ++s)
        {
            const int d = lane.steps[s] - '0';
            if (d <= 0)
                continue;

            const float p = d >= 9 ? 1.0f : juce::jmin (1.0f, (float) d / 9.0f * (0.4f + 1.2f * density));
            if (rng.nextFloat() >= p)
                continue;

            int v = (s % 4 == 0) ? 112 : (s % 2 == 0 ? 98 : 84);
            v += rng.nextInt (17) - 8;
            if (d < 9)
                v -= 12;
            g.steps[(size_t) lane.pad][(size_t) s] = (uint8_t) juce::jlimit (1, 127, v);
        }
    }

    g.bpm = st.bpm;
    g.swing = st.swing;
    g.description = juce::String (st.name) + " groove (local generator)";
    return g;
}

void mutate (PatternGrid& grid, float amount, juce::Random& rng)
{
    amount = juce::jlimit (0.0f, 1.0f, amount);
    for (int p = 0; p < numTracks; ++p)
        for (int s = 0; s < numSteps; ++s)
        {
            auto& v = grid[(size_t) p][(size_t) s];
            const bool backbone = (p <= 3) && (s % 4 == 0); // keep the main kick/snare/clap hits stable
            const float flipChance = amount * 0.15f * (p <= 3 ? 0.4f : 1.0f);

            if (! backbone && rng.nextFloat() < flipChance)
                v = v > 0 ? 0 : (uint8_t) (55 + rng.nextInt (50));
            else if (v > 0)
                v = (uint8_t) juce::jlimit (1, 127, (int) v + rng.nextInt (1 + (int) (amount * 24)) - (int) (amount * 12));
        }
}
} // namespace vg::PatternGenerator
