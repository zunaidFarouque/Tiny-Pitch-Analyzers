#include <gtest/gtest.h>

#include "ChromaFolder.h"
#include "ChromaMap.h"
#include "FftMagnitudes.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

TEST(FftMagnitudes, SineEnergyNearExpectedBin)
{
    constexpr double sr = 44100.0;
    constexpr int n = 4096;
    constexpr float f0 = 440.0f;
    pitchlab::FftMagnitudes fft (12);

    std::vector<float> time (static_cast<std::size_t> (n));
    for (int i = 0; i < n; ++i)
        time[static_cast<std::size_t> (i)] = std::sin (2.0f * std::numbers::pi_v<float> * f0 * static_cast<float> (i) / static_cast<float> (sr));

    std::vector<float> mag (static_cast<std::size_t> (n / 2 + 1));
    fft.computePowerSpectrum (std::span<const float> { time.data(), time.size() },
                              std::span<float> { mag.data(), mag.size() });

    const int expectedBin = static_cast<int> (std::lround (static_cast<double> (f0) * static_cast<double> (n) / sr));
    ASSERT_GE(expectedBin, 2);
    ASSERT_LT(expectedBin + 2, static_cast<int> (mag.size()));

    const float peakNear = mag[static_cast<std::size_t> (expectedBin - 1)]
                           + mag[static_cast<std::size_t> (expectedBin)]
                           + mag[static_cast<std::size_t> (expectedBin + 1)];

    const float peakFar = mag[static_cast<std::size_t> (expectedBin + 40)];

    EXPECT_GT(peakNear, peakFar * 10.0f);
}

TEST(ChromaFolder, Fold384HasNonZeroForActiveSlice)
{
    constexpr int n = 4096;
    std::vector<float> mag (static_cast<std::size_t> (n / 2 + 1), 0.05f);

    std::vector<float> out (384, 0.0f);
    pitchlab::ChromaMap map;
    map.rebuild (44100.0, n);
    std::array<std::uint8_t, 384> harm {};
    pitchlab::foldToChroma384 (map,
                               44100.0,
                               n,
                               std::span<const float> { mag.data(), mag.size() },
                               std::span<float> { out.data(), out.size() },
                               std::span<std::uint8_t> { harm.data(), harm.size() });

    float s = 0.0f;
    for (float x : out)
        s += x;
    EXPECT_GT(s, 1.0f);
}

TEST(ChromaFolder, DeltaBinSetsDominantHarmonic)
{
    constexpr int n = 4096;
    constexpr double sr = 44100.0;
    pitchlab::ChromaMap map;
    map.rebuild (sr, n);

    std::vector<float> mag (static_cast<std::size_t> (n / 2 + 1), 0.0f);
    const int bin2f = static_cast<int> (std::lround (880.0 * static_cast<double> (n) / sr));
    ASSERT_GE(bin2f, 2);
    ASSERT_LT(bin2f + 2, static_cast<int> (mag.size()));
    mag[static_cast<std::size_t> (bin2f)] = 100.0f;

    std::vector<float> out (384, 0.0f);
    std::array<std::uint8_t, 384> harm {};
    pitchlab::FoldToChromaSettings s;
    s.interpMode = pitchlab::FoldInterpMode::Nearest;
    s.harmonicWeightMode = pitchlab::FoldHarmonicWeightMode::Uniform;
    pitchlab::foldToChroma384 (map,
                               sr,
                               n,
                               std::span<const float> { mag.data(), mag.size() },
                               std::span<float> { out.data(), out.size() },
                               std::span<std::uint8_t> { harm.data(), harm.size() },
                               s);

    int best = 0;
    float mx = out[0];
    for (int i = 1; i < 384; ++i)
    {
        if (out[static_cast<std::size_t> (i)] > mx)
        {
            mx = out[static_cast<std::size_t> (i)];
            best = i;
        }
    }

    EXPECT_GT(mx, 50.0f);
    // Our fold stacks octaves into a 65–131 Hz slice range, so a delta at 880 Hz
    // lands on the 8f harmonic (f,2f,4f,8f ⇒ dominant harmonic index 3).
    EXPECT_EQ(harm[static_cast<std::size_t> (best)], 3u);
}

