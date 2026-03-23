#include "AgcInt16.h"

#include <algorithm>
#include <cmath>

namespace pitchlab
{

std::int32_t bitwiseAbsInt16ToInt32 (std::int16_t v) noexcept
{
    const std::int32_t x = static_cast<std::int32_t> (v);
    const std::int32_t mask = x >> 31;
    return (x ^ mask) - mask;
}

void applyAgcInt16InPlace (std::span<std::int16_t> window) noexcept
{
    if (window.empty())
        return;

    std::int32_t aMax = 0;
    for (std::int16_t s : window)
        aMax = std::max (aMax, bitwiseAbsInt16ToInt32 (s));

    if (aMax == 0)
        return;

    for (auto& s : window)
    {
        const std::int32_t v = (static_cast<std::int32_t> (s) * 32767) / aMax;
        s = static_cast<std::int16_t> (std::clamp (v, static_cast<std::int32_t> (-32768),
                                                    static_cast<std::int32_t> (32767)));
    }
}

} // namespace pitchlab
