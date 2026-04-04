#pragma once

namespace pitchlab
{

/** Log-frequency band for waterfall peak column and synthetic peak line rendering (must stay in sync). */
namespace WaterfallFreqAxis
{
inline constexpr float kVisMinHz = 50.0f;
inline constexpr float kVisMaxHz = 8000.0f;
}

} // namespace pitchlab
