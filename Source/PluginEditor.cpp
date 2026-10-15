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
    { vg::TPPan, "Pan" }, { vg::TPAmpA, "A" }, { vg::TPAmpD, "D" }, { vg::TPAmpS, "S" }, { vg::TPAmpR, "R" },
    { vg::TPFCut, "Cutoff" }, { vg::TPFRes, "Res" }, { vg::TPFEnv, "Env Amt" }, { vg::TPFA, "A" }, { vg::TPFD, "D" },
    { vg::TPFS, "S" }, { vg::TPFR, "R" }, { vg::TPLRate, "Rate" }, { vg::TPLAmt, "Amount" },
};
} // namespace

// ------------------------------------------------------------ LookAndFeel

LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, col::bg);
    setColour (juce::Slider::rotarySliderFillColourId, col::accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, col::panelHi);
    setColour (juce::Slider::thumbColourId, col::text);
    setColour (juce::Label::textColourId, col::text);
    setColour (juce::TextButton::buttonColourId, col::panelHi);
    setColour (juce::TextButton::buttonOnColourId, col::accent);
    setColour (juce::TextButton::textColourOffId, col::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    setColour (juce::ToggleButton::textColourId, col::text);
    setColour (juce::ToggleButton::tickColourId, col::accent);
    setColour (juce::ComboBox::backgroundColourId, col::panelHi);
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::TooltipWindow::backgroundColourId, col::panelHi);
    setColour (juce::TooltipWindow::textColourId, col::text);
    setColour (juce::BubbleComponent::backgroundColourId, col::panelHi);
    setColour (juce::BubbleComponent::outlineColourId, col::accent);
}

// ------------------------------------------------------------ Knob / DragHandle

void Knob::init (juce::Component& parent, const juce::String& text)
{
    slider.setPopupDisplayEnabled (true, true, nullptr);
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (10.0f));
    label.setColour (juce::Label::textColourId, col::dim);
    label.setInterceptsMouseClicks (false, false);
    parent.addAndMakeVisible (slider);
    parent.addAndMakeVisible (label);
}

void Knob::attach (juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    attachment.reset();
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider);
}

void Knob::setBounds (int x, int y, int w, int h)
{
    label.setBounds (x, y, w, 11);
    slider.setBounds (x, y + 11, w, h - 11);
}

void DragHandle::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (col::accent2.withAlpha (0.18f));
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (col::accent2);
    g.drawRoundedRectangle (b, 6.0f, 1.5f);
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 2; ++j)
            g.fillEllipse (10.0f + (float) i * 5.0f, b.getCentreY() - 5.0f + (float) j * 7.0f, 3.0f, 3.0f);
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
        }

        g.setColour (col::dim);
        g.setFont (juce::FontOptions (9.0f));
        g.drawText (juce::String (t + 1), 4, (int) y, 14, (int) ch, juce::Justification::centredLeft);
        g.setColour (muted ? col::dim.withAlpha (0.6f) : (sel ? col::accent : col::text));
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
                g.setColour ((muted ? col::dim : col::accent).withAlpha (0.35f + 0.65f * (float) v / 127.0f));
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

    const auto barColour = lane == vg::Velocity ? col::accent : col::accent2;

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

void WaveformView::rebuildPeaks()
{
    peaks.assign ((size_t) juce::jmax (1, getWidth()), { 0.0f, 0.0f });
    if (cached == nullptr)
        return;

    const int N = cached->numFrames(), w = (int) peaks.size();
    for (int x = 0; x < w; ++x)
    {
        const int a = (int) ((int64_t) x * N / w);
        const int n = juce::jmax (1, (int) ((int64_t) (x + 1) * N / w) - a);
        float mn = 0.0f, mx = 0.0f;
        for (int c = 0; c < cached->data.getNumChannels(); ++c)
        {
            const auto r = juce::FloatVectorOperations::findMinAndMax (cached->data.getReadPointer (c) + a, juce::jmin (n, N - a));
            mn = juce::jmin (mn, r.getStart());
            mx = juce::jmax (mx, r.getEnd());
        }
        peaks[(size_t) x] = { mn, mx };
    }
}

