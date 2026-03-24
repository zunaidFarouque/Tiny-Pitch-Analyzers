#include "MainComponent.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <span>

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
    addAndMakeVisible (statusLabel_);
    addAndMakeVisible (engineLabel_);
    addAndMakeVisible (vizModeCombo_);
    addAndMakeVisible (windowKindCombo_);
    addAndMakeVisible (backendCombo_);
    addAndMakeVisible (audioDeviceLabel_);
    addAndMakeVisible (peakLabel_);
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
    windowKindCombo_.setSelectedId (1, juce::dontSendNotification);
    windowKindCombo_.onChange = [this] {
        const int id = windowKindCombo_.getSelectedId();
        engine_.state().setWindowKind (id == 2 ? pitchlab::WindowKind::Gaussian : pitchlab::WindowKind::Hanning);
    };

    backendCombo_.addItem ("Backend: Auto", 1);
    backendCombo_.addItem ("Backend: GPU", 2);
    backendCombo_.addItem ("Backend: CPU", 3);
    backendCombo_.setSelectedId (2, juce::dontSendNotification);
    backendCombo_.onChange = [this] { renderBackendPolicyChanged(); };

    renderBackendPolicy_ = RenderBackendPolicy::ForceGpu;
    activeRenderer_->setMode (VisualizationMode::Waterfall);
    cpuHost_.setMode (VisualizationMode::Waterfall);

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
    fftSizeCombo_.setSelectedId (engine_.state().fftSize, juce::dontSendNotification);
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

    waterfallEnergyScaleSlider_.setValue (0.03, juce::dontSendNotification);
    waterfallAlphaPowerSlider_.setValue (1.0, juce::dontSendNotification);
    waterfallAlphaThresholdSlider_.setValue (0.0050, juce::dontSendNotification);

    waterfallEnergyScaleSlider_.onValueChange = [this] {
        openGlHost_.setWaterfallEnergyScale ((float) waterfallEnergyScaleSlider_.getValue());
    };
    waterfallAlphaPowerSlider_.onValueChange = [this] {
        openGlHost_.setWaterfallAlphaPower ((float) waterfallAlphaPowerSlider_.getValue());
    };
    waterfallAlphaThresholdSlider_.onValueChange = [this] {
        openGlHost_.setWaterfallAlphaThreshold ((float) waterfallAlphaThresholdSlider_.getValue());
    };

    statusLabel_.setJustificationType (juce::Justification::centredLeft);
    engineLabel_.setJustificationType (juce::Justification::centredLeft);
    engineLabel_.setFont (juce::FontOptions { 13.0f });

    setSize (800, 600);
    startTimerHz (30);
    setAudioChannels (2, 2);

    refreshExampleAudioList();
}

MainComponent::~MainComponent()
{
    stopTimer();
    shutdownAudio();
}

void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    sampleRate_ = sampleRate;
    samplesPerBlockExpected_ = samplesPerBlockExpected;
    engine_.prepareToPlay (sampleRate, samplesPerBlockExpected);
    openGlHost_.setStaticTablesPtr (engine_.tables());
    cpuHost_.setStaticTablesPtr (engine_.tables());
    syncRendererBackend();

    const juce::ScopedLock sl (transportLock_);
    transport_.prepareToPlay (samplesPerBlockExpected, sampleRate);
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
    audioDeviceLabel_.setBounds (row2.reduced (0, 2));
    r.removeFromTop (6);

    auto fftRow = r.removeFromTop (22);
    fftSizeLabel_.setBounds (fftRow.removeFromLeft (90).reduced (0, 2));
    fftSizeCombo_.setBounds (fftRow.reduced (0, 2));

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

    openGlHost_.setBounds (r);
    cpuHost_.setBounds (r);
}

void MainComponent::timerCallback()
{
    syncRendererBackend();
    updatePositionFromTransport();
    updateDeviceAndPeakLabels();

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

void MainComponent::visualizationModeChanged()
{
    const auto m = mapComboIdToMode (vizModeCombo_.getSelectedId());
    openGlHost_.setMode (m);
    cpuHost_.setMode (m);
    activeRenderer_->setMode (m);
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
}

void MainComponent::refreshExampleAudioList()
{
    exampleCombo_.clear (juce::dontSendNotification);
    exampleAudioFiles_.clear();
    exampleCombo_.addItem ("Example clips...", 1);

    const juce::File dir (PITCHLAB_EXAMPLE_AUDIO_DIR);
    if (! dir.isDirectory())
    {
        exampleCombo_.setSelectedId (1, juce::dontSendNotification);
        return;
    }

    juce::Array<juce::File> found;
    dir.findChildFiles (found, juce::File::findFiles, false);

    std::vector<juce::File> filtered;
    filtered.reserve (static_cast<std::size_t> (found.size()));

    for (const auto& f : found)
    {
        if (! f.existsAsFile())
            continue;

        auto ext = f.getFileExtension().toLowerCase().trimCharactersAtStart (".");
        if (! isAllowedAudioExtension (ext))
            continue;

        filtered.push_back (f);
    }

    std::sort (filtered.begin(), filtered.end(), [] (const juce::File& a, const juce::File& b) {
        return a.getFileName().compareNatural (b.getFileName()) < 0;
    });

    int nextId = 2;
    for (const auto& f : filtered)
    {
        exampleAudioFiles_.push_back (f);
        exampleCombo_.addItem (f.getFileName(), nextId++);
    }

    exampleCombo_.setSelectedId (1, juce::dontSendNotification);
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
    }
    else
    {
        transport_.stop();

        const juce::ScopedLock sl (transportLock_);
        transport_.setSource (nullptr, 0, nullptr, 0.0);
        inputMode_ = InputMode::LiveMicrophone;
        inputToggleButton_.setButtonText ("Input: Mic");
        playPauseButton_.setButtonText ("Play");
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

    // Update visualizers with the rebuilt tables.
    openGlHost_.setStaticTablesPtr (engine_.tables());
    cpuHost_.setStaticTablesPtr (engine_.tables());
    if (activeRenderer_ != nullptr)
        activeRenderer_->setStaticTablesPtr (engine_.tables());
}

void MainComponent::updatePositionFromTransport()
{
    if (inputMode_ != InputMode::FilePlayback || readerSource_ == nullptr)
        return;

    const juce::ScopedLock sl (transportLock_);
    if (transport_.isPlaying())
        positionSlider_.setValue (transport_.getCurrentPosition(), juce::dontSendNotification);
}
