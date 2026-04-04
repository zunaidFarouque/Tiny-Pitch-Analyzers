#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_opengl/juce_opengl.h>
#include <RenderFrameData.h>
#include <EngineState.h>
#include "IRendererHost.h"
#include "SharedWaterfallRing.h"

#include <array>
#include <atomic>
#include <memory>
#include <span>
#include <vector>

namespace pitchlab
{
class StaticTables;
}

/**
    OpenGL viewport: waveform (D0), waterfall strip (G), batcher + film reel (F).
    New Plan §4.1: if black on Windows with D2D, try heavyweight peer / D2D workarounds.
 */
class OpenGLVisualizerHost final : public juce::Component,
                                   public IRendererHost,
                                   private juce::OpenGLRenderer
{
public:
    OpenGLVisualizerHost();
    ~OpenGLVisualizerHost() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    juce::Component& component() noexcept override { return *this; }
    void setRenderFrame (const pitchlab::RenderFrameData& frame) noexcept override;
    void setStaticTablesPtr (const pitchlab::StaticTables* tables) noexcept override { staticTables_ = tables; }

    void setMode (VisualizationMode m) noexcept override { mode_ = m; }
    [[nodiscard]] VisualizationMode mode() const noexcept override { return mode_; }
    [[nodiscard]] bool isBackendHealthy() const noexcept { return backendHealthy_; }

    // Waterfall tuning: grayscale-only mode uses these to shape brightness + noise gating.
    void setWaterfallEnergyScale (float s) noexcept;
    void setWaterfallAlphaPower (float p) noexcept;
    void setWaterfallAlphaThreshold (float t) noexcept;
    void setWaterfallDisplayCurveMode (pitchlab::WaterfallDisplayCurveMode m) noexcept;
    void setWaterfallTextureFilterMode (pitchlab::WaterfallTextureFilterMode m) noexcept;

    /** Film reel dimensions (New Plan §4.3). */
    static constexpr int kFilmWidth = 1024;
    static constexpr int kFilmHeight = 384;
    /** Fraction of texture width occupied by the 384-bin chroma row (leftmost texels). */
    static constexpr float kChromaTexWidthFraction = 384.0f / static_cast<float> (kFilmWidth);

    /** Upload one chroma row from audio/DSP thread (384 floats or bytes scaled later). */
    void pushWaterfallRow (std::span<const float> row384) override;
    void commitWaterfallGrid384 (std::span<const float> rowMajor384x384) noexcept override;

private:
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

    void renderWaveform();
    void renderWaterfall();
    void renderNeedle();
    void renderStrobeComposite();
    void renderChordMatrix();
    void createFilmTextureIfNeeded();
    void ensureWhiteTexture();
    void ensureStrobeTexture();
    void rebuildAxisOverlayImage();
    void uploadAxisOverlayTextureIfNeeded();
    juce::OpenGLContext openGLContext_;

    const ::pitchlab::StaticTables* staticTables_ = nullptr;
    pitchlab::RenderFrameData latestFrame_{};

    VisualizationMode mode_ = VisualizationMode::Waveform;

    std::unique_ptr<juce::OpenGLShaderProgram> lineShader_;
    std::unique_ptr<juce::OpenGLShaderProgram::Attribute> linePosAttrib_;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> lineColorUniform_;

    std::unique_ptr<juce::OpenGLShaderProgram> texShader_;
    std::unique_ptr<juce::OpenGLShaderProgram::Attribute> texPosAttrib_;
    std::unique_ptr<juce::OpenGLShaderProgram::Attribute> texUvAttrib_;
    std::unique_ptr<juce::OpenGLShaderProgram::Attribute> texColorAttrib_;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> texSamplerUniform_;

    // Waterfall uses a UV-transpose sampling rule so time/history maps to screen X.
    std::unique_ptr<juce::OpenGLShaderProgram> waterfallTexShader_;
    std::unique_ptr<juce::OpenGLShaderProgram::Attribute> waterfallTexPosAttrib_;
    std::unique_ptr<juce::OpenGLShaderProgram::Attribute> waterfallTexUvAttrib_;
    std::unique_ptr<juce::OpenGLShaderProgram::Attribute> waterfallTexColorAttrib_;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> waterfallTexSamplerUniform_;

    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> waterfallEnergyScaleUniform_;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> waterfallAlphaPowerUniform_;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> waterfallAlphaThresholdUniform_;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> waterfallDisplayCurveModeUniform_;

    GLuint lineVbo_ = 0;
    GLuint waterfallVbo_ = 0;
    GLuint dynamicVbo_ = 0;
    GLuint waterfallTex_ = 0;
    GLuint whiteTex_ = 0;
    GLuint strobeTex_ = 0;
    GLuint axisOverlayTex_ = 0;
    int waterfallWriteY_ = 0;

    SharedWaterfallRing waterfallRing_;
    juce::CriticalSection frameLock_;
    juce::CriticalSection waterfallBulkLock_;
    std::vector<float> waterfallBulkPending_;
    std::atomic<bool> waterfallBulkUploadPending_ { false };
    bool backendHealthy_ = false;

    std::atomic<float> waterfallEnergyScale_ { 0.064f };
    std::atomic<float> waterfallAlphaPower_ { 2.55f };
    std::atomic<float> waterfallAlphaThreshold_ { 0.0050f };
    std::atomic<std::uint8_t> waterfallDisplayCurveModeRaw_ { static_cast<std::uint8_t> (pitchlab::WaterfallDisplayCurveMode::Sqrt) };
    std::atomic<std::uint8_t> waterfallTextureFilterModeRaw_ { static_cast<std::uint8_t> (pitchlab::WaterfallTextureFilterMode::Nearest) };

    juce::Image axisOverlayImage_;
    bool axisOverlayDirty_ = true;

    std::vector<float> lineScratch_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpenGLVisualizerHost)
};
