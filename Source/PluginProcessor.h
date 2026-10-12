#pragma once

#include "AI/ClaudeClient.h"
#include "AI/PatternGenerator.h"
#include "Engine/Drums.h"
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

    // Parameter ids
    static juce::String padId (int pad, const char* what) { return "p" + juce::String (pad) + "_" + what; }

    // --- Message-thread API used by the editor ---
    void triggerPadFromUI (int pad, float velocity);
    bool loadSample (int pad, const juce::File& file);
    void clearSample (int pad);
    juce::String getSampleName (int pad) const;

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
    std::array<std::atomic<float>, vg::numPads> padFlash;
    juce::String aiStatus; // message thread only

private:
    struct Event { int pos; int pad; float vel; };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void applyGroove (const vg::GeneratedGroove&);
    void setParam (const juce::String& id, float plainValue);
    void startVoice (int pad, float vel);

    std::array<vg::DrumVoice, vg::numPads> voices;
    std::array<float, vg::numPads> gainL {}, gainR {};

    std::array<std::shared_ptr<const vg::Sample>, vg::numPads> samples;
    mutable juce::SpinLock sampleLock;
    juce::AudioFormatManager formatManager;

    // UI -> audio trigger queue
    static constexpr int uiQueueSize = 64;
    juce::AbstractFifo uiFifo { uiQueueSize };
    std::array<int, uiQueueSize> uiPads {};
    std::array<float, uiQueueSize> uiVels {};

    std::array<Event, 1024> events;

    // Cached parameter pointers
    std::array<std::atomic<float>*, vg::numPads> pLevel {}, pPan {}, pTune {}, pDecay {}, pMute {};
    std::atomic<float>* pMaster = nullptr;
    std::atomic<float>* pTempo = nullptr;
    std::atomic<float>* pSwing = nullptr;
    std::atomic<float>* pFilter = nullptr;
    std::atomic<float>* pReverb = nullptr;
    std::atomic<float>* pRun = nullptr;

    double sr = 44100.0;
    double internalPpq = 0.0;
    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> lowpass;
    juce::Reverb reverb;
    juce::SmoothedValue<float> masterGain;

    vg::ClaudeClient claude;
    juce::Random rng;
    mutable juce::ApplicationProperties appProps;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VassaGruuvProcessor)
};
