#include <gtest/gtest.h>

#include "StaticTables.h"
#include "WindowApplyQ24.h"

#include <vector>

TEST(WindowApplyQ24, HanningAttenuatesConstantSignal)
{
    pitchlab::StaticTables tab (4096);
    const auto& win = tab.hanningWindowQ24();
    ASSERT_GE(win.size(), 4096u);

    std::vector<std::int16_t> in (4096, 1000);
    std::vector<std::int16_t> out (4096, 0);
    pitchlab::applyHanningWindowQ24 (std::span<const std::int16_t> { in.data(), in.size() },
                                     std::span<std::int16_t> { out.data(), out.size() },
                                     std::span<const std::int32_t> { win.data(), 4096 });

    EXPECT_LT(std::abs(out[2048]), std::abs(in[2048]));
    EXPECT_GE(out[2048], -32768);
    EXPECT_LE(out[2048], 32767);
}

TEST(WindowApplyQ24, GaussianAttenuatesEndsStrongerThanCenter)
{
    pitchlab::StaticTables tab (4096);
    const auto& g = tab.gaussianWindowQ24();
    ASSERT_GE(g.size(), 4096u);

    std::vector<std::int16_t> in (4096, 2000);
    std::vector<std::int16_t> out (4096, 0);
    pitchlab::applyHanningWindowQ24 (std::span<const std::int16_t> { in.data(), in.size() },
                                     std::span<std::int16_t> { out.data(), out.size() },
                                     std::span<const std::int32_t> { g.data(), 4096 });

    EXPECT_LT(std::abs(out[10]), std::abs(out[2048]));
    EXPECT_LT(std::abs(out[4080]), std::abs(out[2048]));
}
