#include "MainComponent.h"

#include "SharedWaterfallRing.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace
{
constexpr const char* kAudioFileChooserWildcards =
    "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a;*.aac;*.wma;*.caf";

bool isAllowedAudioExtension (const juce::String& extLowerNoDot)
{
    static const juce::String allowed[] = {
        "wav", "aif", "aiff", "flac", "mp3", "ogg", "m4a", "aac", "wma", "caf"
    };

    for (const auto& a : allowed)
        if (extLowerNoDot == a)
            return true;

    return false;
}

[[nodiscard]] VisualizationMode mapComboIdToMode (int id)
{
    switch (id)
    {
        case 2:
            return VisualizationMode::Waterfall;
        case 3:
            return VisualizationMode::Needle;
        case 4:
            return VisualizationMode::StrobeRadial;
        case 5:
            return VisualizationMode::ChordMatrix;
        default:
            return VisualizationMode::Waveform;
    }
}

void collectAudioFilesFromDir (const juce::File& dir, std::vector<juce::File>& out)
{
    if (! dir.isDirectory())
        return;

    juce::Array<juce::File> found;
    dir.findChildFiles (found, juce::File::findFiles, false);

    for (const auto& f : found)
    {
        if (! f.existsAsFile())
            continue;

        auto ext = f.getFileExtension().toLowerCase().trimCharactersAtStart (".");
        if (! isAllowedAudioExtension (ext))
            continue;

        out.push_back (f);
    }
}

/** Mono float window [endSample-fftSize, endSample); zero-pad before file start. */
void readMonoFloatWindow (juce::AudioFormatReader& reader,
                          int fftSize,
                          int64_t endSample,
                          std::vector<float>& dst)
{
    dst.resize (static_cast<std::size_t> (fftSize));
    std::fill (dst.begin(), dst.end(), 0.0f);

    const int numCh = juce::jlimit (1, 64, static_cast<int> (reader.numChannels));
    const int64_t total = reader.lengthInSamples;
    const int64_t winStart = endSample - static_cast<int64_t> (fftSize);

    int dstOffset = 0;
    int64_t fileReadStart = winStart;
    if (winStart < 0)
    {
        dstOffset = static_cast<int> (-winStart);
        fileReadStart = 0;
    }

    const int len = fftSize - dstOffset;
    if (len <= 0 || fileReadStart >= total)
        return;

    const int toRead = static_cast<int> (juce::jmin<int64_t> (len, total - fileReadStart));
    if (toRead <= 0)
        return;

    std::vector<std::vector<float>> chBuf (static_cast<std::size_t> (numCh));
    std::vector<float*> ptrs;
    ptrs.reserve (static_cast<std::size_t> (numCh));
    for (int c = 0; c < numCh; ++c)
    {
        chBuf[static_cast<std::size_t> (c)].resize (static_cast<std::size_t> (toRead));
        ptrs.push_back (chBuf[static_cast<std::size_t> (c)].data());
    }

    reader.read (ptrs.data(), numCh, fileReadStart, toRead);

    for (int i = 0; i < toRead; ++i)
    {
        float s = 0.0f;
        for (int c = 0; c < numCh; ++c)
            s += ptrs[static_cast<std::size_t> (c)][static_cast<std::size_t> (i)];
        dst[static_cast<std::size_t> (dstOffset + i)] = s / static_cast<float> (numCh);
    }
}

[[nodiscard]] juce::File getAudioDeviceSettingsFile()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("TinyPitchAnalyzer");
    (void) dir.createDirectory();
    return dir.getChildFile ("audio_device.xml");
}
} // namespace

