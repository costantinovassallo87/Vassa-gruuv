#include "PluginEditor.h"

using namespace vgui;
using vg::TP;

namespace
{
const juce::Colour muteCol { 0xffe05555 }, soloCol { 0xffe6c229 };

void setParamPlain (juce::RangedAudioParameter* p, float plain)
{
    if (p == nullptr)
        return;
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 (plain));
    p->endChangeGesture();
}

struct KnobLabel { int tp; const char* text; };
const KnobLabel knobLabels[] = {
    { vg::TPStart, "Start" }, { vg::TPEnd, "End" }, { vg::TPLoopStart, "Loop St" }, { vg::TPLoopEnd, "Loop End" },
    { vg::TPLoopFade, "Loop Fade" }, { vg::TPFadeIn, "Fade In" }, { vg::TPFadeOut, "Fade Out" }, { vg::TPSCount, "Slices" },
    { vg::TPSSens, "Sens" }, { vg::TPTranspose, "Transp" }, { vg::TPDetune, "Detune" }, { vg::TPGain, "Gain" },
    { vg::TPPan, "Pan" },
    { vg::TPAmpA, "A" }, { vg::TPAmpD, "D" }, { vg::TPAmpS, "S" }, { vg::TPAmpR, "R" },
    { vg::TPPitchA, "A" }, { vg::TPPitchD, "D" }, { vg::TPPitchS, "S" }, { vg::TPPitchR, "R" }, { vg::TPPitchAmt, "Amount" },
    { vg::TPFltA, "A" }, { vg::TPFltD, "D" }, { vg::TPFltS, "S" }, { vg::TPFltR, "R" }, { vg::TPFltAmt, "Amount" },
    { vg::TPFCut, "Cutoff" }, { vg::TPFRes, "Res" }, { vg::TPLRate, "Rate" }, { vg::TPLAmt, "Amount" },
};
} // namespace

// ------------------------------------------------------------ LookAndFeel

LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, col::bg);
    setColour (juce::Slider::rotarySliderFillColourId, col::accent);
    setColour (juce::Slider::textBoxTextColourId, col::dim);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, col::text);
    setColour (juce::TextButton::buttonColourId, col::panelHi);
    setColour (juce::TextButton::buttonOnColourId, col::accent);
    setColour (juce::TextButton::textColourOffId, col::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    setColour (juce::ComboBox::backgroundColourId, col::panelHi);
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::arrowColourId, col::dim);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    setColour (juce::BubbleComponent::backgroundColourId, col::panelHi);
    setColour (juce::BubbleComponent::outlineColourId, col::accent);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float d = juce::jmin (b.getWidth(), b.getHeight());
    b = b.withSizeKeepingCentre (d, d).reduced (2.0f);
    const auto c = b.getCentre();
    const float r = b.getWidth() * 0.5f, thick = 3.4f, ar = r - thick * 0.5f;

    const auto& props = s.getProperties();
    const juce::Colour colour = props.contains ("col") ? juce::Colour ((juce::uint32) (int) props["col"]) : col::accent;
    const bool bipolar = props.contains ("bipolar");
    const float ang = a0 + pos * (a1 - a0);

    juce::Path track;
    track.addCentredArc (c.x, c.y, ar, ar, 0.0f, a0, a1, true);
    g.setColour (col::panelHi);
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float from = bipolar ? (a0 + a1) * 0.5f : a0;
    if (std::abs (ang - from) > 0.01f)
    {
        juce::Path value;
        value.addCentredArc (c.x, c.y, ar, ar, 0.0f, juce::jmin (from, ang), juce::jmax (from, ang), true);
        g.setColour (s.isEnabled() ? colour : col::dim);
        g.strokePath (value, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float br = r - thick - 3.0f;
    g.setGradientFill (juce::ColourGradient (col::panelHi.brighter (0.15f), c.x, c.y - br, col::panel2, c.x, c.y + br, false));
    g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);
    g.setColour (col::border.brighter (0.2f));
    g.drawEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f, 1.0f);

    const float dx = std::sin (ang), dy = -std::cos (ang);
    g.setColour (col::text);
    g.drawLine (c.x + dx * br * 0.3f, c.y + dy * br * 0.3f, c.x + dx * br * 0.88f, c.y + dy * br * 0.88f, 2.2f);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const auto& props = b.getProperties();
    const juce::Colour onCol = props.contains ("col") ? juce::Colour ((juce::uint32) (int) props["col"]) : col::accent;

    g.setColour (on ? onCol : col::panelHi.brighter (highlighted ? 0.1f : 0.0f));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? juce::Colours::black : col::text);
    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    g.drawText (b.getButtonText(), r.toNearestInt(), juce::Justification::centred);
}

juce::Label* LookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = juce::LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (juce::FontOptions (10.0f));
    l->setJustificationType (juce::Justification::centred);
    l->setBorderSize ({ 0, 0, 0, 0 });
    return l;
}

void vgui::drawPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, int titleWidth)
{
    g.setColour (col::panel);
    g.fillRoundedRectangle (r.toFloat(), 8.0f);
    g.setColour (col::border);
    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);

    g.setColour (col::accent);
    g.fillRoundedRectangle ((float) r.getX() + 10.0f, (float) r.getY() + 8.0f, 3.0f, 11.0f, 1.5f);
    g.setColour (col::text);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (title, r.getX() + 18, r.getY() + 5, titleWidth, 18, juce::Justification::centredLeft);
}

// ------------------------------------------------------------ Knob / DragHandle

void Knob::init (juce::Component& parent, const juce::String& text)
{
    parent.addAndMakeVisible (slider); // add first so the text box is built with the parent's look and feel
    parent.addAndMakeVisible (label);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 12);
    slider.setTextBoxIsEditable (false);
    slider.sendLookAndFeelChange();
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, col::text.withAlpha (0.75f));
    label.setInterceptsMouseClicks (false, false);
}

void Knob::attach (juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    attachment.reset();
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider);
}

void Knob::setBounds (int x, int y, int w, int h)
{
    label.setBounds (x, y, w, 12);
    slider.setBounds (x, y + 12, w, h - 12);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, w, 12);
}

void DragHandle::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    g.setGradientFill (juce::ColourGradient (col::accent2.withAlpha (0.28f), 0.0f, b.getY(), col::accent2.withAlpha (0.10f), 0.0f, b.getBottom(), false));
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (col::accent2);
    g.drawRoundedRectangle (b, 6.0f, 1.4f);
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 2; ++j)
            g.fillEllipse (11.0f + (float) i * 5.0f, b.getCentreY() - 5.0f + (float) j * 7.0f, 3.0f, 3.0f);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("DRAG STEMS TO DAW", getLocalBounds().withTrimmedLeft (34), juce::Justification::centredLeft);
}

// ------------------------------------------------------------ StepGrid

StepGrid::StepGrid (VassaGruuvProcessor& p, std::function<void (int)> onSelect) : proc (p), select (std::move (onSelect))
{
    for (int t = 0; t < vg::numTracks; ++t)
    {
        mutePtr[(size_t) t] = p.apvts.getRawParameterValue (vg::trackParamId (t, vg::TPMute));
        soloPtr[(size_t) t] = p.apvts.getRawParameterValue (vg::trackParamId (t, vg::TPSolo));
    }
}

bool StepGrid::cellAt (juce::Point<int> p, int& track, int& step) const
{
    const float cw = (float) (getWidth() - gridLabelW) / vg::numSteps;
    const float ch = (float) (getHeight() - headerH) / vg::numTracks;
    if (p.x < gridLabelW || p.y < headerH)
        return false;
    step = juce::jlimit (0, vg::numSteps - 1, (int) ((float) (p.x - gridLabelW) / cw));
    track = juce::jlimit (0, vg::numTracks - 1, (int) ((float) (p.y - headerH) / ch));
    return true;
}

