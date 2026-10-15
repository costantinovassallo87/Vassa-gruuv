#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>

using namespace vg;

namespace
{
constexpr int firstMidiNote = 36; // C1 = track 1
}

juce::AudioProcessorValueTreeState::ParameterLayout VassaGruuvProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "run", 1 }, "Play", false));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "tempo", 1 }, "Tempo",
                                                             juce::NormalisableRange<float> (40.f, 240.f, 0.1f), 120.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "swing", 1 }, "Swing",
                                                             juce::NormalisableRange<float> (0.f, 1.f, 0.01f), 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "filter", 1 }, "Master Filter",
                                                             juce::NormalisableRange<float> (80.f, 20000.f, 1.f, 0.25f), 20000.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "reverb", 1 }, "Reverb",
                                                             juce::NormalisableRange<float> (0.f, 1.f, 0.01f), 0.12f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "master", 1 }, "Master",
                                                             juce::NormalisableRange<float> (0.f, 1.5f, 0.01f), 0.8f));

    for (int t = 0; t < numTracks; ++t)
        for (int i = 0; i < numTP; ++i)
        {
            const auto& sp = tpSpec (i);
            const juce::ParameterID id { trackParamId (t, i), 1 };
            const juce::String name = "Track " + juce::String (t + 1) + " " + sp.name;

            switch (sp.type)
            {
                case TPType::Float:
                    layout.add (std::make_unique<juce::AudioParameterFloat> (
                        id, name, juce::NormalisableRange<float> (sp.min, sp.max, 0.0f, sp.skew), sp.def,
                        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([i] (float v, int) { return formatTP (tpSpec (i), v); })));
                    break;
                case TPType::Int:
                    layout.add (std::make_unique<juce::AudioParameterInt> (id, name, (int) sp.min, (int) sp.max, (int) sp.def,
                                                                           juce::AudioParameterIntAttributes().withStringFromValueFunction ([i] (int v, int) { return formatTP (tpSpec (i), (float) v); })));
                    break;
                case TPType::Bool:
                    layout.add (std::make_unique<juce::AudioParameterBool> (id, name, sp.def > 0.5f));
                    break;
                case TPType::Choice:
                    layout.add (std::make_unique<juce::AudioParameterChoice> (id, name, juce::StringArray::fromTokens (sp.choices, "|", ""),
                                                                              (int) sp.def));
                    break;
            }
        }
    return layout;
}

juce::AudioProcessor::BusesProperties VassaGruuvProcessor::makeBuses()
{
    auto props = BusesProperties().withOutput ("Main Mix", juce::AudioChannelSet::stereo(), true);
    for (int t = 0; t < numTracks; ++t)
        props = props.withOutput ("Track " + juce::String (t + 1), juce::AudioChannelSet::stereo(), false);
    return props;
}

VassaGruuvProcessor::VassaGruuvProcessor()
    : AudioProcessor (makeBuses()),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (int t = 0; t < numTracks; ++t)
        for (int i = 0; i < numTP; ++i)
            tpPtr[(size_t) t][(size_t) i] = apvts.getRawParameterValue (trackParamId (t, i));

    pMaster = apvts.getRawParameterValue ("master");
    pTempo  = apvts.getRawParameterValue ("tempo");
    pSwing  = apvts.getRawParameterValue ("swing");
    pFilter = apvts.getRawParameterValue ("filter");
    pReverb = apvts.getRawParameterValue ("reverb");
    pRun    = apvts.getRawParameterValue ("run");

    for (auto& te : envs)
        for (int k = 0; k < numEnvKinds; ++k)
            te[(size_t) k] = defaultEnv (k);
    envsAudio = envs;

    formatManager.registerBasicFormats();
    for (int t = 0; t < numTracks; ++t)
        resetToFactorySample (t);

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
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    for (const auto& bus : layouts.outputBuses)
        if (! bus.isDisabled() && bus != juce::AudioChannelSet::stereo())
            return false;
    return layouts.inputBuses.isEmpty() || layouts.getMainInputChannelSet().isDisabled();
}

void VassaGruuvProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    for (auto& s : samplers)
        s.reset();

    scratchCap = juce::jmax (samplesPerBlock, 1024);
    scratch.setSize (2, scratchCap);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) scratchCap, 1 };
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

