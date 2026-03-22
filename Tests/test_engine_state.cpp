#include <gtest/gtest.h>

#include "EngineState.h"

TEST(EngineState, DefaultFftAndSampleRate)
{
    pitchlab::EngineState s;
    EXPECT_EQ(s.fftSize, 4096);
    EXPECT_DOUBLE_EQ(s.sampleRate, 44100.0);
    EXPECT_FALSE(s.analysisDirty);
}

TEST(EngineState, ResetAnalysisDisplay)
{
    pitchlab::EngineState s;
    s.currentHz = 440.0f;
    s.tuningError = 3.0f;
    s.analysisDirty = true;
    s.resetAnalysisDisplay();
    EXPECT_FLOAT_EQ(s.currentHz, 0.0f);
    EXPECT_FLOAT_EQ(s.tuningError, 0.0f);
    EXPECT_FALSE(s.analysisDirty);
}
