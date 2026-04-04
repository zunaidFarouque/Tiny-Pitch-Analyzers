#include <gtest/gtest.h>

#include "ChromaPostProcess.h"

#include "EngineState.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

TEST(ChromaWaterfallShading, UniformScalarsMatchSingleHostScale)
{
    std::array<float, 384> row {};
    row.fill (0.25f);

    pitchlab::applyFreqDependentWaterfallShaping384InPlace (
        std::span<float> { row.data(), row.size() },
        pitchlab::WaterfallDisplayCurveMode::Linear,
        1.0f,
        1.0f,
        2.0f,
        2.0f);

    const float expected = std::pow (std::clamp (0.25f * 1.0f, 0.0f, 1.0f), 2.0f);
    for (float v : row)
        EXPECT_NEAR(v, expected, 1.0e-5f);
}

TEST(ChromaWaterfallShading, EnergyLerpRaisesHighBinsVersusLow)
{
    std::array<float, 384> row {};
    row.fill (0.5f);

    pitchlab::applyFreqDependentWaterfallShaping384InPlace (
        std::span<float> { row.data(), row.size() },
        pitchlab::WaterfallDisplayCurveMode::Linear,
        1.0f,
        2.0f,
        2.0f,
        2.0f);

    const float low = std::pow (std::clamp (0.5f * 1.0f, 0.0f, 1.0f), 2.0f);
    const float high = std::pow (std::clamp (0.5f * 2.0f, 0.0f, 1.0f), 2.0f);
    EXPECT_NEAR(row[0], low, 1.0e-5f);
    EXPECT_NEAR(row[383], high, 1.0e-5f);
    EXPECT_GT(row[383], row[0]);
}

TEST(ChromaWaterfallShading, AlphaLerpThinsHighBinsWhenInputUniform)
{
    std::array<float, 384> row {};
    row.fill (0.5f);

    pitchlab::applyFreqDependentWaterfallShaping384InPlace (
        std::span<float> { row.data(), row.size() },
        pitchlab::WaterfallDisplayCurveMode::Linear,
        1.0f,
        1.0f,
        1.0f,
        4.0f);

    const float m = 0.5f;
    EXPECT_NEAR(row[0], std::pow (m, 1.0f), 1.0e-5f);
    EXPECT_NEAR(row[383], std::pow (m, 4.0f), 1.0e-5f);
    EXPECT_LT(row[383], row[0]);
}

TEST(ChromaWaterfallShading, HarmonicAwareLerpDiffersLowVsHighAcousticBins)
{
    std::array<float, 384> row {};
    row.fill (0.5f);
    std::array<std::uint8_t, 384> harm {};
    harm.fill (static_cast<std::uint8_t> (255));
    harm[0] = 0;
    harm[383] = 8;

    pitchlab::applyFreqDependentWaterfallShaping384InPlace (
        std::span<float> { row.data(), row.size() },
        pitchlab::WaterfallDisplayCurveMode::Linear,
        1.0f,
        2.0f,
        2.0f,
        2.0f,
        std::span<const std::uint8_t> { harm.data(), harm.size() },
        pitchlab::FoldHarmonicModel::OctaveStack_Doc_v1,
        44100.0,
        1.0f);

    EXPECT_GT(row[383], row[0]);
}

TEST(ChromaWaterfallShading, LinearVsLogHzBlendChangesEnergyWeighting)
{
    std::array<float, 384> rowLin {};
    std::array<float, 384> rowLog {};
    rowLin.fill (0.5f);
    rowLog.fill (0.5f);
    std::array<std::uint8_t, 384> harm {};
    harm.fill (static_cast<std::uint8_t> (255));
    harm[192] = 4;

    pitchlab::applyFreqDependentWaterfallShaping384InPlace (
        std::span<float> { rowLin.data(), rowLin.size() },
        pitchlab::WaterfallDisplayCurveMode::Linear,
        1.0f,
        2.0f,
        2.0f,
        2.0f,
        std::span<const std::uint8_t> { harm.data(), harm.size() },
        pitchlab::FoldHarmonicModel::OctaveStack_Doc_v1,
        44100.0,
        0.0f);

    pitchlab::applyFreqDependentWaterfallShaping384InPlace (
        std::span<float> { rowLog.data(), rowLog.size() },
        pitchlab::WaterfallDisplayCurveMode::Linear,
        1.0f,
        2.0f,
        2.0f,
        2.0f,
        std::span<const std::uint8_t> { harm.data(), harm.size() },
        pitchlab::FoldHarmonicModel::OctaveStack_Doc_v1,
        44100.0,
        1.0f);

    EXPECT_NE(rowLin[192], rowLog[192]);
}
