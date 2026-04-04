#include <gtest/gtest.h>

#include "AgcInt16.h"

#include <cmath>
#include <numbers>
#include <vector>

TEST(AgcInt16, BitwiseAbsHandlesNegative32768)
{
    EXPECT_EQ(pitchlab::bitwiseAbsInt16ToInt32 (0), 0);
    EXPECT_EQ(pitchlab::bitwiseAbsInt16ToInt32 (-100), 100);
    EXPECT_EQ(pitchlab::bitwiseAbsInt16ToInt32 (100), 100);
    EXPECT_EQ(pitchlab::bitwiseAbsInt16ToInt32 (static_cast<std::int16_t> (-32768)), 32768);
}

TEST(AgcInt16, ScalesPeakTo32767)
{
    std::vector<std::int16_t> w = { 100, -200, 50 };
    pitchlab::applyAgcInt16InPlace (std::span<std::int16_t> { w.data(), w.size() }, 0);
    std::int32_t m = 0;
    for (auto s : w)
        m = std::max (m, pitchlab::bitwiseAbsInt16ToInt32 (s));
    EXPECT_EQ(m, 32767);
}

TEST(AgcInt16, QuietSinePeak100ScalesTo32767)
{
    constexpr int n = 512;
    std::vector<std::int16_t> w (static_cast<std::size_t> (n));
    for (int i = 0; i < n; ++i)
    {
        const float s = 100.0f * std::sin (2.0f * std::numbers::pi_v<float> * static_cast<float> (i) / static_cast<float> (n));
        w[static_cast<std::size_t> (i)] = static_cast<std::int16_t> (std::lround (s));
    }
    std::int32_t peak = 0;
    for (auto s : w)
        peak = std::max (peak, pitchlab::bitwiseAbsInt16ToInt32 (s));
    ASSERT_EQ(peak, 100);

    pitchlab::applyAgcInt16InPlace (std::span<std::int16_t> { w.data(), w.size() }, 0);
    std::int32_t m = 0;
    for (auto s : w)
        m = std::max (m, pitchlab::bitwiseAbsInt16ToInt32 (s));
    EXPECT_EQ(m, 32767);
}

TEST(AgcInt16, SilenceStaysZeroNoCrash)
{
    std::vector<std::int16_t> w (512, 0);
    pitchlab::applyAgcInt16InPlace (std::span<std::int16_t> { w.data(), w.size() }, 0);
    for (auto s : w)
        EXPECT_EQ(s, 0);
}

TEST(AgcInt16, NoiseFloor150ZerosQuietPeak100)
{
    constexpr int n = 512;
    std::vector<std::int16_t> w (static_cast<std::size_t> (n));
    for (int i = 0; i < n; ++i)
    {
        const float s = 100.0f * std::sin (2.0f * std::numbers::pi_v<float> * static_cast<float> (i) / static_cast<float> (n));
        w[static_cast<std::size_t> (i)] = static_cast<std::int16_t> (std::lround (s));
    }
    pitchlab::applyAgcInt16InPlace (std::span<std::int16_t> { w.data(), w.size() }, 150);
    for (auto s : w)
        EXPECT_EQ(s, 0);
}

TEST(AgcInt16, NoiseFloor150StillScalesPeak200)
{
    std::vector<std::int16_t> w = { 100, -200, 50 };
    pitchlab::applyAgcInt16InPlace (std::span<std::int16_t> { w.data(), w.size() }, 150);
    std::int32_t m = 0;
    for (auto s : w)
        m = std::max (m, pitchlab::bitwiseAbsInt16ToInt32 (s));
    EXPECT_EQ(m, 32767);
}
