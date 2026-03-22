#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace pitchlab
{

/** New Plan §2.3 — LUTs generated at init (no runtime exp/log in hot path consumers). */
class StaticTables
{
public:
    explicit StaticTables (int fftSize = 4096);

    [[nodiscard]] std::size_t dbBrightnessSize() const noexcept { return dbBrightness_.size(); }
    [[nodiscard]] std::uint8_t dbBrightness (std::size_t amplitudeIndex) const noexcept;
    [[nodiscard]] const std::array<std::uint8_t, 12 * 3>& spectralPaletteRgb() const noexcept { return spectralPaletteRgb_; }
    [[nodiscard]] std::size_t strobeSize() const noexcept { return strobeGradient_.size(); }
    [[nodiscard]] std::uint8_t strobe (std::size_t i) const noexcept;
    [[nodiscard]] const std::vector<std::int32_t>& hanningWindowQ24() const noexcept { return hanningQ24_; }
    [[nodiscard]] const std::vector<std::int32_t>& gaussianWindowQ24() const noexcept { return gaussianQ24_; }

private:
    std::vector<std::uint8_t> dbBrightness_;
    std::array<std::uint8_t, 12 * 3> spectralPaletteRgb_{};
    std::vector<std::uint8_t> strobeGradient_;
    std::vector<std::int32_t> hanningQ24_;
    std::vector<std::int32_t> gaussianQ24_;

    static void fillSpectralPalette (std::array<std::uint8_t, 12 * 3>& out);
};

} // namespace pitchlab
