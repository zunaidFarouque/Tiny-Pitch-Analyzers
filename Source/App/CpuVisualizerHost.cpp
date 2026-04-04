#include "CpuVisualizerHost.h"

void CpuVisualizerHost::setRenderFrame (const pitchlab::RenderFrameData& frame) noexcept
{
    const juce::ScopedLock sl (frameLock_);
    frame_ = frame;
    repaint();
}

void CpuVisualizerHost::pushWaterfallRow (std::span<const float> row384)
{
    hasWaterfallGrid_ = false;
    waterfallRing_.pushRow (row384);
}

void CpuVisualizerHost::commitWaterfallGrid384 (std::span<const float> rowMajor384x384) noexcept
{
    constexpr int kCells = SharedWaterfallRing::kRowBins * SharedWaterfallRing::kRows;
    if (rowMajor384x384.size() < static_cast<std::size_t> (kCells))
        return;

    const juce::ScopedLock sl (frameLock_);
    waterfallGridStorage_.assign (rowMajor384x384.begin(),
                                  rowMajor384x384.begin() + static_cast<std::ptrdiff_t> (kCells));
    hasWaterfallGrid_ = true;
    waterfallRing_.syncWriteHeadAfterBulkStaticFill();
    repaint();
}

void CpuVisualizerHost::setWaterfallEnergyScale (float s) noexcept
{
    const juce::ScopedLock sl (frameLock_);
    waterfallParams_.energyScale = juce::jmax (0.0f, s);
}

void CpuVisualizerHost::setWaterfallAlphaPower (float p) noexcept
{
    const juce::ScopedLock sl (frameLock_);
    waterfallParams_.alphaPower = juce::jmax (0.0f, p);
}

void CpuVisualizerHost::setWaterfallAlphaThreshold (float t) noexcept
{
    const juce::ScopedLock sl (frameLock_);
    waterfallParams_.alphaThreshold = juce::jmax (0.0f, t);
}

void CpuVisualizerHost::setWaterfallDisplayCurveMode (pitchlab::WaterfallDisplayCurveMode m) noexcept
{
    const juce::ScopedLock sl (frameLock_);
    waterfallParams_.curveMode = m;
}

pitchlab::VizMode CpuVisualizerHost::toVizMode (VisualizationMode m) const noexcept
{
    switch (m)
    {
        case VisualizationMode::Waveform: return pitchlab::VizMode::Waveform;
        case VisualizationMode::Waterfall: return pitchlab::VizMode::Waterfall;
        case VisualizationMode::Needle: return pitchlab::VizMode::Needle;
        case VisualizationMode::StrobeRadial: return pitchlab::VizMode::StrobeRadial;
        case VisualizationMode::ChordMatrix: return pitchlab::VizMode::ChordMatrix;
        case VisualizationMode::SyntheticPeaks: return pitchlab::VizMode::SyntheticPeaks;
        default: return pitchlab::VizMode::Waveform;
    }
}

void CpuVisualizerHost::paint (juce::Graphics& g)
{
    pitchlab::RenderFrameData frame;
    {
        const juce::ScopedLock sl (frameLock_);
        frame = frame_;
    }

    pitchlab::VizFrameData vf;
    vf.waveform = frame.waveform;
    vf.chromaRow = frame.chromaRow;
    vf.chordProbabilities = frame.chordProbabilities;
    vf.tuningError = frame.tuningError;
    vf.strobePhase = frame.strobePhase;
    vf.waterfallWriteY = waterfallRing_.writeY();
    vf.waterfallGrid384 = hasWaterfallGrid_ ? waterfallGridStorage_.data() : nullptr;
    vf.activePeaks = frame.activePeaks;

    pitchlab::VizCpuRenderer renderer (juce::jmax (1, getWidth()), juce::jmax (1, getHeight()));
    renderer.setWaterfallRenderParams (waterfallParams_);
    const juce::Image img = renderer.render (toVizMode (mode_), vf, tables_);
    g.drawImageAt (img, 0, 0);

    g.setColour (juce::Colours::orange.withAlpha (0.95f));
    g.setFont (juce::FontOptions { 12.0f, juce::Font::bold });
    g.drawText ("CPU Mode", getLocalBounds().removeFromTop (20).reduced (6, 2), juce::Justification::centredLeft);
}

