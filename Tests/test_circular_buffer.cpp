#include <gtest/gtest.h>

#include "CircularInt16Buffer.h"

#include <span>
#include <vector>

TEST(CircularInt16Buffer, WriteHeadAdvancesWithoutModulo)
{
    pitchlab::CircularInt16Buffer buf (8);
    std::vector<std::int16_t> a = { 1, 2, 3 };
    buf.push (std::span<const std::int16_t> (a.data(), a.size()));
    EXPECT_EQ(buf.writeHead(), 3u);
}

TEST(CircularInt16Buffer, SplitWriteAcrossEnd)
{
    pitchlab::CircularInt16Buffer buf (4);
    std::vector<std::int16_t> first = { 1, 2 };
    buf.push (std::span<const std::int16_t> (first.data(), first.size()));
    EXPECT_EQ(buf.writeHead(), 2u);

    std::vector<std::int16_t> second = { 3, 4, 5 };
    buf.push (std::span<const std::int16_t> (second.data(), second.size()));
    EXPECT_EQ(buf.writeHead(), 1u);

    EXPECT_EQ(buf.rawAt (0), 5);
    EXPECT_EQ(buf.rawAt (1), 2);
    EXPECT_EQ(buf.rawAt (2), 3);
    EXPECT_EQ(buf.rawAt (3), 4);
}

TEST(CircularInt16Buffer, ResetClearsAndZeroesHead)
{
    pitchlab::CircularInt16Buffer buf (3);
    std::vector<std::int16_t> d = { 9, 8, 7 };
    buf.push (std::span<const std::int16_t> (d.data(), d.size()));
    buf.reset();
    EXPECT_EQ(buf.writeHead(), 0u);
    EXPECT_EQ(buf.rawAt (0), 0);
}
