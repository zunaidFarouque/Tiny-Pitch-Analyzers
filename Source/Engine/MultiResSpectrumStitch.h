#pragma once

#include <span>

namespace pitchlab
{

/** Map LF magnitudes (sr/4, lfFft) and HF magnitudes (sr, hfFft) onto a virtual-length spectrum at native
    `sampleRateNative` with FFT size `virtualFftSize` (typically hfFft * 4). LF branch uses `lfGain`; HF branch
    does not. Output length is `virtualFftSize / 2 + 1`. */
void stitchMultiResMagnitudes (std::span<const float> hfMag,
                               std::span<const float> lfMag,
                               double sampleRateNative,
                               double sampleRateLf,
                               int hfFftSize,
                               int lfFftSize,
                               int virtualFftSize,
                               float crossoverHz,
                               float lfGain,
                               std::span<float> outStitched) noexcept;

} // namespace pitchlab
