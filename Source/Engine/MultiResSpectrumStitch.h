#pragma once

#include <span>

namespace pitchlab
{

/** Map LF magnitudes (sr/4, lfFft) and HF magnitudes (sr, hfFft) into a single HF-sized spectrum at native `sr`.
    LF bins are scaled by `lfGain` (typically hfFft / lfFft) before mapping. Writes `hfBins` samples. */
void stitchMultiResMagnitudes (std::span<const float> hfMag,
                               std::span<const float> lfMag,
                               double sampleRateNative,
                               double sampleRateLf,
                               int hfFftSize,
                               int lfFftSize,
                               float crossoverHz,
                               float lfGain,
                               std::span<float> outStitched) noexcept;

} // namespace pitchlab
