#include "EngineState.h"

namespace pitchlab
{

void EngineState::resetAnalysisDisplay()
{
    currentHz = 0.0f;
    tuningError = 0.0f;
    analysisDirty = false;
}

} // namespace pitchlab