void StepGrid::paint (juce::Graphics& g)
{
    const float cw = (float) (getWidth() - gridLabelW) / vg::numSteps;
    const float ch = (float) (getHeight() - headerH) / vg::numTracks;

    g.setFont (juce::FontOptions (10.0f));
    for (int s = 0; s < vg::numSteps; ++s)
    {
        g.setColour (s % 4 == 0 ? col::text : col::dim);
        g.drawText (juce::String (s + 1), juce::Rectangle<float> (gridLabelW + s * cw, 0, cw, (float) headerH).toNearestInt(),
                    juce::Justification::centred);
    }

    for (int t = 0; t < vg::numTracks; ++t)
    {
        const float y = headerH + t * ch;
        const bool sel = t == selected;
        const bool muted = mutePtr[(size_t) t]->load() > 0.5f, solo = soloPtr[(size_t) t]->load() > 0.5f;

        if (sel)
        {
            g.setColour (col::panelHi.withAlpha (0.6f));
            g.fillRect (0.0f, y, (float) getWidth(), ch);
            g.setColour (col::track (t));
            g.fillRect (0.0f, y + 1.0f, 3.0f, ch - 2.0f);
        }

        g.setColour (col::dim);
        g.setFont (juce::FontOptions (9.0f));
        g.drawText (juce::String (t + 1), 4, (int) y, 14, (int) ch, juce::Justification::centredLeft);
        g.setColour (muted ? col::dim.withAlpha (0.6f) : (sel ? col::track (t) : col::text));
        g.setFont (juce::FontOptions (11.0f, sel ? juce::Font::bold : juce::Font::plain));
        g.drawText (proc.getTrackName (t), 22, (int) y, gridLabelW - 62, (int) ch, juce::Justification::centredLeft);

        const auto mBox = juce::Rectangle<float> ((float) gridLabelW - 36.0f, y + 1.5f, 15.0f, ch - 3.0f);
        const auto sBox = mBox.translated (17.0f, 0.0f);
        g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
        g.setColour (muted ? muteCol : col::panelHi);
        g.fillRoundedRectangle (mBox, 2.0f);
        g.setColour (solo ? soloCol : col::panelHi);
        g.fillRoundedRectangle (sBox, 2.0f);
        g.setColour ((muted ? juce::Colours::black : col::dim));
        g.drawText ("M", mBox.toNearestInt(), juce::Justification::centred);
        g.setColour ((solo ? juce::Colours::black : col::dim));
        g.drawText ("S", sBox.toNearestInt(), juce::Justification::centred);

        for (int s = 0; s < vg::numSteps; ++s)
        {
            const auto cell = juce::Rectangle<float> (gridLabelW + s * cw, y, cw, ch).reduced (1.2f);
            const int v = proc.pattern.get (vg::Velocity, t, s);

            g.setColour ((s / 4) % 2 == 0 ? col::panel : col::panel.brighter (0.06f));
            g.fillRoundedRectangle (cell, 2.5f);

            if (v > 0)
            {
                g.setColour ((muted ? col::dim : col::track (t)).withAlpha (0.30f + 0.70f * (float) v / 127.0f));
                g.fillRoundedRectangle (cell, 2.5f);

                bool modded = false;
                for (int l = 1; l < vg::numLanes && ! modded; ++l)
                    modded = proc.pattern.isNonDefault (l, t, s);
                if (modded)
                {
                    g.setColour (col::accent2);
                    g.fillEllipse (cell.getRight() - 6.0f, cell.getY() + 2.0f, 4.0f, 4.0f);
                }
            }
        }
    }

    if (playhead >= 0)
    {
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.fillRect (gridLabelW + playhead * cw, (float) headerH, cw, getHeight() - (float) headerH);
    }
}

void StepGrid::toggleParam (int track, int tp)
{
    if (auto* p = proc.apvts.getParameter (vg::trackParamId (track, tp)))
        setParamPlain (p, p->getValue() > 0.5f ? 0.0f : 1.0f);
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    const float ch = (float) (getHeight() - headerH) / vg::numTracks;

    if (e.x < gridLabelW && e.y >= headerH)
    {
        const int t = juce::jlimit (0, vg::numTracks - 1, (int) ((float) (e.y - headerH) / ch));
        if (e.x >= gridLabelW - 36 && e.x < gridLabelW - 21)
            toggleParam (t, vg::TPMute);
        else if (e.x >= gridLabelW - 19 && e.x < gridLabelW - 4)
            toggleParam (t, vg::TPSolo);
        else
        {
            select (t);
            proc.previewTrack (t, 0.9f);
        }
        repaint();
        return;
    }

    int t, s;
    if (! cellAt (e.getPosition(), t, s))
        return;

    select (t);
    paintValue = proc.pattern.get (vg::Velocity, t, s) > 0 ? 0 : 100;
    proc.pattern.set (vg::Velocity, t, s, paintValue);
    lastTrack = t;
    lastStep = s;
    repaint();
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    int t, s;
    if (! cellAt (e.getPosition(), t, s) || (t == lastTrack && s == lastStep))
        return;
    proc.pattern.set (vg::Velocity, t, s, paintValue);
    lastTrack = t;
    lastStep = s;
    repaint();
}

// ------------------------------------------------------------ ModGrid

juce::Rectangle<float> ModGrid::barArea() const
{
    return { (float) gridLabelW, 16.0f, (float) (getWidth() - gridLabelW), (float) getHeight() - 20.0f };
}

int ModGrid::stepAt (int x) const
{
    const float cw = (float) (getWidth() - gridLabelW) / vg::numSteps;
    return juce::jlimit (0, vg::numSteps - 1, (int) ((float) (x - gridLabelW) / cw));
}

void ModGrid::paint (juce::Graphics& g)
{
    const auto& li = vg::laneInfo (lane);
    const auto area = barArea();
    const float cw = area.getWidth() / vg::numSteps;
    const float range = (float) (li.max - li.min);

    g.setColour (col::text);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (li.name, 8, 16, gridLabelW - 12, 18, juce::Justification::centredLeft);
    g.setColour (col::dim);
    g.setFont (juce::FontOptions (10.0f));
    g.drawText (juce::String (li.min) + " .. " + juce::String (li.max) + li.unit, 8, 34, gridLabelW - 12, 14, juce::Justification::centredLeft);

    const auto barColour = col::track (track);

    for (int s = 0; s < vg::numSteps; ++s)
    {
        const auto cell = juce::Rectangle<float> (area.getX() + (float) s * cw, area.getY(), cw, area.getHeight()).reduced (1.5f, 0.0f);
        const int v = proc.pattern.get (lane, track, s);
        const bool on = proc.pattern.get (vg::Velocity, track, s) > 0;

        g.setColour ((s / 4) % 2 == 0 ? col::panel : col::panel.brighter (0.06f));
        g.fillRoundedRectangle (cell, 2.5f);

        const float norm = (float) (v - li.min) / range;
        float y0 = cell.getBottom(), y1 = cell.getBottom() - norm * cell.getHeight();
        if (li.bipolar)
            y0 = cell.getCentreY();
        if (y1 < y0 || li.bipolar)
        {
            g.setColour (barColour.withAlpha (on || lane == vg::Velocity ? 0.9f : 0.3f));
            g.fillRect (cell.getX() + 1.0f, juce::jmin (y0, y1), cell.getWidth() - 2.0f, std::abs (y1 - y0) + 1.0f);
        }

        g.setColour (on || lane == vg::Velocity ? col::text : col::dim);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (lane == vg::Velocity && v == 0 ? juce::String ("-") : juce::String (v), cell.withY (0.0f).withHeight (15.0f).toNearestInt(),
                    juce::Justification::centred);
    }

    if (li.bipolar)
    {
        g.setColour (col::dim.withAlpha (0.5f));
        g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());
    }
}

