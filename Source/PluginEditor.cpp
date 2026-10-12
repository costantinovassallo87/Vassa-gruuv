#include "PluginEditor.h"

using namespace vgui;

// ------------------------------------------------------------ LookAndFeel

LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, col::bg);
    setColour (juce::Slider::rotarySliderFillColourId, col::accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, col::panelHi);
    setColour (juce::Slider::thumbColourId, col::text);
    setColour (juce::Slider::trackColourId, col::accent);
    setColour (juce::Slider::backgroundColourId, col::panelHi);
    setColour (juce::Slider::textBoxTextColourId, col::dim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, col::text);
    setColour (juce::TextButton::buttonColourId, col::panelHi);
    setColour (juce::TextButton::buttonOnColourId, col::accent);
    setColour (juce::TextButton::textColourOffId, col::text);
    setColour (juce::ToggleButton::textColourId, col::text);
    setColour (juce::ToggleButton::tickColourId, col::accent);
    setColour (juce::ComboBox::backgroundColourId, col::panelHi);
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::backgroundColourId, col::panel);
    setColour (juce::TextEditor::textColourId, col::text);
    setColour (juce::TextEditor::outlineColourId, col::panelHi);
    setColour (juce::TextEditor::focusedOutlineColourId, col::accent);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
}

// ------------------------------------------------------------ Knob

void Knob::init (juce::Component& parent, const juce::String& text)
{
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 14);
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (11.0f));
    label.setColour (juce::Label::textColourId, col::dim);
    parent.addAndMakeVisible (slider);
    parent.addAndMakeVisible (label);
}

void Knob::attach (juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    attachment.reset();
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider);
}

void Knob::setBounds (juce::Rectangle<int> r)
{
    label.setBounds (r.removeFromTop (14));
    slider.setBounds (r);
}

// ------------------------------------------------------------ PadGrid

juce::Rectangle<int> PadGrid::padBounds (int pad) const
{
    const int w = getWidth() / 4, h = getHeight() / 4;
    const int col = pad % 4, row = 3 - pad / 4;
    return { col * w, row * h, w, h };
}

int PadGrid::padAt (juce::Point<int> p) const
{
    const int w = getWidth() / 4, h = getHeight() / 4;
    if (p.x < 0 || p.y < 0 || p.x >= w * 4 || p.y >= h * 4)
        return -1;
    return (3 - p.y / h) * 4 + p.x / w;
}

