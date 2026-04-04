#pragma once

#include "ChromaMap.h"
#include "CircularInt16Buffer.h"
#include "Decimator4x.h"
#include "EngineState.h"
#include "RenderFrameData.h"

#include <juce_dsp/juce_dsp.h>

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

    /**
        Offline analysis hop aligned to the live chain. STFT / Const-Q: pass exactly fftSize mono floats
        (oldest first), same as readMonoFloatWindow. Multi-res STFT: pass offlineMonoInputSampleCount()
        samples ending at the same time as the HF frame (warmup + 4xLF history + HF window; zero-pad early file).
    */
    void analyzeOfflineWindowFromMonoFloat (std::span<const float> monoNativeSamples, RenderFrameData& out);

    [[nodiscard]] int offlineMonoInputSampleCount() const noexcept;

    [[nodiscard]] const EngineState& state() const noexcept { return state_; }
    [[nodiscard]] EngineState& state() noexcept { return state_; }
    [[nodiscard]] const StaticTables* tables() const noexcept { return hfChain_.tables_.get(); }
    [[nodiscard]] const CircularInt16Buffer& ingressBuffer() const noexcept { return hfChain_.ingress_; }

#if defined(PITCHLAB_ENGINE_TESTING)
    [[nodiscard]] const CircularInt16Buffer& testHfIngress() const noexcept { return hfChain_.ingress_; }
    [[nodiscard]] const CircularInt16Buffer& testLfIngress() const noexcept { return lfChain_.ingress_; }
#endif

private:
    struct AnalysisChain
    {
        static constexpr std::size_t kIngressCapacity = 65536;

        CircularInt16Buffer ingress_;
        std::unique_ptr<StaticTables> tables_;
        std::unique_ptr<FftMagnitudes> fft_;
        std::vector<std::int16_t> timeWindow_;
        std::vector<std::int16_t> windowed_;
        std::vector<float> floatFftIn_;
        std::vector<float> magSpectrum_;
        int fftSize_ = 0;
        double sampleRate_ = 0.0;

        explicit AnalysisChain();

        void prepare (int fftSize, double sampleRate, int maxBlockSamples);
    };

    void runFftOnChain (AnalysisChain& ch,
                        juce::dsp::IIR::Filter<float>& hpFilter,
                        float& lastHpCut,
                        double& lastHpSr) noexcept;

    void runAnalysisChain() noexcept;
    void prepareToPlayImpl (double sampleRate, int maxBlockSamples);

    /** LF chain FFT size for multi-res: min(HF size, 4096); 4x decimation already boosts LF resolution. */
    [[nodiscard]] int lfFftSizeForPrepare (int hfFftSize) const noexcept;

    EngineState state_;
    AnalysisChain hfChain_;
    AnalysisChain lfChain_;
    Decimator4x decimator_;
    std::vector<std::int16_t> conversionScratch_;
    std::vector<float> decimatorFloatScratch_;
    std::vector<float> decimatorFloatOutScratch_;
    std::vector<std::int16_t> decimatorInt16Scratch_;
    ChromaMap chromaMap_;

    std::vector<float> magForFold_;
    std::array<float, 384> chromaRow_ {};
    juce::dsp::IIR::Filter<float> highPassFilter_;
    juce::dsp::IIR::Filter<float> lfHighPassFilter_;
    float lastHighPassCutHz_ = -1.0f;
    double lastHighPassSr_ = 0.0;
    float lastLfHighPassCutHz_ = -1.0f;
    double lastLfHighPassSr_ = 0.0;
    int analysisDecimationCounter_ = 0;
    int lfAnalysisDecimationCounter_ = 0;
    mutable std::mutex frameMutex_;
    RenderFrameData latestFrame_{};
    std::uint64_t frameSequence_ = 0;

    mutable std::shared_mutex configMutex_;
};

} // namespace pitchlab
