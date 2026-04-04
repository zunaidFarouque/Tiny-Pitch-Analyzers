#include <gtest/gtest.h>

#include "ChromaPostProcess.h"

#include <cmath>
#include <vector>

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
