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

enum class FoldInterpMode : std::uint8_t
{
    Nearest = 0,
    Linear2Bin = 1,
    Quadratic3Bin = 2
};

enum class FoldHarmonicWeightMode : std::uint8_t
{
    Uniform = 0,
    InvH = 1,
    InvSqrtH = 2
};

enum class WaterfallDisplayCurveMode : std::uint8_t
{
    Linear = 0,
    Sqrt = 1,
    LogDb = 2
};

enum class WaterfallTextureFilterMode : std::uint8_t
{
    Nearest = 0,
    Linear = 1
};

enum class ChromaShapingMode : std::uint8_t
{
    None = 0,
    LogCompress = 1,
    NoiseFloorSubtract = 2,
    PercentileGate = 3
};

/** Document-style octave stack vs all integer harmonics k * f0. */
enum class FoldHarmonicModel : std::uint8_t
{
    OctaveStack_Doc_v1 = 0,
    IntegerHarmonics_v0_2 = 1
};

enum class SpectralBackendMode : std::uint8_t
{
    STFT_v1_0 = 0,
    ConstQApprox_v0_1 = 1,
    VariableQApprox_v0_1 = 2,
    MultiResSTFT_v1_0 = 3
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

    int fftSize = 8192;
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
    std::atomic<std::uint8_t> windowKindRaw { static_cast<std::uint8_t> (WindowKind::Gaussian) };

    /** AGC controls. */
    std::atomic<bool> agcEnabled { true };
    std::atomic<float> agcStrength { 1.0f };
    /** Peak abs int16 below this (exclusive) → zero window (squelch); <=0 disables gate. */
    std::atomic<int> agcNoiseFloor { 150 };

    /** Leaky-peak chroma trail: multiply prior row by this before max with new frame (0..1). */
    std::atomic<float> temporalRelease { 0.85f };

    /** Waterfall chroma shading (per-bin lerp low..high across 384 bins); hosts use energy=1, alpha=1. */
    std::atomic<float> wEnergyLow { 1.0f };
    std::atomic<float> wEnergyHigh { 1.0f };
    std::atomic<float> wAlphaPowLow { 2.0f };
    std::atomic<float> wAlphaPowHigh { 2.0f };

    /** Map effective Hz (20..~15k) to W-Energy/W-AlphaPow lerp: 0=linear in Hz, 1=log10(Hz) (default). */
    std::atomic<float> wShapingFreqLogBlend { 1.0f };

    /** 1st-order HPF on FFT float input: y[n]=x[n]-0.95*x[n-1]. */
    std::atomic<bool> enablePreEmphasis { false };

    /** Const-Q / Var-Q magnitude smear along FFT bins; off copies through like STFT. */
    std::atomic<bool> spectralSmearingEnabled { true };

    /** Chroma fold controls. */
    std::atomic<int> foldMaxOctaves { 0 }; // <=0 means auto (to Nyquist)
    std::atomic<std::uint8_t> foldInterpModeRaw { static_cast<std::uint8_t> (FoldInterpMode::Linear2Bin) };
    std::atomic<std::uint8_t> foldHarmonicWeightModeRaw { static_cast<std::uint8_t> (FoldHarmonicWeightMode::Uniform) };

    /** Waterfall display/render controls (shared between UI and renderers). */
    std::atomic<std::uint8_t> waterfallDisplayCurveModeRaw { static_cast<std::uint8_t> (WaterfallDisplayCurveMode::Linear) };
    std::atomic<std::uint8_t> waterfallTextureFilterModeRaw { static_cast<std::uint8_t> (WaterfallTextureFilterMode::Nearest) };

    std::atomic<std::uint8_t> chromaShapingModeRaw { static_cast<std::uint8_t> (ChromaShapingMode::NoiseFloorSubtract) };
    std::atomic<std::uint8_t> foldHarmonicModelRaw { static_cast<std::uint8_t> (FoldHarmonicModel::OctaveStack_Doc_v1) };
    std::atomic<std::uint8_t> spectralBackendModeRaw { static_cast<std::uint8_t> (SpectralBackendMode::STFT_v1_0) };

