#include "PitchLabEngine.h"

#include "AgcInt16.h"
#include "ChromaFolder.h"
#include "ChromaMap.h"
#include "ChromaPostProcess.h"
#include "FftMagnitudes.h"
#include "FloatIngress.h"
#include "MultiResSpectrumStitch.h"
#include "PitchFromSpectrum.h"
#include "PitchLabChord.h"
#include "SpectralMagSmear.h"
#include "StaticTables.h"
#include "WindowApplyQ24.h"

#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

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

void PitchLabEngine::runFftOnChain (AnalysisChain& ch,
                                  juce::dsp::IIR::Filter<float>& hpFilter,
                                  float& lastHpCut,
                                  double& lastHpSr) noexcept
{
    if (ch.fft_ == nullptr || ch.tables_ == nullptr)
        return;

    const int n = ch.fftSize_;
    const std::vector<std::int32_t>& win = (state_.windowKind() == WindowKind::Gaussian)
                                               ? ch.tables_->gaussianWindowQ24()
                                               : ch.tables_->hanningWindowQ24();
    if ((int) win.size() < n)
        return;

    ch.ingress_.copyLatestInto (std::span<std::int16_t> { ch.timeWindow_.data(), static_cast<std::size_t> (n) });

    const bool agcOn = state_.agcEnabled.load (std::memory_order_relaxed);
    const float agcK = state_.agcStrength.load (std::memory_order_relaxed);
    auto winSpan = std::span<std::int16_t> { ch.timeWindow_.data(), static_cast<std::size_t> (n) };
    if (agcOn && agcK >= 0.999f)
        applyAgcInt16InPlace (winSpan);
    else
        applyAgcInt16InPlace (winSpan, agcOn, agcK);

    applyHanningWindowQ24 (std::span<const std::int16_t> { ch.timeWindow_.data(), static_cast<std::size_t> (n) },
                           std::span<std::int16_t> { ch.windowed_.data(), static_cast<std::size_t> (n) },
                           std::span<const std::int32_t> { win.data(), static_cast<std::size_t> (n) });

    for (int i = 0; i < n; ++i)
        ch.floatFftIn_[static_cast<std::size_t> (i)] =
            static_cast<float> (ch.windowed_[static_cast<std::size_t> (i)]) * (1.0f / 32768.0f);

    const float hpCut = state_.highPassCutoffHz.load (std::memory_order_relaxed);
    if (hpCut >= EngineState::kHighPassOffHz && ch.sampleRate_ > 0.0)
    {
        if (hpCut != lastHpCut || ch.sampleRate_ != lastHpSr)
        {
            lastHpCut = hpCut;
            lastHpSr = ch.sampleRate_;
            hpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (
                static_cast<float> (ch.sampleRate_), hpCut, 0.707f);
        }
        float* line = ch.floatFftIn_.data();
        juce::dsp::AudioBlock<float> block (&line, 1, static_cast<size_t> (n));
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        hpFilter.process (ctx);
    }

    ch.fft_->computePowerSpectrum (std::span<const float> { ch.floatFftIn_.data(), ch.floatFftIn_.size() },
                                   std::span<float> { ch.magSpectrum_.data(), ch.magSpectrum_.size() });
}

PitchLabEngine::AnalysisChain::AnalysisChain()
    : ingress_ (kIngressCapacity)
{
}

void PitchLabEngine::AnalysisChain::prepare (int fftSize, double sampleRate, int maxBlockSamples)
{
    juce::ignoreUnused (maxBlockSamples);
    fftSize_ = fftSize;
    sampleRate_ = sampleRate;

    const int ord = fftOrderForSize (fftSize);
    if (fftSize <= 0 || ord <= 0)
    {
        tables_.reset();
        fft_.reset();
        timeWindow_.clear();
        windowed_.clear();
        floatFftIn_.clear();
        magSpectrum_.clear();
        return;
    }

    tables_ = std::make_unique<StaticTables> (fftSize);
    fft_ = std::make_unique<FftMagnitudes> (ord);

    const int n = fftSize;
    timeWindow_.assign (static_cast<std::size_t> (n), static_cast<std::int16_t> (0));
    windowed_.assign (static_cast<std::size_t> (n), static_cast<std::int16_t> (0));
    floatFftIn_.assign (static_cast<std::size_t> (n), 0.0f);
    magSpectrum_.assign (static_cast<std::size_t> (n / 2 + 1), 0.0f);
}