MainComponent::MainComponent()
{
    formatManager_.registerBasicFormats();

    addAndMakeVisible (openButton_);
    addAndMakeVisible (exampleCombo_);
    addAndMakeVisible (playPauseButton_);
    addAndMakeVisible (inputToggleButton_);
    addAndMakeVisible (positionSlider_);
    addAndMakeVisible (fftSizeLabel_);
    addAndMakeVisible (fftSizeCombo_);
    addAndMakeVisible (waterfallEnergyScaleSlider_);
    addAndMakeVisible (waterfallAlphaPowerSlider_);
    addAndMakeVisible (waterfallAlphaThresholdSlider_);
    addAndMakeVisible (waterfallEnergyScaleLabel_);
    addAndMakeVisible (waterfallAlphaPowerLabel_);
    addAndMakeVisible (waterfallAlphaThresholdLabel_);
    addAndMakeVisible (agcEnabledToggle_);
    addAndMakeVisible (agcStrengthSlider_);
    addAndMakeVisible (agcStrengthLabel_);
    addAndMakeVisible (foldInterpCombo_);
    addAndMakeVisible (foldWeightCombo_);
    addAndMakeVisible (foldOctavesCombo_);
    addAndMakeVisible (waterfallFilterCombo_);
    addAndMakeVisible (waterfallCurveCombo_);
    addAndMakeVisible (chromaShapingCombo_);
    addAndMakeVisible (foldModelCombo_);
    addAndMakeVisible (spectralBackendCombo_);
    addAndMakeVisible (analysisRateCombo_);
    addAndMakeVisible (highPassSlider_);
    addAndMakeVisible (highPassLabel_);
    addAndMakeVisible (statusLabel_);
    addAndMakeVisible (engineLabel_);
    addAndMakeVisible (vizModeCombo_);
    addAndMakeVisible (windowKindCombo_);
    addAndMakeVisible (backendCombo_);
    addAndMakeVisible (audioSettingsButton_);
    addAndMakeVisible (audioDeviceLabel_);
    addAndMakeVisible (peakLabel_);
    addAndMakeVisible (instantPreviewToggle_);
    addAndMakeVisible (instantScrollBar_);
    instantPreviewToggle_.setTooltip (
        "Instantaneous calculation of example/audio file: Waterfall mode pre-computes only the visible time window (384 analysis columns); "
        "use the scrollbar to move through long files.");
    instantPreviewToggle_.onClick = [this] {
        updateInstantPreviewChrome();
        if (instantPreviewToggle_.getToggleState())
            launchInstantWaterfallJob();
    };
    instantScrollBar_.addListener (this);
    instantScrollBar_.setAutoHide (false);
    instantPreviewToggle_.setToggleState (true, juce::dontSendNotification);
    addAndMakeVisible (openGlHost_);
    addAndMakeVisible (cpuHost_);
    activeRenderer_ = &openGlHost_;
    cpuHost_.setVisible (false);

    vizModeCombo_.addItem ("Waveform", 1);
    vizModeCombo_.addItem ("Waterfall", 2);
    vizModeCombo_.addItem ("Needle", 3);
    vizModeCombo_.addItem ("Strobe (poly + radial)", 4);
    vizModeCombo_.addItem ("Chord matrix", 5);
    vizModeCombo_.setSelectedId (2, juce::dontSendNotification);
    vizModeCombo_.onChange = [this] { visualizationModeChanged(); };

    windowKindCombo_.addItem ("Window: Hanning", 1);
    windowKindCombo_.addItem ("Window: Gaussian", 2);
    windowKindCombo_.setSelectedId (2, juce::dontSendNotification);
    engine_.state().setWindowKind (pitchlab::WindowKind::Gaussian);
    windowKindCombo_.onChange = [this] {
        const int id = windowKindCombo_.getSelectedId();
        engine_.state().setWindowKind (id == 2 ? pitchlab::WindowKind::Gaussian : pitchlab::WindowKind::Hanning);
        bumpInstantPreviewDebounced();
    };

    backendCombo_.addItem ("Backend: Auto", 1);
    backendCombo_.addItem ("Backend: GPU", 2);
    backendCombo_.addItem ("Backend: CPU", 3);
    backendCombo_.setSelectedId (2, juce::dontSendNotification);
    backendCombo_.onChange = [this] { renderBackendPolicyChanged(); };

    renderBackendPolicy_ = RenderBackendPolicy::ForceGpu;
    activeRenderer_->setMode (VisualizationMode::Waterfall);
    cpuHost_.setMode (VisualizationMode::Waterfall);

    audioSettingsButton_.onClick = [this] { audioSettingsClicked(); };

    audioDeviceLabel_.setJustificationType (juce::Justification::centredLeft);
    peakLabel_.setJustificationType (juce::Justification::centredRight);
    peakLabel_.setFont (juce::FontOptions { 12.0f });

    openButton_.onClick = [this] { openFileClicked(); };
    exampleCombo_.onChange = [this] { exampleComboChanged(); };
    playPauseButton_.onClick = [this] { playPauseClicked(); };
    inputToggleButton_.onClick = [this] { toggleInputClicked(); };

    positionSlider_.setRange (0.0, 1.0, 0.0001);
    positionSlider_.setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 20);
    positionSlider_.textFromValueFunction = [] (double secs) { return juce::String (secs, 2) + " s"; };
    positionSlider_.onValueChange = [this] { positionSliderChanged(); };

    fftSizeLabel_.setText ("FFT", juce::dontSendNotification);
    fftSizeCombo_.addItem ("1024", 1024);
    fftSizeCombo_.addItem ("2048", 2048);
    fftSizeCombo_.addItem ("4096", 4096);
    fftSizeCombo_.addItem ("8192", 8192);
    fftSizeCombo_.addItem ("16384", 16384);
    fftSizeCombo_.addItem ("32768", 32768);
    fftSizeCombo_.setSelectedId (8192, juce::dontSendNotification);
    fftSizeCombo_.onChange = [this] { fftSizeChanged(); };

    waterfallEnergyScaleLabel_.setText ("W-Energy", juce::dontSendNotification);
    waterfallAlphaPowerLabel_.setText ("W-AlphaPow", juce::dontSendNotification);
    waterfallAlphaThresholdLabel_.setText ("W-Thresh", juce::dontSendNotification);

    waterfallEnergyScaleSlider_.setSliderStyle (juce::Slider::LinearHorizontal);
    waterfallAlphaPowerSlider_.setSliderStyle (juce::Slider::LinearHorizontal);
    waterfallAlphaThresholdSlider_.setSliderStyle (juce::Slider::LinearHorizontal);

    waterfallEnergyScaleSlider_.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 20);
    waterfallAlphaPowerSlider_.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 20);
    waterfallAlphaThresholdSlider_.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 20);

    waterfallEnergyScaleSlider_.setRange (0.001f, 0.2f, 0.001f);
    waterfallAlphaPowerSlider_.setRange (0.1f, 4.0f, 0.05f);
    waterfallAlphaThresholdSlider_.setRange (0.0f, 0.05f, 0.0005f);

    waterfallEnergyScaleSlider_.setValue (0.064, juce::dontSendNotification);
    waterfallAlphaPowerSlider_.setValue (2.55, juce::dontSendNotification);
    waterfallAlphaThresholdSlider_.setValue (0.0050, juce::dontSendNotification);

    waterfallEnergyScaleSlider_.onValueChange = [this] {
        const float v = (float) waterfallEnergyScaleSlider_.getValue();
        openGlHost_.setWaterfallEnergyScale (v);
        cpuHost_.setWaterfallEnergyScale (v);
    };
    waterfallAlphaPowerSlider_.onValueChange = [this] {
        const float v = (float) waterfallAlphaPowerSlider_.getValue();
        openGlHost_.setWaterfallAlphaPower (v);
        cpuHost_.setWaterfallAlphaPower (v);
    };
    waterfallAlphaThresholdSlider_.onValueChange = [this] {
        const float v = (float) waterfallAlphaThresholdSlider_.getValue();
        openGlHost_.setWaterfallAlphaThreshold (v);
        cpuHost_.setWaterfallAlphaThreshold (v);
    };

    agcEnabledToggle_.setToggleState (true, juce::dontSendNotification);
    engine_.state().agcEnabled.store (true, std::memory_order_relaxed);
    agcEnabledToggle_.onClick = [this] {
        engine_.state().agcEnabled.store (agcEnabledToggle_.getToggleState(), std::memory_order_relaxed);
        bumpInstantPreviewDebounced();
    };

    agcStrengthLabel_.setText ("AGC Strength", juce::dontSendNotification);
    agcStrengthSlider_.setSliderStyle (juce::Slider::LinearHorizontal);
    agcStrengthSlider_.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 20);
    agcStrengthSlider_.setRange (0.0, 1.0, 0.01);
    agcStrengthSlider_.setValue (1.0, juce::dontSendNotification);
    agcStrengthSlider_.onValueChange = [this] {
        engine_.state().agcStrength.store ((float) agcStrengthSlider_.getValue(), std::memory_order_relaxed);
        bumpInstantPreviewDebounced();
    };

    foldInterpCombo_.addItem ("Fold: Nearest", 1);
    foldInterpCombo_.addItem ("Fold: Linear2", 2);
    foldInterpCombo_.addItem ("Fold: Quadratic3", 3);
    foldInterpCombo_.setSelectedId (2, juce::dontSendNotification);
    foldInterpCombo_.onChange = [this] {
        const int id = foldInterpCombo_.getSelectedId();
        pitchlab::FoldInterpMode m = pitchlab::FoldInterpMode::Linear2Bin;
        if (id == 1) m = pitchlab::FoldInterpMode::Nearest;
        else if (id == 3) m = pitchlab::FoldInterpMode::Quadratic3Bin;
        engine_.state().setFoldInterpMode (m);
        bumpInstantPreviewDebounced();
    };

    foldWeightCombo_.addItem ("Weight: Uniform", 1);
    foldWeightCombo_.addItem ("Weight: 1/h", 2);
    foldWeightCombo_.addItem ("Weight: 1/sqrt(h)", 3);
    foldWeightCombo_.setSelectedId (3, juce::dontSendNotification);
    foldWeightCombo_.onChange = [this] {
        const int id = foldWeightCombo_.getSelectedId();
        pitchlab::FoldHarmonicWeightMode m = pitchlab::FoldHarmonicWeightMode::InvSqrtH;
        if (id == 1) m = pitchlab::FoldHarmonicWeightMode::Uniform;
        else if (id == 2) m = pitchlab::FoldHarmonicWeightMode::InvH;
        engine_.state().setFoldHarmonicWeightMode (m);
        bumpInstantPreviewDebounced();
    };

    foldOctavesCombo_.addItem ("Oct: Auto", 0x100);
    foldOctavesCombo_.addItem ("Oct: 1", 1);
    foldOctavesCombo_.addItem ("Oct: 2", 2);
    foldOctavesCombo_.addItem ("Oct: 3", 3);
    foldOctavesCombo_.addItem ("Oct: 4", 4);
    foldOctavesCombo_.addItem ("Oct: 5", 5);
    foldOctavesCombo_.addItem ("Oct: 6", 6);
    foldOctavesCombo_.setSelectedId (0x100, juce::dontSendNotification);
    foldOctavesCombo_.onChange = [this] {
        const int id = foldOctavesCombo_.getSelectedId();
        engine_.state().foldMaxOctaves.store (id == 0x100 ? 0 : id, std::memory_order_relaxed);
        bumpInstantPreviewDebounced();
    };

    waterfallFilterCombo_.addItem ("Filter: Nearest", 1);
    waterfallFilterCombo_.addItem ("Filter: Linear", 2);
    waterfallFilterCombo_.setSelectedId (1, juce::dontSendNotification);
    waterfallFilterCombo_.onChange = [this] {
        const auto m = waterfallFilterCombo_.getSelectedId() == 2
                           ? pitchlab::WaterfallTextureFilterMode::Linear
                           : pitchlab::WaterfallTextureFilterMode::Nearest;
        openGlHost_.setWaterfallTextureFilterMode (m);
        engine_.state().setWaterfallTextureFilterMode (m);
    };

    waterfallCurveCombo_.addItem ("Curve: Linear", 1);
    waterfallCurveCombo_.addItem ("Curve: Sqrt", 2);
    waterfallCurveCombo_.addItem ("Curve: Log dB", 3);
    waterfallCurveCombo_.setSelectedId (2, juce::dontSendNotification);
    waterfallCurveCombo_.onChange = [this] {
        pitchlab::WaterfallDisplayCurveMode m = pitchlab::WaterfallDisplayCurveMode::Sqrt;
        if (waterfallCurveCombo_.getSelectedId() == 1) m = pitchlab::WaterfallDisplayCurveMode::Linear;
        else if (waterfallCurveCombo_.getSelectedId() == 3) m = pitchlab::WaterfallDisplayCurveMode::LogDb;
        openGlHost_.setWaterfallDisplayCurveMode (m);
        cpuHost_.setWaterfallDisplayCurveMode (m);
        engine_.state().setWaterfallDisplayCurveMode (m);
    };

    chromaShapingCombo_.addItem (pitchlab::kUiChromaShapeNone, 1);
    chromaShapingCombo_.addItem (pitchlab::kUiChromaShapeLog, 2);
    chromaShapingCombo_.addItem (pitchlab::kUiChromaShapeNoiseFloor, 3);
    chromaShapingCombo_.addItem (pitchlab::kUiChromaShapePercentile, 4);
    chromaShapingCombo_.setSelectedId (3, juce::dontSendNotification);
    chromaShapingCombo_.onChange = [this] {
        pitchlab::ChromaShapingMode m = pitchlab::ChromaShapingMode::None;
        switch (chromaShapingCombo_.getSelectedId())
        {
            case 2: m = pitchlab::ChromaShapingMode::LogCompress; break;
            case 3: m = pitchlab::ChromaShapingMode::NoiseFloorSubtract; break;
            case 4: m = pitchlab::ChromaShapingMode::PercentileGate; break;
            default: break;
        }
        engine_.state().setChromaShapingMode (m);
        bumpInstantPreviewDebounced();
    };

    foldModelCombo_.addItem (pitchlab::kUiFoldOctaveStack, 1);
    foldModelCombo_.addItem (pitchlab::kUiFoldIntegerHarmonics, 2);
    foldModelCombo_.setSelectedId (1, juce::dontSendNotification);
    foldModelCombo_.onChange = [this] {
        engine_.state().setFoldHarmonicModel (foldModelCombo_.getSelectedId() == 2
                                                   ? pitchlab::FoldHarmonicModel::IntegerHarmonics_v0_2
                                                   : pitchlab::FoldHarmonicModel::OctaveStack_Doc_v1);
        bumpInstantPreviewDebounced();
    };

    spectralBackendCombo_.addItem (pitchlab::kUiSpectrumStft, 1);
    spectralBackendCombo_.addItem (pitchlab::kUiSpectrumConstQApprox, 2);
    spectralBackendCombo_.addItem (pitchlab::kUiSpectrumVarQApprox, 3);
    spectralBackendCombo_.addItem (pitchlab::kUiSpectrumMultiResStft, 4);
    spectralBackendCombo_.setSelectedId (1, juce::dontSendNotification);
    spectralBackendCombo_.onChange = [this] {
        pitchlab::SpectralBackendMode m = pitchlab::SpectralBackendMode::STFT_v1_0;
        if (spectralBackendCombo_.getSelectedId() == 2) m = pitchlab::SpectralBackendMode::ConstQApprox_v0_1;
        else if (spectralBackendCombo_.getSelectedId() == 3) m = pitchlab::SpectralBackendMode::VariableQApprox_v0_1;
        else if (spectralBackendCombo_.getSelectedId() == 4) m = pitchlab::SpectralBackendMode::MultiResSTFT_v1_0;
        engine_.state().setSpectralBackendMode (m);
        if (sampleRate_ > 0.0 && samplesPerBlockExpected_ > 0)
        {
            engine_.prepareToPlay (sampleRate_, samplesPerBlockExpected_);
            const juce::ScopedLock sl (offlineMutex_);
            const int maxBlock = juce::jmax (samplesPerBlockExpected_, engine_.state().fftSize);
            offlineEngine_.prepareToPlay (sampleRate_, maxBlock);
        }
        bumpInstantPreviewDebounced();
    };

    analysisRateCombo_.addItem ("Rate: each callback", 1);
    analysisRateCombo_.addItem ("Rate: 1/2", 2);
    analysisRateCombo_.addItem ("Rate: 1/4", 3);
    analysisRateCombo_.addItem ("Rate: 1/8", 4);
    analysisRateCombo_.setSelectedId (1, juce::dontSendNotification);
    analysisRateCombo_.onChange = [this] {
        const int id = analysisRateCombo_.getSelectedId();
        const int every = 1 << juce::jlimit (0, 3, id - 1);
        engine_.state().analysisEveryNCallbacks.store (every, std::memory_order_relaxed);
        updateInstantPreviewChrome();
        bumpInstantPreviewDebounced();
    };

    highPassLabel_.setText ("HP (Hz)", juce::dontSendNotification);
    highPassSlider_.setSliderStyle (juce::Slider::LinearHorizontal);
    highPassSlider_.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
    highPassSlider_.setRange (0.0, 4000.0, 1.0);
    highPassSlider_.setSkewFactorFromMidPoint (200.0);
    highPassSlider_.textFromValueFunction = [] (double v) {
        return v < static_cast<double> (pitchlab::EngineState::kHighPassOffHz) ? juce::String ("Off")
                                                                                 : juce::String (v, 0) + " Hz";
    };
    highPassSlider_.setValue (0.0, juce::dontSendNotification);
    highPassSlider_.onValueChange = [this] {
        engine_.state().highPassCutoffHz.store (static_cast<float> (highPassSlider_.getValue()),
                                                std::memory_order_relaxed);
        bumpInstantPreviewDebounced();
    };

    // Apply defaults once.
    waterfallEnergyScaleSlider_.onValueChange();
    waterfallAlphaPowerSlider_.onValueChange();
    waterfallAlphaThresholdSlider_.onValueChange();
    waterfallFilterCombo_.onChange();
    waterfallCurveCombo_.onChange();
    chromaShapingCombo_.onChange();
    foldModelCombo_.onChange();
    spectralBackendCombo_.onChange();
    analysisRateCombo_.onChange();
    highPassSlider_.onValueChange();

    statusLabel_.setJustificationType (juce::Justification::centredLeft);
    engineLabel_.setJustificationType (juce::Justification::centredLeft);
    engineLabel_.setFont (juce::FontOptions { 13.0f });

    setSize (800, 680);
    startTimerHz (30);

    std::unique_ptr<juce::XmlElement> savedAudioState = juce::parseXML (getAudioDeviceSettingsFile());
    setAudioChannels (2, 2, savedAudioState.get());

    refreshExampleAudioList();

    const juce::File defaultWav (PITCHLAB_DEFAULT_EXAMPLE_WAV);
    if (defaultWav.existsAsFile())
        loadAudioFile (defaultWav);
}