void ModGrid::setFromMouse (const juce::MouseEvent& e)
{
    const auto& li = vg::laneInfo (lane);
    const auto area = barArea();
    const float t = juce::jlimit (0.0f, 1.0f, 1.0f - (float) (e.y - area.getY()) / area.getHeight());
    const int v = juce::roundToInt ((float) li.min + t * (float) (li.max - li.min));
    proc.pattern.set (lane, track, stepAt (e.x), v);
    repaint();
}

void ModGrid::mouseDown (const juce::MouseEvent& e)
{
    if (e.x < gridLabelW)
        return;
    if (e.mods.isAltDown() || e.getNumberOfClicks() > 1)
    {
        proc.pattern.set (lane, track, stepAt (e.x), vg::Pattern::resetValue (lane));
        repaint();
        return;
    }
    setFromMouse (e);
}

void ModGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (e.x >= gridLabelW && ! e.mods.isAltDown())
        setFromMouse (e);
}

void ModGrid::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.x < gridLabelW)
        return;
    const auto& li = vg::laneInfo (lane);
    const int s = stepAt (e.x);
    const int stepSize = (li.max - li.min) > 100 ? 5 : 1;
    proc.pattern.set (lane, track, s, proc.pattern.get (lane, track, s) + (w.deltaY > 0 ? stepSize : -stepSize));
    repaint();
}

// ------------------------------------------------------------ WaveformView

juce::RangedAudioParameter* WaveformView::param (int tp) const
{
    return proc.apvts.getParameter (vg::trackParamId (track, tp));
}

void WaveformView::setPlain (int tp, float plain)
{
    if (auto* p = param (tp))
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
}

float WaveformView::gateMs() const
{
    return 60000.0f / (float) juce::jmax (20.0, proc.currentBpm.load()) / 4.0f; // one 16th note
}

void WaveformView::rebuildPeaks()
{
    peaks.assign ((size_t) juce::jmax (1, getWidth()), { 0.0f, 0.0f });
    cachedWidth = getWidth();
    cachedStart = viewStart;
    cachedEnd = viewEnd;
    if (cached == nullptr)
        return;

    const int N = cached->numFrames(), w = (int) peaks.size();
    for (int x = 0; x < w; ++x)
    {
        const int a = juce::jlimit (0, N - 1, (int) ((viewStart + (viewEnd - viewStart) * x / w) * N));
        const int n = juce::jmax (1, juce::jmin (N - a, (int) ((viewEnd - viewStart) * N / w) + 1));
        float mn = 0.0f, mx = 0.0f;
        for (int c = 0; c < cached->data.getNumChannels(); ++c)
        {
            const auto r = juce::FloatVectorOperations::findMinAndMax (cached->data.getReadPointer (c) + a, n);
            mn = juce::jmin (mn, r.getStart());
            mx = juce::jmax (mx, r.getEnd());
        }
        peaks[(size_t) x] = { mn, mx };
    }
}

void WaveformView::drawFlag (juce::Graphics& g, float x, juce::Colour c, bool left, bool highlighted, const juce::String& label) const
{
    const float h = (float) waveHeight();
    g.setColour (c.withAlpha (highlighted ? 1.0f : 0.85f));
    g.fillRect (x - (highlighted ? 1.0f : 0.5f), 0.0f, highlighted ? 2.5f : 1.5f, h);

    const float fw = 22.0f, fh = 14.0f;
    const float x0 = left ? x : x - fw;
    g.fillRoundedRectangle (x0, 0.0f, fw, fh, 2.5f);
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.drawText (label, juce::Rectangle<float> (x0, 0.0f, fw, fh).toNearestInt(), juce::Justification::centred);
}

namespace
{
juce::Colour envColour (int kind)
{
    return kind == vg::EnvAmp ? col::accent : (kind == vg::EnvPitch ? col::purple : col::accent2);
}
}

