#include "EngineState.h"

#include <algorithm>

namespace pitchlab
{

void EngineState::resetAnalysisDisplay()
{
    currentHz = 0.0f;
    tuningError = 0.0f;
    strobePhase = 0.0f;
    octaveHarmonicIndex.fill (static_cast<std::uint8_t> (255));
    chordProbabilities.fill (0.0f);
    analysisDirty.store (false, std::memory_order_release);
}

} // namespace pitchlab
