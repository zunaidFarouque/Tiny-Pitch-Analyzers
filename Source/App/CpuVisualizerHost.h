#pragma once

#include "IRendererHost.h"
#include "SharedWaterfallRing.h"
#include "VizCpuRenderer.h"

#include <JuceHeader.h>
#include <vector>

class CpuVisualizerHost final : public juce::Component,
                                public IRendererHost
{
public:
    CpuVisualizerHost() = default;
    ~CpuVisualizerHost() override = default;

    juce::Component& component() noexcept override { return *this; }
    void setMode (VisualizationMode mode) noexcept override { mode_ = mode; repaint(); }
    VisualizationMode mode() const noexcept override { return mode_; }
    void setStaticTablesPtr (const pitchlab::StaticTables* tables) noexcept override { tables_ = tables; }
    void setRenderFrame (const pitchlab::RenderFrameData& frame) noexcept override;
    void pushWaterfallRow (std::span<const float> row384) override;
    void commitWaterfallGrid384 (std::span<const float> rowMajor384x384) noexcept override;
    void setWaterfallEnergyScale (float s) noexcept;
    void setWaterfallAlphaPower (float p) noexcept;
    void setWaterfallAlphaThreshold (float t) noexcept;
    void setWaterfallDisplayCurveMode (pitchlab::WaterfallDisplayCurveMode m) noexcept;

    void paint (juce::Graphics& g) override;

private:
    pitchlab::VizMode toVizMode (VisualizationMode m) const noexcept;

    const pitchlab::StaticTables* tables_ = nullptr;
    VisualizationMode mode_ = VisualizationMode::Waveform;
    pitchlab::RenderFrameData frame_{};
    SharedWaterfallRing waterfallRing_;
    juce::CriticalSection frameLock_;
    pitchlab::WaterfallRenderParams waterfallParams_ {};
    std::vector<float> waterfallGridStorage_;
    bool hasWaterfallGrid_ = false;
};

