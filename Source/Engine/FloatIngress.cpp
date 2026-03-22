#include "FloatIngress.h"

#include <algorithm>
#include <cmath>

namespace pitchlab
{

void convertFloatToInt16Ingress (std::span<const float> src, std::span<std::int16_t> dst)
{
    const std::size_t n = (std::min) (src.size(), dst.size());
    for (std::size_t i = 0; i < n; ++i)
    {
        float x = src[i] * 32767.0f;
        x = std::clamp (x, -32768.0f, 32767.0f);
        dst[i] = static_cast<std::int16_t> (x);
    }
}

void convertDeinterleavedToInt16Mono (const float* const* channelData,
                                      int numChannels,
                                      int numSamples,
                                      std::span<std::int16_t> dstMono)
{
    if (numChannels <= 0 || numSamples <= 0 || dstMono.empty())
        return;

    const int n = (std::min) (numSamples, static_cast<int> (dstMono.size()));
    for (int i = 0; i < n; ++i)
    {
        float sum = 0.0f;
        for (int c = 0; c < numChannels; ++c)
        {
            if (channelData[c] != nullptr)
                sum += channelData[c][i];
        }
        sum /= static_cast<float> (numChannels);
        float x = sum * 32767.0f;
        x = std::clamp (x, -32768.0f, 32767.0f);
        dstMono[static_cast<std::size_t> (i)] = static_cast<std::int16_t> (x);
    }
}

} // namespace pitchlab
