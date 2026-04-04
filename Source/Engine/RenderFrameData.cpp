#include "RenderFrameData.h"

namespace pitchlab
{

RenderFrameData::RenderFrameData()
{
    activePeaks.reserve (static_cast<std::size_t> (PitchPeak::kMaxPeaksCap));
}

} // namespace pitchlab
