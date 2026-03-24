#pragma once

#include "ChromaMap.h"
#include "CircularInt16Buffer.h"
#include "EngineState.h"
#include "RenderFrameData.h"

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <vector>

namespace pitchlab
{

class FftMagnitudes;
class StaticTables;

const char* engineVersionString() noexcept;

/** Facade: prepare / reset / process from audio thread (New Plan ProcessAudio path, Phase 1 ingress only). */
class PitchLabEngine
{
public:
    PitchLabEngine();
    ~PitchLabEngine();

    void prepareToPlay (double sampleRate, int maxBlockSamples);
    /** Rebuild FFT/tables/chroma mapping for a new FFT size.
        Call from UI thread; safe against the audio thread via internal locking. */
    void reconfigureFftSize (int newFftSize, double sampleRate, int maxBlockSamples);
    void reset();

    /** Non-interleaved float samples from JUCE callback; downmixes to mono int16 ingress. */
    void processAudioInterleaved (const float* const* channelData, int numChannels, int numSamples);

    /** Copy latest ingress samples for visualization (audio thread). */
    void copyIngressLatest (std::span<std::int16_t> dst) const noexcept;

    /** Latest 384-bin chroma row after FFT fold (audio thread). */
    void copyChromaRow384 (std::span<float> dst) const noexcept;
    /** Thread-safe immutable render snapshot for UI/renderer threads. */
    void copyLatestRenderFrame (RenderFrameData& dst) const noexcept;

    [[nodiscard]] const EngineState& state() const noexcept { return state_; }
    [[nodiscard]] EngineState& state() noexcept { return state_; }
    [[nodiscard]] const StaticTables* tables() const noexcept { return tables_.get(); }
    [[nodiscard]] const CircularInt16Buffer& ingressBuffer() const noexcept { return ingress_; }

private:
    void runAnalysisChain() noexcept;
    void prepareToPlayImpl (double sampleRate, int maxBlockSamples);

    EngineState state_;
    CircularInt16Buffer ingress_;
    std::vector<std::int16_t> conversionScratch_;
    std::unique_ptr<StaticTables> tables_;
    std::unique_ptr<FftMagnitudes> fft_;
    ChromaMap chromaMap_;

    std::vector<std::int16_t> timeWindow_;
    std::vector<std::int16_t> windowed_;
    std::vector<float> floatFftIn_;
    std::vector<float> magSpectrum_;
    std::array<float, 384> chromaRow_ {};
    mutable std::mutex frameMutex_;
    RenderFrameData latestFrame_{};
    std::uint64_t frameSequence_ = 0;

    static constexpr std::size_t kIngressCapacity = 16384;

    // Hot path readers (audio thread) take shared locks; reconfiguration takes exclusive.
    mutable std::shared_mutex configMutex_;
};

} // namespace pitchlab
