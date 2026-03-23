#include "PitchLabChord.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace pitchlab
{

namespace
{
[[nodiscard]] float classEnergy (std::span<const float> chroma384, int pitchClass) noexcept
{
    float m = 0.0f;
    const int base = pitchClass * 32;
    for (int i = 0; i < 32; ++i)
        m = std::max (m, chroma384[static_cast<std::size_t> (base + i)]);
    return m;
}

struct ChordTemplate
{
    std::array<int, 4> iv {};
    int count = 0;
};

constexpr std::array<ChordTemplate, kChordTypes> kTemplates { {
    { { 0, 4, 7, 0 }, 3 },   // maj
    { { 0, 3, 7, 0 }, 3 },   // min
    { { 0, 3, 6, 0 }, 3 },   // dim
    { { 0, 4, 8, 0 }, 3 },   // aug
    { { 0, 2, 7, 0 }, 3 },   // sus2
    { { 0, 5, 7, 0 }, 3 },   // sus4
    { { 0, 4, 7, 10 }, 4 },  // dom7
} };
} // namespace

void fillChordProbabilitiesFromChroma384 (std::span<const float> chroma384, std::span<float> out) noexcept
{
    if (chroma384.size() < 384 || out.size() < static_cast<std::size_t> (kChordMatrixFloats))
        return;

    float sumAll = 0.0f;

    for (int type = 0; type < kChordTypes; ++type)
    {
        const auto& tp = kTemplates[static_cast<std::size_t> (type)];
        for (int root = 0; root < kChordRoots; ++root)
        {
            float logSum = 0.0f;
            for (int k = 0; k < tp.count; ++k)
            {
                const int pc = (root + tp.iv[static_cast<std::size_t> (k)] + 120) % 12;
                const float e = std::max (1.0e-9f, classEnergy (chroma384, pc));
                logSum += std::log (e);
            }

            const float score = std::exp (logSum / static_cast<float> (tp.count));
            out[static_cast<std::size_t> (root + type * kChordRoots)] = score;
            sumAll += score;
        }
    }

    if (sumAll > 1.0e-12f)
    {
        const float inv = 1.0f / sumAll;
        for (std::size_t i = 0; i < static_cast<std::size_t> (kChordMatrixFloats); ++i)
            out[i] *= inv;
    }
}

void applyAntiChordPenaltyInPlace (std::span<float> chordProbabilities) noexcept
{
    if (chordProbabilities.size() < static_cast<std::size_t> (kChordMatrixFloats))
        return;

    for (int root = 0; root < kChordRoots; ++root)
    {
        const float maj = chordProbabilities[static_cast<std::size_t> (root + 0 * kChordRoots)];
        const float min = chordProbabilities[static_cast<std::size_t> (root + 1 * kChordRoots)];
        float& dim = chordProbabilities[static_cast<std::size_t> (root + 2 * kChordRoots)];
        float& aug = chordProbabilities[static_cast<std::size_t> (root + 3 * kChordRoots)];

        if (maj > 0.12f && aug > 0.018f)
            aug *= 0.55f;
        if (min > 0.12f && dim > 0.018f)
            dim *= 0.55f;
    }

    float s = 0.0f;
    for (float v : chordProbabilities)
        s += v;
    if (s > 1.0e-12f)
    {
        const float inv = 1.0f / s;
        for (float& v : chordProbabilities)
            v *= inv;
    }
}

} // namespace pitchlab
