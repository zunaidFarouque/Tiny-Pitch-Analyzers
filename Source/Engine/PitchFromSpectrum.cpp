#include "PitchFromSpectrum.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace pitchlab
{

float parabolicPeakOffset (float y0, float y1, float y2) noexcept
{
    const float denom = y0 - 2.0f * y1 + y2;
    if (std::abs (denom) < 1.0e-12f)
        return 0.0f;
    return 0.5f * (y0 - y2) / denom;
}

float binToHz (double refinedBin, double sampleRate, int fftSize) noexcept
{
    if (fftSize <= 0 || sampleRate <= 0.0)
        return 0.0f;
    return static_cast<float> (refinedBin * sampleRate / static_cast<double> (fftSize));
}

float refinedPeakBin (std::span<const float> mag) noexcept
{
    if (mag.size() < 3)
        return 0.0f;

    std::size_t peak = 1;
    float best = mag[1];
    for (std::size_t i = 2; i + 1 < mag.size(); ++i)
    {
        if (mag[i] > best)
        {
            best = mag[i];
            peak = i;
        }
    }

    const float y0 = mag[peak - 1];
    const float y1 = mag[peak];
    const float y2 = mag[peak + 1];
    const float off = parabolicPeakOffset (y0, y1, y2);
    return static_cast<float> (static_cast<double> (peak) + static_cast<double> (off));
}

float centsVsTempered (float hz) noexcept
{
    if (hz <= 1.0e-6f)
        return 0.0f;

    constexpr float ln2 = 0.69314718055994530942f;
    const float midi = 69.0f + 12.0f * (std::log (hz / 440.0f) / ln2);
    const float nearest = std::round (midi);
    return 100.0f * (midi - nearest);
}

} // namespace pitchlab
