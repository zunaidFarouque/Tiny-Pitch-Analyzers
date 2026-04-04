#include "WaterfallPeakRow.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace pitchlab
{

void fillWaterfallRowFromPeaks (std::span<const PitchPeak> peaks,
                                std::span<float> row384,
                                float minHz,
                                float maxHz) noexcept
{
    if (row384.size() != static_cast<std::size_t> (384))
        return;

    std::fill (row384.begin(), row384.end(), 0.0f);

    if (! (maxHz > minHz && minHz > 0.0f))
        return;

    const double logMin = std::log2 (static_cast<double> (minHz));
    const double logDen = std::log2 (static_cast<double> (maxHz)) - logMin;
    if (logDen <= 1.0e-18)
        return;

    for (const PitchPeak& p : peaks)
    {
        const float hz = p.frequencyHz;
        if (! (hz >= minHz && hz <= maxHz))
            continue;

        const double t = (std::log2 (static_cast<double> (hz)) - logMin) / logDen;
        int k = static_cast<int> (std::lround (t * 383.0));
        k = std::clamp (k, 0, 383);
        const std::size_t ku = static_cast<std::size_t> (k);
        row384[ku] = std::max (row384[ku], p.magnitude);
    }
}

} // namespace pitchlab