void WaveformView::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float w = b.getWidth(), wh = (float) waveHeight(), mid = wh * 0.5f, ampTop = mid * 0.92f;
    const auto tc = col::track (track);

    g.setColour (col::bg);
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (col::border);
    g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);

    auto smp = proc.getSample (track);
    if (smp == nullptr)
    {
        g.setColour (col::dim);
        g.drawText ("Drop an audio file here", getLocalBounds(), juce::Justification::centred);
        return;
    }
    if (smp != cached)
    {
        cached = smp;
        viewStart = 0.0;
        viewEnd = 1.0;
        cachedWidth = -1;
    }
    if (getWidth() != cachedWidth || ! juce::exactlyEqual (viewStart, cachedStart) || ! juce::exactlyEqual (viewEnd, cachedEnd))
        rebuildPeaks();

    const auto prm = proc.readTrackParams (track);
    const int mode = prm.idx (vg::TPMode);
    const int N = smp->numFrames();
    const double secs = (double) N / smp->rate;
    const bool classic = mode == vg::ModeClassic;
    const float gate = gateMs();
    const auto ec = envColour (envKind);

    // Time ruler
    g.setColour (col::panel);
    g.fillRect (0.0f, wh, w, (float) rulerH);
    {
        static const double steps[] = { 0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10, 20 };
        const double visible = secs * (viewEnd - viewStart);
        double tick = steps[13];
        for (double st : steps)
            if (st / visible * w >= 64.0) { tick = st; break; }
        g.setFont (juce::FontOptions (9.0f));
        for (double t = std::ceil (viewStart * secs / tick) * tick; t < viewEnd * secs; t += tick)
        {
            const float x = toX (t / secs);
            g.setColour (col::dim.withAlpha (0.5f));
            g.drawVerticalLine ((int) x, wh, wh + 5.0f);
            g.setColour (col::dim);
            if (x + 25.0f <= w)
                g.drawText (t < 1.0 ? juce::String (juce::roundToInt (t * 1000.0)) + " ms" : juce::String (t, 2) + " s", (int) x + 3, (int) wh + 2, 50, 12,
                            juce::Justification::left);
        }
    }

    g.setColour (col::dim.withAlpha (0.12f));
    g.drawHorizontalLine ((int) mid, 0.0f, w);
    for (int i = 1; i < 8; ++i)
        g.drawVerticalLine ((int) (w * (float) i / 8.0f), 0.0f, wh);

    // --- Envelope-adaptive waveform: the raw sample is a faint ghost, the solid shape is what you actually hear
    const float regionStart = prm[vg::TPStart], regionEnd = juce::jmax (prm[vg::TPEnd], prm[vg::TPStart] + 0.002f);
    std::vector<float> sliceStarts { regionStart };
    if (mode == vg::ModeSlice)
        for (float m : vg::sliceBoundaries (*smp, prm))
            sliceStarts.push_back (m);

    const float aMs = prm[vg::TPAmpA], dMs = prm[vg::TPAmpD], sLv = prm[vg::TPAmpS], rMs = prm[vg::TPAmpR];
    const float fadeInMs = juce::jmax (0.7f, prm[vg::TPFadeIn]), fadeOutMs = juce::jmax (0.7f, prm[vg::TPFadeOut]);
    const float msPerN = (float) (secs * 1000.0);

    auto timeSinceStart = [&] (double n)
    {
        float s0 = sliceStarts[0];
        for (float st : sliceStarts)
            if (st <= n) s0 = st;
        return ((float) n - s0) * msPerN;
    };

    juce::Path ghost, wave, envLine, envLineLow;
    ghost.startNewSubPath (0.0f, mid);
    wave.startNewSubPath (0.0f, mid);
    std::vector<float> factor (peaks.size(), 0.0f), lineLevel (peaks.size(), 0.0f);
    for (size_t x = 0; x < peaks.size(); ++x)
    {
        const double n = toN ((float) x);
        float f = 0.0f;
        if (n >= regionStart && n <= regionEnd)
        {
            const float t = timeSinceStart (n);
            f = vg::adsrLevel (t, aMs, dMs, sLv, rMs, classic ? gate : -1.0f);
            f *= juce::jmin (1.0f, t / fadeInMs);
            if (! (classic && prm.on (vg::TPLoop)))
                f *= juce::jmin (1.0f, ((float) regionEnd - (float) n) * msPerN / fadeOutMs);
            const int ek = envKind;
            const float lv = vg::adsrLevel (t, prm[vg::envParam (ek, 0)], prm[vg::envParam (ek, 1)], prm[vg::envParam (ek, 2)],
                                            prm[vg::envParam (ek, 3)], classic ? gate : -1.0f);
            lineLevel[x] = lv;
        }
        factor[x] = f;
        ghost.lineTo ((float) x, mid - peaks[x].second * ampTop);
        wave.lineTo ((float) x, mid - peaks[x].second * f * ampTop);
    }
    for (size_t x = peaks.size(); x-- > 0;)
    {
        ghost.lineTo ((float) x, mid - peaks[x].first * ampTop);
        wave.lineTo ((float) x, mid - peaks[x].first * factor[x] * ampTop);
    }
    ghost.closeSubPath();
    wave.closeSubPath();

    g.setColour (tc.withAlpha (0.16f));
    g.fillPath (ghost);
    g.setGradientFill (juce::ColourGradient (tc.withAlpha (0.95f), 0.0f, 0.0f, tc.withAlpha (0.55f), 0.0f, wh, false));
    g.fillPath (wave);
    g.setColour (tc.brighter (0.4f).withAlpha (0.6f));
    g.strokePath (wave, juce::PathStrokeType (0.8f));

    // Slice markers
    if (mode == vg::ModeSlice)
    {
        g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
        for (size_t i = 0; i < sliceStarts.size(); ++i)
        {
            const float x = toX (sliceStarts[i]);
            const float nx = i + 1 < sliceStarts.size() ? toX (sliceStarts[i + 1]) : toX (regionEnd);
            if (i > 0)
            {
                g.setColour (col::accent2);
                g.drawVerticalLine ((int) x, 14.0f, wh);
            }
            g.setColour (col::accent2.withAlpha (0.18f));
            g.fillRect (x + 1.0f, wh - 14.0f, nx - x - 1.0f, 14.0f);
            g.setColour (col::accent2);
            g.drawText (juce::String ((int) i + 1), (int) x + 4, (int) wh - 14, 24, 14, juce::Justification::left);
        }
    }

    // Loop region
    const bool looping = classic && prm.on (vg::TPLoop);
    if (looping)
    {
        const float ls = toX (prm[vg::TPLoopStart]), le = toX (prm[vg::TPLoopEnd]);
        g.setColour (col::accent2.withAlpha (0.14f));
        g.fillRect (ls, 0.0f, le - ls, wh);
    }

    // Outside-region shading
    const float sx = toX (regionStart), ex = toX (regionEnd);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRect (0.0f, 0.0f, juce::jmax (0.0f, sx), wh);
    g.fillRect (juce::jmin (w, ex), 0.0f, juce::jmax (0.0f, w - ex), wh);

    // --- ADSR overlay (selected envelope)
    {
        const int ek = envKind;
        const float A = prm[vg::envParam (ek, 0)], D = prm[vg::envParam (ek, 1)], S = prm[vg::envParam (ek, 2)], R = prm[vg::envParam (ek, 3)];
        auto msToX = [&] (float ms) { return toX (regionStart + ms / msPerN); };
        auto lvToY = [&] (float lv) { return mid - lv * ampTop; };

        juce::Path line;
        bool started = false;
        for (size_t x = 0; x < peaks.size(); ++x)
        {
            const double n = toN ((float) x);
            if (n < regionStart || n > regionEnd)
                continue;
            const juce::Point<float> p ((float) x, lvToY (lineLevel[x]));
            started ? line.lineTo (p) : line.startNewSubPath (p);
            started = true;
        }
        if (started)
        {
            if (ek != vg::EnvAmp)
            {
                juce::Path fill (line);
                fill.lineTo (line.getCurrentPosition().x, mid);
                fill.lineTo (juce::jmax (0.0f, sx), mid);
                fill.closeSubPath();
                g.setColour (ec.withAlpha (0.14f));
                g.fillPath (fill);
            }
            g.setColour (ec);
            g.strokePath (line, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved));
        }

        if (classic)
        {
            const float gx = msToX (gate);
            const float dashes[] = { 3.0f, 3.0f };
            g.setColour (juce::Colours::white.withAlpha (0.3f));
            g.drawDashedLine ({ { gx, 16.0f }, { gx, wh } }, dashes, 2, 1.0f);
            g.setFont (juce::FontOptions (8.5f, juce::Font::bold));
            g.drawText ("STEP END", (int) gx + 3, (int) wh - 24, 50, 10, juce::Justification::left);
        }

        struct Node { juce::Point<float> p; const char* label; juce::String value; };
        std::vector<std::pair<int, Node>> nodes;
        nodes.push_back ({ 0, { { msToX (A), lvToY (1.0f) }, "A", juce::String (A, 0) + " ms" } });
        nodes.push_back ({ 1, { { msToX (A + D), lvToY (S) }, "D/S", "D " + juce::String (D, 0) + " ms  S " + juce::String (S, 2) } });
        if (classic)
            nodes.push_back ({ 2, { { msToX (gate + R), lvToY (0.0f) }, "R", juce::String (R, 0) + " ms" } });

        for (const auto& [id, nd] : nodes)
        {
            const bool hot = id == envHover || id == envDragging;
            const float rad = hot ? 6.5f : 5.0f;
            g.setColour (col::bg);
            g.fillEllipse (nd.p.x - rad - 1.5f, nd.p.y - rad - 1.5f, (rad + 1.5f) * 2.0f, (rad + 1.5f) * 2.0f);
            g.setColour (ec);
            g.fillEllipse (nd.p.x - rad, nd.p.y - rad, rad * 2.0f, rad * 2.0f);
            g.setColour (juce::Colours::black.withAlpha (0.8f));
            g.setFont (juce::FontOptions (7.5f, juce::Font::bold));
            g.drawText (id == 1 ? "D" : nd.label, (int) (nd.p.x - 8), (int) (nd.p.y - 5), 16, 10, juce::Justification::centred);

            if (hot)
            {
                const int tw = 108;
                const int tx = juce::jlimit (2, getWidth() - tw - 2, (int) nd.p.x - tw / 2);
                const int ty = juce::jlimit (16, (int) wh - 18, (int) nd.p.y - 24);
                g.setColour (juce::Colours::black.withAlpha (0.8f));
                g.fillRoundedRectangle ((float) tx, (float) ty, (float) tw, 15.0f, 3.0f);
                g.setColour (col::text);
                g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
                g.drawText (nd.value, tx, ty, tw, 15, juce::Justification::centred);
            }
        }

        // Envelope badge (which one is shown, and the amount for pitch / filter)
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.setColour (ec);
        juce::String badge = juce::String (vg::envName (ek)).toUpperCase() + " ENV";
        if (ek != vg::EnvAmp)
            badge << "   " << (prm[vg::envParam (ek, 4)] >= 0.0f ? "+" : "") << juce::String (prm[vg::envParam (ek, 4)], 1) << " st";
        g.drawText (badge, juce::Rectangle<int> (6, 16, 180, 14), juce::Justification::left);
    }

    // Fades
    const float pxPerMs = (float) (smp->rate * 0.001 / N / (viewEnd - viewStart)) * w;
    g.setColour (juce::Colours::white.withAlpha (0.4f));
    if (prm[vg::TPFadeIn] > 0.5f)
        g.drawLine (sx, wh, sx + prm[vg::TPFadeIn] * pxPerMs, wh - 8.0f, 1.0f);
    if (prm[vg::TPFadeOut] > 0.5f && ! looping)
        g.drawLine (ex - prm[vg::TPFadeOut] * pxPerMs, wh - 8.0f, ex, wh, 1.0f);

    // Markers
    if (looping)
    {
        drawFlag (g, toX (prm[vg::TPLoopStart]), col::accent2, true, hover == vg::TPLoopStart || dragging == vg::TPLoopStart, "L");
        drawFlag (g, toX (prm[vg::TPLoopEnd]), col::accent2, false, hover == vg::TPLoopEnd || dragging == vg::TPLoopEnd, "L");
    }
    drawFlag (g, sx, col::green, true, hover == vg::TPStart || dragging == vg::TPStart, "S");
    drawFlag (g, ex, juce::Colour (0xffff5a5a), false, hover == vg::TPEnd || dragging == vg::TPEnd, "E");

    if (dragging >= 0)
    {
        const float x = toX (prm[dragging]);
        const auto text = juce::String (prm[dragging] * secs * 1000.0, 1) + " ms";
        const int tw = 64;
        const int tx = juce::jlimit (2, getWidth() - tw - 2, (int) x - tw / 2);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.fillRoundedRectangle ((float) tx, 18.0f, (float) tw, 15.0f, 3.0f);
        g.setColour (col::text);
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText (text, tx, 18, tw, 15, juce::Justification::centred);
    }

    const float ph = proc.getPlayhead (track);
    if (ph >= 0.0f)
    {
        g.setColour (juce::Colours::white);
        g.drawVerticalLine ((int) toX (ph), 0.0f, wh);
    }

    g.setColour (col::dim);
    g.setFont (juce::FontOptions (9.5f));
    const double zoom = 1.0 / (viewEnd - viewStart);
    g.drawText (juce::String (secs, 2) + " s" + (zoom > 1.01 ? "   x" + juce::String (zoom, 1) : juce::String()),
                getLocalBounds().withTrimmedRight (6).withHeight (14).withY (16), juce::Justification::topRight);
}

