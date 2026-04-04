#include <gtest/gtest.h>

#include "ChromaFolder.h"
#include "ChromaMap.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace
{
[[nodiscard]] float midiToHz (float midi) noexcept
{
    return 440.0f * std::pow (2.0f, (midi - 69.0f) / 12.0f);
}
} // namespace

TEST(ChromaFolder, C5SpikeDominatesCBucketsOverWideLowC2Bump)
{
    constexpr int n = 8192;
    constexpr double sr = 44100.0;
    pitchlab::ChromaMap map;
    map.rebuild (sr, n);

    const float hzC2 = midiToHz (36.0f);
    const float hzC5 = midiToHz (72.0f);
    const int binC2 = static_cast<int> (std::lround (static_cast<double> (hzC2) * static_cast<double> (n) / sr));
    const int binC5 = static_cast<int> (std::lround (static_cast<double> (hzC5) * static_cast<double> (n) / sr));

    std::vector<float> mag (static_cast<std::size_t> (n / 2 + 1), 0.0f);
    for (int b = binC2 - 2; b <= binC2 + 2; ++b)
        if (b >= 0 && b < static_cast<int> (mag.size()))
            mag[static_cast<std::size_t> (b)] = 0.05f;

    ASSERT_GE(binC5, 1);
    ASSERT_LT(binC5, static_cast<int> (mag.size()));
    mag[static_cast<std::size_t> (binC5)] = 50.0f;

    std::vector<float> out (384, 0.0f);
    std::array<std::uint8_t, 384> harm {};
    pitchlab::FoldToChromaSettings s;
    s.interpMode = pitchlab::FoldInterpMode::Nearest;
    s.harmonicWeightMode = pitchlab::FoldHarmonicWeightMode::Uniform;
    s.harmonicModel = pitchlab::FoldHarmonicModel::OctaveStack_Doc_v1;
    s.maxOctaves = 0;

    pitchlab::foldToChroma384 (map,
                               sr,
                               n,
                               std::span<const float> { mag.data(), mag.size() },
                               std::span<float> { out.data(), out.size() },
                               std::span<std::uint8_t> { harm.data(), harm.size() },
                               s);

    float maxC = 0.0f;
    for (int i = 0; i < 32; ++i)
        maxC = std::max (maxC, out[static_cast<std::size_t> (i)]);

    float maxCsharp = 0.0f;
    for (int i = 32; i < 64; ++i)
        maxCsharp = std::max (maxCsharp, out[static_cast<std::size_t> (i)]);

    EXPECT_GT(maxC, maxCsharp * 8.0f);
}