void PadGrid::paint (juce::Graphics& g)
{
    for (int i = 0; i < vg::numPads; ++i)
    {
        const auto r = padBounds (i).reduced (3).toFloat();
        const float f = flash[(size_t) i];

        g.setColour (col::panel.interpolatedWith (col::accent, f * 0.9f));
        g.fillRoundedRectangle (r, 6.0f);

        if (i == selected)
        {
            g.setColour (col::accent);
            g.drawRoundedRectangle (r.reduced (1.0f), 6.0f, 2.0f);
        }

        g.setColour (f > 0.3f ? juce::Colours::black : col::text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (vg::padInfo (i).name, r.toNearestInt().reduced (6, 4), juce::Justification::centredLeft);
        g.setColour (col::dim);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (juce::String (i + 1), r.toNearestInt().reduced (6, 4), juce::Justification::topRight);
    }
}

void PadGrid::mouseDown (const juce::MouseEvent& e)
{
    const int pad = padAt (e.getPosition());
    if (pad < 0)
        return;
    select (pad);
    // Higher on the pad = louder
    const auto b = padBounds (pad);
    const float vel = juce::jlimit (0.3f, 1.0f, 1.0f - (float) (e.y - b.getY()) / (float) b.getHeight() * 0.6f);
    proc.triggerPadFromUI (pad, vel);
}

void PadGrid::filesDropped (const juce::StringArray& files, int x, int y)
{
    const int pad = padAt ({ x, y });
    if (pad < 0 || files.isEmpty())
        return;
    select (pad);
    if (proc.loadSample (pad, juce::File (files[0])) && onSampleChanged)
        onSampleChanged();
}

// ------------------------------------------------------------ StepGrid

bool StepGrid::cellAt (juce::Point<int> p, int& pad, int& step) const
{
    const float cw = (float) (getWidth() - labelW) / vg::numSteps;
    const float ch = (float) (getHeight() - headerH) / vg::numPads;
    if (p.x < labelW || p.y < headerH)
        return false;
    step = juce::jlimit (0, vg::numSteps - 1, (int) ((float) (p.x - labelW) / cw));
    pad = juce::jlimit (0, vg::numPads - 1, (int) ((float) (p.y - headerH) / ch));
    return true;
}

void StepGrid::paint (juce::Graphics& g)
{
    const float cw = (float) (getWidth() - labelW) / vg::numSteps;
    const float ch = (float) (getHeight() - headerH) / vg::numPads;

    g.setFont (juce::FontOptions (10.0f));
    for (int s = 0; s < vg::numSteps; ++s)
    {
        g.setColour (s % 4 == 0 ? col::text : col::dim);
        g.drawText (juce::String (s + 1), juce::Rectangle<float> (labelW + s * cw, 0, cw, (float) headerH).toNearestInt(),
                    juce::Justification::centred);
    }

    for (int p = 0; p < vg::numPads; ++p)
    {
        const float y = headerH + p * ch;
        if (p == selected)
        {
            g.setColour (col::panelHi.withAlpha (0.55f));
            g.fillRect (0.0f, y, (float) getWidth(), ch);
        }

        g.setColour (p == selected ? col::accent : col::dim);
        g.setFont (juce::FontOptions (11.0f, p == selected ? juce::Font::bold : juce::Font::plain));
        g.drawText (vg::padInfo (p).name, juce::Rectangle<float> (4, y, (float) labelW - 6, ch).toNearestInt(),
                    juce::Justification::centredLeft);

        for (int s = 0; s < vg::numSteps; ++s)
        {
            const auto cell = juce::Rectangle<float> (labelW + s * cw, y, cw, ch).reduced (1.5f);
            const int v = proc.pattern.get (p, s);

            g.setColour ((s / 4) % 2 == 0 ? col::panel : col::panel.brighter (0.06f));
            g.fillRoundedRectangle (cell, 3.0f);

            if (v > 0)
            {
                g.setColour (col::accent.withAlpha (0.35f + 0.65f * (float) v / 127.0f));
                g.fillRoundedRectangle (cell, 3.0f);
            }
        }
    }

    if (playhead >= 0)
    {
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.fillRect (labelW + playhead * cw, (float) headerH, cw, getHeight() - (float) headerH);
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    int pad, step;
    if (e.x < labelW && e.y >= headerH)
    {
        select (juce::jlimit (0, vg::numPads - 1, (e.y - headerH) * vg::numPads / juce::jmax (1, getHeight() - headerH)));
        return;
    }
    if (! cellAt (e.getPosition(), pad, step))
        return;

    paintValue = proc.pattern.get (pad, step) > 0 ? 0 : 100;
    proc.pattern.set (pad, step, paintValue);
    lastPad = pad;
    lastStep = step;
    repaint();
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    int pad, step;
    if (! cellAt (e.getPosition(), pad, step) || (pad == lastPad && step == lastStep))
        return;
    proc.pattern.set (pad, step, paintValue);
    lastPad = pad;
    lastStep = step;
    repaint();
}

void StepGrid::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    int pad, step;
    if (! cellAt (e.getPosition(), pad, step))
        return;
    const int v = proc.pattern.get (pad, step);
    if (v == 0)
        return;
    proc.pattern.set (pad, step, (uint8_t) juce::jlimit (1, 127, v + juce::roundToInt (w.deltaY * 60.0f)));
    repaint();
}

// ------------------------------------------------------------ Editor

VassaGruuvEditor::VassaGruuvEditor (VassaGruuvProcessor& p)
    : AudioProcessorEditor (&p),
      proc (p),
      padGrid (p, [this] (int pad) { selectPad (pad); }),
      stepGrid (p, [this] (int pad) { selectPad (pad); })
{
    setLookAndFeel (&laf);

    addAndMakeVisible (playButton);
    playAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "run", playButton);

    tempo.init (*this, "TEMPO");   tempo.attach (proc.apvts, "tempo");
    swing.init (*this, "SWING");   swing.attach (proc.apvts, "swing");
    filter.init (*this, "FILTER"); filter.attach (proc.apvts, "filter");
    reverb.init (*this, "REVERB"); reverb.attach (proc.apvts, "reverb");
    master.init (*this, "MASTER"); master.attach (proc.apvts, "master");
    filter.slider.setTextValueSuffix (" Hz");

    addAndMakeVisible (padGrid);
    addAndMakeVisible (stepGrid);
    padGrid.onSampleChanged = [this] { refreshSampleInfo(); };

    padTitle.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    padTitle.setColour (juce::Label::textColourId, col::accent);
    sampleInfo.setFont (juce::FontOptions (11.0f));
    sampleInfo.setColour (juce::Label::textColourId, col::dim);
    addAndMakeVisible (padTitle);
    addAndMakeVisible (sampleInfo);
    level.init (*this, "LEVEL");
    pan.init (*this, "PAN");
    tune.init (*this, "TUNE");
    decay.init (*this, "DECAY");
    addAndMakeVisible (muteButton);
    addAndMakeVisible (loadButton);
    addAndMakeVisible (clearSampleButton);
    loadButton.onClick = [this] { chooseSample(); };
    clearSampleButton.onClick = [this]
    {
        proc.clearSample (selectedPad);
        refreshSampleInfo();
    };

    aiTitle.setText ("AI GROOVE", juce::dontSendNotification);
    aiTitle.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    aiTitle.setColour (juce::Label::textColourId, col::accent);
    addAndMakeVisible (aiTitle);
    statusLabel.setColour (juce::Label::textColourId, col::dim);
    statusLabel.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (statusLabel);

    promptBox.setTextToShowWhenEmpty ("Describe a beat: \"dark techno, offbeat hats, rolling toms, 128 bpm\"", col::dim);
    promptBox.setReturnKeyStartsNewLine (false);
    promptBox.onReturnKey = [this] { askClaude(); };
    addAndMakeVisible (promptBox);
    addAndMakeVisible (askButton);
    addAndMakeVisible (keyButton);
    askButton.onClick = [this] { askClaude(); };
    keyButton.onClick = [this] { showApiKeyDialog(); };

    styleBox.addItemList (vg::PatternGenerator::styleNames(), 1);
    styleBox.setSelectedId (1, juce::dontSendNotification);
    addAndMakeVisible (styleBox);
    densitySlider.setRange (0.0, 1.0, 0.01);
    densitySlider.setValue (0.6, juce::dontSendNotification);
    densitySlider.setTooltip ("Density");
    addAndMakeVisible (densitySlider);
    addAndMakeVisible (generateButton);
    addAndMakeVisible (mutateButton);
    addAndMakeVisible (clearButton);
    generateButton.onClick = [this] { proc.generateLocal (styleBox.getSelectedId() - 1, (float) densitySlider.getValue()); };
    mutateButton.onClick = [this] { proc.mutatePattern (0.5f); };
    clearButton.onClick = [this] { proc.clearPattern(); };

    setSize (980, 700);
    selectPad (0);
    startTimerHz (30);
}