TrackParams VassaGruuvProcessor::readTrackParams (int track, bool fromAudioThread) const
{
    TrackParams p;
    for (int i = 0; i < numTP; ++i)
        p.v[i] = tpPtr[(size_t) track][(size_t) i]->load (std::memory_order_relaxed);

    if (fromAudioThread)
    {
        for (int k = 0; k < numEnvKinds; ++k)
            p.env[k] = envsAudio[(size_t) track][(size_t) k];
    }
    else
    {
        const juce::SpinLock::ScopedLockType l (envLock);
        for (int k = 0; k < numEnvKinds; ++k)
            p.env[k] = envs[(size_t) track][(size_t) k];
    }
    return p;
}

EnvShape VassaGruuvProcessor::getEnv (int track, int kind) const
{
    const juce::SpinLock::ScopedLockType l (envLock);
    return envs[(size_t) track][(size_t) kind];
}

void VassaGruuvProcessor::setEnv (int track, int kind, const EnvShape& shape)
{
    auto e = shape;
    e.sanitize (kind);
    {
        const juce::SpinLock::ScopedLockType l (envLock);
        envs[(size_t) track][(size_t) kind] = e;
    }
    envVersion.fetch_add (1);
}

void VassaGruuvProcessor::startTrig (const Trig& e, bool anySolo)
{
    const auto t = (size_t) e.track;
    if (e.noteOff)
    {
        samplers[t].releaseHeld();
        return;
    }

    if (tpPtr[t][TPMute]->load() > 0.5f || (anySolo && tpPtr[t][TPSolo]->load() < 0.5f))
        return;

    std::shared_ptr<const Sample> smp;
    {
        const juce::SpinLock::ScopedTryLockType l (sampleLock);
        if (l.isLocked())
            smp = samples[t];
    }
    if (smp == nullptr)
        return;

    samplers[t].noteOn (readTrackParams (e.track, true), std::move (smp), e.vel, e.pitch, e.slice, e.gate, sr);
}

void VassaGruuvProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    auto mainBus = getBusBuffer (buffer, false, 0);
    float* mainL = mainBus.getWritePointer (0);
    float* mainR = mainBus.getWritePointer (1);

    // Output routing: a track goes to its own bus when the host enabled it, otherwise into the main mix.
    std::array<float*, numTracks> dstL {}, dstR {};
    for (int t = 0; t < numTracks; ++t)
    {
        dstL[(size_t) t] = mainL;
        dstR[(size_t) t] = mainR;
        if (auto* bus = getBus (false, t + 1); bus != nullptr && bus->isEnabled())
        {
            auto bb = getBusBuffer (buffer, false, t + 1);
            dstL[(size_t) t] = bb.getWritePointer (0);
            dstR[(size_t) t] = bb.getWritePointer (1);
        }
    }

    if (const int v = envVersion.load(); v != envVersionAudio)
    {
        const juce::SpinLock::ScopedTryLockType l (envLock);
        if (l.isLocked())
        {
            envsAudio = envs;
            envVersionAudio = v;
        }
    }

    int numEvents = 0;
    auto addEvent = [&] (Trig e)
    {
        if (numEvents < (int) events.size())
        {
            e.pos = juce::jlimit (0, n - 1, e.pos);
            events[(size_t) numEvents++] = e;
        }
    };

    // UI previews
    {
        const auto scope = uiFifo.read (uiFifo.getNumReady());
        for (int i = 0; i < scope.blockSize1; ++i) addEvent (uiTrigs[(size_t) (scope.startIndex1 + i)]);
        for (int i = 0; i < scope.blockSize2; ++i) addEvent (uiTrigs[(size_t) (scope.startIndex2 + i)]);
    }

    // MIDI notes -> tracks
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int track = m.getNoteNumber() - firstMidiNote;
        if (track < 0 || track >= numTracks)
            continue;
        if (m.isNoteOn())
        {
            Trig e;
            e.pos = meta.samplePosition;
            e.track = track;
            e.vel = m.getFloatVelocity();
            e.gate = -1;
            e.slice = 0;
            addEvent (e);
        }
        else if (m.isNoteOff())
        {
            Trig e;
            e.pos = meta.samplePosition;
            e.track = track;
            e.noteOff = true;
            addEvent (e);
        }
    }
    midi.clear();

    // Sequencer: host transport, or the internal clock when the host is stopped / absent
    double bpm = pTempo->load();
    bool running = false;
    double ppq0 = 0.0;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto hostBpm = pos->getBpm())
                bpm = *hostBpm;
            if (pos->getIsPlaying() && pos->getPpqPosition().hasValue())
            {
                running = true;
                ppq0 = *pos->getPpqPosition();
            }
        }

    if (! running && pRun->load() > 0.5f)
    {
        running = true;
        ppq0 = internalPpq;
    }
    currentBpm.store (bpm, std::memory_order_relaxed);

    if (running)
    {
        const double ppq1 = ppq0 + n * (bpm / 60.0 / sr);
        currentStep.store ((int) (((long long) std::floor (ppq0 / stepPpq) % numSteps + numSteps) % numSteps));

        const int num = collectTrigs (pattern, ppq0, ppq1, bpm, pSwing->load(), sr, scratchTrigs.data(), (int) scratchTrigs.size());
        for (int i = 0; i < num; ++i)
            addEvent (scratchTrigs[(size_t) i]);

        internalPpq = pRun->load() > 0.5f ? ppq1 : 0.0;
    }
    else
    {
        internalPpq = 0.0;
        currentStep.store (-1);
    }

    std::stable_sort (events.begin(), events.begin() + numEvents, [] (const Trig& a, const Trig& b) { return a.pos < b.pos; });

    bool anySolo = false;
    for (int t = 0; t < numTracks; ++t)
        anySolo |= tpPtr[(size_t) t][TPSolo]->load() > 0.5f;

    // Render, splitting the block at each trigger
    int pos = 0;
    auto renderTo = [&] (int end)
    {
        while (pos < end)
        {
            const int cnt = juce::jmin (end - pos, scratchCap);
            for (int t = 0; t < numTracks; ++t)
            {
                auto& s = samplers[(size_t) t];
                if (! s.isActive())
                {
                    s.playhead.store (-1.0f, std::memory_order_relaxed);
                    continue;
                }
                float* sl = scratch.getWritePointer (0);
                float* sr2 = scratch.getWritePointer (1);
                juce::FloatVectorOperations::clear (sl, cnt);
                juce::FloatVectorOperations::clear (sr2, cnt);
                s.render (sl, sr2, cnt);
                juce::FloatVectorOperations::add (dstL[(size_t) t] + pos, sl, cnt);
                juce::FloatVectorOperations::add (dstR[(size_t) t] + pos, sr2, cnt);
            }
            pos += cnt;
        }
    };

    for (int e = 0; e < numEvents; ++e)
    {
        renderTo (events[(size_t) e].pos);
        startTrig (events[(size_t) e], anySolo);
    }
    renderTo (n);

    // Master chain on the main mix only: lowpass -> reverb -> gain -> soft clip
    const float cutoff = pFilter->load();
    if (cutoff < 19900.0f)
    {
        for (auto& f : lowpass)
            f.setCutoffFrequency (cutoff);
        for (int i = 0; i < n; ++i)
        {
            mainL[i] = lowpass[0].processSample (0, mainL[i]);
            mainR[i] = lowpass[1].processSample (0, mainR[i]);
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
        reverb.processStereo (mainL, mainR, n);
    }

    masterGain.setTargetValue (pMaster->load());
    for (int i = 0; i < n; ++i)
    {
        const float g = masterGain.getNextValue();
        mainL[i] = std::tanh (mainL[i] * g);
        mainR[i] = std::tanh (mainR[i] * g);
    }
}

// ---------------------------------------------------------------- samples

std::shared_ptr<const Sample> VassaGruuvProcessor::getSample (int track) const
{
    const juce::SpinLock::ScopedLockType l (sampleLock);
    return samples[(size_t) track];
}

void VassaGruuvProcessor::setSample (int track, std::shared_ptr<const Sample> s)
{
    trackNames[(size_t) track] = s != nullptr ? s->name : juce::String();
    {
        const juce::SpinLock::ScopedLockType l (sampleLock);
        if (samples[(size_t) track] != nullptr)
            graveyard.push_back (std::move (samples[(size_t) track]));
        samples[(size_t) track] = std::move (s);
    }
    // Released here (message thread) once no voice references them any more
    graveyard.erase (std::remove_if (graveyard.begin(), graveyard.end(), [] (const auto& p) { return p.use_count() == 1; }), graveyard.end());
}

