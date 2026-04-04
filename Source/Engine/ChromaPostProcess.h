#pragma once

#include "EngineState.h"

#include <span>

namespace pitchlab
{

/** In-place shaping on the 384-bin chroma row after fold, before chords / upload. */
void applyChromaShaping384 (ChromaShapingMode mode, std::span<float> chroma384) noexcept;

} // namespace pitchlab
