#include "MainComponent.h"

#include <algorithm>

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
} // namespace

MainComponent::MainComponent()
{
    formatManager_.registerBasicFormats();

    addAndMakeVisible (openButton_);
    addAndMakeVisible (exampleCombo_);
    addAndMakeVisible (playPauseButton_);
    addAndMakeVisible (inputToggleButton_);
    addAndMakeVisible (positionSlider_);
    addAndMakeVisible (statusLabel_);
    addAndMakeVisible (engineLabel_);
    addAndMakeVisible (openGlHost_);

    openButton_.onClick = [this] { openFileClicked(); };
    exampleCombo_.onChange = [this] { exampleComboChanged(); };
    playPauseButton_.onClick = [this] { playPauseClicked(); };
    inputToggleButton_.onClick = [this] { toggleInputClicked(); };

    positionSlider_.setRange (0.0, 1.0, 0.0001);
    positionSlider_.setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 20);
    positionSlider_.textFromValueFunction = [] (double secs) { return juce::String (secs, 2) + " s"; };
    positionSlider_.onValueChange = [this] { positionSliderChanged(); };

    statusLabel_.setJustificationType (juce::Justification::centredLeft);
    engineLabel_.setJustificationType (juce::Justification::centredLeft);
    engineLabel_.setFont (juce::FontOptions { 13.0f });

    setSize (800, 600);
    startTimerHz (20);
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
    engine_.prepareToPlay (sampleRate, samplesPerBlockExpected);

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
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto glArea = r.removeFromBottom (220);
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
    positionSlider_.setBounds (r.removeFromTop (28));
    r.removeFromTop (8);
    statusLabel_.setBounds (r.removeFromTop (24));
    engineLabel_.setBounds (r.removeFromTop (22));

    openGlHost_.setBounds (glArea);
}

void MainComponent::timerCallback()
{
    updatePositionFromTransport();

    engineLabel_.setText ("Engine " + juce::String (pitchlab::engineVersionString())
                              + (engine_.state().analysisDirty ? "  (analysis dirty)" : ""),
                          juce::dontSendNotification);
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

void MainComponent::updatePositionFromTransport()
{
    if (inputMode_ != InputMode::FilePlayback || readerSource_ == nullptr)
        return;

    const juce::ScopedLock sl (transportLock_);
    if (transport_.isPlaying())
        positionSlider_.setValue (transport_.getCurrentPosition(), juce::dontSendNotification);
}
