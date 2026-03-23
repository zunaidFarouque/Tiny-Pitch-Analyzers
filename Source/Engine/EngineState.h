#pragma once

#include "PitchLabChord.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace pitchlab
{

enum class WindowKind : std::uint8_t
{
    Hanning = 0,
    Gaussian = 1
};

/** Logical fields aligned with New Plan §2.1 offset map (names, not packed binary layout). */
struct EngineState
{
    // Texture / GPU handles (placeholders until OpenGL wiring); roadmap 0x004 / 0x008 style IDs
    std::uint32_t textureIdMain = 0;
    std::uint32_t textureIdSpectrogram = 0;

    // Roadmap listed currentHz at 0x004 in table — here a distinct field for analysis
    float currentHz = 0.0f;
    float tuningError = 0.0f; // roadmap ~0xB08 — cents vs nearest tempered semitone

    int fftSize = 4096;
    double sampleRate = 44100.0;
    int audioBufferSize = 512;

    float viewScrollX = 0.0f;
    float viewHeight = 0.0f;
    float strobePhase = 0.0f;

    /** New Plan §3.4 OctaveBuffer (~0x8a0): per-slice winning harmonic index (0 = f, 1 = 2f, …); 255 = negligible. */
    std::array<std::uint8_t, 384> octaveHarmonicIndex{};

    /** §5.5 / §6.3 — 12×7 chord grid; index = root + type * 12 (see PitchLabChord.h). */
    std::array<float, kChordMatrixFloats> chordProbabilities{};

    /** Roadmap 0xB00 — new analysis frame available for render thread (audio sets, GL may clear). */
    std::atomic<bool> analysisDirty { false };

    /** §3.2 — Hanning vs Gaussian Q24 window (message thread sets, audio thread reads relaxed). */
    std::atomic<std::uint8_t> windowKindRaw { static_cast<std::uint8_t> (WindowKind::Hanning) };

    [[nodiscard]] WindowKind windowKind() const noexcept
    {
        return static_cast<WindowKind> (windowKindRaw.load (std::memory_order_relaxed));
    }

    void setWindowKind (WindowKind w) noexcept
    {
        windowKindRaw.store (static_cast<std::uint8_t> (w), std::memory_order_relaxed);
    }

    void resetAnalysisDisplay();
};

} // namespace pitchlab
