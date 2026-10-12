#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>

using namespace vg;

namespace
{
constexpr int firstMidiNote = 36; // C1 = pad 1, like most groove boxes
constexpr double stepPpq = 0.25;  // one 16th note
}

juce::AudioProcessorValueTreeState::ParameterLayout VassaGruuvProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "run", 1 }, "Play", false));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "tempo", 1 }, "Tempo",
                                                             juce::NormalisableRange<float> (40.f, 240.f, 0.1f), 120.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "swing", 1 }, "Swing",
                                                             juce::NormalisableRange<float> (0.f, 1.f, 0.01f), 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "filter", 1 }, "Filter",
                                                             juce::NormalisableRange<float> (80.f, 20000.f, 1.f, 0.25f), 20000.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "reverb", 1 }, "Reverb",
                                                             juce::NormalisableRange<float> (0.f, 1.f, 0.01f), 0.12f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "master", 1 }, "Master",
                                                             juce::NormalisableRange<float> (0.f, 1.5f, 0.01f), 0.8f));

    for (int i = 0; i < numPads; ++i)
    {
        const juce::String n = padInfo (i).name;
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { padId (i, "level"), 1 }, n + " Level",
                                                                 juce::NormalisableRange<float> (0.f, 1.5f, 0.01f), 0.9f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { padId (i, "pan"), 1 }, n + " Pan",
                                                                 juce::NormalisableRange<float> (-1.f, 1.f, 0.01f), 0.f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { padId (i, "tune"), 1 }, n + " Tune",
                                                                 juce::NormalisableRange<float> (-12.f, 12.f, 0.1f), 0.f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { padId (i, "decay"), 1 }, n + " Decay",
                                                                 juce::NormalisableRange<float> (0.2f, 3.f, 0.01f, 0.6f), 1.f));
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { padId (i, "mute"), 1 }, n + " Mute", false));
    }
    return layout;
}

VassaGruuvProcessor::VassaGruuvProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (auto& f : padFlash)
        f.store (0.0f);

    for (int i = 0; i < numPads; ++i)
    {
        pLevel[(size_t) i] = apvts.getRawParameterValue (padId (i, "level"));
        pPan[(size_t) i]   = apvts.getRawParameterValue (padId (i, "pan"));
        pTune[(size_t) i]  = apvts.getRawParameterValue (padId (i, "tune"));
        pDecay[(size_t) i] = apvts.getRawParameterValue (padId (i, "decay"));
        pMute[(size_t) i]  = apvts.getRawParameterValue (padId (i, "mute"));
    }
    pMaster = apvts.getRawParameterValue ("master");
    pTempo  = apvts.getRawParameterValue ("tempo");
    pSwing  = apvts.getRawParameterValue ("swing");
    pFilter = apvts.getRawParameterValue ("filter");
    pReverb = apvts.getRawParameterValue ("reverb");
    pRun    = apvts.getRawParameterValue ("run");

    formatManager.registerBasicFormats();

    juce::PropertiesFile::Options opts;
    opts.applicationName = "VassaGruuv";
    opts.filenameSuffix = ".settings";
    opts.osxLibrarySubFolder = "Application Support";
    opts.folderName = "VassaGruuv";
    appProps.setStorageParameters (opts);

    claude.addChangeListener (this);
}

VassaGruuvProcessor::~VassaGruuvProcessor()
{
    claude.removeChangeListener (this);
}

bool VassaGruuvProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
           && layouts.getMainInputChannelSet().isDisabled();
}

void VassaGruuvProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    for (auto& v : voices)
        v.prepare (sampleRate);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    for (auto& f : lowpass)
    {
        f.prepare (spec);
        f.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        f.reset();
    }
    reverb.setSampleRate (sampleRate);
    reverb.reset();
    masterGain.reset (sampleRate, 0.02);
    masterGain.setCurrentAndTargetValue (pMaster->load());
    internalPpq = 0.0;
}

