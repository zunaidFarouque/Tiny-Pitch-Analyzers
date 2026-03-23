#pragma once

#include <algorithm>
#include <cstdint>

namespace pitchlab
{

/**
    New Plan §6.1 — integer-style brightness combine (legacy texture formatter style).
    Use when packing chroma/waterfall texels without float round-trips.
 */
[[nodiscard]] inline std::uint8_t blendEnergyRgbQ16 (std::int32_t energy, std::int32_t rgbByte) noexcept
{
    const auto v = (static_cast<std::int64_t> (energy) * static_cast<std::int64_t> (rgbByte) * 320) >> 16;
    return static_cast<std::uint8_t> (std::clamp (v, std::int64_t { 0 }, std::int64_t { 255 }));
}

} // namespace pitchlab
