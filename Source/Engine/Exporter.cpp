#include "Exporter.h"
#include <algorithm>
#include <vector>

namespace vg
{
juce::Array<juce::File> exportStems (const ExportInput& in, int loops, const juce::File& dir, const juce::String& baseName)
{
    juce::Array<juce::File> files;
    if (in.pattern == nullptr || ! dir.createDirectory().wasOk())
        return files;

    loops = juce::jmax (1, loops);
    const double sr = in.sampleRate;
    const double ppqLen = 4.0 * loops;
    const double ppqPerSample = in.bpm / 60.0 / sr;
    const int total = (int) std::ceil (ppqLen / ppqPerSample);
    const int tail = (int) (sr * 2.0);
    constexpr int block = 512;

    bool anySolo = false;
    for (auto& p : in.params)
        anySolo |= p.on (TPSolo);

    juce::WavAudioFormat wav;
    std::vector<Trig> trigs (2048);

    for (int t = 0; t < numTracks; ++t)
    {
        const auto& prm = in.params[(size_t) t];
        if (prm.on (TPMute) || (anySolo && ! prm.on (TPSolo)) || ! in.pattern->trackHasSteps (t) || in.samples[(size_t) t] == nullptr)
            continue;

        TrackSampler sampler;
        juce::AudioBuffer<float> buf (2, total + tail);
        buf.clear();

        for (int frame = 0; frame < total + tail; frame += block)
        {
            const int n = juce::jmin (block, total + tail - frame);
            const double ppq0 = (double) frame * ppqPerSample;

            std::vector<Trig> mine;
            if (ppq0 < ppqLen)
            {
                const int num = collectTrigs (*in.pattern, ppq0, juce::jmin (ppq0 + n * ppqPerSample, ppqLen), in.bpm, in.swing, sr,
                                              trigs.data(), (int) trigs.size());
                for (int i = 0; i < num; ++i)
                    if (trigs[(size_t) i].track == t)
                        mine.push_back (trigs[(size_t) i]);
                std::stable_sort (mine.begin(), mine.end(), [] (const Trig& a, const Trig& b) { return a.pos < b.pos; });
            }

            int pos = 0;
            float* L = buf.getWritePointer (0) + frame;
            float* R = buf.getWritePointer (1) + frame;
            for (const auto& e : mine)
            {
                const int at = juce::jlimit (pos, n, e.pos);
                sampler.render (L + pos, R + pos, at - pos);
                pos = at;
                sampler.noteOn (prm, in.samples[(size_t) t], e.vel, e.pitch, e.slice, e.gate, sr);
            }
            sampler.render (L + pos, R + pos, n - pos);
        }

        for (int c = 0; c < 2; ++c)
        {
            float* d = buf.getWritePointer (c);
            for (int i = 0; i < tail; ++i)
                d[i % total] += d[total + i];
        }

        auto name = juce::File::createLegalFileName (baseName + "_" + juce::String (t + 1).paddedLeft ('0', 2) + "_" + in.names[(size_t) t]);
        auto file = dir.getChildFile (name).withFileExtension ("wav");
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());
        if (stream == nullptr)
            continue;

        if (auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (2).withBitsPerSample (24));
            writer != nullptr)
        {
            writer->writeFromAudioSampleBuffer (buf, 0, total);
            writer.reset();
            files.add (file);
        }
    }
    return files;
}
} // namespace vg
