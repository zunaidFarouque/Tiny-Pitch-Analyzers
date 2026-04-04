#include "PolyphonicPeakPicker.h"

#include "PitchFromSpectrum.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace pitchlab
{

namespace
{
[[nodiscard]] float fastProminenceLeft (std::span<const float> mag, std::size_t i) noexcept
{
    float v = mag[i - 1];
    if (i < 2)
        return v;
    for (std::ptrdiff_t j = static_cast<std::ptrdiff_t> (i) - 2; j >= 0; --j)
    {
        const std::size_t ju = static_cast<std::size_t> (j);
        if (mag[ju] > mag[ju + 1])
            break;
        v = std::min (v, mag[ju]);
    }
    return v;
}

[[nodiscard]] float fastProminenceRight (std::span<const float> mag, std::size_t i) noexcept
{
    const std::size_t n = mag.size();
    float v = mag[i + 1];
    if (i + 2 >= n)
        return v;
    for (std::size_t j = i + 2; j < n; ++j)
    {
        if (mag[j] > mag[j - 1])
            break;
        v = std::min (v, mag[j]);
    }
    return v;
}

[[nodiscard]] float hzToMidiPitch (float hz) noexcept
{
    if (hz <= 1.0e-6f)
        return 0.0f;
    return 69.0f + 12.0f * std::log2 (hz / 440.0f);
}
} // namespace

void extractPeaks (std::span<const float> magSpectrum,
                   double sampleRate,
                   int virtualFftSize,
                   float threshold,
                   float prominence,
                   int maxPeaks,
                   std::vector<PitchPeak>& candidateScratch,
                   std::vector<PitchPeak>& outPeaks) noexcept
{
    outPeaks.clear();
    candidateScratch.clear();

    if (maxPeaks <= 0 || magSpectrum.size() < 3 || virtualFftSize <= 0 || sampleRate <= 0.0)
        return;

    float maxMag = 0.0f;
    for (float v : magSpectrum)
        maxMag = std::max (maxMag, v);

    constexpr float kNormFloor = 1.0e-18f;
    if (maxMag <= kNormFloor)
        return;

    const float thrAbs = threshold * maxMag;
    const float promAbs = prominence * maxMag;

    const std::size_t n = magSpectrum.size();
    for (std::size_t i = 1; i + 1 < n; ++i)
    {
        const float m = magSpectrum[i];
        if (m <= magSpectrum[i - 1] || m <= magSpectrum[i + 1])
            continue;
        if (m < thrAbs)
            continue;

        const float leftValley = fastProminenceLeft (magSpectrum, i);
        const float rightValley = fastProminenceRight (magSpectrum, i);
        const float prom = m - std::max (leftValley, rightValley);
        if (prom < promAbs)
            continue;

        const float y0 = magSpectrum[i - 1];
        const float y1 = m;
        const float y2 = magSpectrum[i + 1];
        const float off = parabolicPeakOffset (y0, y1, y2);
        const double refinedBin = static_cast<double> (i) + static_cast<double> (off);
        const float hz = binToHz (refinedBin, sampleRate, virtualFftSize);

        PitchPeak pk;
        pk.frequencyHz = hz;
        pk.magnitude = m;
        pk.midiPitch = hzToMidiPitch (hz);
        candidateScratch.push_back (pk);
    }

    const int cap = std::min (maxPeaks, PitchPeak::kMaxPeaksCap);
    if (cap <= 0 || candidateScratch.empty())
        return;

    const std::size_t take = std::min (static_cast<std::size_t> (cap), candidateScratch.size());
    std::sort (candidateScratch.begin(),
               candidateScratch.end(),
               [] (const PitchPeak& a, const PitchPeak& b) noexcept {
                   return a.magnitude > b.magnitude;
               });

    for (std::size_t k = 0; k < take; ++k)
        outPeaks.push_back (candidateScratch[k]);
}

} // namespace pitchlab
