#include "ClaudeClient.h"
#include "Engine/Drums.h"

namespace vg
{
bool parseGrooveJson (const juce::String& text, GeneratedGroove& out, juce::String& error)
{
    const int a = text.indexOfChar ('{');
    const int b = text.lastIndexOfChar ('}');
    if (a < 0 || b <= a)
    {
        error = "No JSON found in the reply";
        return false;
    }

    const auto json = juce::JSON::parse (text.substring (a, b + 1));
    const auto* rows = json["pattern"].getArray();
    if (rows == nullptr || rows->isEmpty())
    {
        error = "Reply has no \"pattern\" array";
        return false;
    }

    GeneratedGroove g;
    for (int p = 0; p < juce::jmin (numPads, rows->size()); ++p)
    {
        const auto* cols = (*rows)[p].getArray();
        if (cols == nullptr)
            continue;
        for (int s = 0; s < juce::jmin (numSteps, cols->size()); ++s)
            g.steps[(size_t) p][(size_t) s] = (uint8_t) juce::jlimit (0, 127, (int) (*cols)[s]);
    }

    if (json["bpm"].isDouble() || json["bpm"].isInt())
        g.bpm = juce::jlimit (40.0, 240.0, (double) json["bpm"]);
    if (json["swing"].isDouble() || json["swing"].isInt())
        g.swing = juce::jlimit (0.0f, 1.0f, (float) (double) json["swing"]);
    g.description = json["description"].toString();

    out = g;
    return true;
}

juce::String ClaudeClient::systemPrompt()
{
    juce::String pads;
    for (int i = 0; i < numPads; ++i)
        pads << i << " = " << padInfo (i).name << (i + 1 < numPads ? ", " : "");

    return "You are the groove designer inside a drum machine plug-in. The user describes a beat; you answer with ONE "
           "JSON object and nothing else (no prose, no code fences).\n"
           "The machine has 16 pads (rows) and a 16-step grid (one bar of 16th notes).\n"
           "Pads: " + pads + ".\n"
           "JSON format: {\"bpm\": number 40-240, \"swing\": number 0-1, \"description\": short string, "
           "\"pattern\": [16 rows in pad order, each an array of exactly 16 integers]}.\n"
           "Each integer is a velocity: 0 = no hit, 1-127 = hit. Use accents (e.g. 110-127 on strong beats, 60-90 on "
           "ghost notes), keep it musical and sparse where appropriate, and leave unused pads as all zeros.";
}

bool ClaudeClient::request (const juce::String& apiKey, const juce::String& userPrompt, const PatternGrid& current, double bpm)
{
    if (isThreadRunning())
        return false;

    juce::Array<juce::var> rows;
    for (const auto& row : current)
    {
        juce::Array<juce::var> cols;
        for (auto v : row)
            cols.add ((int) v);
        rows.add (cols);
    }

    {
        const juce::ScopedLock sl (lock);
        key = apiKey;
        prompt = userPrompt;
        currentJson = juce::JSON::toString (juce::var (rows), true);
        currentBpm = bpm;
    }

    startThread();
    return true;
}

ClaudeClient::Result ClaudeClient::takeResult()
{
    const juce::ScopedLock sl (lock);
    return result;
}

void ClaudeClient::run()
{
    juce::String k, p, cur;
    double bpm;
    {
        const juce::ScopedLock sl (lock);
        k = key;
        p = prompt;
        cur = currentJson;
        bpm = currentBpm;
    }

    Result res;
    auto finish = [&]
    {
        {
            const juce::ScopedLock sl (lock);
            result = res;
        }
        sendChangeMessage();
    };

    const juce::String userText = "Request: " + p + "\n\nCurrent tempo: " + juce::String (bpm, 1)
                                + " BPM.\nCurrent pattern (may be empty; use it as the starting point if the request "
                                  "asks to modify or extend it, otherwise ignore it):\n" + cur;

    auto* msg = new juce::DynamicObject();
    msg->setProperty ("role", "user");
    msg->setProperty ("content", userText);

    auto* root = new juce::DynamicObject();
    root->setProperty ("model", modelName());
    root->setProperty ("max_tokens", 2048);
    root->setProperty ("system", systemPrompt());
    root->setProperty ("messages", juce::Array<juce::var> { juce::var (msg) });
    const auto body = juce::JSON::toString (juce::var (root), true);

    int status = 0;
    const auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                             .withConnectionTimeoutMs (90000)
                             .withExtraHeaders ("content-type: application/json\r\nx-api-key: " + k
                                                + "\r\nanthropic-version: 2023-06-01\r\n")
                             .withHttpRequestCmd ("POST")
                             .withStatusCode (&status);

    auto stream = juce::URL ("https://api.anthropic.com/v1/messages").withPOSTData (body).createInputStream (options);
    if (stream == nullptr)
    {
        res.message = "Could not reach api.anthropic.com";
        finish();
        return;
    }

    const auto response = stream->readEntireStreamAsString();
    const auto json = juce::JSON::parse (response);

    if (status != 200)
    {
        const auto apiMsg = json["error"]["message"].toString();
        res.message = "API error " + juce::String (status) + (apiMsg.isNotEmpty() ? ": " + apiMsg : juce::String());
        finish();
        return;
    }

    juce::String text;
    if (const auto* blocks = json["content"].getArray())
        for (const auto& b : *blocks)
            text << b["text"].toString();

    juce::String error;
    if (parseGrooveJson (text, res.groove, error))
    {
        res.ok = true;
        res.message = res.groove.description.isNotEmpty() ? res.groove.description : juce::String ("done");
    }
    else
    {
        res.message = error;
    }
    finish();
}
} // namespace vg
