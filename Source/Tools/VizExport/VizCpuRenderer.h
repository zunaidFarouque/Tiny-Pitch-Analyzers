#pragma once

#include "PitchLabEngine.h"

#include <juce_graphics/juce_graphics.h>

#include <array>

namespace pitchlab
{

enum class VizMode
{
    Waveform,
    Waterfall,
    Needle,
    StrobeRadial,
    ChordMatrix
};

struct VizFrameData
{
    std::array<float, 384> chromaRow {};
    std::array<std::int16_t, 2048> waveform {};
    std::array<float, kChordMatrixFloats> chordProbabilities {};
    float tuningError = 0.0f;
    float strobePhase = 0.0f;
    int waterfallWriteY = 1; // mirrors one uploaded row in the film reel
    /** If non-null, 384×384 row-major (row=time, col=chroma); overrides repeated chromaRow for waterfall. */
    const float* waterfallGrid384 = nullptr;
};

struct WaterfallRenderParams
{
    float energyScale = 0.064f;
    float alphaPower = 2.55f;
    float alphaThreshold = 0.0050f;
    WaterfallDisplayCurveMode curveMode = WaterfallDisplayCurveMode::Sqrt;
};

class VizCpuRenderer
{
public:
    VizCpuRenderer (int width, int height);
    void setWaterfallRenderParams (const WaterfallRenderParams& p) noexcept;

    [[nodiscard]] juce::Image render (VizMode mode,
                                      const VizFrameData& frame,
                                      const StaticTables* tables) const;

private:
    juce::Image renderWaveform (const VizFrameData& frame) const;
    juce::Image renderWaterfall (const VizFrameData& frame) const;
    juce::Image renderNeedle (const VizFrameData& frame) const;
    juce::Image renderStrobe (const VizFrameData& frame, const StaticTables* tables) const;
    juce::Image renderChordMatrix (const VizFrameData& frame) const;

    int width_ = 1024;
    int height_ = 384;
    WaterfallRenderParams waterfallParams_ {};
};

[[nodiscard]] const char* modeToName (VizMode mode) noexcept;
[[nodiscard]] bool parseModeList (const juce::String& csv, juce::Array<VizMode>& outModes);

} // namespace pitchlab

