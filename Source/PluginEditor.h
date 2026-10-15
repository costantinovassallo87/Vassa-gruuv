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
    const juce::Colour accent2 { 0xff3ec1d3 };
    const juce::Colour text    { 0xffe8e8ea };
    const juce::Colour dim     { 0xff8b909c };
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
};

constexpr int gridLabelW = 140; // shared by the step and modulation grids so their columns line up

class StepGrid : public juce::Component
{
public:
    StepGrid (VassaGruuvProcessor& p, std::function<void (int)> onSelect);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

    int selected = 0;
    int playhead = -1;

private:
    static constexpr int headerH = 14;
    bool cellAt (juce::Point<int>, int& track, int& step) const;
    void toggleParam (int track, int tp);

    VassaGruuvProcessor& proc;
    std::function<void (int)> select;
    std::array<std::atomic<float>*, vg::numTracks> mutePtr {}, soloPtr {};
    int paintValue = 100, lastTrack = -1, lastStep = -1;
};

// Bar editor for one modulation lane of the selected track.
class ModGrid : public juce::Component
{
public:
    explicit ModGrid (VassaGruuvProcessor& p) : proc (p) {}

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    int track = 0, lane = vg::Velocity;

private:
    juce::Rectangle<float> barArea() const;
    int stepAt (int x) const;
    void setFromMouse (const juce::MouseEvent&);

    VassaGruuvProcessor& proc;
};

class WaveformView : public juce::Component,
                     public juce::FileDragAndDropTarget
{
public:
    explicit WaveformView (VassaGruuvProcessor& p) : proc (p) {}

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray&, int, int) override;

    int track = 0;
    std::function<void()> onSampleChanged;

private:
    int handleAt (int x) const;
    juce::RangedAudioParameter* param (int tp) const;
    void rebuildPeaks();

    VassaGruuvProcessor& proc;
    std::shared_ptr<const vg::Sample> cached;
    int cachedWidth = -1;
    std::vector<std::pair<float, float>> peaks;
    int dragging = -1; // vg::TP being dragged
};

struct Knob
{
    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    void init (juce::Component& parent, const juce::String& text);
    void attach (juce::AudioProcessorValueTreeState& apvts, const juce::String& id);
    void setBounds (int x, int y, int w, int h);
    void setVisible (bool v) { slider.setVisible (v); label.setVisible (v); }
};

class DragHandle : public juce::Component
{
public:
    std::function<void()> onDrag;
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent& e) override { if (e.getDistanceFromDragStart() > 4 && onDrag) onDrag(); }
    void mouseEnter (const juce::MouseEvent&) override { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
};
} // namespace vgui

class VassaGruuvEditor : public juce::AudioProcessorEditor,
                         public juce::DragAndDropContainer,
                         private juce::Timer
{
public:
    explicit VassaGruuvEditor (VassaGruuvProcessor&);
    ~VassaGruuvEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    void timerCallback() override;
    void selectTrack (int track);
    void selectLane (int lane);
    void layoutSamplerControls();
    void chooseSample();
    void startStemDrag();
    void exportStemsToFolder();
    void bindCombo (juce::ComboBox&, std::unique_ptr<APVTS::ComboBoxAttachment>&, int tp);
    void setChoice (int tp, int index);

    VassaGruuvProcessor& proc;
    vgui::LookAndFeel laf;
    int selectedTrack = 0, selectedLane = 0, shownKey = -1;
    bool dragInProgress = false;

    // Top bar
    juce::ToggleButton playButton { "PLAY" };
    std::unique_ptr<APVTS::ButtonAttachment> playAttachment;
    vgui::Knob tempo, swing, filter, reverb, master;
    vgui::DragHandle dragHandle;
    juce::ComboBox loopsBox;
    juce::TextButton exportButton { "Export..." };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File lastExportDir;

    // Step sequencer
    juce::ComboBox styleBox;
    juce::TextButton generateButton { "Generate" }, mutateButton { "Mutate" }, clearButton { "Clear" };
    vgui::StepGrid stepGrid;
    juce::Label statusLabel;

    // Modulation sequencer
    std::array<juce::TextButton, vg::numLanes> laneButtons;
    vgui::ModGrid modGrid;

    // Sampler
    juce::Label trackTitle, sampleInfo;
    std::array<juce::TextButton, 3> modeButtons;
    juce::TextButton loadButton { "Load" }, factoryButton { "Factory" }, previewButton { "Preview" };
    juce::ToggleButton loopToggle { "Loop" }, reverseToggle { "Reverse" };
    std::unique_ptr<APVTS::ButtonAttachment> loopAttachment, reverseAttachment;
    vgui::WaveformView waveform;
    vgui::Knob knobs[vg::numTP];
    juce::ComboBox fTypeBox, lDestBox, sModeBox;
    std::unique_ptr<APVTS::ComboBoxAttachment> fTypeAtt, lDestAtt, sModeAtt;
    juce::Rectangle<int> groupRects[6];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VassaGruuvEditor)
};
