#include "MultiResSpectrumStitch.h"

#include <algorithm>
#include <cmath>

namespace pitchlab
{

void stitchMultiResMagnitudes (std::span<const float> hfMag,
                               std::span<const float> lfMag,
                               double sampleRateNative,
                               double sampleRateLf,
                               int hfFftSize,
                               int lfFftSize,
                               float crossoverHz,
                               float lfGain,
                               std::span<float> outStitched) noexcept
{
    const int hfBins = hfFftSize / 2 + 1;
    if (hfFftSize <= 0 || lfFftSize <= 0 || sampleRateNative <= 0.0 || sampleRateLf <= 0.0
        || static_cast<int> (hfMag.size()) < hfBins || static_cast<int> (outStitched.size()) < hfBins)
        return;

    const int lfBins = lfFftSize / 2 + 1;
    if (static_cast<int> (lfMag.size()) < lfBins)
        return;

    const double hfBinHz = sampleRateNative / static_cast<double> (hfFftSize);
    const double lfBinHz = sampleRateLf / static_cast<double> (lfFftSize);

    for (int k = 0; k < hfBins; ++k)
    {
        const double hz = static_cast<double> (k) * hfBinHz;
        if (hz < static_cast<double> (crossoverHz))
        {
            const double lfFrac = hz / lfBinHz;
            const int j0 = static_cast<int> (std::floor (lfFrac));
            const int j1 = j0 + 1;
            const float t = static_cast<float> (lfFrac - static_cast<double> (j0));
            float v = 0.0f;
            if (j0 >= 0 && j0 < lfBins)
                v = lfMag[static_cast<std::size_t> (j0)] * (1.0f - t);
            if (j1 >= 0 && j1 < lfBins)
                v += lfMag[static_cast<std::size_t> (j1)] * t;
            outStitched[static_cast<std::size_t> (k)] = v * lfGain;
        }
        else
        {
            outStitched[static_cast<std::size_t> (k)] = hfMag[static_cast<std::size_t> (k)];
        }
    }
}

} // namespace pitchlab
