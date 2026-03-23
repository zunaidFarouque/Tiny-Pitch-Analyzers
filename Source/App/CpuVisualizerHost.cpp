#include "CpuVisualizerHost.h"

void CpuVisualizerHost::setRenderFrame (const pitchlab::RenderFrameData& frame) noexcept
{
    const juce::ScopedLock sl (frameLock_);
    frame_ = frame;
    repaint();
}

void CpuVisualizerHost::pushWaterfallRow (std::span<const float> row384)
{
    waterfallRing_.pushRow (row384);
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

    pitchlab::VizCpuRenderer renderer (juce::jmax (1, getWidth()), juce::jmax (1, getHeight()));
    const juce::Image img = renderer.render (toVizMode (mode_), vf, tables_);
    g.drawImageAt (img, 0, 0);

    g.setColour (juce::Colours::orange.withAlpha (0.95f));
    g.setFont (juce::FontOptions { 12.0f, juce::Font::bold });
    g.drawText ("CPU Mode", getLocalBounds().removeFromTop (20).reduced (6, 2), juce::Justification::centredLeft);
}