int WaveformView::envNodeAt (juce::Point<float> pos, const vg::TrackParams& prm) const
{
    const int ek = envKind;
    const float wh = (float) waveHeight(), mid = wh * 0.5f, ampTop = mid * 0.92f;
    const float rs = prm[vg::TPStart], msPerN = cached != nullptr ? (float) ((double) cached->numFrames() / cached->rate * 1000.0) : 1000.0f;
    const bool classic = prm.idx (vg::TPMode) == vg::ModeClassic;
    const float A = prm[vg::envParam (ek, 0)], D = prm[vg::envParam (ek, 1)], S = prm[vg::envParam (ek, 2)], R = prm[vg::envParam (ek, 3)];

    auto at = [&] (float ms, float lv) { return juce::Point<float> (toX (rs + ms / msPerN), mid - lv * ampTop); };
    int best = -1;
    float bestD = 10.0f;
    auto test = [&] (int id, juce::Point<float> p)
    {
        const float d = p.getDistanceFrom (pos);
        if (d < bestD) { bestD = d; best = id; }
    };
    test (0, at (A, 1.0f));
    test (1, at (A + D, S));
    if (classic)
        test (2, at (gateMs() + R, 0.0f));
    return best;
}

int WaveformView::handleAt (int x) const
{
    const auto prm = proc.readTrackParams (track);
    struct C { int tp; float px; };
    std::vector<C> c { { vg::TPStart, toX (prm[vg::TPStart]) }, { vg::TPEnd, toX (prm[vg::TPEnd]) } };
    if (prm.idx (vg::TPMode) == vg::ModeClassic && prm.on (vg::TPLoop))
    {
        c.push_back ({ vg::TPLoopStart, toX (prm[vg::TPLoopStart]) });
        c.push_back ({ vg::TPLoopEnd, toX (prm[vg::TPLoopEnd]) });
    }

    int best = -1;
    float bestDist = 9.0f;
    for (const auto& h : c)
        if (std::abs ((float) x - h.px) < bestDist)
        {
            bestDist = std::abs ((float) x - h.px);
            best = h.tp;
        }
    return best;
}

void WaveformView::mouseMove (const juce::MouseEvent& e)
{
    const auto prm = proc.readTrackParams (track);
    const int node = envNodeAt (e.position, prm);
    const int h = node >= 0 ? -1 : handleAt (e.x);
    if (h != hover || node != envHover)
    {
        hover = h;
        envHover = node;
        repaint();
    }
    setMouseCursor (node >= 0 ? juce::MouseCursor::DraggingHandCursor : (h >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor));
}

void WaveformView::mouseDown (const juce::MouseEvent& e)
{
    const auto prm = proc.readTrackParams (track);
    envDragging = envNodeAt (e.position, prm);
    if (envDragging >= 0)
    {
        const auto begin = [this] (int tp) { if (auto* p = param (tp)) p->beginChangeGesture(); };
        begin (vg::envParam (envKind, envDragging));
        if (envDragging == 1)
            begin (vg::envParam (envKind, 2));
        return;
    }

    dragging = handleAt (e.x);
    if (dragging >= 0)
    {
        if (auto* p = param (dragging))
            p->beginChangeGesture();
    }
    else
        proc.previewTrack (track, 0.9f);
}

void WaveformView::mouseDrag (const juce::MouseEvent& e)
{
    const auto prm = proc.readTrackParams (track);

    if (envDragging >= 0 && cached != nullptr)
    {
        const float wh = (float) waveHeight(), mid = wh * 0.5f, ampTop = mid * 0.92f;
        const float msPerN = (float) ((double) cached->numFrames() / cached->rate * 1000.0);
        const float t = ((float) toN ((float) e.x) - prm[vg::TPStart]) * msPerN;
        const int kind = envKind;
        const float A = prm[vg::envParam (kind, 0)];

        if (envDragging == 0)
            setPlain (vg::envParam (kind, 0), juce::jlimit (0.0f, 2000.0f, t));
        else if (envDragging == 1)
        {
            float s = juce::jlimit (0.0f, 1.0f, (mid - (float) e.y) / ampTop);
            if (s > 0.97f) s = 1.0f;
            if (s < 0.03f) s = 0.0f;
            setPlain (vg::envParam (kind, 1), juce::jlimit (1.0f, 5000.0f, t - A));
            setPlain (vg::envParam (kind, 2), s);
        }
        else
            setPlain (vg::envParam (kind, 3), juce::jlimit (1.0f, 5000.0f, t - gateMs()));
        repaint();
        return;
    }

    if (dragging < 0)
        return;

    float v = (float) juce::jlimit (0.0, 1.0, toN ((float) e.x));
    constexpr float gap = 0.0005f;
    switch (dragging)
    {
        case vg::TPStart:     v = juce::jmin (v, prm[vg::TPEnd] - gap); break;
        case vg::TPEnd:       v = juce::jmax (v, prm[vg::TPStart] + gap); break;
        case vg::TPLoopStart: v = juce::jlimit (prm[vg::TPStart], prm[vg::TPLoopEnd] - gap, v); break;
        case vg::TPLoopEnd:   v = juce::jlimit (prm[vg::TPLoopStart] + gap, prm[vg::TPEnd], v); break;
        default: break;
    }
    if (auto* p = param (dragging))
        p->setValueNotifyingHost (v); // these parameters span 0..1, so normalised == plain
}

