#pragma once

#include "EngineState.h"

#include <span>

namespace pitchlab
{

/** In-place shaping on the 384-bin chroma row after fold, before chords / upload. */
void applyChromaShaping384 (ChromaShapingMode mode, std::span<float> chroma384) noexcept;

/** Leaky peak hold: acc[i] = max(frame[i], acc[i] * release). */
void accumulateLeakyPeakChroma384 (std::span<const float> frame384,
                                   std::span<float> accumulator384,
                                   float release) noexcept;

} // namespace pitchlab
