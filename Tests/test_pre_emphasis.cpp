#include <gtest/gtest.h>

#include "PreEmphasis.h"

#include <array>
#include <cmath>
#include <vector>

TEST(PreEmphasis, DcConstantAttenuatesToNearZero)
{
    pitchlab::PreEmphasis pe;
    std::array<float, 8> buf {};
    buf.fill (1.0f);

    pe.process (std::span<float> { buf.data(), buf.size() }, true);

    // y[0] = x[0] - 0.95 * x[-1] with x[-1]=0  => 1.0; steady-state y = 1 - 0.95*1 = 0.05
    EXPECT_NEAR(buf[0], 1.0f, 1.0e-5f);
    for (int i = 1; i < 8; ++i)
        EXPECT_NEAR(buf[static_cast<std::size_t> (i)], 0.05f, 1.0e-4f);
}

TEST(PreEmphasis, DisabledLeavesBufferUnchangedAndResetsState)
{
    pitchlab::PreEmphasis pe;
    std::array<float, 4> buf { 1.0f, 1.0f, 1.0f, 1.0f };
    const std::array<float, 4> copy = buf;

    pe.process (std::span<float> { buf.data(), buf.size() }, false);
    EXPECT_EQ(buf, copy);

    pe.process (std::span<float> { buf.data(), buf.size() }, true);
    EXPECT_NEAR(buf[0], 1.0f, 1.0e-5f);

    pe.process (std::span<float> { buf.data(), buf.size() }, false);
    std::array<float, 4> fresh { 1.0f, 1.0f, 1.0f, 1.0f };
    pe.process (std::span<float> { fresh.data(), fresh.size() }, true);
    EXPECT_NEAR(fresh[0], 1.0f, 1.0e-5f);
}

TEST(PreEmphasis, AlternatingSignHasLargerRmsThanDcAfterFilter)
{
    pitchlab::PreEmphasis peDc;
    std::vector<float> dc (64, 0.5f);
    peDc.process (std::span<float> { dc.data(), dc.size() }, true);
    float sumSqDc = 0.0f;
    for (float v : dc)
        sumSqDc += v * v;
    const float rmsDc = std::sqrt (sumSqDc / static_cast<float> (dc.size()));

    pitchlab::PreEmphasis peAlt;
    std::vector<float> alt (64);
    for (std::size_t i = 0; i < alt.size(); ++i)
        alt[i] = (i % 2 == 0) ? 1.0f : -1.0f;
    peAlt.process (std::span<float> { alt.data(), alt.size() }, true);
    float sumSqAlt = 0.0f;
    for (float v : alt)
        sumSqAlt += v * v;
    const float rmsAlt = std::sqrt (sumSqAlt / static_cast<float> (alt.size()));

    EXPECT_GT(rmsAlt, rmsDc * 5.0f);
}
