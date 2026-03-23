#include <gtest/gtest.h>

#include "PitchLabEngine.h"
#include "StaticTables.h"

TEST(PitchLabEngine, PrepareCreatesTablesAndResetClearsIngress)
{
    pitchlab::PitchLabEngine eng;
    eng.prepareToPlay (48000.0, 256);
    ASSERT_NE(eng.tables(), nullptr);
    EXPECT_EQ(eng.tables()->hanningWindowQ24().size(), 4096u);

    const float c0[] = { 0.5f, 0.5f };
    const float* ch[] = { c0 };
    eng.processAudioInterleaved (ch, 1, 2);
    EXPECT_TRUE(eng.state().analysisDirty.load());

    eng.reset();
    EXPECT_FALSE(eng.state().analysisDirty.load());
    EXPECT_EQ(eng.ingressBuffer().writeHead(), 0u);
}

TEST(PitchLabEngine, ProcessFillsIngress)
{
    pitchlab::PitchLabEngine eng;
    eng.prepareToPlay (44100.0, 4);
    const float c0[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    const float* ch[] = { c0 };
    eng.processAudioInterleaved (ch, 1, 4);
    EXPECT_EQ(eng.ingressBuffer().writeHead(), 4u);
    EXPECT_EQ(eng.ingressBuffer().rawAt (0), 32767);
}
