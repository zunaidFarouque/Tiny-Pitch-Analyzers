#pragma once

#include <span>

namespace pitchlab
{

constexpr int kChordRoots = 12;
constexpr int kChordTypes = 7;
constexpr int kChordMatrixFloats = kChordRoots * kChordTypes;

/** §5.5 — fill 12×7 matrix (index = root + type * 12). Types: maj, min, dim, aug, sus2, sus4, dom7. */
void fillChordProbabilitiesFromChroma384 (std::span<const float> chroma384, std::span<float> out) noexcept;

/** §6.3 — damp ambiguous aug/dim when triadic profiles dominate. */
void applyAntiChordPenaltyInPlace (std::span<float> chordProbabilities) noexcept;

} // namespace pitchlab
