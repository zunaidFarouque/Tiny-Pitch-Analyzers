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

void applyAgcInt16InPlace (std::span<std::int16_t> window, int noiseFloorAbs) noexcept
{
    if (window.empty())
        return;

    std::int32_t aMax = 0;
    for (std::int16_t s : window)
        aMax = std::max (aMax, bitwiseAbsInt16ToInt32 (s));

    if (aMax == 0 || (noiseFloorAbs > 0 && aMax < noiseFloorAbs))
    {
        std::fill (window.begin(), window.end(), static_cast<std::int16_t> (0));
        return;
    }

    for (auto& s : window)
    {
        const std::int32_t v = (static_cast<std::int32_t> (s) * 32767) / aMax;
        s = static_cast<std::int16_t> (std::clamp (v, static_cast<std::int32_t> (-32768),
                                                    static_cast<std::int32_t> (32767)));
    }
}

void applyAgcInt16InPlace (std::span<std::int16_t> window, bool enabled, float strength, int noiseFloorAbs) noexcept
{
    if (! enabled || window.empty())
        return;

    const float k = std::clamp (strength, 0.0f, 1.0f);
    if (k <= 0.0f)
        return;

    if (k >= 0.999f)
    {
        applyAgcInt16InPlace (window, noiseFloorAbs);
        return;
    }

    std::int32_t aMax = 0;
    for (std::int16_t s : window)
        aMax = std::max (aMax, bitwiseAbsInt16ToInt32 (s));

    if (aMax == 0 || (noiseFloorAbs > 0 && aMax < noiseFloorAbs))
    {
        std::fill (window.begin(), window.end(), static_cast<std::int16_t> (0));
        return;
    }

    for (auto& s : window)
    {
        const float agc = static_cast<float> ((static_cast<std::int32_t> (s) * 32767) / aMax);
        const float blended = static_cast<float> (s) + (agc - static_cast<float> (s)) * k;
        const auto v = static_cast<std::int32_t> (std::lround (blended));
        s = static_cast<std::int16_t> (std::clamp (v, static_cast<std::int32_t> (-32768),
                                                    static_cast<std::int32_t> (32767)));
    }
}

} // namespace pitchlab
