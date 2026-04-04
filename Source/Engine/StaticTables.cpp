#include "StaticTables.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace pitchlab
{

namespace
{
    constexpr double kPi = 3.14159265358979323846264338327950288;

    constexpr int kQ24Shift = 24;
    constexpr float kQ24Scale = static_cast<float> (1 << kQ24Shift);

    [[nodiscard]] std::int32_t floatToQ24 (float x)
    {
        const float c = std::round (std::clamp (x, -8.0f, 8.0f) * kQ24Scale);
        return static_cast<std::int32_t> (c);
    }

    /** Single process-wide LUT: linear amplitude index 0..32767 → display byte (log10 curve). */
    [[nodiscard]] const std::array<std::uint8_t, 32768>& globalDbBrightnessLut() noexcept
    {
        static const std::array<std::uint8_t, 32768> table = [] {
            std::array<std::uint8_t, 32768> out {};
            for (std::size_t i = 0; i < out.size(); ++i)
            {
                if (i == 0)
                {
                    out[i] = 0;
                    continue;
                }
                const float amp = static_cast<float> (i) / 32767.0f;
                float db = 20.0f * std::log10 (amp);
                db = std::clamp (db, -100.0f, 0.0f);
                const float t = (db + 100.0f) / 100.0f;
                out[i] = static_cast<std::uint8_t> (std::lround (t * 255.0f));
            }
            return out;
        }();
        return table;
    }
} // namespace

void StaticTables::fillSpectralPalette (std::array<std::uint8_t, 12 * 3>& out)
{
    // Roadmap samples + distinct chromatic strip (C, C#, D, …, B)
    static constexpr std::uint8_t rows[12][3] = {
        { 0xB2, 0xFF, 0x66 }, // C  lime (roadmap)
        { 0x88, 0xFF, 0xCC }, // C#
        { 0x66, 0xE5, 0xFF }, // D
        { 0x66, 0xB3, 0xFF }, // D# / E family (roadmap E)
        { 0x66, 0xB3, 0xFF }, // E  sky blue (roadmap)
        { 0x99, 0x99, 0xFF }, // F
        { 0xCC, 0x88, 0xFF }, // F#
        { 0xFF, 0x66, 0xFF }, // G
        { 0xFF, 0x66, 0x99 }, // G#
        { 0xFF, 0x66, 0x66 }, // A  red (roadmap)
        { 0xFF, 0x99, 0x66 }, // A#
        { 0xFF, 0xCC, 0x66 }, // B
    };
    for (std::size_t i = 0; i < 12; ++i)
    {
        out[i * 3 + 0] = rows[i][0];
        out[i * 3 + 1] = rows[i][1];
        out[i * 3 + 2] = rows[i][2];
    }
}

StaticTables::StaticTables (int fftSize)
{
    fillSpectralPalette (spectralPaletteRgb_);
    (void) globalDbBrightnessLut(); // ensure one-time init before any audio use

    constexpr int kStrobeWidth = 4096;
    constexpr float kStrobeScalar = 2.0f;
    strobeGradient_.resize (static_cast<std::size_t> (kStrobeWidth));
    for (int i = 0; i < kStrobeWidth; ++i)
    {
        const float x = static_cast<float> (i) / static_cast<float> (kStrobeWidth);
        const float val = ((x + 0.5f) * kStrobeScalar - 1.0f);
        const float sq = val * val;
        strobeGradient_[static_cast<std::size_t> (i)] = static_cast<std::uint8_t> (std::clamp (sq * 255.0f, 0.0f, 255.0f));
    }

    const int n = std::max (2, fftSize);
    hanningQ24_.resize (static_cast<std::size_t> (n));
    gaussianQ24_.resize (static_cast<std::size_t> (n));

    for (int i = 0; i < n; ++i)
    {
        const double phase = static_cast<double> (i) / static_cast<double> (n - 1);
        const float han = static_cast<float> (0.5 * (1.0 - std::cos (2.0 * kPi * phase)));
        hanningQ24_[static_cast<std::size_t> (i)] = floatToQ24 (han);

        const double xg = (phase - 0.5) * 6.0;
        const float gauss = static_cast<float> (std::exp (-xg * xg));
        gaussianQ24_[static_cast<std::size_t> (i)] = floatToQ24 (gauss);
    }
}

std::uint8_t StaticTables::dbBrightness (std::size_t amplitudeIndex) const noexcept
{
    const auto& lut = globalDbBrightnessLut();
    if (amplitudeIndex >= lut.size())
        return 0;
    return lut[amplitudeIndex];
}

void StaticTables::fillDisplayChromaFromLinear384 (std::span<const float> linear384,
                                                   std::span<float> outDisplay384) const noexcept
{
    if (linear384.size() < 384 || outDisplay384.size() < 384)
        return;

    float mx = 0.0f;
    for (int i = 0; i < 384; ++i)
        mx = std::max (mx, linear384[static_cast<std::size_t> (i)]);

    constexpr float kEps = 1.0e-12f;
    const float gain = (mx > kEps) ? (32767.0f / mx) : 0.0f;

    for (int i = 0; i < 384; ++i)
    {
        if (gain <= 0.0f)
        {
            outDisplay384[static_cast<std::size_t> (i)] = 0.0f;
            continue;
        }

        const float v = linear384[static_cast<std::size_t> (i)] * gain;
        const int idx = static_cast<int> (std::lround (v));
        const int clamped = std::clamp (idx, 0, 32767);
        outDisplay384[static_cast<std::size_t> (i)] =
            static_cast<float> (dbBrightness (static_cast<std::size_t> (clamped))) * (1.0f / 255.0f);
    }
}

std::uint8_t StaticTables::strobe (std::size_t i) const noexcept
{
    if (i >= strobeGradient_.size())
        return 0;
    return strobeGradient_[i];
}

} // namespace pitchlab
