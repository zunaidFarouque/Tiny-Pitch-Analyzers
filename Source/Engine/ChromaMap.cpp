#include "ChromaMap.h"

#include <algorithm>
#include <cmath>

namespace pitchlab
{

void ChromaMap::rebuild (double sampleRate, int fftSize) noexcept
{
    if (sampleRate <= 0.0 || fftSize <= 0)
    {
        startBin_.fill (0);
        return;
    }

    const int maxBin = fftSize / 2;

    for (int c = 0; c < 12; ++c)
    {
        const double midi = 36.0 + static_cast<double> (c);
        const double hz = 440.0 * std::pow (2.0, (midi - 69.0) / 12.0);
        int bin = static_cast<int> (std::lround (hz * static_cast<double> (fftSize) / sampleRate));
        bin = std::clamp (bin, 0, maxBin);
        startBin_[static_cast<std::size_t> (c)] = bin;
    }
}

int ChromaMap::startBin (int pitchClass) const noexcept
{
    const int pc = std::clamp (pitchClass, 0, 11);
    return startBin_[static_cast<std::size_t> (pc)];
}

} // namespace pitchlab
