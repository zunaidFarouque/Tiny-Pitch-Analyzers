#pragma once

#include <cstdint>

namespace pitchlab
{

/** Logical fields aligned with New Plan §2.1 offset map (names, not packed binary layout). */
struct EngineState
{
    // Texture / GPU handles (placeholders until OpenGL wiring); roadmap 0x004 / 0x008 style IDs
    std::uint32_t textureIdMain = 0;
    std::uint32_t textureIdSpectrogram = 0;

    // Roadmap listed currentHz at 0x004 in table — here a distinct field for analysis
    float currentHz = 0.0f;
    float tuningError = 0.0f; // roadmap ~0xB08

    int fftSize = 4096;
    double sampleRate = 44100.0;
    int audioBufferSize = 512;

    float viewScrollX = 0.0f;
    float viewHeight = 0.0f;
    float strobePhase = 0.0f;

    /** Roadmap 0xB00 — new analysis frame available for render thread */
    bool analysisDirty = false;

    void resetAnalysisDisplay();
};

} // namespace pitchlab
