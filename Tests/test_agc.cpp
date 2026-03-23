#include <gtest/gtest.h>

#include "AgcInt16.h"

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
    pitchlab::applyAgcInt16InPlace (std::span<std::int16_t> { w.data(), w.size() });
    std::int32_t m = 0;
    for (auto s : w)
        m = std::max (m, pitchlab::bitwiseAbsInt16ToInt32 (s));
    EXPECT_EQ(m, 32767);
}
