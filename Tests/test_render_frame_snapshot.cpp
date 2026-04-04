#include <gtest/gtest.h>

#include "PitchLabEngine.h"

#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include "PitchPeak.h"

TEST(RenderFrameSnapshot, SnapshotCarriesPitchAndChroma)
{
    pitchlab::PitchLabEngine eng;
    constexpr double sr = 44100.0;
    constexpr int n = 4096;
    eng.prepareToPlay (sr, n);

    std::vector<float> mono (static_cast<std::size_t> (n));
    for (int i = 0; i < n; ++i)
        mono[static_cast<std::size_t> (i)] =
            0.3f * std::sin (2.0f * std::numbers::pi_v<float> * 440.0f * static_cast<float> (i) / static_cast<float> (sr));

    const float* ch[] = { mono.data() };
    eng.processAudioInterleaved (ch, 1, n);

    pitchlab::RenderFrameData frame;
    eng.copyLatestRenderFrame (frame);
    EXPECT_GT(frame.sequence, 0u);
    EXPECT_NEAR(frame.currentHz, 440.0f, 8.0f);
    EXPECT_LT(std::abs (frame.tuningError), 20.0f);
    EXPECT_NE(frame.chromaRow[0], 0.0f);
}

TEST(RenderFrameSnapshot, SnapshotCarriesActivePeaksWithinCap)
{
    pitchlab::PitchLabEngine eng;
    constexpr double sr = 44100.0;
    constexpr int n = 4096;
    eng.state().setSpectralBackendMode (pitchlab::SpectralBackendMode::STFT_v1_0);
    eng.prepareToPlay (sr, n);

    std::vector<float> mono (static_cast<std::size_t> (n));
    for (int i = 0; i < n; ++i)
    {
        const float t = static_cast<float> (i) / static_cast<float> (sr);
        mono[static_cast<std::size_t> (i)] =
            0.22f * std::sin (2.0f * std::numbers::pi_v<float> * 440.0f * t)
            + 0.22f * std::sin (2.0f * std::numbers::pi_v<float> * 523.25f * t);
    }

    const float* ch[] = { mono.data() };
    eng.processAudioInterleaved (ch, 1, n);

    pitchlab::RenderFrameData frame;
    eng.copyLatestRenderFrame (frame);
    EXPECT_GT(frame.sequence, 0u);
    EXPECT_GE(frame.activePeaks.size(), 1u);
    EXPECT_LE(frame.activePeaks.size(), static_cast<std::size_t> (pitchlab::PitchPeak::kMaxPeaksCap));
}