void WaveformView::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (col::bg.brighter (0.03f));
    g.fillRoundedRectangle (b, 5.0f);

    auto smp = proc.getSample (track);
    if (smp == nullptr)
    {
        g.setColour (col::dim);
        g.drawText ("Drop an audio file here", getLocalBounds(), juce::Justification::centred);
        return;
    }
    if (smp != cached || getWidth() != cachedWidth)
    {
        cached = smp;
        cachedWidth = getWidth();
        rebuildPeaks();
    }

    const auto prm = proc.readTrackParams (track);
    const int mode = prm.idx (vg::TPMode);
    const float w = b.getWidth(), h = b.getHeight(), mid = h * 0.5f;

    g.setColour (col::dim.withAlpha (0.25f));
    g.drawHorizontalLine ((int) mid, 0.0f, w);

    g.setColour (col::accent.withAlpha (0.85f));
    for (size_t x = 0; x < peaks.size(); ++x)
        g.drawVerticalLine ((int) x, mid - peaks[x].second * mid * 0.92f, mid - peaks[x].first * mid * 0.92f + 1.0f);

    const float sx = prm[vg::TPStart] * w, ex = juce::jmax (prm[vg::TPEnd], prm[vg::TPStart] + 0.002f) * w;
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRect (0.0f, 0.0f, sx, h);
    g.fillRect (ex, 0.0f, w - ex, h);

    const float framesToPx = (float) smp->rate / (float) smp->numFrames() * w * 0.001f;
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    if (prm[vg::TPFadeIn] > 0.5f)
        g.drawLine (sx, h, sx + prm[vg::TPFadeIn] * framesToPx, 2.0f);
    if (prm[vg::TPFadeOut] > 0.5f && ! (mode == vg::ModeClassic && prm.on (vg::TPLoop)))
        g.drawLine (ex - prm[vg::TPFadeOut] * framesToPx, 2.0f, ex, h);

    if (mode == vg::ModeSlice)
    {
        const auto marks = vg::sliceBoundaries (*smp, prm);
        g.setFont (juce::FontOptions (9.0f));
        float prev = sx;
        int i = 1;
        for (const auto m : marks)
        {
            g.setColour (col::accent2);
            g.drawVerticalLine ((int) (m * w), 0.0f, h);
            g.setColour (col::accent2.withAlpha (0.9f));
            g.drawText (juce::String (i), (int) prev + 2, 1, 20, 11, juce::Justification::left);
            prev = m * w;
            ++i;
        }
        g.drawText (juce::String (i), (int) prev + 2, 1, 20, 11, juce::Justification::left);
    }

    if (mode == vg::ModeClassic && prm.on (vg::TPLoop))
    {
        const float ls = prm[vg::TPLoopStart] * w, le = prm[vg::TPLoopEnd] * w;
        g.setColour (col::accent2.withAlpha (0.18f));
        g.fillRect (ls, 0.0f, le - ls, h);
        g.setColour (col::accent2);
        g.drawVerticalLine ((int) ls, 0.0f, h);
        g.drawVerticalLine ((int) le, 0.0f, h);
        g.fillRect (ls, 0.0f, 7.0f, 4.0f);
        g.fillRect (le - 7.0f, 0.0f, 7.0f, 4.0f);
    }

    g.setColour (juce::Colour (0xff4cd964));
    g.fillRect (sx - 0.5f, 0.0f, 1.5f, h);
    g.fillRect (sx, 0.0f, 9.0f, 5.0f);
    g.setColour (juce::Colour (0xffff5a5a));
    g.fillRect (ex - 0.5f, 0.0f, 1.5f, h);
    g.fillRect (ex - 9.0f, 0.0f, 9.0f, 5.0f);

    const float ph = proc.getPlayhead (track);
    if (ph >= 0.0f)
    {
        g.setColour (juce::Colours::white);
        g.drawVerticalLine ((int) (ph * w), 0.0f, h);
    }
}

