#include "ChromaPostProcess.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
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

} // namespace pitchlab
