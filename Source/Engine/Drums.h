#pragma once

#include "Pattern.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <memory>

namespace vg
{
struct Sample
{
    juce::AudioBuffer<float> data; // 1 or 2 channels
    double rate = 44100.0;
    juce::String path;
};

enum class Kind { Kick, Snare, Clap, Rim, HatClosed, HatOpen, Tom, Cowbell, Crash, Shaker, Zap };

struct PadInfo
{
    const char* name;
    Kind kind;
    float freq;     // base frequency (Hz) for tonal sounds
    int chokeGroup; // -1 = none
};

inline const PadInfo& padInfo (int pad)
{
    static const PadInfo table[numPads] = {
        { "Kick",    Kind::Kick,       58.f,  -1 },
        { "Kick 2",  Kind::Kick,       44.f,  -1 },
        { "Snare",   Kind::Snare,      190.f, -1 },
        { "Clap",    Kind::Clap,       0.f,   -1 },
        { "Rim",     Kind::Rim,        1700.f, -1 },
        { "Hat C",   Kind::HatClosed,  0.f,    1 },
        { "Hat O",   Kind::HatOpen,    0.f,    1 },
        { "Tom Lo",  Kind::Tom,        85.f,  -1 },
        { "Tom Hi",  Kind::Tom,        150.f, -1 },
        { "Cowbell", Kind::Cowbell,    560.f, -1 },
        { "Crash",   Kind::Crash,      0.f,   -1 },
        { "Shaker",  Kind::Shaker,     0.f,   -1 },
        { "Perc 1",  Kind::Zap,        800.f, -1 },
        { "Perc 2",  Kind::Zap,        380.f, -1 },
        { "Perc 3",  Kind::Rim,        2600.f, -1 },
        { "FX",      Kind::Zap,        1500.f, -1 },
    };
    return table[juce::jlimit (0, numPads - 1, pad)];
}

// One monophonic drum voice per pad: built-in synthesis, or sample playback when a sample is loaded.
class DrumVoice
{
public:
    void prepare (double sr) { sampleRate = sr; active = false; }
    bool isActive() const noexcept { return active; }
    void choke() noexcept { choking = true; }

    void trigger (int pad, float velocity, float tuneSemitones, float decayScale, std::shared_ptr<const Sample> s)
    {
        info = &padInfo (pad);
        vel = velocity;
        ratio = std::pow (2.0f, tuneSemitones / 12.0f);
        decay = decayScale;
        sample = std::move (s);

        age = 0;
        samplePos = 0.0;
        phase = phase2 = 0.0f;
        for (auto& p : metal) p = 0.0f;
        hpState = lpState = 0.0f;
        ampEnv = env2 = pitchEnv = 1.0f;
        chokeGain = 1.0f;
        choking = false;
        configureEnvelopes();
        active = true;
    }

    void render (float& outL, float& outR)
    {
        outL = outR = 0.0f;
        if (! active)
            return;

        float l = 0.0f, r = 0.0f;

        if (sample != nullptr)
            renderSample (l, r);
        else
            l = r = renderSynth();

        if (choking)
        {
            chokeGain *= chokeCoeff();
            if (chokeGain < 1.0e-3f)
                active = false;
        }

        outL = l * vel * chokeGain;
        outR = r * vel * chokeGain;
        ++age;
    }

private:
    static constexpr float twoPi = 6.28318530718f;

    float coeff (float tau) const { return std::exp (-1.0f / (tau * (float) sampleRate)); }
    float chokeCoeff() const { return coeff (0.004f); }
    float noise() { return rng.nextFloat() * 2.0f - 1.0f; }

    float highpass (float x, float fc)
    {
        fc = juce::jmin (fc, 0.45f * (float) sampleRate);
        const float a = 1.0f - std::exp (-twoPi * fc / (float) sampleRate);
        hpState += a * (x - hpState);
        return x - hpState;
    }

    float lowpass (float x, float fc)
    {
        fc = juce::jmin (fc, 0.45f * (float) sampleRate);
        const float a = 1.0f - std::exp (-twoPi * fc / (float) sampleRate);
        lpState += a * (x - lpState);
        return lpState;
    }

    void configureEnvelopes()
    {
        float tauAmp = 0.2f, tau2 = 0.05f, tauPitch = 0.03f;

        switch (info->kind)
        {
            case Kind::Kick:      tauAmp = 0.22f; tau2 = 0.004f; tauPitch = 0.035f; break;
            case Kind::Snare:     tauAmp = 0.13f; tau2 = 0.07f;  tauPitch = 0.02f;  break;
            case Kind::Clap:      tauAmp = 0.11f; break;
            case Kind::Rim:       tauAmp = 0.03f; tau2 = 0.004f; break;
            case Kind::HatClosed: tauAmp = 0.035f; break;
            case Kind::HatOpen:   tauAmp = 0.32f; break;
            case Kind::Tom:       tauAmp = 0.26f; tauPitch = 0.06f; tau2 = 0.004f; break;
            case Kind::Cowbell:   tauAmp = 0.28f; tau2 = 0.02f; break;
            case Kind::Crash:     tauAmp = 0.6f; break;
            case Kind::Shaker:    tauAmp = 0.07f; tau2 = 0.012f; break;
            case Kind::Zap:       tauAmp = 0.2f;  tauPitch = 0.07f; break;
        }

        ampCoeff = coeff (tauAmp * decay);
        env2Coeff = coeff (tau2);
        pitchCoeff = coeff (tauPitch);

        if (sample != nullptr)
            ampCoeff = decay < 0.99f ? coeff (0.5f * decay) : 1.0f; // samples: decay<1 shortens the tail
    }