MainComponent::~MainComponent()
{
    saveAudioDeviceState();
    instantScrollBar_.removeListener (this);
    ++instantJobGeneration_;
    stopTimer();
    shutdownAudio();
}

void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    sampleRate_ = sampleRate;
    samplesPerBlockExpected_ = samplesPerBlockExpected;
    engine_.prepareToPlay (sampleRate, samplesPerBlockExpected);
    {
        const juce::ScopedLock sl (offlineMutex_);
        const int maxBlock = juce::jmax (samplesPerBlockExpected, engine_.state().fftSize);
        offlineEngine_.prepareToPlay (sampleRate, maxBlock);
    }
    openGlHost_.setStaticTablesPtr (engine_.tables());
    cpuHost_.setStaticTablesPtr (engine_.tables());
    syncRendererBackend();

    const juce::ScopedLock sl (transportLock_);
    transport_.prepareToPlay (samplesPerBlockExpected, sampleRate);
    updateInstantPreviewChrome();
    if (instantPreviewToggle_.getToggleState())
        launchInstantWaterfallJob();
}

void MainComponent::releaseResources()
{
    const juce::ScopedLock sl (transportLock_);
    transport_.releaseResources();
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    auto* buf = bufferToFill.buffer;
    const int n = bufferToFill.numSamples;
    const int start = bufferToFill.startSample;

    if (buf == nullptr || n <= 0)
        return;

    {
        const juce::ScopedLock sl (transportLock_);

        if (inputMode_ == InputMode::FilePlayback && readerSource_ != nullptr)
        {
            // AudioSourcePlayer copies inputs into the output buffer first; clear so file
            // playback is not mixed with (possibly silent) mic pass-through.
            for (int c = 0; c < buf->getNumChannels(); ++c)
                buf->clear (c, start, n);

            juce::AudioSourceChannelInfo info (buf, start, n);
            transport_.getNextAudioBlock (info);
        }
        else if (inputMode_ == InputMode::FilePlayback)
        {
            for (int c = 0; c < buf->getNumChannels(); ++c)
                buf->clear (c, start, n);
        }
        // LiveMicrophone: AudioSourcePlayer already copied device input into buffer for pass-through.
    }

    const int ch = buf->getNumChannels();
    channelPtrScratch_.resize (static_cast<std::size_t> (ch));
    for (int c = 0; c < ch; ++c)
        channelPtrScratch_[static_cast<std::size_t> (c)] = buf->getReadPointer (c, start);

    engine_.processAudioInterleaved (channelPtrScratch_.data(), ch, n);

    float pk = 0.0f;
    for (int c = 0; c < ch; ++c)
    {
        const float* p = buf->getReadPointer (c, start);
        for (int i = 0; i < n; ++i)
            pk = std::max (pk, std::abs (p[i]));
    }

    float prev = audioPeakHold_.load (std::memory_order_relaxed);
    while (pk > prev && ! audioPeakHold_.compare_exchange_weak (prev, pk, std::memory_order_relaxed))
    {
    }

    pitchlab::RenderFrameData frame;
    engine_.copyLatestRenderFrame (frame);
    activeRenderer_->setRenderFrame (frame);

    const bool suppressWaterfall = instantPreviewToggle_.getToggleState()
                                 && inputMode_ == InputMode::FilePlayback && currentReader_ != nullptr
                                 && mapComboIdToMode (vizModeCombo_.getSelectedId()) == VisualizationMode::Waterfall;

    if (! suppressWaterfall)
        activeRenderer_->pushWaterfallRow (std::span<const float> { frame.chromaRow.data(), frame.chromaRow.size() });
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto r = getLocalBounds().reduced (12);
    r.removeFromBottom (8);

    auto top = r.removeFromTop (36);
    openButton_.setBounds (top.removeFromLeft (120).reduced (0, 4));
    top.removeFromLeft (8);
    inputToggleButton_.setBounds (top.removeFromRight (140).reduced (0, 4));
    top.removeFromRight (8);
    playPauseButton_.setBounds (top.removeFromRight (80).reduced (0, 4));
    top.removeFromRight (8);
    exampleCombo_.setBounds (top.reduced (0, 4));
    r.removeFromTop (8);

    auto row2 = r.removeFromTop (26);
    vizModeCombo_.setBounds (row2.removeFromLeft (200).reduced (0, 2));
    row2.removeFromLeft (8);
    windowKindCombo_.setBounds (row2.removeFromLeft (180).reduced (0, 2));
    row2.removeFromLeft (8);
    backendCombo_.setBounds (row2.removeFromLeft (180).reduced (0, 2));
    row2.removeFromLeft (8);
    peakLabel_.setBounds (row2.removeFromRight (100).reduced (0, 2));
    row2.removeFromRight (8);
    instantPreviewToggle_.setBounds (row2.removeFromRight (200).reduced (0, 2));
    row2.removeFromRight (8);
    audioSettingsButton_.setBounds (row2.removeFromRight (76).reduced (0, 2));
    row2.removeFromRight (6);
    audioDeviceLabel_.setBounds (row2.reduced (0, 2));
    r.removeFromTop (6);

    auto fftRow = r.removeFromTop (22);
    fftSizeLabel_.setBounds (fftRow.removeFromLeft (90).reduced (0, 2));
    fftSizeCombo_.setBounds (fftRow.removeFromLeft (120).reduced (0, 2));
    fftRow.removeFromLeft (8);
    agcEnabledToggle_.setBounds (fftRow.removeFromLeft (65).reduced (0, 2));
    fftRow.removeFromLeft (8);
    agcStrengthLabel_.setBounds (fftRow.removeFromLeft (90).reduced (0, 2));
    agcStrengthSlider_.setBounds (fftRow.removeFromLeft (fftRow.getWidth() / 2).reduced (0, 2));

    r.removeFromTop (4);
    auto hpRow = r.removeFromTop (24);
    highPassLabel_.setBounds (hpRow.removeFromLeft (56).reduced (0, 2));
    highPassSlider_.setBounds (hpRow.removeFromLeft (hpRow.getWidth() / 2).reduced (0, 2));
    hpRow.removeFromLeft (6);
    analysisRateCombo_.setBounds (hpRow.reduced (0, 2));

    r.removeFromTop (4);
    auto modelRow = r.removeFromTop (24);
    const int mw = modelRow.getWidth() / 3;
    foldModelCombo_.setBounds (modelRow.removeFromLeft (mw).reduced (0, 2));
    spectralBackendCombo_.setBounds (modelRow.removeFromLeft (mw).reduced (0, 2));
    chromaShapingCombo_.setBounds (modelRow.reduced (0, 2));

    r.removeFromTop (4);
    auto optRow = r.removeFromTop (24);
    const int optW = optRow.getWidth() / 5;
    foldInterpCombo_.setBounds (optRow.removeFromLeft (optW).reduced (0, 2));
    foldWeightCombo_.setBounds (optRow.removeFromLeft (optW).reduced (0, 2));
    foldOctavesCombo_.setBounds (optRow.removeFromLeft (optW).reduced (0, 2));
    waterfallFilterCombo_.setBounds (optRow.removeFromLeft (optW).reduced (0, 2));
    waterfallCurveCombo_.setBounds (optRow.reduced (0, 2));

    auto ctrlRow = r.removeFromTop (56);
    const int segW = ctrlRow.getWidth() / 3;

    auto seg0 = ctrlRow.removeFromLeft (segW);
    waterfallEnergyScaleLabel_.setBounds (seg0.removeFromTop (14));
    waterfallEnergyScaleSlider_.setBounds (seg0.reduced (0, 2));

    auto seg1 = ctrlRow.removeFromLeft (segW);
    waterfallAlphaPowerLabel_.setBounds (seg1.removeFromTop (14));
    waterfallAlphaPowerSlider_.setBounds (seg1.reduced (0, 2));

    // Remaining width in case of rounding
    auto seg2 = ctrlRow;
    waterfallAlphaThresholdLabel_.setBounds (seg2.removeFromTop (14));
    waterfallAlphaThresholdSlider_.setBounds (seg2.reduced (0, 2));

    positionSlider_.setBounds (r.removeFromTop (28));
    r.removeFromTop (8);
    statusLabel_.setBounds (r.removeFromTop (24));
    engineLabel_.setBounds (r.removeFromTop (22));

    const bool showInstantScroll = instantPreviewToggle_.getToggleState() && inputMode_ == InputMode::FilePlayback
                                   && currentReader_ != nullptr && sampleRate_ > 0.0 && samplesPerBlockExpected_ > 0;
    int scrollH = 0;
    if (showInstantScroll)
    {
        const int hop = samplesPerBlockExpected_
                        * juce::jmax (1, engine_.state().analysisEveryNCallbacks.load (std::memory_order_relaxed));
        const double visibleSec = static_cast<double> (SharedWaterfallRing::kRows * hop) / sampleRate_;
        const double dur = static_cast<double> (currentReader_->lengthInSamples)
                           / juce::jmax (1.0e-6, currentReader_->sampleRate);
        if (dur > visibleSec + 1.0e-6)
            scrollH = 18;
    }

    if (scrollH > 0)
    {
        instantScrollBar_.setVisible (true);
        instantScrollBar_.setBounds (r.removeFromBottom (scrollH).reduced (24, 2));
        r.removeFromBottom (4);
    }
    else
    {
        instantScrollBar_.setVisible (false);
    }

    openGlHost_.setBounds (r);
    cpuHost_.setBounds (r);
}

