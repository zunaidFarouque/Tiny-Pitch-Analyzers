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

#include "IRendererHost.h"
#include "CpuVisualizerHost.h"
#include "OpenGLVisualizerHost.h"
#include "PitchLabEngine.h"
#include "RendererBackendPolicy.h"

enum class InputMode
{
    LiveMicrophone,
    FilePlayback
};

class MainComponent final : public juce::AudioAppComponent,
                            private juce::Timer
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

    void openFileClicked();
    void loadAudioFile (const juce::File& file);
    void refreshExampleAudioList();
    void exampleComboChanged();

    void playPauseClicked();
    void toggleInputClicked();
    void positionSliderChanged();
    void updatePositionFromTransport();
    void updateDeviceAndPeakLabels();
    void visualizationModeChanged();
    void renderBackendPolicyChanged();
    void syncRendererBackend();

    juce::CriticalSection transportLock_;

    pitchlab::PitchLabEngine engine_;
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
    juce::Label statusLabel_;
    juce::Label engineLabel_;
    juce::ComboBox vizModeCombo_;
    juce::ComboBox windowKindCombo_;
    juce::ComboBox backendCombo_;
    juce::Label audioDeviceLabel_;
    juce::Label peakLabel_;
    OpenGLVisualizerHost openGlHost_;
    CpuVisualizerHost cpuHost_;
    IRendererHost* activeRenderer_ = nullptr;
    RenderBackendPolicy renderBackendPolicy_ = RenderBackendPolicy::Auto;

    std::vector<const float*> channelPtrScratch_;
    std::vector<juce::File> exampleAudioFiles_;

    std::unique_ptr<juce::FileChooser> fileChooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
