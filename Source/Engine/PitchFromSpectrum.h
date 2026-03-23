#pragma once

#include <span>

namespace pitchlab
{

/** Parabolic peak refinement + Hz; cents vs nearest equal-tempered semitone (§5.3). */
[[nodiscard]] float parabolicPeakOffset (float y0, float y1, float y2) noexcept;

[[nodiscard]] float binToHz (double refinedBin, double sampleRate, int fftSize) noexcept;

/** Peak search in [1, mag.size()-2]; returns refined fractional bin. */
[[nodiscard]] float refinedPeakBin (std::span<const float> mag) noexcept;

[[nodiscard]] float centsVsTempered (float hz) noexcept;

} // namespace pitchlab