void MainComponent::timerCallback()
{
    syncRendererBackend();
    updatePositionFromTransport();
    updateDeviceAndPeakLabels();
    updateInstantPreviewChrome();

    engineLabel_.setText ("Engine " + juce::String (pitchlab::engineVersionString())
                              + (engine_.state().analysisDirty.load() ? "  (analysis dirty)" : ""),
                          juce::dontSendNotification);
}

void MainComponent::updateDeviceAndPeakLabels()
{
    if (auto* dev = deviceManager.getCurrentAudioDevice())
    {
        audioDeviceLabel_.setText (dev->getName() + "  "
                                       + juce::String (dev->getCurrentSampleRate(), 0) + " Hz  "
                                       + juce::String (dev->getCurrentBufferSizeSamples()) + " smp",
                                   juce::dontSendNotification);
    }
    else
    {
        audioDeviceLabel_.setText ("No audio device", juce::dontSendNotification);
    }

    float v = audioPeakHold_.load (std::memory_order_relaxed);
    audioPeakHold_.store (v * 0.9f, std::memory_order_relaxed);
    const float db = 20.0f * std::log10 (juce::jmax (1.0e-6f, v));
    peakLabel_.setText (juce::String (db, 1) + " dBFS", juce::dontSendNotification);
}

void MainComponent::saveAudioDeviceState() const
{
    if (auto xml = deviceManager.createStateXml())
        (void) xml->writeTo (getAudioDeviceSettingsFile());
}

