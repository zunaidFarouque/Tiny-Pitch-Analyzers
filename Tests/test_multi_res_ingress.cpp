#include <vector>

#include <gtest/gtest.h>

#include "PitchLabEngine.h"

#ifndef PITCHLAB_ENGINE_TESTING
#error "PITCHLAB_ENGINE_TESTING must be set for test_multi_res_ingress.cpp"
#endif

TEST (PitchLabEngineMultiRes, HfAndLfIngressAdvanceOneToFour)
{
    pitchlab::PitchLabEngine eng;
    eng.state().setSpectralBackendMode (pitchlab::SpectralBackendMode::MultiResSTFT_v1_0);
    eng.prepareToPlay (44100.0, 4096);

    const std::size_t h0 = eng.testHfIngress().writeHead();
    const std::size_t l0 = eng.testLfIngress().writeHead();

    constexpr int n = 4000;
    std::vector<float> buf (static_cast<std::size_t> (n), 0.01f);
    const float* ch[] = { buf.data() };
    eng.processAudioInterleaved (ch, 1, n);

    EXPECT_EQ (eng.testHfIngress().writeHead() - h0, static_cast<std::size_t> (n));
    EXPECT_EQ (eng.testLfIngress().writeHead() - l0, static_cast<std::size_t> (n / 4));
}
