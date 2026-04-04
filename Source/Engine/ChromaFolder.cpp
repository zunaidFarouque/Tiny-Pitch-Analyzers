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

[[nodiscard]] double hzToBinFrac (double hz, double sampleRate, int fftSize) noexcept
{
    if (hz <= 0.0 || sampleRate <= 0.0 || fftSize <= 0)
        return 0.0;

    return hz * static_cast<double> (fftSize) / sampleRate;
}

[[nodiscard]] float sampleMagAtBinFrac (std::span<const float> mag, double binFrac, FoldInterpMode mode) noexcept
{
    if (mag.empty())
        return 0.0f;

    const int maxIdx = static_cast<int> (mag.size()) - 1;
    const double x = std::clamp (binFrac, 0.0, static_cast<double> (maxIdx));

    if (mode == FoldInterpMode::Nearest)
    {
        const int k = std::clamp (static_cast<int> (std::lround (x)), 0, maxIdx);
        return mag[static_cast<std::size_t> (k)];
    }

    const int k0 = std::clamp (static_cast<int> (std::floor (x)), 0, maxIdx);
    const int k1 = std::clamp (k0 + 1, 0, maxIdx);
    const float t = static_cast<float> (x - static_cast<double> (k0));
    const float linear = mag[static_cast<std::size_t> (k0)] * (1.0f - t)
                         + mag[static_cast<std::size_t> (k1)] * t;

    if (mode == FoldInterpMode::Linear2Bin || maxIdx < 2)
        return linear;

    const int km1 = std::clamp (k0 - 1, 0, maxIdx);
    const int kp1 = std::clamp (k0 + 1, 0, maxIdx);
    const float ym1 = mag[static_cast<std::size_t> (km1)];
    const float y0 = mag[static_cast<std::size_t> (k0)];
    const float y1 = mag[static_cast<std::size_t> (kp1)];
    const float u = static_cast<float> (x - static_cast<double> (k0));
    const float quad = y0 + 0.5f * u * (y1 - ym1 + u * (y1 - 2.0f * y0 + ym1));
    return std::max (0.0f, quad);
}

[[nodiscard]] float harmonicWeight (int hIdx, FoldHarmonicWeightMode mode) noexcept
{
    const int harmonicNumber = 1 << hIdx;
    switch (mode)
    {
        case FoldHarmonicWeightMode::Uniform: return 1.0f;
        case FoldHarmonicWeightMode::InvH: return 1.0f / static_cast<float> (harmonicNumber);
        case FoldHarmonicWeightMode::InvSqrtH:
        default: return 1.0f / std::sqrt (static_cast<float> (harmonicNumber));
    }
}

[[nodiscard]] float harmonicWeightIntegerK (int k, FoldHarmonicWeightMode mode) noexcept
{
    if (k <= 0)
        return 0.0f;
    switch (mode)
    {
        case FoldHarmonicWeightMode::Uniform: return 1.0f;
        case FoldHarmonicWeightMode::InvH: return 1.0f / static_cast<float> (k);
        case FoldHarmonicWeightMode::InvSqrtH:
        default: return 1.0f / std::sqrt (static_cast<float> (k));
    }
}
} // namespace

void foldToChroma384 (const ChromaMap& map,
                      double sampleRate,
                      int fftSize,
                      std::span<const float> mag,
                      std::span<float> out384,
                      std::span<std::uint8_t> dominantHarmonic384,
                      const FoldToChromaSettings& settings) noexcept
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
        const double fundBinFrac = hzToBinFrac (static_cast<double> (f0), sampleRate, fftSize);
        const float fundEstimate = sampleMagAtBinFrac (mag, fundBinFrac, settings.interpMode);
        const float w0 = harmonicWeight (0, settings.harmonicWeightMode);

        float sum = (settings.interpMode == FoldInterpMode::Nearest ? mag[fundBin] : fundEstimate) * w0;
        float maxContrib = sum;
        std::uint8_t winner = 0;

        if (settings.harmonicModel == FoldHarmonicModel::OctaveStack_Doc_v1)
        {
            for (int hIdx = 1;; ++hIdx)
            {
                if (settings.maxOctaves > 0 && hIdx > settings.maxOctaves)
                    break;

                const double mult = std::pow (2.0, static_cast<double> (hIdx));
                const double fh = static_cast<double> (f0) * mult;
                if (fh >= nyquist)
                    break;

                const float contribRaw = (settings.interpMode == FoldInterpMode::Nearest)
                                             ? mag[hzToBin (static_cast<float> (fh), sampleRate, fftSize, magBins)]
                                             : sampleMagAtBinFrac (mag, hzToBinFrac (fh, sampleRate, fftSize), settings.interpMode);
                const float contrib = contribRaw * harmonicWeight (hIdx, settings.harmonicWeightMode);
                sum += contrib;
                if (contrib > maxContrib)
                {
                    maxContrib = contrib;
                    winner = static_cast<std::uint8_t> (std::min (hIdx, 254));
                }
            }
        }
        else
        {
            const int kMax = std::max (1, settings.maxHarmonicK);
            for (int k = 2; k <= kMax; ++k)
            {
                const double fh = static_cast<double> (f0) * static_cast<double> (k);
                if (fh >= nyquist)
                    break;

                const float contribRaw = (settings.interpMode == FoldInterpMode::Nearest)
                                             ? mag[hzToBin (static_cast<float> (fh), sampleRate, fftSize, magBins)]
                                             : sampleMagAtBinFrac (mag, hzToBinFrac (fh, sampleRate, fftSize), settings.interpMode);
                const float contrib = contribRaw * harmonicWeightIntegerK (k, settings.harmonicWeightMode);
                sum += contrib;
                if (contrib > maxContrib)
                {
                    maxContrib = contrib;
                    winner = static_cast<std::uint8_t> (std::min (k - 1, 254));
                }
            }
        }

        out384[static_cast<std::size_t> (s)] = sum;
        if (writeHarmonic)
            dominantHarmonic384[static_cast<std::size_t> (s)] = (sum > 1.0e-12f) ? winner : static_cast<std::uint8_t> (255);
    }
}

} // namespace pitchlab