int WaveformView::handleAt (int x) const
{
    const auto prm = proc.readTrackParams (track);
    const float w = (float) getWidth();
    struct C { int tp; float px; };
    std::vector<C> c { { vg::TPStart, prm[vg::TPStart] * w }, { vg::TPEnd, prm[vg::TPEnd] * w } };
    if (prm.idx (vg::TPMode) == vg::ModeClassic && prm.on (vg::TPLoop))
    {
        c.push_back ({ vg::TPLoopStart, prm[vg::TPLoopStart] * w });
        c.push_back ({ vg::TPLoopEnd, prm[vg::TPLoopEnd] * w });
    }

    int best = -1;
    float bestDist = 8.0f;
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
    setMouseCursor (handleAt (e.x) >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
}

void WaveformView::mouseDown (const juce::MouseEvent& e)
{
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
    if (dragging < 0)
        return;

    const auto prm = proc.readTrackParams (track);
    float v = juce::jlimit (0.0f, 1.0f, (float) e.x / (float) getWidth());
    constexpr float gap = 0.002f;
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
    if (dragging >= 0)
        if (auto* p = param (dragging))
            p->endChangeGesture();
    dragging = -1;
}

void WaveformView::filesDropped (const juce::StringArray& files, int, int)
{
    if (! files.isEmpty() && proc.loadSample (track, juce::File (files[0])) && onSampleChanged)
        onSampleChanged();
}

// ------------------------------------------------------------ Editor

VassaGruuvEditor::VassaGruuvEditor (VassaGruuvProcessor& p)
    : AudioProcessorEditor (&p),
      proc (p),
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

    for (auto* box : { &fTypeBox, &lDestBox, &sModeBox })
        addAndMakeVisible (box);
    fTypeBox.addItemList (juce::StringArray::fromTokens (vg::tpSpec (vg::TPFType).choices, "|", ""), 1);
    lDestBox.addItemList (juce::StringArray::fromTokens (vg::tpSpec (vg::TPLDest).choices, "|", ""), 1);
    sModeBox.addItemList (juce::StringArray::fromTokens (vg::tpSpec (vg::TPSMode).choices, "|", ""), 1);

    setSize (1000, 788);
    selectTrack (0);
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
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("VASSA GRUUV", 16, 8, 200, 28, juce::Justification::centredLeft);
    g.setColour (col::dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("groove box", 18, 34, 200, 14, juce::Justification::centredLeft);

    const juce::Rectangle<int> panels[] = { { 8, 56, 984, 296 }, { 8, 356, 984, 132 }, { 8, 492, 984, 288 } };
    const char* titles[] = { "STEP SEQUENCER", "MODULATION", "SAMPLER" };
    for (int i = 0; i < 3; ++i)
    {
        g.setColour (col::panel);
        g.fillRoundedRectangle (panels[i].toFloat(), 8.0f);
        g.setColour (col::accent);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        if (i != 2)
            g.drawText (titles[i], panels[i].getX() + 10, panels[i].getY() + 5, 130, 18, juce::Justification::centredLeft);
        else
            g.drawText (titles[i], panels[i].getX() + 10, panels[i].getY() + 5, 70, 18, juce::Justification::centredLeft);
    }

    g.setColour (col::dim);
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("drag = paint bars  |  double-click / Alt = reset  |  wheel = fine", 740, 360, 244, 22, juce::Justification::centredRight);

    const char* groupTitles[] = { "OUTPUT", "AMP ENVELOPE", "LFO", "FILTER", "FILTER ENVELOPE", "SAMPLE" };
    for (int i = 0; i < 6; ++i)
    {
        g.setColour (col::bg.withAlpha (0.45f));
        g.fillRoundedRectangle (groupRects[i].toFloat(), 5.0f);
        g.setColour (col::dim);
        g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
        g.drawText (groupTitles[i], groupRects[i].getX() + 6, groupRects[i].getY() + 1, 120, 13, juce::Justification::centredLeft);
    }
}

void VassaGruuvEditor::resized()
{
    // Top bar
    playButton.setBounds (210, 14, 70, 26);
    int x = 296;
    for (auto* k : { &tempo, &swing, &filter, &reverb, &master })
    {
        k->setBounds (x, 4, 54, 50);
        x += 60;
    }
    dragHandle.setBounds (640, 12, 180, 30);
    loopsBox.setBounds (828, 14, 74, 26);
    exportButton.setBounds (908, 14, 84, 26);

    // Step sequencer
    styleBox.setBounds (590, 60, 120, 22);
    generateButton.setBounds (716, 60, 80, 22);
    mutateButton.setBounds (800, 60, 70, 22);
    clearButton.setBounds (874, 60, 60, 22);
    statusLabel.setBounds (150, 60, 430, 22);
    stepGrid.setBounds (8, 84, 984, 266);

    // Modulation
    for (int l = 0; l < vg::numLanes; ++l)
        laneButtons[(size_t) l].setBounds (150 + l * 84, 360, 80, 22);
    modGrid.setBounds (8, 384, 984, 102);

    // Sampler header
    trackTitle.setBounds (80, 496, 130, 24);
    sampleInfo.setBounds (200, 498, 200, 20);
    for (int i = 0; i < 3; ++i)
        modeButtons[(size_t) i].setBounds (404 + i * 74, 497, 72, 22);
    loopToggle.setBounds (634, 497, 60, 22);
    reverseToggle.setBounds (698, 497, 76, 22);
    loadButton.setBounds (788, 497, 50, 22);
    factoryButton.setBounds (842, 497, 60, 22);
    previewButton.setBounds (906, 497, 74, 22);

    waveform.setBounds (16, 528, 470, 166);

    const int kw = 59, kh = 60;
    groupRects[0] = { 500, 528, 236, 76 };   // output
    groupRects[1] = { 500, 606, 236, 76 };   // amp env
    groupRects[2] = { 500, 684, 236, 76 };   // lfo
    groupRects[3] = { 748, 528, 244, 76 };   // filter
    groupRects[4] = { 748, 606, 244, 76 };   // filter env
    groupRects[5] = { 16, 700, 470, 76 };    // sample

    auto row = [&] (const juce::Rectangle<int>& r, std::initializer_list<int> tps)
    {
        int xx = r.getX() + 2;
        for (int tp : tps)
        {
            knobs[tp].setBounds (xx, r.getY() + 14, kw, kh);
            xx += kw;
        }
    };
    row (groupRects[0], { vg::TPTranspose, vg::TPDetune, vg::TPGain, vg::TPPan });
    row (groupRects[1], { vg::TPAmpA, vg::TPAmpD, vg::TPAmpS, vg::TPAmpR });
    row (groupRects[4], { vg::TPFA, vg::TPFD, vg::TPFS, vg::TPFR });

    lDestBox.setBounds (groupRects[2].getX() + 6, groupRects[2].getY() + 32, 82, 24);
    knobs[vg::TPLRate].setBounds (groupRects[2].getX() + 100, groupRects[2].getY() + 14, kw, kh);
    knobs[vg::TPLAmt].setBounds (groupRects[2].getX() + 100 + kw, groupRects[2].getY() + 14, kw, kh);

    fTypeBox.setBounds (groupRects[3].getX() + 6, groupRects[3].getY() + 32, 70, 24);
    knobs[vg::TPFCut].setBounds (groupRects[3].getX() + 82, groupRects[3].getY() + 14, 52, kh);
    knobs[vg::TPFRes].setBounds (groupRects[3].getX() + 82 + 52, groupRects[3].getY() + 14, 52, kh);
    knobs[vg::TPFEnv].setBounds (groupRects[3].getX() + 82 + 104, groupRects[3].getY() + 14, 52, kh);

    layoutSamplerControls();
}

void VassaGruuvEditor::layoutSamplerControls()
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
    const int y = groupRects[5].getY() + 14;
    for (int tp : order)
    {
        if (tp < 0)
        {
            sModeBox.setBounds (x, y + 18, 72, 24);
            sModeBox.setVisible (true);
            x += 72;
        }
        else
        {
            knobs[tp].setBounds (x, y, 62, 60);
            knobs[tp].setVisible (true);
            x += 62;
        }
    }
}

