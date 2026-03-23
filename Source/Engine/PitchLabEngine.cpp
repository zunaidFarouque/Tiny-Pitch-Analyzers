#include "PitchLabEngine.h"

#include "AgcInt16.h"
#include "ChromaFolder.h"
#include "ChromaMap.h"
#include "FftMagnitudes.h"
#include "FloatIngress.h"
#include "PitchFromSpectrum.h"
#include "PitchLabChord.h"
#include "StaticTables.h"
#include "WindowApplyQ24.h"

#include <algorithm>
#include <cmath>
#include <span>

namespace pitchlab
{

namespace
{
[[nodiscard]] int fftOrderForSize (int n) noexcept
{
    if (n <= 0)
        return -1;

    for (int o = 1; o < 30; ++o)
        if ((1 << o) == n)
            return o;

    return -1;
}
} // namespace

const char* engineVersionString() noexcept
{
    return "0.3.0-phase2";
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

    const int ord = fftOrderForSize (state_.fftSize);
    if (ord > 0)
        fft_ = std::make_unique<FftMagnitudes> (ord);
    else
        fft_.reset();

    const int n = state_.fftSize;
    timeWindow_.assign (static_cast<std::size_t> (n), static_cast<std::int16_t> (0));
    windowed_.assign (static_cast<std::size_t> (n), static_cast<std::int16_t> (0));
    floatFftIn_.assign (static_cast<std::size_t> (n), 0.0f);
    magSpectrum_.assign (static_cast<std::size_t> (n / 2 + 1), 0.0f);
    chromaRow_.fill (0.0f);
    chromaMap_.rebuild (sampleRate, n);
    {
        const std::scoped_lock lk (frameMutex_);
        latestFrame_ = RenderFrameData {};
        frameSequence_ = 0;
    }
}

void PitchLabEngine::reset()
{
    ingress_.reset();
    state_.resetAnalysisDisplay();
    state_.viewScrollX = 0.0f;
    state_.strobePhase = 0.0f;
    chromaRow_.fill (0.0f);
    {
        const std::scoped_lock lk (frameMutex_);
        latestFrame_ = RenderFrameData {};
        frameSequence_ = 0;
    }
}

void PitchLabEngine::runAnalysisChain() noexcept
{
    if (fft_ == nullptr || tables_ == nullptr)
        return;

    const int n = state_.fftSize;
    const std::vector<std::int32_t>& win = (state_.windowKind() == WindowKind::Gaussian)
                                               ? tables_->gaussianWindowQ24()
                                               : tables_->hanningWindowQ24();
    if ((int) win.size() < n)
        return;

    ingress_.copyLatestInto (std::span<std::int16_t> { timeWindow_.data(), static_cast<std::size_t> (n) });

    applyAgcInt16InPlace (std::span<std::int16_t> { timeWindow_.data(), static_cast<std::size_t> (n) });

    applyHanningWindowQ24 (std::span<const std::int16_t> { timeWindow_.data(), static_cast<std::size_t> (n) },
                           std::span<std::int16_t> { windowed_.data(), static_cast<std::size_t> (n) },
                           std::span<const std::int32_t> { win.data(), static_cast<std::size_t> (n) });

    for (int i = 0; i < n; ++i)
        floatFftIn_[static_cast<std::size_t> (i)] = static_cast<float> (windowed_[static_cast<std::size_t> (i)]) * (1.0f / 32768.0f);

    fft_->computePowerSpectrum (std::span<const float> { floatFftIn_.data(), floatFftIn_.size() },
                                std::span<float> { magSpectrum_.data(), magSpectrum_.size() });

    foldToChroma384 (chromaMap_,
                     state_.sampleRate,
                     n,
                     std::span<const float> { magSpectrum_.data(), magSpectrum_.size() },
                     std::span<float> { chromaRow_.data(), chromaRow_.size() },
                     std::span<std::uint8_t> { state_.octaveHarmonicIndex.data(), state_.octaveHarmonicIndex.size() });

    const float peakBin = refinedPeakBin (std::span<const float> { magSpectrum_.data(), magSpectrum_.size() });
    state_.currentHz = binToHz (static_cast<double> (peakBin), state_.sampleRate, n);
    state_.tuningError = centsVsTempered (state_.currentHz);

    fillChordProbabilitiesFromChroma384 (std::span<const float> { chromaRow_.data(), chromaRow_.size() },
                                         std::span<float> { state_.chordProbabilities.data(), state_.chordProbabilities.size() });
    applyAntiChordPenaltyInPlace (std::span<float> { state_.chordProbabilities.data(), state_.chordProbabilities.size() });

    constexpr float twoPi = 6.2831855f;
    const float hop = static_cast<float> (std::max (1, state_.audioBufferSize));
    state_.strobePhase = std::fmod (state_.strobePhase + twoPi * state_.currentHz * hop / static_cast<float> (state_.sampleRate), twoPi);

    RenderFrameData snap;
    ingress_.copyLatestInto (std::span<std::int16_t> { snap.waveform.data(), snap.waveform.size() });
    std::copy (chromaRow_.begin(), chromaRow_.end(), snap.chromaRow.begin());
    std::copy (state_.chordProbabilities.begin(), state_.chordProbabilities.end(), snap.chordProbabilities.begin());
    snap.currentHz = state_.currentHz;
    snap.tuningError = state_.tuningError;
    snap.strobePhase = state_.strobePhase;
    snap.sequence = ++frameSequence_;

    const std::scoped_lock lk (frameMutex_);
    latestFrame_ = snap;
}

void PitchLabEngine::processAudioInterleaved (const float* const* channelData, int numChannels, int numSamples)
{
    if (numSamples <= 0 || conversionScratch_.size() < static_cast<std::size_t> (numSamples))
        return;

    const std::span<std::int16_t> dst { conversionScratch_.data(), static_cast<std::size_t> (numSamples) };
    convertDeinterleavedToInt16Mono (channelData, numChannels, numSamples, dst);
    ingress_.push (std::span<const std::int16_t> { conversionScratch_.data(), static_cast<std::size_t> (numSamples) });
    state_.analysisDirty.store (true, std::memory_order_release);

    runAnalysisChain();
}

void PitchLabEngine::copyIngressLatest (std::span<std::int16_t> dst) const noexcept
{
    ingress_.copyLatestInto (dst);
}

void PitchLabEngine::copyChromaRow384 (std::span<float> dst) const noexcept
{
    const int n = (std::min) (static_cast<int> (dst.size()), 384);
    for (int i = 0; i < n; ++i)
        dst[static_cast<std::size_t> (i)] = chromaRow_[static_cast<std::size_t> (i)];
}

void PitchLabEngine::copyLatestRenderFrame (RenderFrameData& dst) const noexcept
{
    const std::scoped_lock lk (frameMutex_);
    dst = latestFrame_;
}

} // namespace pitchlab