const char* engineVersionString() noexcept
{
    return "0.3.0-phase2";
}

PitchLabEngine::PitchLabEngine() = default;

PitchLabEngine::~PitchLabEngine() = default;

int PitchLabEngine::lfFftSizeForPrepare (int hfFftSize) const noexcept
{
    if (hfFftSize <= 0)
        return 0;

    return std::min (hfFftSize, 4096);
}

int PitchLabEngine::offlineMonoInputSampleCount() const noexcept
{
    const int hfN = state_.fftSize;
    if (hfN <= 0)
        return 0;
    if (state_.spectralBackendMode() != SpectralBackendMode::MultiResSTFT_v1_0)
        return hfN;
    const int lfN = lfFftSizeForPrepare (hfN);
    constexpr int kWarmup = 1024;
    return hfN + 4 * lfN + kWarmup;
}

void PitchLabEngine::prepareToPlay (double sampleRate, int maxBlockSamples)
{
    std::unique_lock lk (configMutex_);
    prepareToPlayImpl (sampleRate, maxBlockSamples);
}

void PitchLabEngine::prepareToPlayImpl (double sampleRate, int maxBlockSamples)
{
    state_.sampleRate = sampleRate;
    state_.audioBufferSize = std::max (1, maxBlockSamples);
    const int scratchSamples = std::max ({ 1, maxBlockSamples, state_.fftSize });
    conversionScratch_.resize (static_cast<std::size_t> (scratchSamples));

    const int hfN = state_.fftSize;
    const int lfN = lfFftSizeForPrepare (hfN);
    const bool multiRes = (state_.spectralBackendMode() == SpectralBackendMode::MultiResSTFT_v1_0);
    const int virtualN = multiRes ? (hfN * 4) : hfN;

    hfChain_.prepare (hfN, sampleRate, maxBlockSamples);
    lfChain_.prepare (lfN, sampleRate / 4.0, std::max (1, maxBlockSamples / 4 + 1));

    decimator_.prepare (sampleRate);

    const std::size_t maxB = static_cast<std::size_t> (scratchSamples);
    decimatorFloatScratch_.resize (maxB);
    const std::size_t decOutCap = maxB / 4 + 2;
    decimatorFloatOutScratch_.resize (std::max (std::size_t { 1 }, decOutCap));
    decimatorInt16Scratch_.resize (std::max (std::size_t { 1 }, decOutCap));

    if (hfN > 0 && fftOrderForSize (hfN) > 0)
        magForFold_.assign (static_cast<std::size_t> (virtualN / 2 + 1), 0.0f);
    else
        magForFold_.clear();

    chromaRow_.fill (0.0f);
    chromaMap_.rebuild (sampleRate, virtualN);

    juce::dsp::ProcessSpec hpSpec;
    hpSpec.sampleRate = sampleRate;
    hpSpec.maximumBlockSize = (juce::uint32) std::max (maxBlockSamples, hfN);
    hpSpec.numChannels = 1;
    highPassFilter_.prepare (hpSpec);
    highPassFilter_.reset();

    juce::dsp::ProcessSpec lfHpSpec;
    lfHpSpec.sampleRate = sampleRate / 4.0;
    lfHpSpec.maximumBlockSize =
        (juce::uint32) std::max (std::max (1, maxBlockSamples / 4 + 1), lfN);
    lfHpSpec.numChannels = 1;
    lfHighPassFilter_.prepare (lfHpSpec);
    lfHighPassFilter_.reset();

    lastHighPassCutHz_ = -1.0f;
    lastHighPassSr_ = 0.0;
    lastLfHighPassCutHz_ = -1.0f;
    lastLfHighPassSr_ = 0.0;
    analysisDecimationCounter_ = 0;
    lfAnalysisDecimationCounter_ = 0;
    {
        const std::scoped_lock lk (frameMutex_);
        latestFrame_ = RenderFrameData {};
        frameSequence_ = 0;
    }
}

