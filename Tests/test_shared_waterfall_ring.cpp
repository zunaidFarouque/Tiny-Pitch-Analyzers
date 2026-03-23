#include <gtest/gtest.h>

#include "SharedWaterfallRing.h"

#include <array>

TEST(SharedWaterfallRing, WriteIndexWrapsAcross384Rows)
{
    SharedWaterfallRing ring;
    std::array<float, SharedWaterfallRing::kRowBins> row {};
    row[0] = 1.0f;

    int writeY = -1;
    std::array<float, SharedWaterfallRing::kRowBins> out {};

    for (int i = 0; i < SharedWaterfallRing::kRows + 2; ++i)
    {
        ring.pushRow (row);
        ASSERT_TRUE(ring.consumePendingRow (out, writeY));
    }

    EXPECT_EQ(ring.writeY(), 2);
    EXPECT_FLOAT_EQ(out[0], 1.0f);
}

