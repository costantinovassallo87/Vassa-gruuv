#pragma once

#include "Envelope.h"
#include "Sample.h"
#include "TrackParams.h"
#include <array>
#include <atomic>
#include <memory>

namespace vg
{
// ---------------------------------------------------------------- slicing

inline int numSlices (const Sample& s, const TrackParams& p)
{
    if (p.idx (TPSMode) == 0)
        return juce::jmax (2, p.idx (TPSCount));

    const float s0 = p[TPStart], e0 = juce::jmax (p[TPEnd], s0 + 0.0005f), thr = 1.0f - p[TPSSens];
    int n = 1;
    for (const auto& o : s.onsets)
        if (o.pos > s0 + 0.002f && o.pos < e0 - 0.002f && o.strength >= thr)
            ++n;
    return n;
}

// Start/end (normalised) of slice k, wrapping k into range.
inline void sliceRegion (const Sample& s, const TrackParams& p, int k, float& a, float& b)
{
    const float s0 = p[TPStart], e0 = juce::jmax (p[TPEnd], s0 + 0.0005f);
    const int n = numSlices (s, p);
    k = ((k % n) + n) % n;

    if (p.idx (TPSMode) == 0)
    {
        a = s0 + (e0 - s0) * (float) k / (float) n;
        b = s0 + (e0 - s0) * (float) (k + 1) / (float) n;
        return;
    }

    const float thr = 1.0f - p[TPSSens];
    float lo = s0;
    int i = 0;
    for (const auto& o : s.onsets)
    {
        if (! (o.pos > s0 + 0.002f && o.pos < e0 - 0.002f && o.strength >= thr))
            continue;
        if (i == k)
        {
            a = lo;
            b = o.pos;
            return;
        }
        lo = o.pos;
        ++i;
    }
    a = lo;
    b = e0;
}

// Interior slice boundaries (for drawing).
inline std::vector<float> sliceBoundaries (const Sample& s, const TrackParams& p)
{
    std::vector<float> out;
    const int n = numSlices (s, p);
    for (int k = 1; k < n; ++k)
    {
        float a, b;
        sliceRegion (s, p, k, a, b);
        out.push_back (a);
    }
    return out;
}

// ---------------------------------------------------------------- DSP bits

struct Svf // TPT state-variable filter, 2 channels
{
    float ic1[2] {}, ic2[2] {}, a1 = 0, a2 = 0, a3 = 0, k = 1;

    void reset() { ic1[0] = ic1[1] = ic2[0] = ic2[1] = 0.0f; }

    void set (float fc, float res, double sr)
    {
        fc = juce::jlimit (20.0f, 0.45f * (float) sr, fc);
        const float g = std::tan (juce::MathConstants<float>::pi * fc / (float) sr);
        k = 2.0f - 1.95f * juce::jlimit (0.0f, 1.0f, res);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    float tick (int ch, float x, int type) // 1 = LP, 2 = HP, 3 = BP
    {
        const float v3 = x - ic2[ch];
        const float v1 = a1 * ic1[ch] + a2 * v3;
        const float v2 = ic2[ch] + a2 * ic1[ch] + a3 * v3;
        ic1[ch] = 2.0f * v1 - ic1[ch];
        ic2[ch] = 2.0f * v2 - ic2[ch];
        return type == 1 ? v2 : (type == 2 ? x - k * v1 - v2 : v1);
    }
};

// ---------------------------------------------------------------- voice

class SamplerVoice
{
public:
    bool active = false;
    int64_t age = 0;