void MainComponent::audioSettingsClicked()
{
    auto* audioSettingsComp = new juce::AudioDeviceSelectorComponent (deviceManager,
                                                                      0,
                                                                      2,
                                                                      0,
                                                                      2,
                                                                      false,
                                                                      false,
                                                                      true,
                                                                      true);

    audioSettingsComp->setSize (500, 450);

    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (audioSettingsComp);
    o.dialogTitle = "Audio settings";
    o.componentToCentreAround = this;
    o.dialogBackgroundColour = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;

    if (auto* w = o.create())
    {
        const juce::Component::SafePointer<MainComponent> safeThis (this);

        w->enterModalState (true,
                            juce::ModalCallbackFunction::create ([safeThis] (int) {
                                if (safeThis != nullptr)
                                    safeThis->saveAudioDeviceState();
                            }),
                            true);
    }
}

void MainComponent::visualizationModeChanged()
{
    const auto m = mapComboIdToMode (vizModeCombo_.getSelectedId());
    openGlHost_.setMode (m);
    cpuHost_.setMode (m);
    activeRenderer_->setMode (m);
    bumpInstantPreviewDebounced();
}

void MainComponent::renderBackendPolicyChanged()
{
    const int id = backendCombo_.getSelectedId();
    if (id == 2)
        renderBackendPolicy_ = RenderBackendPolicy::ForceGpu;
    else if (id == 3)
        renderBackendPolicy_ = RenderBackendPolicy::ForceCpu;
    else
        renderBackendPolicy_ = RenderBackendPolicy::Auto;

    syncRendererBackend();
}

