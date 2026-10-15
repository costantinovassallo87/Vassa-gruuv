#pragma once

#include "AI/ClaudeClient.h"
#include "AI/PatternGenerator.h"
#include "Engine/Exporter.h"
#include "Engine/Factory.h"
#include "Engine/Sampler.h"
#include "Engine/Scheduler.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class VassaGruuvProcessor : public juce::AudioProcessor,
                            private juce::ChangeListener
{
public:
    VassaGruuvProcessor();
    ~VassaGruuvProcessor() override;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Vassa Gruuv"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // --- Message-thread API used by the editor ---
    vg::TrackParams readTrackParams (int track, bool fromAudioThread = false) const;
    vg::EnvShape getEnv (int track, int kind) const;
    void setEnv (int track, int kind, const vg::EnvShape&);
    std::shared_ptr<const vg::Sample> getSample (int track) const;
    juce::String getTrackName (int track) const { return trackNames[(size_t) track]; }
    bool loadSample (int track, const juce::File& file);
    void resetToFactorySample (int track);
    float getPlayhead (int track) const { return samplers[(size_t) track].playhead.load (std::memory_order_relaxed); }

    void previewTrack (int track, float velocity = 1.0f);

    // Offline render of the current pattern to one WAV per track (multitrack stems).
    juce::Array<juce::File> exportStems (int loops, const juce::File& dir);

    // Pattern tools (the AI hooks live here)
    void generateLocal (int style, float density);
    void mutatePattern (float amount);
    void clearPattern();
    bool requestClaude (const juce::String& prompt);
    bool isClaudeBusy() const { return claude.isBusy(); }
    juce::String getApiKey() const;
    void setApiKey (const juce::String&);

    juce::AudioProcessorValueTreeState apvts;
    vg::Pattern pattern;
    std::atomic<int> currentStep { -1 };
    std::atomic<double> currentBpm { 120.0 };
    juce::String aiStatus; // message thread only

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static BusesProperties makeBuses();
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void applyGroove (const vg::GeneratedGroove&);
    void setParam (const juce::String& id, float plainValue);
    void startTrig (const vg::Trig&, bool anySolo);
    void setSample (int track, std::shared_ptr<const vg::Sample>);

    std::array<vg::TrackSampler, vg::numTracks> samplers;
    std::array<std::shared_ptr<const vg::Sample>, vg::numTracks> samples;
    std::vector<std::shared_ptr<const vg::Sample>> graveyard; // keeps replaced samples alive until the audio thread let go
    std::array<juce::String, vg::numTracks> trackNames; // message thread only
    mutable juce::SpinLock sampleLock;

    // Envelopes: edited on the message thread, snapshotted for the audio thread once per block
    using TrackEnvs = std::array<vg::EnvShape, vg::numEnvKinds>;
    mutable juce::SpinLock envLock;
    std::array<TrackEnvs, vg::numTracks> envs, envsAudio;
    std::atomic<int> envVersion { 0 };
    int envVersionAudio = 0;
    juce::AudioFormatManager formatManager;

    // UI -> audio trigger queue
    static constexpr int uiQueueSize = 64;
    juce::AbstractFifo uiFifo { uiQueueSize };
    std::array<vg::Trig, uiQueueSize> uiTrigs;

    std::array<vg::Trig, 2048> events;
    std::array<vg::Trig, 1024> scratchTrigs;

    std::array<std::array<std::atomic<float>*, vg::numTP>, vg::numTracks> tpPtr {};
    std::atomic<float>* pMaster = nullptr;
    std::atomic<float>* pTempo = nullptr;
    std::atomic<float>* pSwing = nullptr;
    std::atomic<float>* pFilter = nullptr;
    std::atomic<float>* pReverb = nullptr;
    std::atomic<float>* pRun = nullptr;

    double sr = 44100.0;
    double internalPpq = 0.0;
    int scratchCap = 1024;
    juce::AudioBuffer<float> scratch;
    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> lowpass;
    juce::Reverb reverb;
    juce::SmoothedValue<float> masterGain;

    vg::ClaudeClient claude;
    juce::Random rng;
    mutable juce::ApplicationProperties appProps;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VassaGruuvProcessor)
};
