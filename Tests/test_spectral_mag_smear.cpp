#include <gtest/gtest.h>

#include "SpectralMagSmear.h"

#include <cmath>
#include <span>
#include <vector>

TEST(SpectralMagSmear, StftCopiesInput)
{
    constexpr int n = 256;
    std::vector<float> in (static_cast<std::size_t> (n / 2 + 1), 0.0f);
    for (int i = 0; i < static_cast<int> (in.size()); ++i)
        in[static_cast<std::size_t> (i)] = static_cast<float> (i);

    std::vector<float> out (in.size(), -1.0f);
    pitchlab::buildMagForFold (std::span<const float> { in.data(), in.size() },
                               std::span<float> { out.data(), out.size() },
                               44100.0,
                               256,
                               pitchlab::SpectralBackendMode::STFT_v1_0);
    for (std::size_t i = 0; i < in.size(); ++i)
        EXPECT_FLOAT_EQ(in[i], out[i]);
}

TEST (SpectralMagSmear, MultiResCopiesLikeStft)
{
    constexpr int n = 256;
    std::vector<float> in (static_cast<std::size_t> (n / 2 + 1), 0.0f);
    for (int i = 0; i < static_cast<int> (in.size()); ++i)
        in[static_cast<std::size_t> (i)] = static_cast<float> (i);

    std::vector<float> out (in.size(), -1.0f);
    pitchlab::buildMagForFold (std::span<const float> { in.data(), in.size() },
                               std::span<float> { out.data(), out.size() },
                               44100.0,
                               256,
                               pitchlab::SpectralBackendMode::MultiResSTFT_v1_0);
    for (std::size_t i = 0; i < in.size(); ++i)
        EXPECT_FLOAT_EQ (in[i], out[i]);
}

TEST(SpectralMagSmear, ConstQApproxSpreadsIsolatedPeakToNeighbors)
{
    constexpr int n = 4096;
    std::vector<float> in (static_cast<std::size_t> (n / 2 + 1), 0.0f);
    in[static_cast<std::size_t> (200)] = 10.171f;

    std::vector<float> out (in.size(), 0.0f);
    pitchlab::buildMagForFold (std::span<const float> { in.data(), in.size() },
                               std::span<float> { out.data(), out.size() },
                               44100.0,
                               n,
                               pitchlab::SpectralBackendMode::ConstQApprox_v0_1);
    const float side = out[static_cast<std::size_t> (199)] + out[static_cast<std::size_t> (201)];
    const float ctr = out[static_cast<std::size_t> (200)];
    EXPECT_GT(side, 0.15f);
    EXPECT_GT(ctr, 0.0f);
    EXPECT_LT(ctr, 10.0f);
}