void MainComponent::syncRendererBackend()
{
    IRendererHost* next = shouldUseCpuBackend (renderBackendPolicy_, openGlHost_.isBackendHealthy())
                              ? static_cast<IRendererHost*> (&cpuHost_)
                              : static_cast<IRendererHost*> (&openGlHost_);

    if (next == activeRenderer_)
        return;

    const auto modeNow = mapComboIdToMode (vizModeCombo_.getSelectedId());
    next->setStaticTablesPtr (engine_.tables());
    next->setMode (modeNow);
    activeRenderer_ = next;
    openGlHost_.setVisible (activeRenderer_ == &openGlHost_);
    cpuHost_.setVisible (activeRenderer_ == &cpuHost_);
    bumpInstantPreviewDebounced();
}

void MainComponent::openFileClicked()
{
    fileChooser_ = std::make_unique<juce::FileChooser> ("Open audio file",
                                                        juce::File {},
                                                        kAudioFileChooserWildcards);

    const auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser_->launchAsync (chooserFlags, [this] (const juce::FileChooser& fc)
    {
        const juce::File f (fc.getResult());
        if (! f.existsAsFile())
            return;

        loadAudioFile (f);
    });
}

void MainComponent::loadAudioFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return;

    // stop() waits for the audio thread; never hold transportLock_ during stop() or the
    // audio callback cannot enter its critical section and we deadlock (no playback).
    transport_.stop();

    const juce::ScopedLock sl (transportLock_);
    transport_.setSource (nullptr, 0, nullptr, 0.0);
    readerSource_.reset();
    currentReader_.reset();

    currentReader_.reset (formatManager_.createReaderFor (file));
    if (currentReader_ == nullptr)
    {
        statusLabel_.setText ("Could not open file.", juce::dontSendNotification);
        return;
    }

    readerSource_ = std::make_unique<juce::AudioFormatReaderSource> (currentReader_.get(), false);

    if (auto* dev = deviceManager.getCurrentAudioDevice())
    {
        transport_.setSource (readerSource_.get(), 0, nullptr, currentReader_->sampleRate);
        transport_.prepareToPlay (dev->getCurrentBufferSizeSamples(), dev->getCurrentSampleRate());
    }

    inputMode_ = InputMode::FilePlayback;
    inputToggleButton_.setButtonText ("Input: File");
    playPauseButton_.setButtonText ("Play");

    const double lenSec = static_cast<double> (currentReader_->lengthInSamples)
                          / juce::jmax (1.0, currentReader_->sampleRate);
    positionSlider_.setRange (0.0, juce::jmax (0.001, lenSec), 0.0001);
    positionSlider_.setValue (0.0, juce::dontSendNotification);

    statusLabel_.setText (file.getFileName(), juce::dontSendNotification);

    currentAudioFileForInstant_ = file;
    instantScrollStartSec_.store (0.0, std::memory_order_relaxed);
    updateInstantPreviewChrome();
    if (instantPreviewToggle_.getToggleState())
        launchInstantWaterfallJob();
}

