#include <cmath>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "MultiResSpectrumStitch.h"

TEST (SpectrumStitching, OutputLengthMatchesVirtualBins)
{
    constexpr int hfFft = 4096;
    constexpr int lfFft = 4096;
    constexpr int virtualFft = hfFft * 4;
    constexpr double sr = 44100.0;
    const int hfBins = hfFft / 2 + 1;
    const int lfBins = lfFft / 2 + 1;
    const int outBins = virtualFft / 2 + 1;

    std::vector<float> hf (static_cast<std::size_t> (hfBins), 0.0f);
    std::vector<float> lf (static_cast<std::size_t> (lfBins), 0.0f);
    std::vector<float> out (static_cast<std::size_t> (outBins), 0.0f);

    pitchlab::stitchMultiResMagnitudes (std::span<const float> { hf.data(), hf.size() },
                                        std::span<const float> { lf.data(), lf.size() },
                                        sr,
                                        sr / 4.0,
                                        hfFft,
                                        lfFft,
                                        virtualFft,
                                        1000.0f,
                                        static_cast<float> (hfFft) / static_cast<float> (lfFft),
                                        std::span<float> { out.data(), out.size() });

    EXPECT_EQ (out.size(), static_cast<std::size_t> (outBins));
    for (float v : out)
    {
        EXPECT_TRUE (std::isfinite (v));
        EXPECT_GE (v, 0.0f);
    }
}

TEST (SpectrumStitching, MockTonesMergeWithBoundedCrossoverJump)
{
    constexpr int hfFft = 4096;
    constexpr int lfFft = 4096;
    constexpr int virtualFft = hfFft * 4;
    constexpr double sr = 44100.0;
    const int hfBins = hfFft / 2 + 1;
    const int lfBins = lfFft / 2 + 1;
    const int outBins = virtualFft / 2 + 1;
    const float lfGain = static_cast<float> (hfFft) / static_cast<float> (lfFft);

    std::vector<float> hf (static_cast<std::size_t> (hfBins), 0.0f);
    std::vector<float> lf (static_cast<std::size_t> (lfBins), 0.0f);

    for (int k = 0; k < hfBins; ++k)
    {
        const double hz = static_cast<double> (k) * sr / static_cast<double> (hfFft);
        hf[static_cast<std::size_t> (k)] = hz >= 1000.0 ? 1.0f : 0.0f;
    }

    for (int j = 0; j < lfBins; ++j)
    {
        const double hz = static_cast<double> (j) * (sr / 4.0) / static_cast<double> (lfFft);
        lf[static_cast<std::size_t> (j)] = hz < 1000.0 ? (1.0f / lfGain) : 0.0f;
    }

    std::vector<float> out (static_cast<std::size_t> (outBins), 0.0f);
    pitchlab::stitchMultiResMagnitudes (std::span<const float> { hf.data(), hf.size() },
                                        std::span<const float> { lf.data(), lf.size() },
                                        sr,
                                        sr / 4.0,
                                        hfFft,
                                        lfFft,
                                        virtualFft,
                                        1000.0f,
                                        lfGain,
                                        std::span<float> { out.data(), out.size() });

    const double virtualBinHz = sr / static_cast<double> (virtualFft);
    int kCross = -1;
    for (int k = 0; k < outBins - 1; ++k)
    {
        const double hz0 = static_cast<double> (k) * virtualBinHz;
        const double hz1 = static_cast<double> (k + 1) * virtualBinHz;
        if (hz0 < 1000.0 && hz1 >= 1000.0)
        {
            kCross = k;
            break;
        }
    }
    ASSERT_GE (kCross, 0);

    const float a = out[static_cast<std::size_t> (kCross)];
    const float b = out[static_cast<std::size_t> (kCross + 1)];
    EXPECT_GT (a, 0.4f);
    EXPECT_GT (b, 0.4f);
    const float ratio = (std::max) (a, b) / (std::max) (1.0e-6f, (std::min) (a, b));
    EXPECT_LT (ratio, 3.0f);
}
