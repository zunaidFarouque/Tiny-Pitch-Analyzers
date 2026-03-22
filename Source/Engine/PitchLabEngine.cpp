#include "PitchLabEngine.h"

#include "FloatIngress.h"
#include "StaticTables.h"

#include <algorithm>
#include <span>

namespace pitchlab
{

const char* engineVersionString() noexcept
{
    return "0.2.0-phase1";
}

PitchLabEngine::PitchLabEngine()
    : ingress_ (kIngressCapacity)
{
}

PitchLabEngine::~PitchLabEngine() = default;

void PitchLabEngine::prepareToPlay (double sampleRate, int maxBlockSamples)
{
    state_.sampleRate = sampleRate;
    state_.audioBufferSize = std::max (1, maxBlockSamples);
    conversionScratch_.resize (static_cast<std::size_t> (std::max (1, maxBlockSamples)));
    tables_ = std::make_unique<StaticTables> (state_.fftSize);
}

void PitchLabEngine::reset()
{
    ingress_.reset();
    state_.resetAnalysisDisplay();
    state_.viewScrollX = 0.0f;
    state_.strobePhase = 0.0f;
}

void PitchLabEngine::processAudioInterleaved (const float* const* channelData, int numChannels, int numSamples)
{
    if (numSamples <= 0 || conversionScratch_.size() < static_cast<std::size_t> (numSamples))
        return;

    const std::span<std::int16_t> dst { conversionScratch_.data(), static_cast<std::size_t> (numSamples) };
    convertDeinterleavedToInt16Mono (channelData, numChannels, numSamples, dst);
    ingress_.push (std::span<const std::int16_t> { conversionScratch_.data(), static_cast<std::size_t> (numSamples) });
    state_.analysisDirty = true;
}

} // namespace pitchlab
