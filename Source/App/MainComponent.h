#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <atomic>
#include <vector>


#ifndef PITCHLAB_EXAMPLE_AUDIO_DIR
 #define PITCHLAB_EXAMPLE_AUDIO_DIR ""
#endif
#ifndef PITCHLAB_TEST_ASSETS_DIR
 #define PITCHLAB_TEST_ASSETS_DIR ""
#endif
#ifndef PITCHLAB_DEFAULT_EXAMPLE_WAV
 #define PITCHLAB_DEFAULT_EXAMPLE_WAV ""
#endif

#include "IRendererHost.h"
#include "CpuVisualizerHost.h"
#include "OpenGLVisualizerHost.h"
#include "AnalysisModelIds.h"
#include "PitchLabEngine.h"
#include "RendererBackendPolicy.h"

enum class InputMode
{
    LiveMicrophone,
    FilePlayback
};

class MainComponent final : public juce::AudioAppComponent,
                            private juce::Timer,
                            private juce::ScrollBar::Listener
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void scrollBarMoved (juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart) override;

    void openFileClicked();
    void loadAudioFile (const juce::File& file);
    void refreshExampleAudioList();
    void exampleComboChanged();

    void playPauseClicked();
    void toggleInputClicked();
    void positionSliderChanged();
    void fftSizeChanged();
    void updatePositionFromTransport();
    void updateDeviceAndPeakLabels();
    void audioSettingsClicked();
    void saveAudioDeviceState() const;
    void visualizationModeChanged();
    void renderBackendPolicyChanged();
    void syncRendererBackend();

    void updateInstantPreviewChrome();
    void bumpInstantPreviewDebounced();
    void launchInstantWaterfallJob();
    void syncOfflineEngineFromLive (double analysisSampleRate = -1.0);

    bool instantScrollShownLast_ = false;

    struct InstantDebounce final : juce::Timer
    {
        explicit InstantDebounce (MainComponent& o) : owner (o) {}
        void timerCallback() override
        {
            stopTimer();
            owner.launchInstantWaterfallJob();
        }
        MainComponent& owner;
    };

    juce::CriticalSection transportLock_;
    juce::CriticalSection offlineMutex_;

    pitchlab::PitchLabEngine engine_;
    pitchlab::PitchLabEngine offlineEngine_;
    std::atomic<float> audioPeakHold_ { 0.0f };

    juce::AudioFormatManager formatManager_;
    std::unique_ptr<juce::AudioFormatReader> currentReader_;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource_;
    juce::AudioTransportSource transport_;

    InputMode inputMode_ = InputMode::LiveMicrophone;

    juce::TextButton openButton_ { "Open file..." };
    juce::ComboBox exampleCombo_;
    juce::TextButton playPauseButton_ { "Play" };
    juce::TextButton inputToggleButton_ { "Input: Mic" };
    juce::Slider positionSlider_;
    juce::ComboBox fftSizeCombo_;
    juce::Label fftSizeLabel_;
    double sampleRate_ { 0.0 };
    int samplesPerBlockExpected_ { 0 };
    juce::Slider waterfallEnergyLowSlider_;
    juce::Slider waterfallEnergyHighSlider_;
    juce::Slider waterfallAlphaPowLowSlider_;
    juce::Slider waterfallAlphaPowHighSlider_;
    juce::Slider waterfallAlphaThresholdSlider_;
    juce::Label waterfallEnergyLowLabel_;
    juce::Label waterfallEnergyHighLabel_;
    juce::Label waterfallAlphaPowLowLabel_;
    juce::Label waterfallAlphaPowHighLabel_;
    juce::Label waterfallAlphaThresholdLabel_;
    juce::Slider waterfallShapingFreqLogSlider_;
    juce::Label waterfallShapingFreqLogLabel_;
    juce::ToggleButton preEmphasisToggle_ { "Pre-emphasis" };
    juce::ToggleButton spectralSmearingToggle_ { "Spectral smear" };
    juce::ToggleButton agcEnabledToggle_ { "AGC" };
    juce::Slider agcStrengthSlider_;
    juce::Label agcStrengthLabel_;
    juce::ComboBox foldInterpCombo_;
    juce::ComboBox foldWeightCombo_;
    juce::ComboBox foldOctavesCombo_;
    juce::ComboBox waterfallFilterCombo_;
    juce::ComboBox waterfallCurveCombo_;
    juce::ComboBox chromaShapingCombo_;
    juce::ComboBox foldModelCombo_;
    juce::ComboBox spectralBackendCombo_;
    juce::ComboBox analysisRateCombo_;
    juce::Slider highPassSlider_;
    juce::Label highPassLabel_;
    juce::Label statusLabel_;
    juce::Label engineLabel_;
    juce::ComboBox vizModeCombo_;
    juce::ComboBox windowKindCombo_;
    juce::ComboBox backendCombo_;
    juce::TextButton audioSettingsButton_ { "Audio..." };
    juce::Label audioDeviceLabel_;
    juce::Label peakLabel_;
    juce::ToggleButton instantPreviewToggle_ { "Instant full-file waterfall" };
    juce::ScrollBar instantScrollBar_ { false };
    OpenGLVisualizerHost openGlHost_;
    CpuVisualizerHost cpuHost_;
    IRendererHost* activeRenderer_ = nullptr;
    RenderBackendPolicy renderBackendPolicy_ = RenderBackendPolicy::Auto;

    std::vector<const float*> channelPtrScratch_;
    std::vector<juce::File> exampleAudioFiles_;

    juce::File currentAudioFileForInstant_;
    InstantDebounce instantDebounce_ { *this };
    std::atomic<std::uint64_t> instantJobGeneration_ { 0 };
    std::atomic<double> instantScrollStartSec_ { 0.0 };

    std::unique_ptr<juce::FileChooser> fileChooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
