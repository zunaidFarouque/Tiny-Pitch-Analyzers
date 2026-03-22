#pragma once

#include "CircularInt16Buffer.h"
#include "EngineState.h"

#include <memory>
#include <vector>

namespace pitchlab
{

class StaticTables;

const char* engineVersionString() noexcept;

/** Facade: prepare / reset / process from audio thread (New Plan ProcessAudio path, Phase 1 ingress only). */
class PitchLabEngine
{
public:
    PitchLabEngine();
    ~PitchLabEngine();

    void prepareToPlay (double sampleRate, int maxBlockSamples);
    void reset();

    /** Non-interleaved float samples from JUCE callback; downmixes to mono int16 ingress. */
    void processAudioInterleaved (const float* const* channelData, int numChannels, int numSamples);

    [[nodiscard]] const EngineState& state() const noexcept { return state_; }
    [[nodiscard]] EngineState& state() noexcept { return state_; }
    [[nodiscard]] const StaticTables* tables() const noexcept { return tables_.get(); }
    [[nodiscard]] const CircularInt16Buffer& ingressBuffer() const noexcept { return ingress_; }

private:
    EngineState state_;
    CircularInt16Buffer ingress_;
    std::vector<std::int16_t> conversionScratch_;
    std::unique_ptr<StaticTables> tables_;

    static constexpr std::size_t kIngressCapacity = 16384;
};

} // namespace pitchlab
