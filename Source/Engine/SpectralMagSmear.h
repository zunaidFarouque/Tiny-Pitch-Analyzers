#pragma once

#include "EngineState.h"

#include <span>

namespace pitchlab
{

/** Smear |X[k]| along bins to approximate constant / variable-Q energy per bin (v0.1). STFT copies through. */
void buildMagForFold (std::span<const float> magIn,
                      std::span<float> magOut,
                      double sampleRate,
                      int fftSize,
                      SpectralBackendMode backend,
                      bool spectralSmearingEnabled = true) noexcept;

} // namespace pitchlab
