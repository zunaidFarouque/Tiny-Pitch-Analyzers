#include <gtest/gtest.h>

#include "FftMagnitudes.h"
#include "PolyphonicPeakPicker.h"
#include "PitchFromSpectrum.h"

#include <cmath>
#include <numbers>
#include <span>
#include <vector>

namespace
{
constexpr double kSr = 44100.0;
constexpr int kVirtualFft = 4096;
constexpr int kBins = kVirtualFft / 2 + 1;

void addGaussianPeak (std::vector<float>& mag, int centerBin, float height, float widthBins)
{
    for (int b = 0; b < static_cast<int> (mag.size()); ++b)
    {
        const float d = static_cast<float> (b - centerBin) / widthBins;
        mag[static_cast<std::size_t> (b)] += height * std::exp (-d * d);
    }
}
} // namespace

TEST (PolyphonicPeakPicker, ThreeDistinctPeaks)
{
    std::vector<float> mag (static_cast<std::size_t> (kBins), 0.02f);
    addGaussianPeak (mag, 80, 1.0f, 4.0f);
    addGaussianPeak (mag, 320, 0.85f, 5.0f);
    addGaussianPeak (mag, 900, 0.7f, 6.0f);

    std::vector<pitchlab::PitchPeak> scratch;
    scratch.reserve (mag.size());
    std::vector<pitchlab::PitchPeak> out;
    out.reserve (static_cast<std::size_t> (pitchlab::PitchPeak::kMaxPeaksCap));

    pitchlab::extractPeaks (std::span<const float> { mag.data(), mag.size() },
                            kSr,
                            kVirtualFft,
                            0.05f,
                            0.08f,
                            32,
                            scratch,
                            out);

    ASSERT_EQ (out.size(), 3u);

    const float e0 = pitchlab::binToHz (80.0, kSr, kVirtualFft);
    const float e1 = pitchlab::binToHz (320.0, kSr, kVirtualFft);
    const float e2 = pitchlab::binToHz (900.0, kSr, kVirtualFft);

    std::vector<float> gotHz;
    gotHz.reserve (3);
    for (const auto& p : out)
        gotHz.push_back (p.frequencyHz);

    auto nearAny = [&gotHz] (float target, float tol) {
        for (float g : gotHz)
            if (std::abs (g - target) <= tol)
                return true;
        return false;
    };

    const float tol = 12.0f;
    EXPECT_TRUE (nearAny (e0, tol));
    EXPECT_TRUE (nearAny (e1, tol));
    EXPECT_TRUE (nearAny (e2, tol));
}

TEST (PolyphonicPeakPicker, LowProminenceShoulderRejected)
{
    std::vector<float> mag (static_cast<std::size_t> (256), 0.05f);

    mag[48] = 0.15f;
    mag[49] = 0.88f;
    mag[50] = 0.50f;
    mag[51] = 0.55f;
    mag[52] = 0.65f;
    mag[53] = 0.78f;
    mag[54] = 0.92f;
    mag[55] = 1.0f;
    mag[56] = 0.35f;

    std::vector<pitchlab::PitchPeak> scratch;
    scratch.reserve (mag.size());
    std::vector<pitchlab::PitchPeak> out;
    out.reserve (static_cast<std::size_t> (pitchlab::PitchPeak::kMaxPeaksCap));

    constexpr int vfft = 512;
    pitchlab::extractPeaks (std::span<const float> { mag.data(), mag.size() },
                            kSr,
                            vfft,
                            0.3f,
                            0.39f,
                            8,
                            scratch,
                            out);

    ASSERT_EQ (out.size(), 1u);
    EXPECT_GT (out[0].frequencyHz, pitchlab::binToHz (52.0, kSr, vfft));
}

/** Real JUCE FFT magnitudes are not 0..1; defaults must still find a 440 Hz tone (STFT path). */
TEST (PolyphonicPeakPicker, StftMagnitudeScaleWithDefaultEngineKnobs)
{
    constexpr double sr = 44100.0;
    constexpr int n = 4096;
    constexpr float f0 = 440.0f;

    std::vector<float> time (static_cast<std::size_t> (n));
    for (int i = 0; i < n; ++i)
        time[static_cast<std::size_t> (i)] =
            0.25f * std::sin (2.0f * std::numbers::pi_v<float> * f0 * static_cast<float> (i) / static_cast<float> (sr));

    pitchlab::FftMagnitudes fft (12);
    std::vector<float> mag (static_cast<std::size_t> (n / 2 + 1));
    fft.computePowerSpectrum (std::span<const float> { time.data(), time.size() },
                              std::span<float> { mag.data(), mag.size() });

    std::vector<pitchlab::PitchPeak> scratch;
    scratch.reserve (mag.size());
    std::vector<pitchlab::PitchPeak> out;
    out.reserve (static_cast<std::size_t> (pitchlab::PitchPeak::kMaxPeaksCap));

    pitchlab::extractPeaks (std::span<const float> { mag.data(), mag.size() },
                            sr,
                            n,
                            0.05f,
                            0.1f,
                            32,
                            scratch,
                            out);

    ASSERT_GE (out.size(), 1u);
    EXPECT_NEAR (out[0].frequencyHz, f0, 18.0f);
}
