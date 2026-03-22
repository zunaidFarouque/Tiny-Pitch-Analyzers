#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace pitchlab
{

/** New Plan §2.2: scale float [-1,1] to int16 via 32767.f; clamp for out-of-range (incl. -32768 edge). */
void convertFloatToInt16Ingress (std::span<const float> src, std::span<std::int16_t> dst);

/** Downmix first numChannels interleaved groups from non-interleaved channel pointers. */
void convertDeinterleavedToInt16Mono (const float* const* channelData,
                                      int numChannels,
                                      int numSamples,
                                      std::span<std::int16_t> dstMono);

} // namespace pitchlab