void VassaGruuvProcessor::startVoice (int pad, float vel)
{
    const auto p = (size_t) pad;
    if (pMute[p]->load() > 0.5f)
        return;

    std::shared_ptr<const Sample> smp;
    {
        const juce::SpinLock::ScopedTryLockType l (sampleLock);
        if (l.isLocked())
            smp = samples[p];
    }

    const int group = padInfo (pad).chokeGroup;
    if (group >= 0)
        for (int o = 0; o < numPads; ++o)
            if (o != pad && padInfo (o).chokeGroup == group)
                voices[(size_t) o].choke();

    voices[p].trigger (pad, vel, pTune[p]->load(), pDecay[p]->load(), std::move (smp));

    const float angle = (pPan[p]->load() + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    const float level = pLevel[p]->load() * 1.4142f;
    gainL[p] = std::cos (angle) * level;
    gainR[p] = std::sin (angle) * level;
    padFlash[p].store (vel, std::memory_order_relaxed);
}

void VassaGruuvProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    int numEvents = 0;
    auto addEvent = [&] (int pos, int pad, float vel)
    {
        if (numEvents < (int) events.size())
            events[(size_t) numEvents++] = { juce::jlimit (0, n - 1, pos), pad, vel };
    };

    // --- Triggers from the UI ---
    {
        const auto scope = uiFifo.read (uiFifo.getNumReady());
        for (int i = 0; i < scope.blockSize1; ++i) addEvent (0, uiPads[(size_t) (scope.startIndex1 + i)], uiVels[(size_t) (scope.startIndex1 + i)]);
        for (int i = 0; i < scope.blockSize2; ++i) addEvent (0, uiPads[(size_t) (scope.startIndex2 + i)], uiVels[(size_t) (scope.startIndex2 + i)]);
    }

    // --- MIDI notes -> pads ---
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            const int pad = m.getNoteNumber() - firstMidiNote;
            if (pad >= 0 && pad < numPads)
                addEvent (meta.samplePosition, pad, m.getFloatVelocity());
        }
    }
    midi.clear();

    // --- Sequencer (host transport, or internal clock when the host is stopped / absent) ---
    double bpm = pTempo->load();
    bool running = false;
    double ppq0 = 0.0;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (pos->getIsPlaying() && pos->getPpqPosition().hasValue())
            {
                running = true;
                ppq0 = *pos->getPpqPosition();
                if (auto hostBpm = pos->getBpm())
                    bpm = *hostBpm;
            }

    if (! running && pRun->load() > 0.5f)
    {
        running = true;
        ppq0 = internalPpq;
    }

    if (running)
    {
        const double ppqPerSample = bpm / 60.0 / sr;
        const double ppq1 = ppq0 + n * ppqPerSample;
        const double swing = pSwing->load();

        currentStep.store ((int) (((long long) std::floor (ppq0 / stepPpq) % numSteps + numSteps) % numSteps));

        for (long long k = (long long) std::floor (ppq0 / stepPpq) - 1;; ++k)
        {
            const double t = (double) k * stepPpq + ((k & 1) ? swing * stepPpq / 3.0 : 0.0);
            if (t >= ppq1)
                break;
            if (t < ppq0)
                continue;

            const int step = (int) ((k % numSteps + numSteps) % numSteps);
            const int samplePos = (int) ((t - ppq0) / ppqPerSample);
            for (int p = 0; p < numPads; ++p)
                if (const int v = pattern.get (p, step))
                    addEvent (samplePos, p, (float) v / 127.0f);
        }

        if (pRun->load() > 0.5f)
            internalPpq = ppq1;
    }
    else
    {
        internalPpq = 0.0;
        currentStep.store (-1);
    }

    std::stable_sort (events.begin(), events.begin() + numEvents, [] (const Event& a, const Event& b) { return a.pos < b.pos; });

    // --- Render ---
    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getWritePointer (1);
    int pos = 0;

    auto renderTo = [&] (int end)
    {
        for (int i = pos; i < end; ++i)
        {
            float l = 0.0f, r = 0.0f;
            for (size_t p = 0; p < voices.size(); ++p)
                if (voices[p].isActive())
                {
                    float vl, vr;
                    voices[p].render (vl, vr);
                    l += vl * gainL[p];
                    r += vr * gainR[p];
                }
            outL[i] = l;
            outR[i] = r;
        }
        pos = end;
    };

    for (int e = 0; e < numEvents; ++e)
    {
        renderTo (events[(size_t) e].pos);
        startVoice (events[(size_t) e].pad, events[(size_t) e].vel);
    }
    renderTo (n);

    // --- Master chain: lowpass -> reverb -> gain -> soft clip ---
    const float cutoff = pFilter->load();
    if (cutoff < 19900.0f)
    {
        for (auto& f : lowpass)
            f.setCutoffFrequency (cutoff);
        for (int i = 0; i < n; ++i)
        {
            outL[i] = lowpass[0].processSample (0, outL[i]);
            outR[i] = lowpass[1].processSample (0, outR[i]);
        }
    }

    const float mix = pReverb->load();
    if (mix > 0.001f)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.55f;
        rp.damping = 0.5f;
        rp.width = 1.0f;
        rp.wetLevel = mix * 0.5f;
        rp.dryLevel = 1.0f;
        reverb.setParameters (rp);
        reverb.processStereo (outL, outR, n);
    }

    masterGain.setTargetValue (pMaster->load());
    for (int i = 0; i < n; ++i)
    {
        const float g = masterGain.getNextValue();
        outL[i] = std::tanh (outL[i] * g);
        outR[i] = std::tanh (outR[i] * g);
    }
}

// ---------------------------------------------------------------- UI API

void VassaGruuvProcessor::triggerPadFromUI (int pad, float velocity)
{
    const auto scope = uiFifo.write (1);
    if (scope.blockSize1 > 0)
    {
        uiPads[(size_t) scope.startIndex1] = pad;
        uiVels[(size_t) scope.startIndex1] = velocity;
    }
}