    void renderSample (float& l, float& r)
    {
        const auto& d = sample->data;
        const int n = d.getNumSamples();
        const int i = (int) samplePos;
        if (i >= n - 1)
        {
            active = false;
            return;
        }

        const float f = (float) (samplePos - (double) i);
        const float* c0 = d.getReadPointer (0);
        const float* c1 = d.getNumChannels() > 1 ? d.getReadPointer (1) : c0;
        l = c0[i] * (1.0f - f) + c0[i + 1] * f;
        r = c1[i] * (1.0f - f) + c1[i + 1] * f;
        l *= ampEnv;
        r *= ampEnv;

        samplePos += (double) ratio * sample->rate / sampleRate;
        ampEnv *= ampCoeff;
        if (ampEnv < 1.0e-4f)
            active = false;
    }

    float renderSynth()
    {
        const float sr = (float) sampleRate;
        const float t = (float) age / sr;
        float s = 0.0f;

        auto advance = [&] (float& ph, float freq) { ph += freq / sr; ph -= std::floor (ph); return ph; };

        switch (info->kind)
        {
            case Kind::Kick:
            {
                const float f = info->freq * ratio * (1.0f + 3.5f * pitchEnv);
                s = std::sin (twoPi * advance (phase, f)) * ampEnv + noise() * env2 * 0.25f;
                s = std::tanh (s * 1.4f) * 0.9f;
                break;
            }
            case Kind::Snare:
            {
                const float f = info->freq * ratio * (1.0f + 0.4f * pitchEnv);
                const float tone = std::sin (twoPi * advance (phase, f)) * env2;
                const float nz = highpass (noise(), 1500.0f * ratio) * ampEnv;
                s = 0.55f * tone + 0.7f * nz;
                break;
            }
            case Kind::Clap:
            {
                float y = lowpass (highpass (noise(), 900.0f * ratio), 3800.0f);
                const float e = t < 0.036f ? std::exp (-std::fmod (t, 0.012f) / 0.0025f) : ampEnv;
                s = y * e * 1.6f;
                break;
            }
            case Kind::Rim:
            {
                const float f = info->freq * ratio;
                s = (0.6f * std::sin (twoPi * advance (phase, f)) + 0.4f * std::sin (twoPi * advance (phase2, f * 1.58f))) * ampEnv
                    + noise() * env2 * 0.3f;
                break;
            }
            case Kind::HatClosed:
            case Kind::HatOpen:
            {
                static const float ratios[6] = { 263.f, 400.f, 421.f, 474.f, 587.f, 845.f };
                float m = 0.0f;
                for (int i = 0; i < 6; ++i)
                    m += advance (metal[i], ratios[i] * 2.2f * ratio) < 0.5f ? 1.0f : -1.0f;
                m = m / 6.0f * 0.8f + noise() * 0.25f;
                s = highpass (m, 7000.0f * ratio) * ampEnv;
                break;
            }
            case Kind::Tom:
            {
                const float f = info->freq * ratio * (1.0f + 1.2f * pitchEnv);
                s = std::sin (twoPi * advance (phase, f)) * ampEnv + noise() * env2 * 0.15f;
                break;
            }
            case Kind::Cowbell:
            {
                const float f = info->freq * ratio;
                float q = (advance (phase, f) < 0.5f ? 1.0f : -1.0f) + (advance (phase2, f * 1.504f) < 0.5f ? 1.0f : -1.0f);
                s = lowpass (q * 0.35f, 4500.0f) * (0.4f * ampEnv + 0.6f * env2);
                break;
            }
            case Kind::Crash:
            {
                static const float ratios[6] = { 263.f, 400.f, 421.f, 474.f, 587.f, 845.f };
                float m = 0.0f;
                for (int i = 0; i < 6; ++i)
                    m += advance (metal[i], ratios[i] * 3.1f * ratio) < 0.5f ? 1.0f : -1.0f;
                s = highpass (noise() * 0.8f + m / 6.0f * 0.4f, 5000.0f * ratio) * ampEnv * 0.7f;
                break;
            }
            case Kind::Shaker:
                s = highpass (noise(), 5000.0f * ratio) * ampEnv * (1.0f - env2) * 0.8f;
                break;
            case Kind::Zap:
            {
                const float f = info->freq * ratio * (1.0f + 3.0f * pitchEnv);
                const float mod = std::sin (twoPi * advance (phase2, f * 2.01f));
                s = std::sin (twoPi * (advance (phase, f) + 1.5f * mod * pitchEnv)) * ampEnv * 0.8f;
                break;
            }
        }

        ampEnv *= ampCoeff;
        env2 *= env2Coeff;
        pitchEnv *= pitchCoeff;
        if (ampEnv < 1.0e-4f)
            active = false;
        return s;
    }

    const PadInfo* info = &padInfo (0);
    std::shared_ptr<const Sample> sample;
    juce::Random rng;
    double sampleRate = 44100.0;
    double samplePos = 0.0;
    int age = 0;
    bool active = false, choking = false;
    float vel = 1.0f, ratio = 1.0f, decay = 1.0f;
    float ampEnv = 0.0f, env2 = 0.0f, pitchEnv = 0.0f;
    float ampCoeff = 0.999f, env2Coeff = 0.999f, pitchCoeff = 0.999f;
    float phase = 0.0f, phase2 = 0.0f, hpState = 0.0f, lpState = 0.0f, chokeGain = 1.0f;
    float metal[6] {};
};
} // namespace vg
