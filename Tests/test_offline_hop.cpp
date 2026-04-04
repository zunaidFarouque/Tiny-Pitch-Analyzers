#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "PitchLabEngine.h"
#include "RenderFrameData.h"

TEST(PitchLabEngine, OfflineWindowAnalysisIsDeterministic)
{
    pitchlab::PitchLabEngine eng;
    constexpr int fft = 1024;
    eng.state().fftSize = fft;
    eng.prepareToPlay (48000.0, fft);

    std::vector<float> win (static_cast<std::size_t> (fft));
    for (int i = 0; i < fft; ++i)
        win[static_cast<std::size_t> (i)] = 0.3f * std::sin (2.0f * 3.14159265f * 440.0f * static_cast<float> (i) / 48000.0f);

    pitchlab::RenderFrameData a {};
    pitchlab::RenderFrameData b {};
    eng.analyzeOfflineWindowFromMonoFloat (std::span<const float> { win.data(), win.size() }, a);
    eng.analyzeOfflineWindowFromMonoFloat (std::span<const float> { win.data(), win.size() }, b);

    for (int i = 0; i < 384; ++i)
        EXPECT_FLOAT_EQ (a.chromaRow[static_cast<std::size_t> (i)], b.chromaRow[static_cast<std::size_t> (i)]);

    EXPECT_GT (a.sequence, 0u);
}

TEST (PitchLabEngine, OfflineWindowFillsChromaWhenMultiResBackendSelected)
{
    pitchlab::PitchLabEngine eng;
    constexpr int fft = 1024;
    eng.state().fftSize = fft;
    eng.state().setSpectralBackendMode (pitchlab::SpectralBackendMode::MultiResSTFT_v1_0);
    eng.prepareToPlay (48000.0, fft);

    std::vector<float> win (static_cast<std::size_t> (fft), 0.2f);
    pitchlab::RenderFrameData out {};
    eng.analyzeOfflineWindowFromMonoFloat (std::span<const float> { win.data(), win.size() }, out);

    float sum = 0.0f;
    for (float c : out.chromaRow)
        sum += c;
    EXPECT_GT (sum, 1.0e-6f);
    EXPECT_EQ (eng.state().spectralBackendMode(), pitchlab::SpectralBackendMode::MultiResSTFT_v1_0);
}
