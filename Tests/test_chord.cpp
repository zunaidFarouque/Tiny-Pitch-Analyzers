#include <gtest/gtest.h>

#include "PitchLabChord.h"

#include <array>
#include <numeric>
#include <span>

TEST(PitchLabChord, FillAndAntiChordNormalize)
{
    std::array<float, 384> chroma {};
    for (int i = 0; i < 32; ++i)
        chroma[static_cast<std::size_t> (9 * 32 + i)] = 10.0f + static_cast<float> (i) * 0.1f;

    std::array<float, pitchlab::kChordMatrixFloats> probs {};
    pitchlab::fillChordProbabilitiesFromChroma384 (std::span<const float> { chroma.data(), chroma.size() },
                                                   std::span<float> { probs.data(), probs.size() });

    float s = 0.0f;
    for (float v : probs)
        s += v;
    EXPECT_NEAR(s, 1.0f, 1.0e-3f);

    probs.fill (0.0f);
    probs[0 + 0 * 12] = 0.5f;
    probs[0 + 3 * 12] = 0.5f;
    pitchlab::applyAntiChordPenaltyInPlace (std::span<float> { probs.data(), probs.size() });
    s = std::accumulate (probs.begin(), probs.end(), 0.0f);
    EXPECT_NEAR(s, 1.0f, 1.0e-3f);
    EXPECT_LT(probs[0 + 3 * 12], 0.45f);
}
