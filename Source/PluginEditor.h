#pragma once

#include "PluginProcessor.h"

namespace vgui
{
namespace col
{
    const juce::Colour bg      { 0xff14151a };
    const juce::Colour panel   { 0xff1f2128 };
    const juce::Colour panelHi { 0xff2b2e38 };
    const juce::Colour accent  { 0xffff7a1a };
    const juce::Colour text    { 0xffe8e8ea };
    const juce::Colour dim     { 0xff8b909c };
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
};

// 4x4 pad grid, pad 1 at the bottom-left like on Maschine.
class PadGrid : public juce::Component,
                public juce::FileDragAndDropTarget
{
public:
    PadGrid (VassaGruuvProcessor& p, std::function<void (int)> onSelect) : proc (p), select (std::move (onSelect)) {}

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray&, int x, int y) override;

    int selected = 0;
    std::array<float, vg::numPads> flash {};
    std::function<void()> onSampleChanged;

private:
    juce::Rectangle<int> padBounds (int pad) const;
    int padAt (juce::Point<int>) const;

    VassaGruuvProcessor& proc;
    std::function<void (int)> select;
};

// 16 lanes x 16 steps. Click/drag to paint, wheel to change velocity, click a lane name to select the pad.
class StepGrid : public juce::Component
{
public:
    StepGrid (VassaGruuvProcessor& p, std::function<void (int)> onSelect) : proc (p), select (std::move (onSelect)) {}

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    int selected = 0;
    int playhead = -1;

private:
    static constexpr int labelW = 64, headerH = 18;
    bool cellAt (juce::Point<int>, int& pad, int& step) const;

    VassaGruuvProcessor& proc;
    std::function<void (int)> select;
    uint8_t paintValue = 100;
    int lastPad = -1, lastStep = -1;
};

struct Knob
{
    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    void init (juce::Component& parent, const juce::String& text);
    void attach (juce::AudioProcessorValueTreeState& apvts, const juce::String& id);
    void setBounds (juce::Rectangle<int> r);
};
} // namespace vgui

class VassaGruuvEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit VassaGruuvEditor (VassaGruuvProcessor&);
    ~VassaGruuvEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void selectPad (int pad);
    void refreshSampleInfo();
    void chooseSample();
    void showApiKeyDialog();
    void askClaude();

    VassaGruuvProcessor& proc;
    vgui::LookAndFeel laf;

    // Global controls
    juce::ToggleButton playButton { "PLAY" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> playAttachment;
    vgui::Knob tempo, swing, filter, reverb, master;

    // Pads / steps
    vgui::PadGrid padGrid;
    vgui::StepGrid stepGrid;

    // Selected pad strip
    juce::Label padTitle, sampleInfo;
    vgui::Knob level, pan, tune, decay;
    juce::ToggleButton muteButton { "Mute" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAttachment;
    juce::TextButton loadButton { "Load sample" }, clearSampleButton { "Clear" };
    std::unique_ptr<juce::FileChooser> chooser;

    // AI panel
    juce::Label aiTitle, statusLabel;
    juce::TextEditor promptBox;
    juce::TextButton askButton { "Ask Claude" }, keyButton { "API key..." };
    juce::ComboBox styleBox;
    juce::Slider densitySlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::TextButton generateButton { "Generate" }, mutateButton { "Mutate" }, clearButton { "Clear pattern" };

    int selectedPad = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VassaGruuvEditor)
};