TEST(ChromaFolder, InterpolationModeProducesDifferentEnergy)
{
    constexpr int n = 4096;
    constexpr double sr = 44100.0;
    pitchlab::ChromaMap map;
    map.rebuild (sr, n);

    std::vector<float> mag (static_cast<std::size_t> (n / 2 + 1), 0.0f);
    for (int k = 0; k < 20; ++k)
        mag[static_cast<std::size_t> (40 + k)] = static_cast<float> (k) * 0.25f;

    std::vector<float> outNearest (384, 0.0f);
    std::vector<float> outLinear (384, 0.0f);
    std::array<std::uint8_t, 384> harm {};

    pitchlab::FoldToChromaSettings nearest;
    nearest.interpMode = pitchlab::FoldInterpMode::Nearest;
    nearest.harmonicWeightMode = pitchlab::FoldHarmonicWeightMode::Uniform;
    pitchlab::foldToChroma384 (map, sr, n,
                               std::span<const float> { mag.data(), mag.size() },
                               std::span<float> { outNearest.data(), outNearest.size() },
                               std::span<std::uint8_t> { harm.data(), harm.size() },
                               nearest);

    pitchlab::FoldToChromaSettings linear = nearest;
    linear.interpMode = pitchlab::FoldInterpMode::Linear2Bin;
    pitchlab::foldToChroma384 (map, sr, n,
                               std::span<const float> { mag.data(), mag.size() },
                               std::span<float> { outLinear.data(), outLinear.size() },
                               std::span<std::uint8_t> { harm.data(), harm.size() },
                               linear);

    float diff = 0.0f;
    for (int i = 0; i < 384; ++i)
        diff += std::abs (outNearest[static_cast<std::size_t> (i)] - outLinear[static_cast<std::size_t> (i)]);
    EXPECT_GT(diff, 0.0f);
}

TEST(ChromaFolder, MaxOctavesLimitReducesFoldedEnergy)
{
    constexpr int n = 4096;
    constexpr double sr = 44100.0;
    pitchlab::ChromaMap map;
    map.rebuild (sr, n);

    std::vector<float> mag (static_cast<std::size_t> (n / 2 + 1), 0.25f);
    std::vector<float> outAuto (384, 0.0f);
    std::vector<float> outOneOct (384, 0.0f);
    std::array<std::uint8_t, 384> harm {};

    pitchlab::FoldToChromaSettings sAuto;
    sAuto.interpMode = pitchlab::FoldInterpMode::Nearest;
    sAuto.harmonicWeightMode = pitchlab::FoldHarmonicWeightMode::Uniform;
    sAuto.maxOctaves = 0;
    pitchlab::foldToChroma384 (map, sr, n,
                               std::span<const float> { mag.data(), mag.size() },
                               std::span<float> { outAuto.data(), outAuto.size() },
                               std::span<std::uint8_t> { harm.data(), harm.size() },
                               sAuto);

    auto sOne = sAuto;
    sOne.maxOctaves = 1;
    pitchlab::foldToChroma384 (map, sr, n,
                               std::span<const float> { mag.data(), mag.size() },
                               std::span<float> { outOneOct.data(), outOneOct.size() },
                               std::span<std::uint8_t> { harm.data(), harm.size() },
                               sOne);

    float sumAuto = 0.0f;
    float sumOne = 0.0f;
    for (int i = 0; i < 384; ++i)
    {
        sumAuto += outAuto[static_cast<std::size_t> (i)];
        sumOne += outOneOct[static_cast<std::size_t> (i)];
    }
    EXPECT_GT(sumAuto, sumOne);
}

TEST(ChromaFolder, IntegerHarmonicsDetectsThirdPartialOctaveDoesNot)
{
    constexpr int n = 4096;
    constexpr double sr = 44100.0;
    pitchlab::ChromaMap map;
    map.rebuild (sr, n);

    std::vector<float> mag (static_cast<std::size_t> (n / 2 + 1), 0.0f);
    const int bin3f = static_cast<int> (std::lround (330.0 * static_cast<double> (n) / sr));
    ASSERT_GE(bin3f, 2);
    ASSERT_LT(bin3f + 1, static_cast<int> (mag.size()));
    mag[static_cast<std::size_t> (bin3f)] = 100.0f;

    constexpr int sliceA2 = 9 * 32;
    std::vector<float> outOct (384, 0.0f);
    std::vector<float> outInt (384, 0.0f);
    std::array<std::uint8_t, 384> harm {};

    pitchlab::FoldToChromaSettings sOct;
    sOct.interpMode = pitchlab::FoldInterpMode::Nearest;
    sOct.harmonicWeightMode = pitchlab::FoldHarmonicWeightMode::Uniform;
    sOct.harmonicModel = pitchlab::FoldHarmonicModel::OctaveStack_Doc_v1;
    pitchlab::foldToChroma384 (map, sr, n,
                               std::span<const float> { mag.data(), mag.size() },
                               std::span<float> { outOct.data(), outOct.size() },
                               std::span<std::uint8_t> { harm.data(), harm.size() },
                               sOct);

    pitchlab::FoldToChromaSettings sInt = sOct;
    sInt.harmonicModel = pitchlab::FoldHarmonicModel::IntegerHarmonics_v0_2;
    sInt.maxHarmonicK = 8;
    pitchlab::foldToChroma384 (map, sr, n,
                               std::span<const float> { mag.data(), mag.size() },
                               std::span<float> { outInt.data(), outInt.size() },
                               std::span<std::uint8_t> { harm.data(), harm.size() },
                               sInt);

    EXPECT_GT(outInt[static_cast<std::size_t> (sliceA2)], outOct[static_cast<std::size_t> (sliceA2)] * 5.0f);
    EXPECT_EQ(harm[static_cast<std::size_t> (sliceA2)], 2u);
}
