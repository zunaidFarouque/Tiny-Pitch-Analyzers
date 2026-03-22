#pragma once

#include <JuceHeader.h>

/**
    Minimal OpenGL viewport for future waterfall / batcher (New Plan §4.1).
    If this region is black on Windows with D2D, try heavyweight peer / D2D workarounds from the roadmap.
 */
class OpenGLVisualizerHost final : public juce::Component,
                                   private juce::OpenGLRenderer
{
public:
    OpenGLVisualizerHost();
    ~OpenGLVisualizerHost() override;

    void paint (juce::Graphics& g) override;

private:
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

    juce::OpenGLContext openGLContext_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpenGLVisualizerHost)
};
