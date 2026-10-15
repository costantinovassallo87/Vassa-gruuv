#pragma once

#include "PluginProcessor.h"

namespace vgui
{
namespace col
{
    const juce::Colour bg      { 0xff101116 };
    const juce::Colour panel   { 0xff1a1c23 };
    const juce::Colour panel2  { 0xff22252e };
    const juce::Colour panelHi { 0xff2d313c };
    const juce::Colour border  { 0xff2f3340 };
    const juce::Colour accent  { 0xffff7a1a };
    const juce::Colour accent2 { 0xff3ec1d3 };
    const juce::Colour purple  { 0xffb07cff };
    const juce::Colour green   { 0xff4cd964 };
    const juce::Colour text    { 0xffeceef2 };
    const juce::Colour dim     { 0xff8b909c };

    // One colour per track, grouped by instrument family
    inline juce::Colour track (int t)
    {
        static const juce::uint32 c[16] = { 0xffff7a1a, 0xffff7a1a, 0xffffb020, 0xffffb020, 0xffffb020, 0xff3ec1d3, 0xff3ec1d3, 0xff3fcf8e,
                                            0xff3fcf8e, 0xffb07cff, 0xff3ec1d3, 0xff3ec1d3, 0xffff6fa5, 0xffff6fa5, 0xffff6fa5, 0xffff6fa5 };
        return juce::Colour (c[juce::jlimit (0, 15, t)]);
    }
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

// Section panel with a title chip
void drawPanel (juce::Graphics&, juce::Rectangle<int>, const juce::String& title, int titleWidth = 120);

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
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { viewStart = 0.0; viewEnd = 1.0; repaint(); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray&, int, int) override;

    int track = 0, envKind = vg::EnvAmp; // which ADSR is drawn / edited on top of the waveform
    std::function<void()> onSampleChanged;

private:
    static constexpr int rulerH = 16;
    float toX (double n) const { return (float) ((n - viewStart) / (viewEnd - viewStart) * getWidth()); }
    double toN (float x) const { return viewStart + (double) x / getWidth() * (viewEnd - viewStart); }
    int waveHeight() const { return getHeight() - rulerH; }
    int handleAt (int x) const;
    juce::RangedAudioParameter* param (int tp) const;
    void rebuildPeaks();
    float gateMs() const;
    int envNodeAt (juce::Point<float>, const vg::TrackParams&) const;
    void setPlain (int tp, float plain);
    void drawFlag (juce::Graphics&, float x, juce::Colour, bool left, bool highlighted, const juce::String& label) const;

    VassaGruuvProcessor& proc;
    std::shared_ptr<const vg::Sample> cached;
    int cachedWidth = -1;
    double cachedStart = -1.0, cachedEnd = -1.0, viewStart = 0.0, viewEnd = 1.0;
    std::vector<std::pair<float, float>> peaks;
    int dragging = -1, hover = -1;           // vg::TP of the start/end/loop marker
    int envDragging = -1, envHover = -1;     // 0 = attack, 1 = decay/sustain, 2 = release node
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

// The whole UI lives in a fixed design size (designW x designH); the editor scales it to the window.
class MainPanel : public juce::Component,
                  private juce::Timer
{
public:
    static constexpr int designW = 1000, designH = 860;

    explicit MainPanel (VassaGruuvProcessor&);
    ~MainPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    void timerCallback() override;
    void selectTrack (int track);
    void selectLane (int lane);
    void layoutSamplerControls();
    void layoutEnvKnobs();
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
    std::array<juce::TextButton, vg::numEnvKinds> envTabs;
    int envKindSel = vg::EnvAmp;
    juce::ComboBox fTypeBox, lDestBox, sModeBox;
    std::unique_ptr<APVTS::ComboBoxAttachment> fTypeAtt, lDestAtt, sModeAtt;
    juce::Rectangle<int> groupRects[7];
    juce::Rectangle<int> samplerHeader;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainPanel)
};

class VassaGruuvEditor : public juce::AudioProcessorEditor
{
public:
    explicit VassaGruuvEditor (VassaGruuvProcessor&);
    void resized() override;

private:
    juce::ComponentBoundsConstrainer constrainer;
    MainPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VassaGruuvEditor)
};