void MainComponent::refreshExampleAudioList()
{
    exampleCombo_.clear (juce::dontSendNotification);
    exampleAudioFiles_.clear();
    exampleCombo_.addItem ("Example clips...", 1);

    std::vector<juce::File> filtered;
    collectAudioFilesFromDir (juce::File (PITCHLAB_EXAMPLE_AUDIO_DIR), filtered);
    collectAudioFilesFromDir (juce::File (PITCHLAB_TEST_ASSETS_DIR), filtered);

    std::sort (filtered.begin(), filtered.end(), [] (const juce::File& a, const juce::File& b) {
        return a.getFullPathName().compareIgnoreCase (b.getFullPathName()) < 0;
    });
    filtered.erase (std::unique (filtered.begin(),
                                 filtered.end(),
                                 [] (const juce::File& a, const juce::File& b) {
                                     return a.getFullPathName().equalsIgnoreCase (b.getFullPathName());
                                 }),
                    filtered.end());
    std::sort (filtered.begin(), filtered.end(), [] (const juce::File& a, const juce::File& b) {
        return a.getFileName().compareNatural (b.getFileName()) < 0;
    });

    int nextId = 2;
    for (const auto& f : filtered)
    {
        exampleAudioFiles_.push_back (f);
        exampleCombo_.addItem (f.getFileName(), nextId++);
    }

    const juce::File defaultWav (PITCHLAB_DEFAULT_EXAMPLE_WAV);
    int selectId = 1;
    if (defaultWav.existsAsFile())
    {
        for (int i = 0; i < static_cast<int> (exampleAudioFiles_.size()); ++i)
        {
            if (exampleAudioFiles_[static_cast<std::size_t> (i)].getFullPathName().equalsIgnoreCase (defaultWav.getFullPathName()))
            {
                selectId = i + 2;
                break;
            }
        }
    }

    exampleCombo_.setSelectedId (selectId, juce::dontSendNotification);
}

void MainComponent::exampleComboChanged()
{
    const int id = exampleCombo_.getSelectedId();
    if (id < 2)
        return;

    const int idx = id - 2;
    if (idx < 0 || idx >= static_cast<int> (exampleAudioFiles_.size()))
        return;

    loadAudioFile (exampleAudioFiles_[static_cast<std::size_t> (idx)]);
}

void MainComponent::playPauseClicked()
{
    if (inputMode_ != InputMode::FilePlayback || readerSource_ == nullptr)
        return;

    const juce::ScopedLock sl (transportLock_);
    if (transport_.isPlaying())
    {
        transport_.stop();
        playPauseButton_.setButtonText ("Play");
    }
    else
    {
        transport_.start();
        playPauseButton_.setButtonText ("Pause");
    }
}

void MainComponent::toggleInputClicked()
{
    if (inputMode_ == InputMode::LiveMicrophone)
    {
        const juce::ScopedLock sl (transportLock_);

        if (readerSource_ == nullptr)
        {
            openFileClicked();
            return;
        }

        if (auto* dev = deviceManager.getCurrentAudioDevice())
        {
            transport_.setSource (readerSource_.get(), 0, nullptr, currentReader_->sampleRate);
            transport_.prepareToPlay (dev->getCurrentBufferSizeSamples(), dev->getCurrentSampleRate());
        }

        inputMode_ = InputMode::FilePlayback;
        inputToggleButton_.setButtonText ("Input: File");
        updateInstantPreviewChrome();
        resized();
        if (instantPreviewToggle_.getToggleState())
            launchInstantWaterfallJob();
    }
    else
    {
        transport_.stop();

        const juce::ScopedLock sl (transportLock_);
        transport_.setSource (nullptr, 0, nullptr, 0.0);
        inputMode_ = InputMode::LiveMicrophone;
        inputToggleButton_.setButtonText ("Input: Mic");
        playPauseButton_.setButtonText ("Play");
        updateInstantPreviewChrome();
        resized();
    }
}

void MainComponent::positionSliderChanged()
{
    if (inputMode_ != InputMode::FilePlayback || readerSource_ == nullptr)
        return;

    const juce::ScopedLock sl (transportLock_);
    transport_.setPosition (positionSlider_.getValue());
}

void MainComponent::fftSizeChanged()
{
    const int newFft = fftSizeCombo_.getSelectedId();
    if (newFft <= 0)
        return;
    if (sampleRate_ <= 0.0 || samplesPerBlockExpected_ <= 0)
        return;

    engine_.reconfigureFftSize (newFft, sampleRate_, samplesPerBlockExpected_);
    {
        const juce::ScopedLock sl (offlineMutex_);
        offlineEngine_.reconfigureFftSize (newFft, sampleRate_, juce::jmax (samplesPerBlockExpected_, newFft));
    }

    // Update visualizers with the rebuilt tables.
    openGlHost_.setStaticTablesPtr (engine_.tables());
    cpuHost_.setStaticTablesPtr (engine_.tables());
    if (activeRenderer_ != nullptr)
        activeRenderer_->setStaticTablesPtr (engine_.tables());
    updateInstantPreviewChrome();
    bumpInstantPreviewDebounced();
}

void MainComponent::updatePositionFromTransport()
{
    if (inputMode_ != InputMode::FilePlayback || readerSource_ == nullptr)
        return;

    const juce::ScopedLock sl (transportLock_);
    if (transport_.isPlaying())
        positionSlider_.setValue (transport_.getCurrentPosition(), juce::dontSendNotification);
}

void MainComponent::scrollBarMoved (juce::ScrollBar* bar, double newRangeStart)
{
    if (bar != &instantScrollBar_)
        return;

    instantScrollStartSec_.store (newRangeStart, std::memory_order_relaxed);
    bumpInstantPreviewDebounced();
}

void MainComponent::syncOfflineEngineFromLive (double analysisSampleRate)
{
    const auto& L = engine_.state();
    auto& O = offlineEngine_.state();
    const double sr = analysisSampleRate > 0.0 ? analysisSampleRate : sampleRate_;
    const int maxBlock = juce::jmax (samplesPerBlockExpected_, L.fftSize);
    offlineEngine_.reconfigureFftSize (L.fftSize, sr, maxBlock);
    O.audioBufferSize = samplesPerBlockExpected_;
    O.sampleRate = sr;

    O.agcEnabled.store (L.agcEnabled.load (std::memory_order_relaxed), std::memory_order_relaxed);
    O.agcStrength.store (L.agcStrength.load (std::memory_order_relaxed), std::memory_order_relaxed);
    O.setWindowKind (L.windowKind());
    O.setFoldInterpMode (L.foldInterpMode());
    O.setFoldHarmonicWeightMode (L.foldHarmonicWeightMode());
    O.foldMaxOctaves.store (L.foldMaxOctaves.load (std::memory_order_relaxed), std::memory_order_relaxed);
    O.setWaterfallDisplayCurveMode (L.waterfallDisplayCurveMode());
    O.setWaterfallTextureFilterMode (L.waterfallTextureFilterMode());
    O.setChromaShapingMode (L.chromaShapingMode());
    O.setFoldHarmonicModel (L.foldHarmonicModel());
    O.setSpectralBackendMode (L.spectralBackendMode());
    O.analysisEveryNCallbacks.store (L.analysisEveryNCallbacks.load (std::memory_order_relaxed),
                                    std::memory_order_relaxed);
    O.highPassCutoffHz.store (L.highPassCutoffHz.load (std::memory_order_relaxed), std::memory_order_relaxed);
    O.maxHarmonicK.store (L.maxHarmonicK.load (std::memory_order_relaxed), std::memory_order_relaxed);
}

