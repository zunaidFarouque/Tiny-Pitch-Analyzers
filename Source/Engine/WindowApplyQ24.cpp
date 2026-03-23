#include "WindowApplyQ24.h"

#include <algorithm>
#include <cstdint>

namespace pitchlab
{

void applyHanningWindowQ24 (std::span<const std::int16_t> input,
                            std::span<std::int16_t> output,
                            std::span<const std::int32_t> q24Window) noexcept
{
    const std::size_t n = (std::min) ({ input.size(), output.size(), q24Window.size() });
    for (std::size_t i = 0; i < n; ++i)
    {
        const auto prod = static_cast<std::int64_t> (input[i]) * static_cast<std::int64_t> (q24Window[i]);
        const auto shifted = prod >> 24;
        const auto v = static_cast<std::int32_t> (std::clamp (shifted,
                                                              static_cast<std::int64_t> (-32768),
                                                              static_cast<std::int64_t> (32767)));
        output[i] = static_cast<std::int16_t> (v);
    }
}

} // namespace pitchlab