    // gateSamples < 0: held until release() (MIDI). Only Classic mode honours the gate.
    void start (const TrackParams& p, std::shared_ptr<const Sample> s, float velocity, float pitchSemis,
                int sliceIdx, int gateSamples, double sr)
    {
        smp = std::move (s);
        const int N = smp != nullptr ? smp->numFrames() : 0;
        if (N < 4)
        {
            active = false;
            return;
        }

        prm = p;
        sampleRate = sr;
        mode = p.idx (TPMode);
        pitch = pitchSemis;

        float a = p[TPStart], b = juce::jmax (p[TPEnd], a + 0.0005f);
        if (mode == ModeSlice)
            sliceRegion (*smp, p, sliceIdx, a, b);

        rs = juce::jlimit (0.0, (double) N - 2.0, (double) a * N);
        re = juce::jlimit (rs + 2.0, (double) N, (double) b * N);
        reverse = p.on (TPReverse);

        loop = false;
        if (mode == ModeClassic && p.on (TPLoop))
        {
            const double ls = juce::jlimit (rs, re - 2.0, (double) p[TPLoopStart] * N);
            const double le = juce::jlimit (ls + 2.0, re, (double) p[TPLoopEnd] * N);
            if (le - ls >= 2.0)
            {
                loop = true;
                loopLen = le - ls;
                pls = reverse ? rs + re - le : ls;
                ple = reverse ? rs + re - ls : le;
            }
        }
        xfade = loop ? (double) p[TPLoopFade] * loopLen : 0.0;

        pos = rs;
        baseRate = smp->rate / sr;
        gain = std::pow (10.0f, p[TPGain] / 20.0f) * velocity;
        fadeInLen = juce::jmax (32.0, (double) p[TPFadeIn] * 0.001 * sr);
        fadeOutLen = juce::jmax (32.0, (double) p[TPFadeOut] * 0.001 * sr);

        const bool oneShot = mode != ModeClassic; // one-shot / slice never release: they hold the sustain point until the sample ends
        ampEnv.start (p.env[EnvAmp], sr);
        pitchEnv.start (p.env[EnvPitch], sr);
        fltEnv.start (p.env[EnvFilter], sr);
        released = false;
        releaseFade = 1.0f;
        releaseMul = std::exp (-4.0f / (0.02f * (float) sr));

        gated = ! oneShot && gateSamples >= 0;
        gateLeft = gated ? juce::jmax (1, gateSamples) : 0;

        lfoPhase = 0.0f;
        svf.reset();
        age = 0;
        active = true;
        updateControl();
    }

    void release()
    {
        released = true;
        ampEnv.release();
        pitchEnv.release();
        fltEnv.release();
    }

    bool isHeld() const noexcept { return active && mode == ModeClassic && ! gated; }

    float playPosNorm() const
    {
        if (smp == nullptr || ! active)
            return -1.0f;
        const double q = reverse ? rs + re - 1.0 - pos : pos;
        return (float) (q / smp->numFrames());
    }

