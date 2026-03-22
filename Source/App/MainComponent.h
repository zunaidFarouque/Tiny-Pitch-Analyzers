#pragma once

#include <JuceHeader.h>
#include <vector>

#ifndef PITCHLAB_EXAMPLE_AUDIO_DIR
 #define PITCHLAB_EXAMPLE_AUDIO_DIR ""
#endif

#include "OpenGLVisualizerHost.h"
#include "PitchLabEngine.h"

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

    juce::CriticalSection transportLock_;

    pitchlab::PitchLabEngine engine_;

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
    OpenGLVisualizerHost openGlHost_;

    std::vector<const float*> channelPtrScratch_;
    std::vector<juce::File> exampleAudioFiles_;

    std::unique_ptr<juce::FileChooser> fileChooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