void WaveformView::mouseUp (const juce::MouseEvent&)
{
    if (envDragging >= 0)
    {
        const auto end = [this] (int tp) { if (auto* p = param (tp)) p->endChangeGesture(); };
        end (vg::envParam (envKind, envDragging));
        if (envDragging == 1)
            end (vg::envParam (envKind, 2));
    }
    if (dragging >= 0)
        if (auto* p = param (dragging))
            p->endChangeGesture();
    dragging = -1;
    envDragging = -1;
    repaint();
}

void WaveformView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const double span = viewEnd - viewStart;
    if (e.mods.isShiftDown())
    {
        const double d = (std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY) * span * 0.5;
        viewStart = juce::jlimit (0.0, 1.0 - span, viewStart - d);
        viewEnd = viewStart + span;
    }
    else
    {
        const auto smp = proc.getSample (track);
        const double minSpan = smp != nullptr ? juce::jmin (1.0, 64.0 / smp->numFrames()) : 0.01;
        const double anchor = toN ((float) e.x);
        const double newSpan = juce::jlimit (minSpan, 1.0, span * (w.deltaY > 0 ? 0.8 : 1.25));
        viewStart = juce::jlimit (0.0, 1.0 - newSpan, anchor - (anchor - viewStart) * newSpan / span);
        viewEnd = viewStart + newSpan;
    }
    repaint();
}

void WaveformView::filesDropped (const juce::StringArray& files, int, int)
{
    if (! files.isEmpty() && proc.loadSample (track, juce::File (files[0])) && onSampleChanged)
        onSampleChanged();
}

// ------------------------------------------------------------ Editor

MainPanel::MainPanel (VassaGruuvProcessor& p)
    : proc (p),
      stepGrid (p, [this] (int t) { selectTrack (t); }),
      modGrid (p),
      waveform (p)
{
    setLookAndFeel (&laf);

    // Top bar
    addAndMakeVisible (playButton);
    playAttachment = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "run", playButton);
    tempo.init (*this, "TEMPO");    tempo.attach (proc.apvts, "tempo");
    swing.init (*this, "SWING");    swing.attach (proc.apvts, "swing");
    filter.init (*this, "FILTER");  filter.attach (proc.apvts, "filter");
    reverb.init (*this, "REVERB");  reverb.attach (proc.apvts, "reverb");
    master.init (*this, "MASTER");  master.attach (proc.apvts, "master");

    addAndMakeVisible (dragHandle);
    dragHandle.onDrag = [this] { startStemDrag(); };
    loopsBox.addItemList ({ "1 bar", "2 bars", "4 bars", "8 bars" }, 1);
    loopsBox.setSelectedId (1, juce::dontSendNotification);
    addAndMakeVisible (loopsBox);
    addAndMakeVisible (exportButton);
    exportButton.onClick = [this] { exportStemsToFolder(); };

    // Step sequencer
    addAndMakeVisible (stepGrid);
    styleBox.addItemList (vg::PatternGenerator::styleNames(), 1);
    styleBox.setSelectedId (1, juce::dontSendNotification);
    addAndMakeVisible (styleBox);
    addAndMakeVisible (generateButton);
    addAndMakeVisible (mutateButton);
    addAndMakeVisible (clearButton);
    generateButton.onClick = [this] { proc.generateLocal (styleBox.getSelectedId() - 1, 0.6f); };
    mutateButton.onClick = [this] { proc.mutatePattern (0.5f); };
    clearButton.onClick = [this] { proc.clearPattern(); };
    statusLabel.setColour (juce::Label::textColourId, col::dim);
    statusLabel.setFont (juce::FontOptions (11.0f));
    addAndMakeVisible (statusLabel);

    // Modulation sequencer
    for (int l = 0; l < vg::numLanes; ++l)
    {
        auto& b = laneButtons[(size_t) l];
        b.setButtonText (vg::laneInfo (l).name);
        b.setClickingTogglesState (true);
        b.setRadioGroupId (1001);
        b.onClick = [this, l] { selectLane (l); };
        addAndMakeVisible (b);
    }
    laneButtons[0].setToggleState (true, juce::dontSendNotification);
    addAndMakeVisible (modGrid);

    // Sampler
    trackTitle.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    trackTitle.setColour (juce::Label::textColourId, col::accent);
    sampleInfo.setFont (juce::FontOptions (11.0f));
    sampleInfo.setColour (juce::Label::textColourId, col::dim);
    addAndMakeVisible (trackTitle);
    addAndMakeVisible (sampleInfo);

    const char* modeNames[] = { "Classic", "One-Shot", "Slice" };
    for (int i = 0; i < 3; ++i)
    {
        auto& b = modeButtons[(size_t) i];
        b.setButtonText (modeNames[i]);
        b.onClick = [this, i] { setChoice (vg::TPMode, i); };
        addAndMakeVisible (b);
    }
    addAndMakeVisible (loopToggle);
    addAndMakeVisible (reverseToggle);
    addAndMakeVisible (loadButton);
    addAndMakeVisible (factoryButton);
    addAndMakeVisible (previewButton);
    loadButton.onClick = [this] { chooseSample(); };
    factoryButton.onClick = [this] { proc.resetToFactorySample (selectedTrack); };
    previewButton.onClick = [this] { proc.previewTrack (selectedTrack); };
    addAndMakeVisible (waveform);

    for (const auto& kl : knobLabels)
        knobs[kl.tp].init (*this, kl.text);

    // Envelope selector: the ADSR of the chosen envelope is drawn on the waveform and its knobs are shown at the side
    for (int k = 0; k < vg::numEnvKinds; ++k)
    {
        auto& b = envTabs[(size_t) k];
        b.setButtonText (juce::String (vg::envName (k)).toUpperCase());
        b.setClickingTogglesState (true);
        b.setRadioGroupId (1002);
        b.setColour (juce::TextButton::buttonOnColourId, k == vg::EnvAmp ? col::accent : (k == vg::EnvPitch ? col::purple : col::accent2));
        b.onClick = [this, k]
        {
            envKindSel = k;
            waveform.envKind = k;
            layoutEnvKnobs();
            waveform.repaint();
        };
        addAndMakeVisible (b);
    }
    envTabs[0].setToggleState (true, juce::dontSendNotification);

    for (auto* box : { &fTypeBox, &lDestBox, &sModeBox })
        addAndMakeVisible (box);
    fTypeBox.addItemList (juce::StringArray::fromTokens (vg::tpSpec (vg::TPFType).choices, "|", ""), 1);
    lDestBox.addItemList (juce::StringArray::fromTokens (vg::tpSpec (vg::TPLDest).choices, "|", ""), 1);
    sModeBox.addItemList (juce::StringArray::fromTokens (vg::tpSpec (vg::TPSMode).choices, "|", ""), 1);

    // Colour coding and bipolar arcs
    auto tint = [this] (std::initializer_list<int> tps, juce::Colour c)
    {
        for (int tp : tps)
            knobs[tp].slider.getProperties().set ("col", (int) c.getARGB());
    };
    tint ({ vg::TPTranspose, vg::TPDetune, vg::TPGain, vg::TPPan }, col::accent);
    tint ({ vg::TPFCut, vg::TPFRes, vg::TPFltA, vg::TPFltD, vg::TPFltS, vg::TPFltR, vg::TPFltAmt }, col::accent2);
    tint ({ vg::TPAmpA, vg::TPAmpD, vg::TPAmpS, vg::TPAmpR }, col::accent);
    tint ({ vg::TPPitchA, vg::TPPitchD, vg::TPPitchS, vg::TPPitchR, vg::TPPitchAmt }, col::purple);
    tint ({ vg::TPLRate, vg::TPLAmt }, col::purple);
    tint ({ vg::TPStart, vg::TPEnd, vg::TPLoopStart, vg::TPLoopEnd, vg::TPLoopFade, vg::TPFadeIn, vg::TPFadeOut, vg::TPSCount, vg::TPSSens }, col::green);
    for (int tp : { vg::TPPan, vg::TPDetune, vg::TPTranspose, vg::TPPitchAmt, vg::TPFltAmt })
        knobs[tp].slider.getProperties().set ("bipolar", true);
    playButton.getProperties().set ("col", (int) col::green.getARGB());
    loopToggle.getProperties().set ("col", (int) col::accent2.getARGB());

    setSize (designW, designH);
    selectTrack (0);
    startTimerHz (30);
}