VassaGruuvEditor::~VassaGruuvEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void VassaGruuvEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);

    g.setColour (col::accent);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("VASSA GRUUV", 16, 14, 260, 30, juce::Justification::centredLeft);
    g.setColour (col::dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("groove box", 18, 44, 200, 16, juce::Justification::centredLeft);

    g.setColour (col::panel);
    g.fillRoundedRectangle (juce::Rectangle<float> (12, 424, 324, 156), 8.0f);
    g.fillRoundedRectangle (juce::Rectangle<float> (12, 588, 956, 100), 8.0f);
}

void VassaGruuvEditor::resized()
{
    // Top bar
    playButton.setBounds (260, 28, 80, 28);
    int x = 380;
    for (auto* k : { &tempo, &swing, &filter, &reverb, &master })
    {
        k->setBounds ({ x, 8, 82, 92 });
        x += 118;
    }

    padGrid.setBounds (16, 104, 316, 316);
    stepGrid.setBounds (348, 104, 620, 476);

    // Selected pad strip
    padTitle.setBounds (22, 428, 150, 24);
    sampleInfo.setBounds (22, 450, 200, 14);
    muteButton.setBounds (260, 430, 70, 22);
    const int kw = 76;
    level.setBounds ({ 18, 466, kw, 80 });
    pan.setBounds ({ 18 + kw, 466, kw, 80 });
    tune.setBounds ({ 18 + kw * 2, 466, kw, 80 });
    decay.setBounds ({ 18 + kw * 3, 466, kw, 80 });
    loadButton.setBounds (22, 550, 150, 24);
    clearSampleButton.setBounds (178, 550, 70, 24);

    // AI panel
    aiTitle.setBounds (22, 592, 100, 20);
    statusLabel.setBounds (120, 592, 840, 20);
    promptBox.setBounds (22, 616, 700, 30);
    askButton.setBounds (730, 616, 110, 30);
    keyButton.setBounds (848, 616, 110, 30);
    styleBox.setBounds (22, 652, 140, 28);
    densitySlider.setBounds (170, 652, 150, 28);
    generateButton.setBounds (330, 652, 100, 28);
    mutateButton.setBounds (438, 652, 100, 28);
    clearButton.setBounds (546, 652, 110, 28);
}

