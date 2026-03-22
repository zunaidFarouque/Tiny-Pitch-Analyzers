#include <gtest/gtest.h>

#include "StaticTables.h"

TEST(StaticTables, DbBrightnessSizeAndEndpoints)
{
    pitchlab::StaticTables t (4096);
    EXPECT_EQ(t.dbBrightnessSize(), 32768u);
    EXPECT_EQ(t.dbBrightness (0), 0u);
    EXPECT_GE(t.dbBrightness (32767), 200u);
}

TEST(StaticTables, StrobeLength4096)
{
    pitchlab::StaticTables t (4096);
    EXPECT_EQ(t.strobeSize(), 4096u);
    EXPECT_EQ(t.strobe (0), t.strobe (0));
}

TEST(StaticTables, HanningGaussianQ24PositiveAndBounded)
{
    pitchlab::StaticTables t (4096);
    const auto& h = t.hanningWindowQ24();
    const auto& g = t.gaussianWindowQ24();
    ASSERT_EQ(h.size(), 4096u);
    ASSERT_EQ(g.size(), 4096u);
    EXPECT_GT(h[static_cast<std::size_t> (4096 / 2)], 0);
    EXPECT_GT(g[static_cast<std::size_t> (4096 / 2)], 0);
    for (std::size_t i = 0; i < h.size(); ++i)
    {
        EXPECT_GE(h[i], 0);
        EXPECT_GE(g[i], 0);
    }
}

TEST(StaticTables, SpectralPaletteTwelveNotes)
{
    pitchlab::StaticTables t (512);
    const auto& p = t.spectralPaletteRgb();
    EXPECT_EQ(p.size(), 36u);
    EXPECT_NE(p[0], p[3]);
}
