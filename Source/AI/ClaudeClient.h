#pragma once

#include "PatternGenerator.h"
#include <juce_events/juce_events.h>

namespace vg
{
// Parses the JSON groove format that Claude is asked to produce (tolerates code fences / extra prose).
bool parseGrooveJson (const juce::String& text, GeneratedGroove& out, juce::String& error);

// Asks Claude (Anthropic Messages API) for a groove on a background thread.
// Sends a change message on the message thread when the result is ready.
class ClaudeClient : public juce::Thread,
                     public juce::ChangeBroadcaster
{
public:
    struct Result
    {
        bool ok = false;
        juce::String message;
        GeneratedGroove groove;
    };

    ClaudeClient() : juce::Thread ("ClaudeClient") {}
    ~ClaudeClient() override { stopThread (3000); }

    // Returns false if a request is already running.
    bool request (const juce::String& apiKey, const juce::String& prompt, const PatternGrid& current, double bpm);
    Result takeResult();
    bool isBusy() const { return isThreadRunning(); }

    static juce::String systemPrompt();
    static juce::String modelName() { return "claude-sonnet-5-5"; }

private:
    void run() override;

    juce::CriticalSection lock;
    juce::String key, prompt, currentJson;
    double currentBpm = 120.0;
    Result result;
};
} // namespace vg
