#include <gtest/gtest.h>

#include "PitchLabEngine.h"
#include "StaticTables.h"

TEST(PitchLabEngine, PrepareCreatesTablesAndResetClearsIngress)
{
    pitchlab::PitchLabEngine eng;
    eng.prepareToPlay (48000.0, 256);
    ASSERT_NE(eng.tables(), nullptr);
    EXPECT_EQ(eng.tables()->hanningWindowQ24().size(), 8192u);

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

TEST (PitchLabEngine, StftModeIgnoresLfIngress)
{
    pitchlab::PitchLabEngine eng;
    eng.prepareToPlay (44100.0, 512);
    ASSERT_EQ (eng.state().spectralBackendMode(), pitchlab::SpectralBackendMode::STFT_v1_0);

    const float buf[] = { 0.1f, 0.1f, 0.1f, 0.1f };
    const float* ch[] = { buf };
#if defined(PITCHLAB_ENGINE_TESTING)
    const std::size_t l0 = eng.testLfIngress().writeHead();
#endif
    eng.processAudioInterleaved (ch, 1, 4);
#if defined(PITCHLAB_ENGINE_TESTING)
    EXPECT_EQ (eng.testLfIngress().writeHead(), l0);
#endif
}
