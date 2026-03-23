#include <gtest/gtest.h>

#include "CircularInt16Buffer.h"

#include <span>
#include <vector>

TEST(CircularInt16Buffer, CopyLatestChronologicalOrder)
{
    pitchlab::CircularInt16Buffer buf (8);
    std::vector<std::int16_t> a = { 1, 2, 3, 4, 5 };
    buf.push (std::span<const std::int16_t> (a.data(), a.size()));

    std::vector<std::int16_t> dst (3, static_cast<std::int16_t> (-99));
    buf.copyLatestInto (std::span<std::int16_t> { dst.data(), dst.size() });

    EXPECT_EQ(dst[0], 3);
    EXPECT_EQ(dst[1], 4);
    EXPECT_EQ(dst[2], 5);
}
