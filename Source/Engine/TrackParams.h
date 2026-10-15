#pragma once

#include "Envelope.h"
#include "Pattern.h"

namespace vg
{
// Per-track sampler parameters (Simpler-style). The enum order must match the table below.
enum TP
{
    TPMode, TPStart, TPEnd, TPLoop, TPLoopStart, TPLoopEnd, TPLoopFade, TPFadeIn, TPFadeOut, TPReverse,
    TPGain, TPPan, TPTranspose, TPDetune,
    TPFType, TPFCut, TPFRes,
    TPLDest, TPLRate, TPLAmt,
    TPSMode, TPSCount, TPSSens,
    TPMute, TPSolo,
    numTP
};

enum SamplerMode { ModeClassic = 0, ModeOneShot, ModeSlice };

enum class TPType { Float, Bool, Choice, Int };

struct TPSpec
{
    const char* id;
    const char* name;
    TPType type;
    float min, max, def, skew;
    const char* unit;
    const char* choices; // '|' separated
};

inline const TPSpec& tpSpec (int i)
{
    static const TPSpec t[numTP] = {
        { "mode",      "Mode",        TPType::Choice, 0, 2, 1, 1, "", "Classic|One-Shot|Slice" },
        { "start",     "Start",       TPType::Float, 0, 1, 0, 1, "", "" },
        { "end",       "End",         TPType::Float, 0, 1, 1, 1, "", "" },
        { "loop",      "Loop",        TPType::Bool,  0, 1, 0, 1, "", "" },
        { "loopStart", "Loop Start",  TPType::Float, 0, 1, 0, 1, "", "" },
        { "loopEnd",   "Loop End",    TPType::Float, 0, 1, 1, 1, "", "" },
        { "loopFade",  "Loop Fade",   TPType::Float, 0, 0.5f, 0.02f, 1, "", "" },
        { "fadeIn",    "Fade In",     TPType::Float, 0, 500, 0, 0.4f, " ms", "" },
        { "fadeOut",   "Fade Out",    TPType::Float, 0, 2000, 5, 0.4f, " ms", "" },
        { "reverse",   "Reverse",     TPType::Bool,  0, 1, 0, 1, "", "" },
        { "gain",      "Gain",        TPType::Float, -36, 12, 0, 1, " dB", "" },
        { "pan",       "Pan",         TPType::Float, -1, 1, 0, 1, "", "" },
        { "transpose", "Transpose",   TPType::Int,   -48, 48, 0, 1, " st", "" },
        { "detune",    "Detune",      TPType::Float, -50, 50, 0, 1, " ct", "" },
        { "fType",     "Filter",      TPType::Choice, 0, 3, 0, 1, "", "Off|LP|HP|BP" },
        { "fCut",      "Cutoff",      TPType::Float, 20, 20000, 20000, 0.25f, " Hz", "" },
        { "fRes",      "Resonance",   TPType::Float, 0, 1, 0.1f, 1, "", "" },
        { "lDest",     "LFO Dest",    TPType::Choice, 0, 4, 0, 1, "", "Off|Pitch|Filter|Volume|Pan" },
        { "lRate",     "LFO Rate",    TPType::Float, 0.05f, 20, 4, 0.4f, " Hz", "" },
        { "lAmt",      "LFO Amount",  TPType::Float, 0, 1, 0, 1, "", "" },
        { "sMode",     "Slice Mode",  TPType::Choice, 0, 1, 0, 1, "", "Equal|Transient" },
        { "sCount",    "Slices",      TPType::Int,   2, 32, 8, 1, "", "" },
        { "sSens",     "Sensitivity", TPType::Float, 0, 1, 0.5f, 1, "", "" },
        { "mute",      "Mute",        TPType::Bool,  0, 1, 0, 1, "", "" },
        { "solo",      "Solo",        TPType::Bool,  0, 1, 0, 1, "", "" },
    };
    return t[juce::jlimit (0, (int) numTP - 1, i)];
}

// Display string for a parameter value (used by the host and by the knob text boxes)
inline juce::String formatTP (const TPSpec& sp, float v)
{
    const juce::String unit (sp.unit);
    if (juce::String (sp.id) == "pan")
        return std::abs (v) < 0.005f ? juce::String ("C") : (v < 0 ? "L " : "R ") + juce::String (juce::roundToInt (std::abs (v) * 100.0f));
    if (unit == " ms" || unit == " Hz")
        return (v < 100.0f && sp.max < 100.0f ? juce::String (v, 1) : (v < 10.0f ? juce::String (v, 1) : juce::String (juce::roundToInt (v)))) + unit;
    if (unit.isNotEmpty())
        return juce::String (v, sp.type == TPType::Int ? 0 : 1) + unit;
    return juce::String (v, sp.type == TPType::Int ? 0 : 2);
}

inline juce::String trackParamId (int track, int tp) { return "t" + juce::String (track) + "_" + tpSpec (tp).id; }

struct TrackParams
{
    float v[numTP];
    EnvShape env[numEnvKinds];

    TrackParams()
    {
        for (int i = 0; i < numTP; ++i) v[i] = tpSpec (i).def;
        for (int k = 0; k < numEnvKinds; ++k) env[k] = defaultEnv (k);
    }

    float operator[] (int i) const noexcept { return v[i]; }
    bool on (int i) const noexcept { return v[i] > 0.5f; }
    int idx (int i) const noexcept { return (int) (v[i] + 0.5f); }
};
} // namespace vg
