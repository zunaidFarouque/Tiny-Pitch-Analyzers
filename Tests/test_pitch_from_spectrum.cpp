#include <gtest/gtest.h>

#include "PitchFromSpectrum.h"

#include <cmath>

TEST(PitchFromSpectrum, ParabolicPeakAtCenter)
{
    const float y0 = 0.25f;
    const float y1 = 1.0f;
    const float y2 = 0.25f;
    const float off = pitchlab::parabolicPeakOffset (y0, y1, y2);
    EXPECT_NEAR(off, 0.0f, 1.0e-4f);
}

TEST(PitchFromSpectrum, CentsVsTempered440)
{
    EXPECT_NEAR(pitchlab::centsVsTempered (440.0f), 0.0f, 0.05f);
    EXPECT_GT(std::abs (pitchlab::centsVsTempered (450.0f)), 30.0f);
}
