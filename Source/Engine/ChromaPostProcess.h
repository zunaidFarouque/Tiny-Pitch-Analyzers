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

/**
 * Per-bin waterfall-style curve + pow (matches OpenGL / VizCpuRenderer when hosts use energy=1, alpha=1).
 * With dominantHarmonic384 (384 entries), lerp factor maps effective Hz (20..~15k) via wShapingFreqLogBlend:
 * 0 = linear in Hz, 1 = logarithmic (log10). Empty span falls back to index-based t = i/383 (tests).
 * W-Thresh stays in renderer.
 */
void applyFreqDependentWaterfallShaping384InPlace (std::span<float> chroma384,
                                                   WaterfallDisplayCurveMode curveMode,
                                                   float wEnergyLow,
                                                   float wEnergyHigh,
                                                   float wAlphaPowLow,
                                                   float wAlphaPowHigh,
                                                   std::span<const std::uint8_t> dominantHarmonic384 = {},
                                                   FoldHarmonicModel harmonicModel = FoldHarmonicModel::OctaveStack_Doc_v1,
                                                   double sampleRate = 44100.0,
                                                   float wShapingFreqLogBlend = 1.0f) noexcept;

} // namespace pitchlab