MainPanel::~MainPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MainPanel::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);

    // Top bar
    g.setColour (col::panel);
    g.fillRoundedRectangle (8.0f, 4.0f, 984.0f, 58.0f, 8.0f);
    g.setColour (col::border);
    g.drawRoundedRectangle (8.5f, 4.5f, 983.0f, 57.0f, 8.0f, 1.0f);

    g.setFont (juce::FontOptions (23.0f, juce::Font::bold));
    g.setColour (col::text);
    g.drawText ("VASSA", 22, 10, 80, 30, juce::Justification::centredLeft);
    g.setColour (col::accent);
    g.drawText ("GRUUV", 92, 10, 90, 30, juce::Justification::centredLeft);
    g.setColour (col::dim);
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("GROOVE BOX", 23, 38, 160, 14, juce::Justification::centredLeft);

    g.setColour (col::bg.withAlpha (0.5f));
    g.fillRoundedRectangle (292.0f, 9.0f, 336.0f, 48.0f, 6.0f);

    drawPanel (g, { 8, 68, 984, 296 }, "STEP SEQUENCER", 130);
    drawPanel (g, { 8, 368, 984, 132 }, "MODULATION", 130);
    drawPanel (g, { 8, 504, 984, 348 }, "SAMPLER", 70);

    g.setColour (col::dim);
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("drag the ADSR nodes on the waveform", 720, 540, 266, 12, juce::Justification::centredRight);
    g.drawText ("drag = paint  |  double-click / Alt = reset  |  wheel = fine", 740, 372, 244, 22, juce::Justification::centredRight);

    static const char* groupTitles[] = { "OUTPUT", "ENVELOPE", "LFO", "FILTER", "", "SAMPLE", "TRACK" };
    for (int i = 0; i < 7; ++i)
    {
        if (groupRects[i].isEmpty())
            continue;
        g.setColour (col::bg.withAlpha (0.5f));
        g.fillRoundedRectangle (groupRects[i].toFloat(), 6.0f);
        g.setColour (col::border.withAlpha (0.7f));
        g.drawRoundedRectangle (groupRects[i].toFloat().reduced (0.5f), 6.0f, 1.0f);
        g.setColour (col::dim);
        g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
        g.drawText (groupTitles[i], groupRects[i].getX() + 8, groupRects[i].getY() + 2, 140, 13, juce::Justification::centredLeft);
    }

    // Track info
    {
        const auto r = groupRects[6].reduced (8, 0).withTrimmedTop (18);
        g.setFont (juce::FontOptions (10.5f));
        g.setColour (col::track (selectedTrack));
        g.fillRoundedRectangle ((float) r.getX(), (float) r.getY() + 3.0f, 3.0f, 44.0f, 1.5f);
        g.setColour (col::text);
        g.drawText ("Out: Track " + juce::String (selectedTrack + 1), r.getX() + 9, r.getY(), r.getWidth() - 9, 16, juce::Justification::centredLeft);
        g.drawText ("MIDI: " + juce::MidiMessage::getMidiNoteName (36 + selectedTrack, true, true, 3) + " (" + juce::String (36 + selectedTrack) + ")",
                    r.getX() + 9, r.getY() + 15, r.getWidth() - 9, 16, juce::Justification::centredLeft);
        g.setColour (col::dim);
        g.drawText ("else -> Main Mix", r.getX() + 9, r.getY() + 30, r.getWidth() - 9, 16, juce::Justification::centredLeft);
    }
}

void MainPanel::resized()
{
    // Top bar
    playButton.setBounds (206, 18, 72, 26);
    int x = 298;
    for (auto* k : { &tempo, &swing, &filter, &reverb, &master })
    {
        k->setBounds (x, 8, 60, 50);
        x += 65;
    }
    dragHandle.setBounds (648, 18, 180, 30);
    loopsBox.setBounds (836, 20, 70, 26);
    exportButton.setBounds (912, 20, 72, 26);

    // Step sequencer
    styleBox.setBounds (590, 72, 120, 22);
    generateButton.setBounds (716, 72, 80, 22);
    mutateButton.setBounds (800, 72, 70, 22);
    clearButton.setBounds (874, 72, 60, 22);
    statusLabel.setBounds (150, 72, 430, 22);
    stepGrid.setBounds (8, 96, 984, 266);

    // Modulation
    for (int l = 0; l < vg::numLanes; ++l)
        laneButtons[(size_t) l].setBounds (150 + l * 84, 372, 80, 22);
    modGrid.setBounds (8, 396, 984, 102);

    // Sampler header
    trackTitle.setBounds (100, 509, 120, 24);
    sampleInfo.setBounds (216, 511, 214, 20);
    for (int i = 0; i < 3; ++i)
        modeButtons[(size_t) i].setBounds (432 + i * 69, 510, 67, 22);
    loopToggle.setBounds (646, 510, 54, 22);
    reverseToggle.setBounds (704, 510, 70, 22);
    loadButton.setBounds (784, 510, 44, 22);
    factoryButton.setBounds (832, 510, 58, 22);
    previewButton.setBounds (894, 510, 90, 22);

    waveform.setBounds (16, 538, 620, 232);

    groupRects[5] = { 16, 774, 620, 72 };    // sample
    groupRects[1] = { 648, 538, 344, 118 };  // envelope
    groupRects[0] = { 648, 660, 172, 90 };   // output
    groupRects[3] = { 824, 660, 168, 90 };   // filter
    groupRects[2] = { 648, 754, 172, 92 };   // lfo
    groupRects[6] = { 824, 754, 168, 92 };   // track info
    groupRects[4] = {};

    // Envelope selector (the editor itself lives on the waveform)
    for (int k = 0; k < vg::numEnvKinds; ++k)
        envTabs[(size_t) k].setBounds (654 + k * 62, 558, 60, 20);
    layoutEnvKnobs();

    const int kh = 68;
    int xx = groupRects[0].getX() + 2;
    for (int tp : { vg::TPTranspose, vg::TPDetune, vg::TPGain, vg::TPPan })
    {
        knobs[tp].setBounds (xx, groupRects[0].getY() + 15, 42, kh);
        xx += 42;
    }

    fTypeBox.setBounds (groupRects[3].getX() + 4, groupRects[3].getY() + 34, 58, 22);
    knobs[vg::TPFCut].setBounds (groupRects[3].getX() + 64, groupRects[3].getY() + 15, 54, kh);
    knobs[vg::TPFRes].setBounds (groupRects[3].getX() + 118, groupRects[3].getY() + 15, 48, kh);

    lDestBox.setBounds (groupRects[2].getX() + 4, groupRects[2].getY() + 34, 58, 22);
    knobs[vg::TPLRate].setBounds (groupRects[2].getX() + 64, groupRects[2].getY() + 15, 54, kh);
    knobs[vg::TPLAmt].setBounds (groupRects[2].getX() + 118, groupRects[2].getY() + 15, 52, kh);

    layoutSamplerControls();
}