void VassaGruuvEditor::selectPad (int pad)
{
    selectedPad = pad;
    padGrid.selected = pad;
    stepGrid.selected = pad;

    level.attach (proc.apvts, VassaGruuvProcessor::padId (pad, "level"));
    pan.attach (proc.apvts, VassaGruuvProcessor::padId (pad, "pan"));
    tune.attach (proc.apvts, VassaGruuvProcessor::padId (pad, "tune"));
    decay.attach (proc.apvts, VassaGruuvProcessor::padId (pad, "decay"));
    muteAttachment.reset();
    muteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.apvts, VassaGruuvProcessor::padId (pad, "mute"), muteButton);

    padTitle.setText (juce::String (pad + 1) + "  " + vg::padInfo (pad).name, juce::dontSendNotification);
    refreshSampleInfo();
    padGrid.repaint();
    stepGrid.repaint();
}

void VassaGruuvEditor::refreshSampleInfo()
{
    const auto name = proc.getSampleName (selectedPad);
    sampleInfo.setText (name.isEmpty() ? "built-in synth  (drop a file on the pad to load a sample)" : "sample: " + name,
                        juce::dontSendNotification);
    clearSampleButton.setEnabled (name.isNotEmpty());
}

void VassaGruuvEditor::chooseSample()
{
    chooser = std::make_unique<juce::FileChooser> ("Load sample", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    juce::Component::SafePointer<VassaGruuvEditor> safe (this);
    const int pad = selectedPad;
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe, pad] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (safe == nullptr || ! f.existsAsFile())
                                  return;
                              if (! safe->proc.loadSample (pad, f))
                                  safe->proc.aiStatus = "Could not load " + f.getFileName() + " (max 30 s)";
                              safe->refreshSampleInfo();
                          });
}

void VassaGruuvEditor::askClaude()
{
    const auto prompt = promptBox.getText().trim();
    if (prompt.isEmpty())
    {
        proc.aiStatus = "Type a description first";
        return;
    }
    if (proc.getApiKey().isEmpty())
    {
        showApiKeyDialog();
        return;
    }
    proc.requestClaude (prompt);
}

void VassaGruuvEditor::showApiKeyDialog()
{
    auto* w = new juce::AlertWindow ("Anthropic API key",
                                     "Stored locally in your user settings. You can also set the ANTHROPIC_API_KEY environment variable.",
                                     juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("key", proc.getApiKey(), "API key:");
    if (auto* te = w->getTextEditor ("key"))
        te->setPasswordCharacter ((juce::juce_wchar) 0x2022);
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<juce::AlertWindow> sw (w);
    juce::Component::SafePointer<VassaGruuvEditor> safe (this);
    w->enterModalState (true,
                        juce::ModalCallbackFunction::create ([sw, safe] (int result)
                                                             {
                                                                 if (result == 1 && sw != nullptr && safe != nullptr)
                                                                     safe->proc.setApiKey (sw->getTextEditorContents ("key"));
                                                             }),
                        true);
}

void VassaGruuvEditor::timerCallback()
{
    bool anyFlash = false;
    for (int i = 0; i < vg::numPads; ++i)
    {
        auto& f = padGrid.flash[(size_t) i];
        f = juce::jmax (f * 0.82f, proc.padFlash[(size_t) i].exchange (0.0f, std::memory_order_relaxed));
        if (f < 0.01f)
            f = 0.0f;
        anyFlash |= f > 0.0f;
    }
    if (anyFlash)
        padGrid.repaint();

    const int step = proc.currentStep.load();
    if (step != stepGrid.playhead)
    {
        stepGrid.playhead = step;
        stepGrid.repaint();
    }
    else
    {
        stepGrid.repaint(); // pattern may have been changed by the AI / mutate
    }

    statusLabel.setText (proc.aiStatus, juce::dontSendNotification);
    askButton.setEnabled (! proc.isClaudeBusy());
}
