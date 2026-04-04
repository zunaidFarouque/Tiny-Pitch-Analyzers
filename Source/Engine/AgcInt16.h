#pragma once

#include <cstdint>
#include <span>

namespace pitchlab
{

/** Branchless abs for int16 extended to int32 (New Plan §3.1, §6.1 edge at -32768). */
[[nodiscard]] std::int32_t bitwiseAbsInt16ToInt32 (std::int16_t v) noexcept;

/** Destructive AGC: scale window so peak magnitude becomes 32767 (integer ratio). */
void applyAgcInt16InPlace (std::span<std::int16_t> window) noexcept;

/** Configurable AGC: optional enable and strength blend (0 = bypass, 1 = full AGC). */
void applyAgcInt16InPlace (std::span<std::int16_t> window, bool enabled, float strength) noexcept;

} // namespace pitchlab
