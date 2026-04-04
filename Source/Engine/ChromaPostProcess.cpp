#include "ChromaPostProcess.h"

#include "ChromaFolder.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace pitchlab
{

namespace
{
constexpr float kLogAlpha = 120.0f;

void logCompressInPlace (std::span<float> x) noexcept
{
    float mx = 0.0f;
    for (float v : x)
        mx = std::max (mx, v);
    if (mx <= 1.0e-12f)
        return;

    const float denom = std::log1p (kLogAlpha * mx);
    if (denom <= 1.0e-12f)
        return;

    for (auto& v : x)
        v = std::max (0.0f, std::log1p (kLogAlpha * std::max (0.0f, v)) / denom);
}

void noiseFloorSubtractInPlace (std::span<float> x) noexcept
{
    std::vector<float> sorted (x.begin(), x.end());
    const auto mid = sorted.begin() + static_cast<std::ptrdiff_t> (sorted.size() / 2);
    std::nth_element (sorted.begin(), mid, sorted.end());
    const float med = *mid;
    const float floorv = 0.5f * med;
    for (auto& v : x)
        v = std::max (0.0f, v - floorv);
}

void percentileGateInPlace (std::span<float> x) noexcept
{
    std::vector<float> sorted (x.begin(), x.end());
    constexpr float p = 0.85f;
    const std::size_t idx = std::min (sorted.size() - 1,
                                      static_cast<std::size_t> (p * static_cast<float> (sorted.size() - 1)));
    std::nth_element (sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t> (idx), sorted.end());
    const float thr = 0.72f * sorted[idx];
    for (auto& v : x)
    {
        if (v < thr)
            v = 0.0f;
    }
}
} // namespace

void accumulateLeakyPeakChroma384 (std::span<const float> frame384,
                                   std::span<float> accumulator384,
                                   float release) noexcept
{
    if (frame384.size() < 384 || accumulator384.size() < 384)
        return;

    const float r = std::clamp (release, 0.0f, 1.0f);
    for (int i = 0; i < 384; ++i)
    {
        const float prev = accumulator384[static_cast<std::size_t> (i)] * r;
        const float x = frame384[static_cast<std::size_t> (i)];
        accumulator384[static_cast<std::size_t> (i)] = std::max (x, prev);
    }
}

void applyChromaShaping384 (ChromaShapingMode mode, std::span<float> chroma384) noexcept
{
    if (chroma384.size() < 384)
        return;

    switch (mode)
    {
        case ChromaShapingMode::None:
        default:
            return;
        case ChromaShapingMode::LogCompress:
            logCompressInPlace (chroma384.subspan (0, 384));
            return;
        case ChromaShapingMode::NoiseFloorSubtract:
            noiseFloorSubtractInPlace (chroma384.subspan (0, 384));
            return;
        case ChromaShapingMode::PercentileGate:
            percentileGateInPlace (chroma384.subspan (0, 384));
            return;
    }
}

namespace
{
[[nodiscard]] float waterfallShapingLerpT (int i,
                                           std::span<const std::uint8_t> dominantHarmonic384,
                                           FoldHarmonicModel harmonicModel,
                                           double sampleRate,
                                           float freqLogBlend) noexcept
{
    constexpr int kLast = 383;
    const float tIndex = static_cast<float> (i) / static_cast<float> (kLast);
    const float s = std::clamp (freqLogBlend, 0.0f, 1.0f);

    if (dominantHarmonic384.size() < 384 || sampleRate <= 0.0)
        return tIndex;

    constexpr float kHzLo = 20.0f;
    const float nyqHz = 0.49f * static_cast<float> (sampleRate);
    const float kHzHi = std::min (15000.0f, nyqHz);
    const float spanHz = kHzHi - kHzLo;
    const float logLo = std::log10 (kHzLo);
    const float logHi = std::log10 (kHzHi);
    if (spanHz <= 0.0f || logHi <= logLo)
        return tIndex;

    const float f0 = chromaSliceFundamentalHz (i);
    if (f0 <= 0.0f)
        return tIndex;

    const auto w = dominantHarmonic384[static_cast<std::size_t> (i)];
    float hzEff = f0;
    if (w != 255)
    {
        if (harmonicModel == FoldHarmonicModel::OctaveStack_Doc_v1)
            hzEff = f0 * std::pow (2.0f, static_cast<float> (w));
        else
            hzEff = f0 * static_cast<float> (w + 1);
    }
    else
        return tIndex;

    hzEff = std::clamp (hzEff, kHzLo, kHzHi);
    const float tLin = (hzEff - kHzLo) / spanHz;
    const float tLog = (std::log10 (hzEff) - logLo) / (logHi - logLo);
    return std::clamp (tLin * (1.0f - s) + tLog * s, 0.0f, 1.0f);
}
} // namespace

void applyFreqDependentWaterfallShaping384InPlace (std::span<float> chroma384,
                                                   WaterfallDisplayCurveMode curveMode,
                                                   float wEnergyLow,
                                                   float wEnergyHigh,
                                                   float wAlphaPowLow,
                                                   float wAlphaPowHigh,
                                                   std::span<const std::uint8_t> dominantHarmonic384,
                                                   FoldHarmonicModel harmonicModel,
                                                   double sampleRate,
                                                   float wShapingFreqLogBlend) noexcept
{
    if (chroma384.size() < 384)
        return;

    constexpr float kEps = 1.0e-12f;

    for (int i = 0; i < 384; ++i)
    {
        const float t = waterfallShapingLerpT (i,
                                                 dominantHarmonic384,
                                                 harmonicModel,
                                                 sampleRate,
                                                 wShapingFreqLogBlend);
        const float energy = wEnergyLow + t * (wEnergyHigh - wEnergyLow);
        const float alphaPow = wAlphaPowLow + t * (wAlphaPowHigh - wAlphaPowLow);
        const float e = std::max (0.0f, chroma384[static_cast<std::size_t> (i)]);
        float m = 0.0f;

        switch (curveMode)
        {
            case WaterfallDisplayCurveMode::Linear:
            default:
                m = std::clamp (e * energy, 0.0f, 1.0f);
                break;
            case WaterfallDisplayCurveMode::Sqrt:
                m = std::clamp (std::sqrt (e) * energy, 0.0f, 1.0f);
                break;
            case WaterfallDisplayCurveMode::LogDb:
            {
                // Match VizCpuRenderer / GL: log curve ignores global energy; per-bin gain scales input amplitude.
                const float xe = std::max (e * energy, kEps);
                const float db = 20.0f * std::log10 (xe);
                m = std::clamp ((db + 100.0f) / 100.0f, 0.0f, 1.0f);
                break;
            }
        }

        const float ap = std::max (0.0f, alphaPow);
        chroma384[static_cast<std::size_t> (i)] = std::pow (m, ap);
    }
}

} // namespace pitchlab
