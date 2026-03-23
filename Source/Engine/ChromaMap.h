#pragma once

#include <array>
#include <cstdint>

namespace pitchlab
{

/**
    New Plan §3.4 — Chroma Map (local_5c analogue): start FFT magnitude bin for each
    pitch class 0=C … 11=B at reference octave (MIDI 36 + class).
 */
class ChromaMap
{
public:
    void rebuild (double sampleRate, int fftSize) noexcept;

    [[nodiscard]] int startBin (int pitchClass) const noexcept;
    [[nodiscard]] const std::array<int, 12>& startBins() const noexcept { return startBin_; }

private:
    std::array<int, 12> startBin_ {};
};

} // namespace pitchlab
