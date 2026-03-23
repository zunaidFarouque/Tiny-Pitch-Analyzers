#include <gtest/gtest.h>

#include "PitchLabOptimizations.h"

TEST(PitchLabOptimizations, BlendEnergyRgbClamps)
{
    EXPECT_EQ(pitchlab::blendEnergyRgbQ16 (100, 255), 124u);
    EXPECT_EQ(pitchlab::blendEnergyRgbQ16 (100000, 255), 255u);
}
