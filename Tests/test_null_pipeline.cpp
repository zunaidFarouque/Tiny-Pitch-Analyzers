#include <gtest/gtest.h>

#include "PitchLabEngine.h"

#include <array>
#include <cmath>
#include <numbers>
#include <vector>

TEST(NullPipeline, Sine440HzBlockYieldsPitchAndChromaNearA)
{
    constexpr double sr = 44100.0;
    constexpr int n = 4096;
    constexpr float f0 = 440.0f;

    pitchlab::PitchLabEngine eng;
    eng.state().fftSize = 4096; // golden chroma slice bounds below are for 4096 FFT
    eng.prepareToPlay (sr, n);
    eng.state().setWindowKind (pitchlab::WindowKind::Hanning);

    std::vector<float> mono (static_cast<std::size_t> (n));
    for (int i = 0; i < n; ++i)
        mono[static_cast<std::size_t> (i)] =
            0.25f * std::sin (2.0f * std::numbers::pi_v<float> * f0 * static_cast<float> (i) / static_cast<float> (sr));

    const float* ch[] = { mono.data() };
    eng.processAudioInterleaved (ch, 1, n);

    EXPECT_NEAR(eng.state().currentHz, f0, 6.0f);
    EXPECT_LT(std::abs (eng.state().tuningError), 15.0f);

    std::array<float, 384> row {};
    eng.copyChromaRow384 (std::span<float> { row.data(), row.size() });

    int argmax = 0;
    float mx = row[0];
    for (int i = 1; i < 384; ++i)
    {
        if (row[static_cast<std::size_t> (i)] > mx)
        {
            mx = row[static_cast<std::size_t> (i)];
            argmax = i;
        }
    }

    // Pitch class A (index 9): slices [288, 320)
    EXPECT_GE(argmax, 288);
    EXPECT_LT(argmax, 321);

    // With octave folding into a ~65–131 Hz slice range, 440 Hz (A4) lands on A2's
    // 4× harmonic: f,2f,4f ⇒ dominant harmonic index 2 (mult = 2^2 = 4).
    EXPECT_EQ(eng.state().octaveHarmonicIndex[static_cast<std::size_t> (argmax)], 2u);
}
