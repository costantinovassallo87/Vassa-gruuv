#include "PatternGenerator.h"
#include <algorithm>
#include <vector>

namespace vg::PatternGenerator
{
namespace
{
// A lane is a track plus one or more variants separated by '|'. A variant is 16 digits, one per step:
// 0 = never, 9 = always, 1..8 = probability (scaled by density).
struct Lane { int pad; const char* variants; };

struct Style
{
    const char* name;
    double bpm;
    float swing;
    std::vector<Lane> lanes;
};

// Tracks: 0 Kick, 1 Kick 2, 2 Snare, 3 Clap, 4 Rim, 5 Hat C, 6 Hat O, 7 Tom Lo, 8 Tom Hi, 9 Cowbell,
//         10 Crash, 11 Shaker, 12-14 Perc 1-3, 15 FX
const std::vector<Style>& styles()
{
    static const std::vector<Style> s = {
        { "House", 124, 0.0f, { { 0, "9000900090009000|9000900090009002" }, { 3, "0000900000009000" }, { 5, "6060606060606060|7070707070707070" },
                                { 6, "0090009000900090" }, { 11, "3535353535353535|0303030303030303" }, { 4, "0000000200000002" }, { 12, "0000200000020000" } } },
        { "Techno", 132, 0.0f, { { 0, "9000900090009000" }, { 6, "0090009000900090" }, { 5, "5656565656565656|0606060606060606" }, { 3, "0000800000008000" },
                                 { 4, "0004000300040003|0002000200020002" }, { 13, "0020000200200010" }, { 10, "1000000000000000" } } },
        { "Hip-Hop", 90, 0.5f, { { 0, "9000002000900200|9000000200900020" }, { 2, "0000900000009000" }, { 5, "9090909090909090|8080808080808080" },
                                 { 6, "0000000000000030" }, { 4, "0000000000020000" }, { 3, "0000300000003000" } } },
        { "Boom Bap", 92, 0.55f, { { 0, "9000000090200000|9020000090000200" }, { 2, "0000900000009000|0000900000009020" }, { 5, "8080808080808080|9090909090909090" },
                                   { 6, "0000002000000020" }, { 4, "0000000000010000" } } },
        { "Trap", 140, 0.0f, { { 0, "9000000200900000|9000002000900200" }, { 2, "0000000090000000" }, { 3, "0000000090000000" }, { 5, "9989889989988998|9898989898989898" },
                               { 6, "0000002000000020" }, { 1, "0000000000000200" } } },
        { "Drum & Bass", 174, 0.0f, { { 0, "9000000000900000|9000000020900000" }, { 2, "0000900000009000" }, { 5, "9090909090909090|9999999999999999" },
                                      { 1, "0000000020000200" }, { 6, "0000000000000010" }, { 4, "0200000000020000" } } },
        { "Funk", 104, 0.3f, { { 0, "9002003009020030|9000002009002030" }, { 2, "0000900200009002" }, { 5, "9999999999999999|9090909090909090" },
                               { 6, "0000000000200000" }, { 9, "0000000000000300" }, { 4, "0003000000030000" }, { 11, "0505050505050505" } } },
        { "Disco", 120, 0.0f, { { 0, "9000900090009000" }, { 2, "0000900000009000" }, { 3, "0000900000009000" }, { 6, "0090009000900090" },
                                { 5, "6060606060606060" }, { 11, "3535353535353535" }, { 12, "0000000090000000" } } },
        { "Jazz", 130, 0.6f, { { 5, "9000009090000090|9000009090009090" }, { 0, "5000000300000030|3000000000300000" }, { 2, "0000200000000300" },
                               { 4, "0000900000009000" }, { 6, "0000900000009000" } } },
        { "Reggaeton", 95, 0.0f, { { 0, "9000900090009000" }, { 2, "0009009000090090" }, { 5, "6060606060606060" }, { 4, "0000000200000002" }, { 11, "0505050505050505" } } },
        { "Latin", 100, 0.1f, { { 4, "9009009000909000" }, { 9, "9000900090009000" }, { 0, "9000009000900000" }, { 11, "5555555555555555" },
                                { 7, "0000900000009000" }, { 8, "0000000009000900" } } },
        { "Breakbeat", 135, 0.0f, { { 0, "9000001000900000|9010000010100000" }, { 2, "0000900000009000" }, { 5, "9090909090909090" }, { 4, "0000000100000100" }, { 6, "0000000000000020" } } },
        { "Dubstep", 140, 0.0f, { { 0, "9000000000000000|9000000000100000" }, { 2, "0000000090000000" }, { 5, "0090009000900090" }, { 12, "0000000000000200" }, { 10, "1000000000000000" } } },
        { "UK Garage", 130, 0.3f, { { 0, "9000000000900000" }, { 2, "0000900000009000" }, { 5, "0090009000900090|9090909090909090" }, { 11, "0505050505050505" }, { 4, "0000000200000002" } } },
        { "Electro", 128, 0.0f, { { 0, "9000001000900010" }, { 2, "0000900000009000" }, { 3, "0000900000009000" }, { 5, "0090009000900090" }, { 9, "0000000000900000" }, { 15, "0000000000000001" } } },
        { "Rock", 120, 0.0f, { { 0, "9000001000901000" }, { 2, "0000900000009000" }, { 5, "9090909090909090" }, { 10, "9000000000000000" } } },
        { "Metal", 160, 0.0f, { { 0, "9090909090909090|9999999999999999" }, { 2, "0000900000009000" }, { 6, "9000900090009000" }, { 10, "9000000000000000" } } },
        { "Afrobeat", 110, 0.1f, { { 0, "9000009000900000" }, { 2, "0000900000009000" }, { 5, "9090909090909090" }, { 9, "9009009090090090" }, { 11, "5050505050505050" }, { 8, "0000000090000000" } } },
        { "Lo-Fi", 80, 0.45f, { { 0, "9000000090100000" }, { 2, "0000900000009000" }, { 5, "5050505050505050" }, { 11, "0000000000000030" }, { 4, "0000000000020000" } } },
        { "Trance", 138, 0.0f, { { 0, "9000900090009000" }, { 6, "0090009000900090" }, { 3, "0000900000009000" }, { 5, "3030303030303030" }, { 10, "1000000000000000" },
                                 { 11, "0505050505050505" }, { 15, "0000000000000010" } } },
    };
    return s;
}

juce::String pickVariant (const char* variants, juce::Random& rng)
{
    const auto parts = juce::StringArray::fromTokens (juce::String (variants), "|", "");
    return parts[rng.nextInt (parts.size())];
}

TrackRow renderPattern (const juce::String& steps, float density, juce::Random& rng)
{
    TrackRow row {};
    for (int s = 0; s < numSteps; ++s)
    {
        const int d = steps[s] - '0';
        if (d <= 0)
            continue;

        const float p = d >= 9 ? 1.0f : juce::jmin (1.0f, (float) d / 9.0f * (0.4f + 1.2f * density));
        if (rng.nextFloat() >= p)
            continue;

        int v = (s % 4 == 0) ? 112 : (s % 2 == 0 ? 98 : 84);
        v += rng.nextInt (17) - 8;
        if (d < 9)
            v -= 12;
        row[(size_t) s] = (uint8_t) juce::jlimit (1, 127, v);
    }

    // A regenerated track should never come out silent: keep at least the most likely hit
    if (std::none_of (row.begin(), row.end(), [] (uint8_t v) { return v > 0; }))
    {
        int best = 0;
        for (int s = 1; s < numSteps; ++s)
            if (steps[s] > steps[best])
                best = s;
        if (steps[best] > '0')
            row[(size_t) best] = 90;
    }
    return row;
}

// Evenly spread hits (Euclidean rhythm)
std::vector<int> euclid (int hits, int rotation)
{
    std::vector<int> pos;
    for (int i = 0; i < hits; ++i)
        pos.push_back ((i * numSteps / juce::jmax (1, hits) + rotation) % numSteps);
    return pos;
}

// Generic part for a track the genre does not define, chosen by instrument role
TrackRow fallbackRow (int track, float density, juce::Random& rng)
{
    TrackRow row {};
    int hits, rot, base;
    switch (track)
    {
        case 0: case 1: hits = 3 + rng.nextInt (3); rot = 0; base = 110; break;
        case 2: case 3: hits = 2; rot = 4; base = 105; break;
        case 5: case 6: hits = 5 + rng.nextInt (5); rot = rng.nextInt (2); base = 90; break;
        default:        hits = 2 + rng.nextInt (4); rot = rng.nextInt (numSteps); base = 85; break;
    }
    hits = juce::jlimit (1, numSteps, juce::roundToInt ((float) hits * (0.6f + 0.8f * density)));
    for (int p : euclid (hits, rot))
        row[(size_t) p] = (uint8_t) juce::jlimit (1, 127, base + rng.nextInt (21) - 10);
    return row;
}

const Lane* findLane (const Style& st, int track)
{
    for (const auto& l : st.lanes)
        if (l.pad == track)
            return &l;
    return nullptr;
}
} // namespace

juce::StringArray styleNames()
{
    juce::StringArray names;
    for (auto& s : styles())
        names.add (s.name);
    names.add ("???");
    return names;
}

int hybridIndex() { return (int) styles().size(); }

TrackRow generateTrack (int styleIndex, int track, float density, juce::Random& rng)
{
    density = juce::jlimit (0.0f, 1.0f, density);
    const auto& all = styles();

    if (styleIndex >= hybridIndex()) // "???": any genre that has a part for this track
    {
        std::vector<const Lane*> options;
        for (const auto& st : all)
            if (auto* l = findLane (st, track))
                options.push_back (l);
        if (! options.empty())
            return renderPattern (pickVariant (options[(size_t) rng.nextInt ((int) options.size())]->variants, rng), density, rng);
        return fallbackRow (track, density, rng);
    }

    const auto& st = all[(size_t) juce::jlimit (0, hybridIndex() - 1, styleIndex)];
    if (auto* l = findLane (st, track))
        return renderPattern (pickVariant (l->variants, rng), density, rng);
    return fallbackRow (track, density, rng);
}

GeneratedGroove generate (int styleIndex, float density, juce::Random& rng)
{
    const auto& all = styles();
    GeneratedGroove g;

    if (styleIndex >= hybridIndex())
    {
        // Hybrid: each track comes from a different random genre; tempo and swing follow one of them
        const auto& lead = all[(size_t) rng.nextInt ((int) all.size())];
        for (int t = 0; t < numTracks; ++t)
            if (std::any_of (all.begin(), all.end(), [t] (const Style& s) { return findLane (s, t) != nullptr; }))
                g.steps[(size_t) t] = generateTrack (styleIndex, t, density, rng);
        g.bpm = lead.bpm;
        g.swing = lead.swing;
        g.description = "??? hybrid groove (tempo from " + juce::String (lead.name) + ")";
        return g;
    }

    const auto& st = all[(size_t) juce::jlimit (0, hybridIndex() - 1, styleIndex)];
    for (const auto& lane : st.lanes)
        g.steps[(size_t) lane.pad] = generateTrack (styleIndex, lane.pad, density, rng);

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
