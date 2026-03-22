#include <gtest/gtest.h>

#include "FloatIngress.h"

#include <span>
#include <vector>

TEST(FloatIngress, ZeroAndFullScale)
{
    std::vector<float> f = { 0.0f, 1.0f, -1.0f };
    std::vector<std::int16_t> o (3);
    pitchlab::convertFloatToInt16Ingress (
        std::span<const float> (f.data(), f.size()),
        std::span<std::int16_t> (o.data(), o.size()));
    EXPECT_EQ(o[0], 0);
    EXPECT_EQ(o[1], 32767);
    EXPECT_EQ(o[2], -32767); // -1.f * 32767, within clamp range
}

TEST(FloatIngress, ClampsBeyondUnity)
{
    std::vector<float> f = { 2.0f, -2.0f };
    std::vector<std::int16_t> o (2);
    pitchlab::convertFloatToInt16Ingress (
        std::span<const float> (f.data(), f.size()),
        std::span<std::int16_t> (o.data(), o.size()));
    EXPECT_EQ(o[0], 32767);
    EXPECT_EQ(o[1], -32768);
}

TEST(FloatIngress, MonoDownmixTwoChannels)
{
    const float ch0[] = { 1.0f, 0.0f };
    const float ch1[] = { -1.0f, 0.0f };
    const float* chans[] = { ch0, ch1 };
    std::vector<std::int16_t> o (2);
    pitchlab::convertDeinterleavedToInt16Mono (chans, 2, 2, std::span<std::int16_t> (o.data(), o.size()));
    EXPECT_EQ(o[0], 0);
    EXPECT_EQ(o[1], 0);
}
