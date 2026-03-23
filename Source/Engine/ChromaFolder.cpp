#include "ChromaFolder.h"

#include "ChromaMap.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace pitchlab
{

namespace
{
[[nodiscard]] float midiToHz (float midi) noexcept
{
    return 440.0f * std::pow (2.0f, (midi - 69.0f) / 12.0f);
}

[[nodiscard]] float sliceIndexToFundamentalHz (int slice384) noexcept
{
    const int noteClass = slice384 / 32;
    const int sub = slice384 % 32;
    const float midi = 36.0f + static_cast<float> (noteClass) + static_cast<float> (sub) / 32.0f;
    return midiToHz (midi);
}

[[nodiscard]] std::size_t hzToBin (float hz, double sampleRate, int fftSize, std::size_t magBins) noexcept
{
    if (hz <= 0.0f || sampleRate <= 0.0)
        return 0;

    const auto idx = static_cast<std::size_t> (std::lround (static_cast<double> (hz) * static_cast<double> (fftSize) / sampleRate));
    return std::min (idx, magBins > 0 ? magBins - 1 : 0);
}

[[nodiscard]] std::size_t fundamentalBinFromMap (const ChromaMap& map,
                                                 int noteClass,
                                                 float f0,
                                                 double sampleRate,
                                                 int fftSize,
                                                 std::size_t magBins) noexcept
{
    const int maxBin = fftSize / 2;
    const float hzLow = midiToHz (36.0f + static_cast<float> (noteClass));
    const float hzHigh = (noteClass < 11) ? midiToHz (37.0f + static_cast<float> (noteClass))
                                            : midiToHz (48.0f);

    const float denom = hzHigh - hzLow;
    float t = denom > 1.0e-6f ? (f0 - hzLow) / denom : 0.0f;
    t = std::clamp (t, 0.0f, 1.0f);

    int binLo = map.startBin (noteClass);
    int binHi = (noteClass < 11) ? map.startBin (noteClass + 1)
                                 : static_cast<int> (std::lround (static_cast<double> (midiToHz (48.0f)) * static_cast<double> (fftSize) / sampleRate));

    binLo = std::clamp (binLo, 0, maxBin);
    binHi = std::clamp (binHi, 0, maxBin);
    if (binHi < binLo)
        std::swap (binLo, binHi);

    int fund = static_cast<int> (std::lround (static_cast<double> (binLo) + static_cast<double> (t) * static_cast<double> (binHi - binLo)));
    fund = std::clamp (fund, 0, maxBin);
    return std::min (static_cast<std::size_t> (fund), magBins > 0 ? magBins - 1 : 0);
}
} // namespace

void foldToChroma384 (const ChromaMap& map,
                      double sampleRate,
                      int fftSize,
                      std::span<const float> mag,
                      std::span<float> out384,
                      std::span<std::uint8_t> dominantHarmonic384) noexcept
{
    if (out384.size() < 384 || mag.empty() || fftSize <= 0)
        return;

    const bool writeHarmonic = dominantHarmonic384.size() >= 384;

    const auto magBins = mag.size();
    const double nyquist = 0.5 * sampleRate;

    std::fill (out384.begin(), out384.begin() + 384, 0.0f);
    if (writeHarmonic)
        std::fill (dominantHarmonic384.begin(), dominantHarmonic384.begin() + 384, static_cast<std::uint8_t> (255));

    for (int s = 0; s < 384; ++s)
    {
        const int noteClass = s / 32;
        const int sub = s % 32;
        const float f0 = sliceIndexToFundamentalHz (s);
        if (f0 <= 0.0f)
            continue;

        const std::size_t fundBin = fundamentalBinFromMap (map, noteClass, f0, sampleRate, fftSize, magBins);

        float sum = mag[fundBin];
        float maxContrib = sum;
        std::uint8_t winner = 0;

        for (int hIdx = 1;; ++hIdx)
        {
            const double mult = std::pow (2.0, static_cast<double> (hIdx));
            const double fh = static_cast<double> (f0) * mult;
            if (fh >= nyquist)
                break;

            const std::size_t bin = hzToBin (static_cast<float> (fh), sampleRate, fftSize, magBins);
            const float contrib = mag[bin];
            sum += contrib;
            if (contrib > maxContrib)
            {
                maxContrib = contrib;
                winner = static_cast<std::uint8_t> (std::min (hIdx, 254));
            }
        }

        out384[static_cast<std::size_t> (s)] = sum;
        if (writeHarmonic)
            dominantHarmonic384[static_cast<std::size_t> (s)] = (sum > 1.0e-12f) ? winner : static_cast<std::uint8_t> (255);
    }
}

} // namespace pitchlab