void PitchLabEngine::reconfigureFftSize (int newFftSize, double sampleRate, int maxBlockSamples)
{
    if (newFftSize <= 0)
        return;

    std::unique_lock lk (configMutex_);
    state_.fftSize = newFftSize;
    prepareToPlayImpl (sampleRate, maxBlockSamples);
}

void PitchLabEngine::reset()
{
    hfChain_.ingress_.reset();
    lfChain_.ingress_.reset();
    decimator_.reset();
    state_.resetAnalysisDisplay();
    state_.viewScrollX = 0.0f;
    state_.strobePhase = 0.0f;
    chromaRow_.fill (0.0f);
    analysisDecimationCounter_ = 0;
    lfAnalysisDecimationCounter_ = 0;
    highPassFilter_.reset();
    lfHighPassFilter_.reset();
    lastHighPassCutHz_ = -1.0f;
    lastHighPassSr_ = 0.0;
    lastLfHighPassCutHz_ = -1.0f;
    lastLfHighPassSr_ = 0.0;
    {
        const std::scoped_lock lk (frameMutex_);
        latestFrame_ = RenderFrameData {};
        frameSequence_ = 0;
    }
}

void PitchLabEngine::runAnalysisChain() noexcept
{
    if (hfChain_.fft_ == nullptr || hfChain_.tables_ == nullptr)
        return;

    const int hfN = hfChain_.fftSize_;
    const SpectralBackendMode backend = state_.spectralBackendMode();
    const int virtualN = (backend == SpectralBackendMode::MultiResSTFT_v1_0) ? (hfN * 4) : hfN;

    if (backend == SpectralBackendMode::MultiResSTFT_v1_0)
    {
        if (lfChain_.fft_ == nullptr || lfChain_.tables_ == nullptr)
            return;

        const int every = std::max (1, state_.analysisEveryNCallbacks.load (std::memory_order_relaxed));
        const int lfEvery = std::max (1, every * 2);
        const bool runLf = lfAnalysisDecimationCounter_ >= lfEvery;
        if (runLf)
            lfAnalysisDecimationCounter_ = 0;

        runFftOnChain (hfChain_, highPassFilter_, lastHighPassCutHz_, lastHighPassSr_);
        if (runLf)
            runFftOnChain (lfChain_, lfHighPassFilter_, lastLfHighPassCutHz_, lastLfHighPassSr_);

        constexpr float crossoverHz = 1000.0f;
        const float lfGain = static_cast<float> (hfN) / static_cast<float> (lfChain_.fftSize_);
        stitchMultiResMagnitudes (std::span<const float> { hfChain_.magSpectrum_.data(), hfChain_.magSpectrum_.size() },
                                  std::span<const float> { lfChain_.magSpectrum_.data(), lfChain_.magSpectrum_.size() },
                                  state_.sampleRate,
                                  state_.sampleRate / 4.0,
                                  hfN,
                                  lfChain_.fftSize_,
                                  virtualN,
                                  crossoverHz,
                                  lfGain,
                                  std::span<float> { magForFold_.data(), magForFold_.size() });
    }
    else
    {
        runFftOnChain (hfChain_, highPassFilter_, lastHighPassCutHz_, lastHighPassSr_);

        buildMagForFold (std::span<const float> { hfChain_.magSpectrum_.data(), hfChain_.magSpectrum_.size() },
                         std::span<float> { magForFold_.data(), magForFold_.size() },
                         state_.sampleRate,
                         hfN,
                         backend);
    }

    FoldToChromaSettings foldSettings;
    foldSettings.interpMode = state_.foldInterpMode();
    foldSettings.harmonicWeightMode = state_.foldHarmonicWeightMode();
    foldSettings.maxOctaves = state_.foldMaxOctaves.load (std::memory_order_relaxed);
    foldSettings.harmonicModel = state_.foldHarmonicModel();
    foldSettings.maxHarmonicK = state_.maxHarmonicK.load (std::memory_order_relaxed);
    foldToChroma384 (chromaMap_,
                     state_.sampleRate,
                     virtualN,
                     std::span<const float> { magForFold_.data(), magForFold_.size() },
                     std::span<float> { chromaRow_.data(), chromaRow_.size() },
                     std::span<std::uint8_t> { state_.octaveHarmonicIndex.data(), state_.octaveHarmonicIndex.size() },
                     foldSettings);

    applyChromaShaping384 (state_.chromaShapingMode(),
                           std::span<float> { chromaRow_.data(), chromaRow_.size() });

    const float peakBin = refinedPeakBin (std::span<const float> { magForFold_.data(), magForFold_.size() });
    state_.currentHz = binToHz (static_cast<double> (peakBin), state_.sampleRate, virtualN);
    state_.tuningError = centsVsTempered (state_.currentHz);

    fillChordProbabilitiesFromChroma384 (std::span<const float> { chromaRow_.data(), chromaRow_.size() },
                                         std::span<float> { state_.chordProbabilities.data(), state_.chordProbabilities.size() });
    applyAntiChordPenaltyInPlace (std::span<float> { state_.chordProbabilities.data(), state_.chordProbabilities.size() });

    constexpr float twoPi = 6.2831855f;
    const float hop = static_cast<float> (std::max (1, state_.audioBufferSize));
    state_.strobePhase = std::fmod (state_.strobePhase + twoPi * state_.currentHz * hop / static_cast<float> (state_.sampleRate), twoPi);

    RenderFrameData snap;
    hfChain_.ingress_.copyLatestInto (std::span<std::int16_t> { snap.waveform.data(), snap.waveform.size() });
    hfChain_.tables_->fillDisplayChromaFromLinear384 (std::span<const float> { chromaRow_.data(), chromaRow_.size() },
                                                      std::span<float> { snap.chromaRow.data(), snap.chromaRow.size() });
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
    std::shared_lock lk (configMutex_);
    if (numSamples <= 0 || conversionScratch_.size() < static_cast<std::size_t> (numSamples))
        return;

    const std::span<std::int16_t> dst { conversionScratch_.data(), static_cast<std::size_t> (numSamples) };
    convertDeinterleavedToInt16Mono (channelData, numChannels, numSamples, dst);
    hfChain_.ingress_.push (std::span<const std::int16_t> { conversionScratch_.data(), static_cast<std::size_t> (numSamples) });

    if (state_.spectralBackendMode() == SpectralBackendMode::MultiResSTFT_v1_0
        && static_cast<std::size_t> (numSamples) <= decimatorFloatScratch_.size ()
        && ! decimatorFloatOutScratch_.empty () && ! decimatorInt16Scratch_.empty ())
    {
        for (int i = 0; i < numSamples; ++i)
            decimatorFloatScratch_[static_cast<std::size_t> (i)] =
                static_cast<float> (conversionScratch_[static_cast<std::size_t> (i)]) * (1.0f / 32768.0f);

        const int nd = decimator_.processBlock (
            std::span<const float> { decimatorFloatScratch_.data(), static_cast<std::size_t> (numSamples) },
            std::span<float> { decimatorFloatOutScratch_.data(), decimatorFloatOutScratch_.size() });

        if (nd > 0)
        {
            convertFloatToInt16Ingress (std::span<const float> { decimatorFloatOutScratch_.data(), static_cast<std::size_t> (nd) },
                                        std::span<std::int16_t> { decimatorInt16Scratch_.data(), static_cast<std::size_t> (nd) });
            lfChain_.ingress_.push (std::span<const std::int16_t> { decimatorInt16Scratch_.data(), static_cast<std::size_t> (nd) });
        }
    }

    state_.analysisDirty.store (true, std::memory_order_release);

    if (state_.spectralBackendMode() == SpectralBackendMode::MultiResSTFT_v1_0)
        ++lfAnalysisDecimationCounter_;

    const int every = std::max (1, state_.analysisEveryNCallbacks.load (std::memory_order_relaxed));
    ++analysisDecimationCounter_;
    if (analysisDecimationCounter_ >= every)
    {
        analysisDecimationCounter_ = 0;
        runAnalysisChain();
    }
}