void MainPanel::layoutEnvKnobs()
{
    for (int k = 0; k < vg::numEnvKinds; ++k)
        for (int w = 0; w < 5; ++w)
        {
            const int tp = vg::envParam (k, w);
            if (tp < 0)
                continue;
            knobs[tp].setVisible (k == envKindSel);
            knobs[tp].setBounds (groupRects[1].getX() + 8 + w * 66, groupRects[1].getY() + 40, 62, 72);
        }
}

void MainPanel::layoutSamplerControls()
{
    const auto prm = proc.readTrackParams (selectedTrack);
    const int mode = prm.idx (vg::TPMode);
    const bool equal = prm.idx (vg::TPSMode) == 0;

    for (int tp : { vg::TPStart, vg::TPEnd, vg::TPLoopStart, vg::TPLoopEnd, vg::TPLoopFade, vg::TPFadeIn, vg::TPFadeOut, vg::TPSCount, vg::TPSSens })
        knobs[tp].setVisible (false);
    sModeBox.setVisible (false);
    loopToggle.setVisible (mode == vg::ModeClassic);

    std::vector<int> order { vg::TPStart, vg::TPEnd };
    if (mode == vg::ModeClassic)
        order.insert (order.end(), { vg::TPLoopStart, vg::TPLoopEnd, vg::TPLoopFade });
    if (mode == vg::ModeSlice)
    {
        order.push_back (-1); // slice mode box
        order.push_back (equal ? vg::TPSCount : vg::TPSSens);
    }
    order.insert (order.end(), { vg::TPFadeIn, vg::TPFadeOut });

    int x = groupRects[5].getX() + 4;
    const int y = groupRects[5].getY() + 15;
    for (int tp : order)
    {
        if (tp < 0)
        {
            sModeBox.setBounds (x + 2, y + 24, 66, 22);
            sModeBox.setVisible (true);
            x += 70;
        }
        else
        {
            knobs[tp].setBounds (x, y, 56, 56);
            knobs[tp].setVisible (true);
            x += 58;
        }
    }
}

void MainPanel::bindCombo (juce::ComboBox& box, std::unique_ptr<APVTS::ComboBoxAttachment>& att, int tp)
{
    att.reset();
    att = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, vg::trackParamId (selectedTrack, tp), box);
}

void MainPanel::setChoice (int tp, int index)
{
    setParamPlain (proc.apvts.getParameter (vg::trackParamId (selectedTrack, tp)), (float) index);
}

void MainPanel::selectTrack (int track)
{
    selectedTrack = track;
    stepGrid.selected = track;
    modGrid.track = track;
    waveform.track = track;

    for (const auto& kl : knobLabels)
        knobs[kl.tp].attach (proc.apvts, vg::trackParamId (track, kl.tp));
    bindCombo (fTypeBox, fTypeAtt, vg::TPFType);
    bindCombo (lDestBox, lDestAtt, vg::TPLDest);
    bindCombo (sModeBox, sModeAtt, vg::TPSMode);

    loopAttachment.reset();
    reverseAttachment.reset();
    loopAttachment = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, vg::trackParamId (track, vg::TPLoop), loopToggle);
    reverseAttachment = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, vg::trackParamId (track, vg::TPReverse), reverseToggle);

    shownKey = -1; // force a relayout on the next tick
    layoutSamplerControls();
    repaint();
}

void MainPanel::selectLane (int lane)
{
    selectedLane = lane;
    modGrid.lane = lane;
    modGrid.repaint();
}

void MainPanel::chooseSample()
{
    chooser = std::make_unique<juce::FileChooser> ("Load sample", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    juce::Component::SafePointer<MainPanel> safe (this);
    const int track = selectedTrack;
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe, track] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (safe == nullptr || ! f.existsAsFile())
                                  return;
                              if (! safe->proc.loadSample (track, f))
                                  safe->proc.aiStatus = "Could not load " + f.getFileName() + " (max 60 s)";
                          });
}

void MainPanel::startStemDrag()
{
    if (dragInProgress)
        return;

    if (lastExportDir.isDirectory())
        lastExportDir.deleteRecursively();
    lastExportDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("VassaGruuv_export")
                        .getChildFile (juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S"));

    const auto files = proc.exportStems (1 << (loopsBox.getSelectedId() - 1), lastExportDir);
    if (files.isEmpty())
    {
        proc.aiStatus = "Nothing to export: add some steps first";
        return;
    }

    juce::StringArray paths;
    for (const auto& f : files)
        paths.add (f.getFullPathName());

    dragInProgress = true;
    juce::Component::SafePointer<MainPanel> safe (this);
    juce::DragAndDropContainer::performExternalDragDropOfFiles (paths, false, &dragHandle,
                                                                [safe] { if (safe != nullptr) safe->dragInProgress = false; });
    proc.aiStatus = juce::String (files.size()) + " stems ready - drop them on DAW tracks";
}

void MainPanel::exportStemsToFolder()
{
    chooser = std::make_unique<juce::FileChooser> ("Export stems to...", lastExportDir.getParentDirectory(), "");
    juce::Component::SafePointer<MainPanel> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [safe] (const juce::FileChooser& fc)
                          {
                              const auto dir = fc.getResult();
                              if (safe == nullptr || ! dir.isDirectory())
                                  return;
                              const auto files = safe->proc.exportStems (1 << (safe->loopsBox.getSelectedId() - 1), dir.getChildFile ("VassaGruuv stems"));
                              safe->proc.aiStatus = files.isEmpty() ? "Nothing to export: add some steps first"
                                                                    : "Exported " + juce::String (files.size()) + " stems";
                          });
}

void MainPanel::timerCallback()
{
    const int step = proc.currentStep.load();
    if (step != stepGrid.playhead)
        stepGrid.playhead = step;
    stepGrid.repaint();
    modGrid.repaint();
    waveform.repaint();

    const auto prm = proc.readTrackParams (selectedTrack);
    const int mode = prm.idx (vg::TPMode);
    for (int i = 0; i < 3; ++i)
        modeButtons[(size_t) i].setToggleState (mode == i, juce::dontSendNotification);

    const int key = mode * 10 + prm.idx (vg::TPSMode);
    if (key != shownKey)
    {
        shownKey = key;
        layoutSamplerControls();
    }

    const auto smp = proc.getSample (selectedTrack);
    trackTitle.setColour (juce::Label::textColourId, col::track (selectedTrack));
    trackTitle.setText (juce::String (selectedTrack + 1).paddedLeft ('0', 2) + "  " + proc.getTrackName (selectedTrack), juce::dontSendNotification);
    const auto info = smp != nullptr ? smp->name + "  -  " + juce::String ((double) smp->numFrames() / smp->rate, 2) + " s"
                                         + (smp->factory ? "  (factory)" : "")
                                     : juce::String();
    if (sampleInfo.getText() != info)
        sampleInfo.setText (info, juce::dontSendNotification);
    factoryButton.setEnabled (smp != nullptr && ! smp->factory);

    statusLabel.setText (proc.aiStatus, juce::dontSendNotification);
}

// ------------------------------------------------------------ Editor (scaling wrapper)

VassaGruuvEditor::VassaGruuvEditor (VassaGruuvProcessor& p)
    : AudioProcessorEditor (&p), panel (p)
{
    addAndMakeVisible (panel);
    constrainer.setFixedAspectRatio ((double) MainPanel::designW / MainPanel::designH);
    constrainer.setSizeLimits (MainPanel::designW * 3 / 4, MainPanel::designH * 3 / 4, MainPanel::designW * 3 / 2, MainPanel::designH * 3 / 2);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize (MainPanel::designW, MainPanel::designH);
}

void VassaGruuvEditor::resized()
{
    panel.setBounds (0, 0, MainPanel::designW, MainPanel::designH);
    panel.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) MainPanel::designW));
}
