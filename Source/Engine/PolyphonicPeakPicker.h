#pragma once

#include "PitchPeak.h"

#include <span>
#include <vector>

namespace pitchlab
{

/**
 * Polyphonic peak picking on an STFT magnitude spectrum (same domain as magForFold_).
 *
 * threshold and prominence are relative to the frame maximum magnitude (0..1): internally compared as
 * (threshold * maxMag) and (prominence * maxMag) so raw JUCE FFT scale matches smeared/multi-res backends.
 *
 * @param candidateScratch Pre-reserved (e.g. mag.size()); used for candidates — no allocations when capacity suffices.
 * @param outPeaks Pre-reserved to at least maxPeaks; cleared then filled with top peaks by magnitude.
 *               PitchPeak::magnitude is the raw linear magnitude from magSpectrum.
 */
void extractPeaks (std::span<const float> magSpectrum,
                   double sampleRate,
                   int virtualFftSize,
                   float threshold,
                   float prominence,
                   int maxPeaks,
                   std::vector<PitchPeak>& candidateScratch,
                   std::vector<PitchPeak>& outPeaks) noexcept;

} // namespace pitchlab