void MainComponent::bumpInstantPreviewDebounced()
{
    if (! instantPreviewToggle_.getToggleState()
        || inputMode_ != InputMode::FilePlayback
        || currentReader_ == nullptr
        || mapComboIdToMode (vizModeCombo_.getSelectedId()) != VisualizationMode::Waterfall
        || sampleRate_ <= 0.0
        || samplesPerBlockExpected_ <= 0)
        return;

    instantDebounce_.startTimer (90);
}

void MainComponent::updateInstantPreviewChrome()
{
    const bool showBar = instantPreviewToggle_.getToggleState() && inputMode_ == InputMode::FilePlayback
                         && currentReader_ != nullptr
                         && mapComboIdToMode (vizModeCombo_.getSelectedId()) == VisualizationMode::Waterfall
                         && sampleRate_ > 0.0 && samplesPerBlockExpected_ > 0;

    if (! showBar)
    {
        if (instantScrollShownLast_)
        {
            instantScrollShownLast_ = false;
            resized();
        }
        return;
    }

    const int hop = samplesPerBlockExpected_
                    * juce::jmax (1, engine_.state().analysisEveryNCallbacks.load (std::memory_order_relaxed));
    const double visibleSec = static_cast<double> (SharedWaterfallRing::kRows * hop) / sampleRate_;
    const double dur = static_cast<double> (currentReader_->lengthInSamples)
                       / juce::jmax (1.0e-9, currentReader_->sampleRate);
    const bool needScroll = dur > visibleSec + 1.0e-9;

    if (needScroll != instantScrollShownLast_)
    {
        instantScrollShownLast_ = needScroll;
        resized();
    }

    if (! needScroll)
        return;

    const double maxStart = juce::jmax (0.0, dur - visibleSec);
    double start = instantScrollStartSec_.load (std::memory_order_relaxed);
    start = juce::jlimit (0.0, maxStart, start);
    instantScrollStartSec_.store (start, std::memory_order_relaxed);

    instantScrollBar_.setRangeLimits (0.0, dur, juce::dontSendNotification);
    instantScrollBar_.setCurrentRange (start, visibleSec, juce::dontSendNotification);
}

void MainComponent::launchInstantWaterfallJob()
{
    if (! instantPreviewToggle_.getToggleState()
        || inputMode_ != InputMode::FilePlayback
        || mapComboIdToMode (vizModeCombo_.getSelectedId()) != VisualizationMode::Waterfall
        || ! currentAudioFileForInstant_.existsAsFile()
        || sampleRate_ <= 0.0
        || samplesPerBlockExpected_ <= 0)
        return;

    ++instantJobGeneration_;
    const std::uint64_t gen = instantJobGeneration_.load (std::memory_order_relaxed);
    const juce::File file = currentAudioFileForInstant_;
    const double scrollStartSec = instantScrollStartSec_.load (std::memory_order_relaxed);
    const int hopDevice = samplesPerBlockExpected_
                          * juce::jmax (1, engine_.state().analysisEveryNCallbacks.load (std::memory_order_relaxed));
    const double deviceSr = sampleRate_;
    const int fftSnap = engine_.state().fftSize;

    juce::Thread::launch ([this, gen, file, scrollStartSec, hopDevice, deviceSr, fftSnap] {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
        if (reader == nullptr)
            return;

        constexpr int kCols = SharedWaterfallRing::kRows;
        constexpr int kBins = SharedWaterfallRing::kRowBins;
        std::vector<float> grid (static_cast<std::size_t> (kCols * kBins), 0.0f);

        const double srFile = reader->sampleRate;
        const int hopFile = juce::jmax (1, (int) std::llround ((double) hopDevice * srFile / deviceSr));

        {
            const juce::ScopedLock sl (offlineMutex_);
            if (gen != instantJobGeneration_.load (std::memory_order_relaxed))
                return;

            syncOfflineEngineFromLive (srFile);
            const int offlineNativeSamples = offlineEngine_.offlineMonoInputSampleCount();
            offlineEngine_.reconfigureFftSize (fftSnap, srFile, juce::jmax (hopFile, juce::jmax (fftSnap, offlineNativeSamples)));
            offlineEngine_.state().audioBufferSize = hopFile;

            const std::int64_t len = reader->lengthInSamples;
            const std::int64_t w0 = (std::int64_t) std::llround (scrollStartSec * srFile);

            std::vector<float> win;
            pitchlab::RenderFrameData frame;

            for (int col = 0; col < kCols; ++col)
            {
                if (gen != instantJobGeneration_.load (std::memory_order_relaxed))
                    return;

                const std::int64_t endSample = w0 + static_cast<std::int64_t> (col + 1) * static_cast<std::int64_t> (hopFile);
                if (endSample <= 0 || len <= 0)
                    continue;

                readMonoFloatWindow (*reader, offlineNativeSamples, endSample, win);
                offlineEngine_.analyzeOfflineWindowFromMonoFloat (std::span<const float> { win.data(), win.size() }, frame);
                std::copy (frame.chromaRow.begin(),
                           frame.chromaRow.end(),
                           grid.begin() + static_cast<std::size_t> (col) * static_cast<std::size_t> (kBins));
            }
        }

        if (gen != instantJobGeneration_.load (std::memory_order_relaxed))
            return;

        juce::MessageManager::callAsync ([this, gen, grid = std::move (grid)]() mutable {
            if (gen != instantJobGeneration_.load (std::memory_order_relaxed))
                return;
            activeRenderer_->commitWaterfallGrid384 (std::span<const float> { grid.data(), grid.size() });
        });
    });
}
