#pragma once

#include <cstdint>
#include <span>

namespace pitchlab
{

class ChromaMap;

/**
    32×12 = 384 chroma slices; octave harmonics f, 2f, 4f, … until Nyquist (New Plan §3.4).
    dominantHarmonic384: winning harmonic index — 0 = f, 1 = 2f, 2 = 4f, …; 255 if negligible.
 */
void foldToChroma384 (const ChromaMap& map,
                      double sampleRate,
                      int fftSize,
                      std::span<const float> mag,
                      std::span<float> out384,
                      std::span<std::uint8_t> dominantHarmonic384) noexcept;

} // namespace pitchlab
