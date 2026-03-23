#pragma once

#include "PitchLabChord.h"

#include <array>
#include <cstdint>

namespace pitchlab
{

/** Immutable render snapshot transferred from analysis/audio side to renderer side. */
struct RenderFrameData
{
    static constexpr int kWaveformSamples = 2048;

    std::array<std::int16_t, kWaveformSamples> waveform {};
    std::array<float, 384> chromaRow {};
    std::array<float, kChordMatrixFloats> chordProbabilities {};
    float currentHz = 0.0f;
    float tuningError = 0.0f;
    float strobePhase = 0.0f;
    int waterfallWriteY = 0;
    std::uint64_t sequence = 0;
};

} // namespace pitchlab