void VassaGruuvEditor::bindCombo (juce::ComboBox& box, std::unique_ptr<APVTS::ComboBoxAttachment>& att, int tp)
{
    att.reset();
    att = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, vg::trackParamId (selectedTrack, tp), box);
}

void VassaGruuvEditor::setChoice (int tp, int index)
{
    setParamPlain (proc.apvts.getParameter (vg::trackParamId (selectedTrack, tp)), (float) index);
}

void VassaGruuvEditor::selectTrack (int track)
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

void VassaGruuvEditor::selectLane (int lane)
{
    selectedLane = lane;
    modGrid.lane = lane;
    modGrid.repaint();
}

void VassaGruuvEditor::chooseSample()
{
    chooser = std::make_unique<juce::FileChooser> ("Load sample", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    juce::Component::SafePointer<VassaGruuvEditor> safe (this);
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

void VassaGruuvEditor::startStemDrag()
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
    juce::Component::SafePointer<VassaGruuvEditor> safe (this);
    juce::DragAndDropContainer::performExternalDragDropOfFiles (paths, false, &dragHandle,
                                                                [safe] { if (safe != nullptr) safe->dragInProgress = false; });
    proc.aiStatus = juce::String (files.size()) + " stems ready - drop them on DAW tracks";
}

void VassaGruuvEditor::exportStemsToFolder()
{
    chooser = std::make_unique<juce::FileChooser> ("Export stems to...", lastExportDir.getParentDirectory(), "");
    juce::Component::SafePointer<VassaGruuvEditor> safe (this);
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

void VassaGruuvEditor::timerCallback()
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
    trackTitle.setText (juce::String (selectedTrack + 1).paddedLeft ('0', 2) + "  " + proc.getTrackName (selectedTrack), juce::dontSendNotification);
    const auto info = smp != nullptr ? smp->name + "  -  " + juce::String ((double) smp->numFrames() / smp->rate, 2) + " s"
                                         + (smp->factory ? "  (factory)" : "")
                                     : juce::String();
    if (sampleInfo.getText() != info)
        sampleInfo.setText (info, juce::dontSendNotification);
    factoryButton.setEnabled (smp != nullptr && ! smp->factory);

    statusLabel.setText (proc.aiStatus, juce::dontSendNotification);
}