bool VassaGruuvProcessor::loadSample (int pad, const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples < 2 || reader->lengthInSamples > (juce::int64) (reader->sampleRate * 30.0))
        return false;

    auto s = std::make_shared<Sample>();
    const int channels = juce::jmin (2, (int) reader->numChannels);
    s->data.setSize (channels, (int) reader->lengthInSamples);
    reader->read (&s->data, 0, (int) reader->lengthInSamples, 0, true, channels > 1);
    s->rate = reader->sampleRate;
    s->path = file.getFullPathName();

    const juce::SpinLock::ScopedLockType l (sampleLock);
    samples[(size_t) pad] = std::move (s);
    return true;
}

void VassaGruuvProcessor::clearSample (int pad)
{
    const juce::SpinLock::ScopedLockType l (sampleLock);
    samples[(size_t) pad] = nullptr;
}

juce::String VassaGruuvProcessor::getSampleName (int pad) const
{
    const juce::SpinLock::ScopedLockType l (sampleLock);
    auto s = samples[(size_t) pad];
    return s != nullptr ? juce::File (s->path).getFileName() : juce::String();
}

void VassaGruuvProcessor::setParam (const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
}

void VassaGruuvProcessor::applyGroove (const GeneratedGroove& g)
{
    pattern.assign (g.steps);
    if (g.bpm)   setParam ("tempo", (float) *g.bpm);
    if (g.swing) setParam ("swing", *g.swing);
}

void VassaGruuvProcessor::generateLocal (int style, float density)
{
    const auto g = PatternGenerator::generate (style, density, rng);
    applyGroove (g);
    aiStatus = g.description;
}

void VassaGruuvProcessor::mutatePattern (float amount)
{
    auto grid = pattern.snapshot();
    PatternGenerator::mutate (grid, amount, rng);
    pattern.assign (grid);
    aiStatus = "Pattern mutated";
}

void VassaGruuvProcessor::clearPattern()
{
    pattern.clear();
    aiStatus = "Pattern cleared";
}

bool VassaGruuvProcessor::requestClaude (const juce::String& prompt)
{
    const auto key = getApiKey();
    if (key.isEmpty())
    {
        aiStatus = "Set your Anthropic API key first";
        return false;
    }
    if (! claude.request (key, prompt, pattern.snapshot(), (double) pTempo->load()))
        return false;
    aiStatus = "Asking Claude...";
    return true;
}

void VassaGruuvProcessor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    const auto r = claude.takeResult();
    if (r.ok)
    {
        applyGroove (r.groove);
        aiStatus = "Claude: " + r.message;
    }
    else
    {
        aiStatus = "Error: " + r.message;
    }
}

juce::String VassaGruuvProcessor::getApiKey() const
{
    if (auto* props = appProps.getUserSettings())
    {
        const auto stored = props->getValue ("apiKey");
        if (stored.isNotEmpty())
            return stored;
    }
    return juce::SystemStats::getEnvironmentVariable ("ANTHROPIC_API_KEY", {});
}

void VassaGruuvProcessor::setApiKey (const juce::String& key)
{
    if (auto* props = appProps.getUserSettings())
    {
        props->setValue ("apiKey", key.trim());
        props->saveIfNeeded();
    }
}

// ---------------------------------------------------------------- state

void VassaGruuvProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree root ("VassaGruuvState");
    root.setProperty ("version", 1, nullptr);
    root.setProperty ("pattern", pattern.toHex(), nullptr);
    root.appendChild (apvts.copyState(), nullptr);

    juce::ValueTree smp ("Samples");
    for (int i = 0; i < numPads; ++i)
    {
        std::shared_ptr<const Sample> s;
        {
            const juce::SpinLock::ScopedLockType l (sampleLock);
            s = samples[(size_t) i];
        }
        if (s != nullptr)
        {
            juce::ValueTree c ("Sample");
            c.setProperty ("pad", i, nullptr);
            c.setProperty ("path", s->path, nullptr);
            smp.appendChild (c, nullptr);
        }
    }
    root.appendChild (smp, nullptr);

    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, dest);
}

void VassaGruuvProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr)
        return;

    const auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("VassaGruuvState"))
        return;

    pattern.fromHex (root.getProperty ("pattern").toString());

    const auto params = root.getChildWithName (apvts.state.getType());
    if (params.isValid())
        apvts.replaceState (params);

    for (int i = 0; i < numPads; ++i)
        clearSample (i);
    for (const auto& c : root.getChildWithName ("Samples"))
    {
        const int pad = c.getProperty ("pad");
        const juce::File f (c.getProperty ("path").toString());
        if (pad >= 0 && pad < numPads && f.existsAsFile())
            loadSample (pad, f);
    }
}

juce::AudioProcessorEditor* VassaGruuvProcessor::createEditor()
{
    return new VassaGruuvEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VassaGruuvProcessor();
}