    /** Analysis decimation: run chain every N audio callbacks (1 = every buffer). */
    std::atomic<int> analysisEveryNCallbacks { 1 };

    /** High-pass cutoff Hz; values below kHighPassOffHz disable filtering. */
    std::atomic<float> highPassCutoffHz { 0.0f };
    static constexpr float kHighPassOffHz = 8.0f;

    /** Max harmonic index K for integer-harmonic fold (k = 1 .. K). */
    std::atomic<int> maxHarmonicK { 48 };

    [[nodiscard]] WindowKind windowKind() const noexcept
    {
        return static_cast<WindowKind> (windowKindRaw.load (std::memory_order_relaxed));
    }

    void setWindowKind (WindowKind w) noexcept
    {
        windowKindRaw.store (static_cast<std::uint8_t> (w), std::memory_order_relaxed);
    }

    [[nodiscard]] FoldInterpMode foldInterpMode() const noexcept
    {
        return static_cast<FoldInterpMode> (foldInterpModeRaw.load (std::memory_order_relaxed));
    }

    void setFoldInterpMode (FoldInterpMode m) noexcept
    {
        foldInterpModeRaw.store (static_cast<std::uint8_t> (m), std::memory_order_relaxed);
    }

    [[nodiscard]] FoldHarmonicWeightMode foldHarmonicWeightMode() const noexcept
    {
        return static_cast<FoldHarmonicWeightMode> (foldHarmonicWeightModeRaw.load (std::memory_order_relaxed));
    }

    void setFoldHarmonicWeightMode (FoldHarmonicWeightMode m) noexcept
    {
        foldHarmonicWeightModeRaw.store (static_cast<std::uint8_t> (m), std::memory_order_relaxed);
    }

    [[nodiscard]] WaterfallDisplayCurveMode waterfallDisplayCurveMode() const noexcept
    {
        return static_cast<WaterfallDisplayCurveMode> (waterfallDisplayCurveModeRaw.load (std::memory_order_relaxed));
    }

    void setWaterfallDisplayCurveMode (WaterfallDisplayCurveMode m) noexcept
    {
        waterfallDisplayCurveModeRaw.store (static_cast<std::uint8_t> (m), std::memory_order_relaxed);
    }

    [[nodiscard]] WaterfallTextureFilterMode waterfallTextureFilterMode() const noexcept
    {
        return static_cast<WaterfallTextureFilterMode> (waterfallTextureFilterModeRaw.load (std::memory_order_relaxed));
    }

    void setWaterfallTextureFilterMode (WaterfallTextureFilterMode m) noexcept
    {
        waterfallTextureFilterModeRaw.store (static_cast<std::uint8_t> (m), std::memory_order_relaxed);
    }

    [[nodiscard]] ChromaShapingMode chromaShapingMode() const noexcept
    {
        return static_cast<ChromaShapingMode> (chromaShapingModeRaw.load (std::memory_order_relaxed));
    }

    void setChromaShapingMode (ChromaShapingMode m) noexcept
    {
        chromaShapingModeRaw.store (static_cast<std::uint8_t> (m), std::memory_order_relaxed);
    }

    [[nodiscard]] FoldHarmonicModel foldHarmonicModel() const noexcept
    {
        return static_cast<FoldHarmonicModel> (foldHarmonicModelRaw.load (std::memory_order_relaxed));
    }

    void setFoldHarmonicModel (FoldHarmonicModel m) noexcept
    {
        foldHarmonicModelRaw.store (static_cast<std::uint8_t> (m), std::memory_order_relaxed);
    }

    [[nodiscard]] SpectralBackendMode spectralBackendMode() const noexcept
    {
        return static_cast<SpectralBackendMode> (spectralBackendModeRaw.load (std::memory_order_relaxed));
    }

    void setSpectralBackendMode (SpectralBackendMode m) noexcept
    {
        spectralBackendModeRaw.store (static_cast<std::uint8_t> (m), std::memory_order_relaxed);
    }

    void resetAnalysisDisplay();
};

} // namespace pitchlab
