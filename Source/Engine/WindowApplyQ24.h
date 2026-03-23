#pragma once

#include <cstdint>
#include <span>

namespace pitchlab
{

/** output[i] = clamp( (input[i] * q24Window[i]) >> 24 ) (New Plan §3.2). */
void applyHanningWindowQ24 (std::span<const std::int16_t> input,
                            std::span<std::int16_t> output,
                            std::span<const std::int32_t> q24Window) noexcept;

} // namespace pitchlab
