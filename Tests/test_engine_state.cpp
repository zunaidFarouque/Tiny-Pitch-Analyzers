#include <gtest/gtest.h>

#include "EngineState.h"

TEST(EngineState, DefaultFftAndSampleRate)
{
    pitchlab::EngineState s;
    EXPECT_EQ(s.fftSize, 8192);
    EXPECT_DOUBLE_EQ(s.sampleRate, 44100.0);
    EXPECT_FALSE(s.analysisDirty.load());
    EXPECT_FLOAT_EQ(s.wEnergyLow.load(), 1.0f);
    EXPECT_FLOAT_EQ(s.wEnergyHigh.load(), 1.0f);
    EXPECT_FLOAT_EQ(s.wAlphaPowLow.load(), 2.0f);
    EXPECT_FLOAT_EQ(s.wAlphaPowHigh.load(), 2.0f);
    EXPECT_FLOAT_EQ(s.wShapingFreqLogBlend.load(), 1.0f);
    EXPECT_FALSE(s.enablePreEmphasis.load());
    EXPECT_TRUE(s.spectralSmearingEnabled.load());
}

TEST(EngineState, ResetAnalysisDisplay)
{
    pitchlab::EngineState s;
    s.currentHz = 440.0f;
    s.tuningError = 3.0f;
    s.analysisDirty.store (true);
    s.resetAnalysisDisplay();
    EXPECT_FLOAT_EQ(s.currentHz, 0.0f);
    EXPECT_FLOAT_EQ(s.tuningError, 0.0f);
    EXPECT_FALSE(s.analysisDirty.load());
}
