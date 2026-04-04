#include <algorithm>
#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include "Decimator4x.h"

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kSr = 44100.0;

void fillSine (std::vector<float>& buf, double hz, double sr, float amp)
{
    for (std::size_t i = 0; i < buf.size(); ++i)
        buf[i] = amp * std::sin (static_cast<float> (kPi * 2.0 * hz * static_cast<double> (i) / sr));
}

/** Single-bin DFT magnitude at integer bin k (k cycles per N samples). */
float dftMagAtBin (const std::vector<float>& x, int k)
{
    const int N = static_cast<int> (x.size());
    if (N <= 0 || k < 0 || k >= N)
        return 0.0f;
    double re = 0.0, im = 0.0;
    for (int n = 0; n < N; ++n)
    {
        const double ang = kPi * 2.0 * static_cast<double> (k * n) / static_cast<double> (N);
        re += static_cast<double> (x[static_cast<std::size_t> (n)]) * std::cos (ang);
        im -= static_cast<double> (x[static_cast<std::size_t> (n)]) * std::sin (ang);
    }
    return static_cast<float> (std::sqrt (re * re + im * im));
}

float rms (const std::vector<float>& x)
{
    if (x.empty())
        return 0.0f;
    double s = 0.0;
    for (float v : x)
        s += static_cast<double> (v) * static_cast<double> (v);
    return static_cast<float> (std::sqrt (s / static_cast<double> (x.size())));
}
} // namespace

TEST (Decimator4x, OutputLengthIsInputOverFour)
{
    pitchlab::Decimator4x d;
    d.prepare (kSr);
    std::vector<float> in (4000);
    fillSine (in, 100.0, kSr, 0.5f);
    std::vector<float> out (in.size() / 4);
    const int n = d.processBlock (std::span<const float> { in.data(), in.size() },
                                  std::span<float> { out.data(), out.size() });
    EXPECT_EQ (n, 1000);
    EXPECT_EQ (n, static_cast<int> (in.size() / 4));
}

TEST (Decimator4x, OddInputLengthTrailingSamplesNoExtraOutput)
{
    pitchlab::Decimator4x d;
    d.prepare (kSr);
    std::vector<float> in (4001);
    fillSine (in, 100.0, kSr, 0.5f);
    std::vector<float> out (1000);
    const int n = d.processBlock (std::span<const float> { in.data(), in.size() },
                                  std::span<float> { out.data(), out.size() });
    EXPECT_EQ (n, 1000);
}

TEST (Decimator4x, HundredHzTonePreservesEnergyVsTenKHzAttenuated)
{
    pitchlab::Decimator4x d;
    d.prepare (kSr);
    const int inLen = 16384;
    const int outLen = inLen / 4;
    const double srOut = kSr / 4.0;

    std::vector<float> in100 (static_cast<std::size_t> (inLen));
    fillSine (in100, 100.0, kSr, 0.5f);
    std::vector<float> out100 (static_cast<std::size_t> (outLen));
    ASSERT_EQ (d.processBlock ({ in100.data(), in100.size() }, { out100.data(), out100.size() }), outLen);

    const int k100 = std::clamp (static_cast<int> (std::lround (100.0 * static_cast<double> (outLen) / srOut)),
                                 0, outLen - 1);
    const float mag100 = dftMagAtBin (out100, k100);

    d.reset();
    std::vector<float> in10k (static_cast<std::size_t> (inLen));
    fillSine (in10k, 10000.0, kSr, 0.5f);
    std::vector<float> out10k (static_cast<std::size_t> (outLen));
    ASSERT_EQ (d.processBlock ({ in10k.data(), in10k.size() }, { out10k.data(), out10k.size() }), outLen);

    const float mag10kAt100Bin = dftMagAtBin (out10k, k100);
    const float rms10k = rms (out10k);

    EXPECT_GT (mag100, 1.0e-3f);
    EXPECT_LT (mag10kAt100Bin, mag100 * 0.05f);
    EXPECT_LT (rms10k, rms (out100) * 0.15f);
}
