#include "OpenGLVisualizerHost.h"

OpenGLVisualizerHost::OpenGLVisualizerHost()
{
    openGLContext_.setOpenGLVersionRequired (juce::OpenGLContext::openGL3_2);
    openGLContext_.setComponentPaintingEnabled (false);
    openGLContext_.setRenderer (this);
    openGLContext_.attachTo (*this);
    openGLContext_.setContinuousRepainting (true);
}

OpenGLVisualizerHost::~OpenGLVisualizerHost()
{
    openGLContext_.detach();
}

void OpenGLVisualizerHost::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void OpenGLVisualizerHost::newOpenGLContextCreated() {}

void OpenGLVisualizerHost::renderOpenGL()
{
    juce::OpenGLHelpers::clear (juce::Colours::darkgrey);
}

void OpenGLVisualizerHost::openGLContextClosing() {}