    // Adds into L/R.
    void render (float* L, float* R, int n)
    {
        const float* c0 = smp->data.getReadPointer (0);
        const float* c1 = smp->data.getNumChannels() > 1 ? smp->data.getReadPointer (1) : c0;
        const int N = smp->numFrames();
        const int fType = prm.idx (TPFType);

        auto read = [&] (const float* c, double p)
        {
            const double q = juce::jlimit (0.0, (double) N - 1.0001, reverse ? rs + re - 1.0 - p : p);
            const int i = (int) q;
            const float f = (float) (q - i);
            return c[i] * (1.0f - f) + c[i + 1] * f;
        };

        for (int i = 0; i < n && active; ++i)
        {
            if ((age & 15) == 0)
                updateControl();

            if (gated && gateLeft > 0 && --gateLeft == 0)
                release();

            if (loop)
            {
                while (pos >= ple)
                    pos -= loopLen;
            }
            else if (pos >= re - 1.0)
            {
                active = false;
                break;
            }

            float sl = read (c0, pos), sr = read (c1, pos);
            if (loop && xfade >= 1.0 && pos > ple - xfade)
            {
                const float t = (float) ((pos - (ple - xfade)) / xfade);
                const double p2 = pos - loopLen;
                const float wa = std::cos (t * juce::MathConstants<float>::halfPi), wb = std::sin (t * juce::MathConstants<float>::halfPi);
                sl = sl * wa + read (c0, p2) * wb;
                sr = sr * wa + read (c1, p2) * wb;
            }

            float a = ampEnv.tick();
            pitchEnv.tick();
            fltEnv.tick();
            if (released && ampEnv.finished) // envelope ended on a non-zero level: fade out quickly
            {
                releaseFade *= releaseMul;
                a *= releaseFade;
                if (a < 1.0e-4f)
                {
                    active = false;
                    break;
                }
            }
            float g = a * gain * lfoVol;

            g *= (float) juce::jmin (1.0, (double) age / fadeInLen);
            if (! loop)
                g *= (float) juce::jmin (1.0, (re - 1.0 - pos) / fadeOutLen);

            if (fType > 0)
            {
                sl = svf.tick (0, sl, fType);
                sr = svf.tick (1, sr, fType);
            }

            L[i] += sl * g * panL;
            R[i] += sr * g * panR;

            pos += rate;
            ++age;
        }
    }

private:
    void updateControl()
    {
        const int dest = prm.idx (TPLDest);
        const float amt = prm[TPLAmt];
        lfoPhase += juce::MathConstants<float>::twoPi * prm[TPLRate] * 16.0f / (float) sampleRate;
        if (lfoPhase > juce::MathConstants<float>::twoPi)
            lfoPhase -= juce::MathConstants<float>::twoPi;
        const float lfo = dest > 0 ? std::sin (lfoPhase) * amt : 0.0f;

        const float semis = prm[TPTranspose] + pitch + pitchEnv.level + prm[TPDetune] / 100.0f + (dest == 1 ? lfo * 12.0f : 0.0f);
        rate = baseRate * std::exp2 ((double) semis / 12.0);

        if (prm.idx (TPFType) > 0)
        {
            const float st = fltEnv.level + (dest == 2 ? lfo * 36.0f : 0.0f);
            svf.set (prm[TPFCut] * std::exp2 (st / 12.0f), prm[TPFRes], sampleRate);
        }

        lfoVol = dest == 3 ? 1.0f - amt * (0.5f + 0.5f * std::sin (lfoPhase)) : 1.0f;
        const float pan = juce::jlimit (-1.0f, 1.0f, prm[TPPan] + (dest == 4 ? lfo : 0.0f));
        const float angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        panL = std::cos (angle) * 1.4142f;
        panR = std::sin (angle) * 1.4142f;
    }

    std::shared_ptr<const Sample> smp;
    TrackParams prm;
    LineEnv ampEnv, pitchEnv, fltEnv;
    Svf svf;
    double sampleRate = 44100.0, pos = 0, rs = 0, re = 0, pls = 0, ple = 0, loopLen = 0, xfade = 0;
    double baseRate = 1, rate = 1, fadeInLen = 32, fadeOutLen = 32;
    float releaseFade = 1, releaseMul = 0.9f;
    float gain = 1, pitch = 0, lfoPhase = 0, lfoVol = 1, panL = 1, panR = 1;
    int mode = 0, gateLeft = 0;
    bool reverse = false, loop = false, gated = false, released = false;
};

// Small polyphonic pool for one track.
class TrackSampler
{
public:
    std::atomic<float> playhead { -1.0f };

    void noteOn (const TrackParams& p, std::shared_ptr<const Sample> s, float vel, float pitch, int slice, int gate, double sr)
    {
        SamplerVoice* v = nullptr;
        for (auto& x : voices)
            if (! x.active) { v = &x; break; }
        if (v == nullptr)
        {
            v = &voices[0];
            for (auto& x : voices)
                if (x.age > v->age)
                    v = &x;
        }
        v->start (p, std::move (s), vel, pitch, slice, gate, sr);
    }

    void releaseHeld()
    {
        for (auto& v : voices)
            if (v.isHeld())
                v.release();
    }

    bool isActive() const
    {
        for (auto& v : voices)
            if (v.active)
                return true;
        return false;
    }

    void reset() { for (auto& v : voices) v.active = false; playhead.store (-1.0f); }

    void render (float* L, float* R, int n)
    {
        for (auto& v : voices)
            if (v.active)
                v.render (L, R, n);

        const SamplerVoice* newest = nullptr;
        for (auto& v : voices)
            if (v.active && (newest == nullptr || v.age < newest->age))
                newest = &v;
        playhead.store (newest != nullptr ? newest->playPosNorm() : -1.0f, std::memory_order_relaxed);
    }

private:
    std::array<SamplerVoice, 6> voices;
};
} // namespace vg