void PitchLabEngine::copyIngressLatest (std::span<std::int16_t> dst) const noexcept
{
    hfChain_.ingress_.copyLatestInto (dst);
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

void PitchLabEngine::analyzeOfflineWindowFromMonoFloat (std::span<const float> monoNativeSamples, RenderFrameData& out)
{
    std::unique_lock lk (configMutex_);

    const int hfN = state_.fftSize;
    if (hfN <= 0 || hfChain_.fft_ == nullptr || hfChain_.tables_ == nullptr)
        return;

    const bool multiRes = (state_.spectralBackendMode() == SpectralBackendMode::MultiResSTFT_v1_0);
    const int expected = offlineMonoInputSampleCount();
    if (static_cast<int> (monoNativeSamples.size()) != expected)
        return;

    hfChain_.ingress_.reset();
    lfChain_.ingress_.reset();
    decimator_.reset();
    analysisDecimationCounter_ = 0;
    lfAnalysisDecimationCounter_ = 0;
    state_.strobePhase = 0.0f;
    highPassFilter_.reset();
    lfHighPassFilter_.reset();
    lastHighPassCutHz_ = -1.0f;
    lastHighPassSr_ = 0.0;
    lastLfHighPassCutHz_ = -1.0f;
    lastLfHighPassSr_ = 0.0;

    if (! multiRes)
    {
        if (static_cast<int> (conversionScratch_.size()) < hfN)
            return;

        convertFloatToInt16Ingress (monoNativeSamples,
                                    std::span<std::int16_t> { conversionScratch_.data(), static_cast<std::size_t> (hfN) });
        hfChain_.ingress_.push (std::span<const std::int16_t> { conversionScratch_.data(), static_cast<std::size_t> (hfN) });
        runAnalysisChain();
        copyLatestRenderFrame (out);
        return;
    }

    if (lfChain_.fft_ == nullptr || lfChain_.tables_ == nullptr)
        return;

    if (decimatorFloatScratch_.empty() || decimatorFloatOutScratch_.empty() || decimatorInt16Scratch_.empty())
        return;

    const int chunkCap = static_cast<int> (
        std::min (std::size_t { 2048 },
                  std::min (conversionScratch_.size(), decimatorFloatScratch_.size())));
    if (chunkCap < 1)
        return;

    const int total = static_cast<int> (monoNativeSamples.size());
    for (int off = 0; off < total; off += chunkCap)
    {
        const int n = std::min (chunkCap, total - off);
        const std::span<const float> chunk { monoNativeSamples.data() + static_cast<std::size_t> (off),
                                             static_cast<std::size_t> (n) };
        convertFloatToInt16Ingress (chunk,
                                    std::span<std::int16_t> { conversionScratch_.data(), static_cast<std::size_t> (n) });
        hfChain_.ingress_.push (std::span<const std::int16_t> { conversionScratch_.data(), static_cast<std::size_t> (n) });

        for (int i = 0; i < n; ++i)
            decimatorFloatScratch_[static_cast<std::size_t> (i)] =
                static_cast<float> (conversionScratch_[static_cast<std::size_t> (i)]) * (1.0f / 32768.0f);

        const int nd = decimator_.processBlock (
            std::span<const float> { decimatorFloatScratch_.data(), static_cast<std::size_t> (n) },
            std::span<float> { decimatorFloatOutScratch_.data(), decimatorFloatOutScratch_.size() });

        if (nd > 0)
        {
            convertFloatToInt16Ingress (std::span<const float> { decimatorFloatOutScratch_.data(), static_cast<std::size_t> (nd) },
                                        std::span<std::int16_t> { decimatorInt16Scratch_.data(), static_cast<std::size_t> (nd) });
            lfChain_.ingress_.push (std::span<const std::int16_t> { decimatorInt16Scratch_.data(), static_cast<std::size_t> (nd) });
        }
    }

    const int every = std::max (1, state_.analysisEveryNCallbacks.load (std::memory_order_relaxed));
    lfAnalysisDecimationCounter_ = std::max (1, every * 2);
    runAnalysisChain();
    copyLatestRenderFrame (out);
}

} // namespace pitchlab
