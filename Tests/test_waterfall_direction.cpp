#include <gtest/gtest.h>

#include "WaterfallMapping.h"

TEST(WaterfallDirection, RightmostSamplesNewestTimeIndex)
{
    constexpr int width = 1024;
    constexpr int height = 384;
    constexpr int kWriteY = 11; // newestRow = writeY - 1 = 10

    const auto right = pitchlab::waterfall::mapWaterfallPixel (width - 1, 0, width, height, kWriteY);
    const auto left = pitchlab::waterfall::mapWaterfallPixel (0, 0, width, height, kWriteY);

    const int newestRow = (kWriteY - 1 + pitchlab::waterfall::kFilmHeight) % pitchlab::waterfall::kFilmHeight;
    const int oldestRow = (newestRow + 1) % pitchlab::waterfall::kFilmHeight;

    EXPECT_EQ (right.timeIndex, newestRow);
    EXPECT_EQ (left.timeIndex, oldestRow);
}

TEST(WaterfallDirection, NoteLaneFoldingSelectsCorrectChromaBins)
{
    constexpr int width = 1024;
    constexpr int height = 384;
    constexpr int kWriteY = 1;

    // Bottom pixel => note=0 (C lane), sub=0 => chromaIndex=0
    const auto bottom = pitchlab::waterfall::mapWaterfallPixel (width / 2, height - 1, width, height, kWriteY);
    EXPECT_EQ (bottom.chromaIndex, 0);

    // Top pixel => note=11 (B lane), sub=31 => chromaIndex=383
    const auto top = pitchlab::waterfall::mapWaterfallPixel (width / 2, 0, width, height, kWriteY);
    EXPECT_EQ (top.chromaIndex, 383);
}

