#pragma once

#include <span>

namespace pitchlab
{

struct PreEmphasis
{
    float prevSample = 0.0f;

    void reset() noexcept { prevSample = 0.0f; }

    void process (std::span<float> buffer, bool enabled) noexcept
    {
        if (! enabled)
        {
            prevSample = 0.0f;
            return;
        }

        for (float& sample : buffer)
        {
            const float out = sample - 0.95f * prevSample;
            prevSample = sample;
            sample = out;
        }
    }
};

} // namespace pitchlab