bool VassaGruuvProcessor::loadSample (int track, const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples < 64 || reader->lengthInSamples > (juce::int64) (reader->sampleRate * 60.0))
        return false;

    auto s = std::make_shared<Sample>();
    const int channels = juce::jmin (2, (int) reader->numChannels);
    s->data.setSize (channels, (int) reader->lengthInSamples);
    reader->read (&s->data, 0, (int) reader->lengthInSamples, 0, true, channels > 1);
    s->rate = reader->sampleRate;
    s->path = file.getFullPathName();
    s->name = file.getFileNameWithoutExtension();
    s->detectOnsets();
    setSample (track, std::move (s));
    return true;
}

void VassaGruuvProcessor::resetToFactorySample (int track)
{
    setSample (track, makeFactorySample (track));
}

void VassaGruuvProcessor::previewTrack (int track, float velocity)
{
    const auto scope = uiFifo.write (1);
    if (scope.blockSize1 > 0)
    {
        Trig e;
        e.track = track;
        e.vel = velocity;
        e.gate = (int) (sr * 0.5);
        uiTrigs[(size_t) scope.startIndex1] = e;
    }
}

juce::Array<juce::File> VassaGruuvProcessor::exportStems (int loops, const juce::File& dir)
{
    ExportInput in;
    in.pattern = &pattern;
    in.bpm = currentBpm.load();
    in.swing = apvts.getRawParameterValue ("swing")->load();
    in.sampleRate = sr;
    for (int t = 0; t < numTracks; ++t)
    {
        in.params[(size_t) t] = readTrackParams (t);
        in.samples[(size_t) t] = getSample (t);
        in.names[(size_t) t] = trackNames[(size_t) t];
    }
    return vg::exportStems (in, loops, dir, "VassaGruuv");
}

// ---------------------------------------------------------------- pattern tools

void VassaGruuvProcessor::setParam (const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
}

void VassaGruuvProcessor::applyGroove (const GeneratedGroove& g)
{
    pattern.clearMods();
    pattern.assignVelocities (g.steps);
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
    auto grid = pattern.snapshotVelocities();
    PatternGenerator::mutate (grid, amount, rng);
    pattern.assignVelocities (grid);
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
    if (! claude.request (key, prompt, pattern.snapshotVelocities(), (double) pTempo->load()))
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
    root.setProperty ("version", 2, nullptr);
    root.setProperty ("pattern", pattern.toHex(), nullptr);
    root.appendChild (apvts.copyState(), nullptr);

    juce::ValueTree smp ("Samples");
    for (int t = 0; t < numTracks; ++t)
        if (auto s = getSample (t); s != nullptr && ! s->factory)
        {
            juce::ValueTree c ("Sample");
            c.setProperty ("track", t, nullptr);
            c.setProperty ("path", s->path, nullptr);
            smp.appendChild (c, nullptr);
        }
    root.appendChild (smp, nullptr);

    juce::ValueTree envTree ("Envelopes");
    for (int t = 0; t < numTracks; ++t)
        for (int k = 0; k < numEnvKinds; ++k)
        {
            juce::ValueTree c ("Env");
            c.setProperty ("track", t, nullptr);
            c.setProperty ("kind", k, nullptr);
            c.setProperty ("data", getEnv (t, k).toString(), nullptr);
            envTree.appendChild (c, nullptr);
        }
    root.appendChild (envTree, nullptr);

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

    for (int t = 0; t < numTracks; ++t)
        for (int k = 0; k < numEnvKinds; ++k)
            setEnv (t, k, defaultEnv (k));
    for (const auto& c : root.getChildWithName ("Envelopes"))
    {
        const int t = c.getProperty ("track"), k = c.getProperty ("kind");
        if (t >= 0 && t < numTracks && k >= 0 && k < numEnvKinds)
            setEnv (t, k, EnvShape::fromString (c.getProperty ("data").toString(), k));
    }

    for (int t = 0; t < numTracks; ++t)
        resetToFactorySample (t);
    for (const auto& c : root.getChildWithName ("Samples"))
    {
        const int t = c.getProperty ("track");
        const juce::File f (c.getProperty ("path").toString());
        if (t >= 0 && t < numTracks && f.existsAsFile())
            loadSample (t, f);
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
