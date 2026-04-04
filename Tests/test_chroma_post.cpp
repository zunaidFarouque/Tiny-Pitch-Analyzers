#include <gtest/gtest.h>

#include "ChromaPostProcess.h"

#include <cmath>
#include <array>
#include <vector>

TEST(ChromaPost, LeakyPeakDecaysWhenFrameDrops)
{
    std::array<float, 384> acc {};
    std::array<float, 384> f1 {};
    f1[0] = 1.0f;
    pitchlab::accumulateLeakyPeakChroma384 (std::span<const float> { f1.data(), f1.size() },
                                            std::span<float> { acc.data(), acc.size() },
                                            0.85f);
    EXPECT_FLOAT_EQ(acc[0], 1.0f);

    std::array<float, 384> f0 {};
    pitchlab::accumulateLeakyPeakChroma384 (std::span<const float> { f0.data(), f0.size() },
                                            std::span<float> { acc.data(), acc.size() },
                                            0.85f);
    EXPECT_FLOAT_EQ(acc[0], 0.85f);

    pitchlab::accumulateLeakyPeakChroma384 (std::span<const float> { f0.data(), f0.size() },
                                            std::span<float> { acc.data(), acc.size() },
                                            0.85f);
    EXPECT_NEAR(acc[0], 0.85f * 0.85f, 1.0e-5f);
}

TEST(ChromaPost, LogCompressFiniteAndBounded)
{
    std::vector<float> row (384, 0.0f);
    row[10] = 1.0f;
    row[20] = 0.5f;
    pitchlab::applyChromaShaping384 (pitchlab::ChromaShapingMode::LogCompress,
                                     std::span<float> { row.data(), row.size() });
    for (float v : row)
    {
        EXPECT_TRUE(std::isfinite(v));
        EXPECT_GE(v, 0.0f);
        EXPECT_LE(v, 1.0f);
    }
}

TEST(ChromaPost, NoneLeavesData)
{
    std::vector<float> row (384, 1.25f);
    pitchlab::applyChromaShaping384 (pitchlab::ChromaShapingMode::None,
                                     std::span<float> { row.data(), row.size() });
    EXPECT_FLOAT_EQ(row[0], 1.25f);
}
