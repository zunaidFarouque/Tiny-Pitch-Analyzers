#pragma once

#include "PitchPeak.h"

#include <span>

namespace pitchlab
{

/** Clear row384 and stamp peak magnitudes at log-spaced bins (see WaterfallFreqAxis). */
void fillWaterfallRowFromPeaks (std::span<const PitchPeak> peaks,
                                std::span<float> row384,
                                float minHz,
                                float maxHz) noexcept;

} // namespace pitchlab
