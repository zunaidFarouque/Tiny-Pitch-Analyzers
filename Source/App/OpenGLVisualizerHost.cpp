#include "OpenGLVisualizerHost.h"

#include "LegacyGLBatcher.h"
#include "StaticTables.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
#if JUCE_OPENGL_ES
static constexpr char kLineVert[] = R"(
attribute vec2 position;
void main()
{
    gl_Position = vec4(position, 0.0, 1.0);
}
)";
static constexpr char kLineFrag[] = R"(
uniform lowp vec4 color;
void main()
{
    gl_FragColor = color;
}
)";
static constexpr char kTexVert[] = R"(
attribute vec2 position;
attribute vec2 uv;
attribute vec4 color;
varying lowp vec2 vUV;
varying lowp vec4 vCol;
void main()
{
    gl_Position = vec4(position, 0.0, 1.0);
    vUV = uv;
    vCol = color;
}
)";
static constexpr char kTexFrag[] = R"(
uniform sampler2D tex;
varying lowp vec2 vUV;
varying lowp vec4 vCol;
void main()
{
    gl_FragColor = texture2D(tex, vUV) * vCol;
}
)";

static constexpr char kWaterfallTexFrag[] = R"(
uniform sampler2D tex;
uniform float uEnergyScale;
uniform float uAlphaPower;
uniform float uAlphaThreshold;
uniform float uDisplayCurveMode;
varying lowp vec2 vUV;
varying lowp vec4 vCol;
void main()
{
    const float e = texture2D (tex, vec2 (vUV.y, vUV.x)).r;
    const float x = max (e, 0.0);
    float m = clamp (x * uEnergyScale, 0.0, 1.0);
    if (uDisplayCurveMode > 0.5 && uDisplayCurveMode < 1.5)
        m = clamp (sqrt (x) * uEnergyScale, 0.0, 1.0);
    else if (uDisplayCurveMode >= 1.5)
    {
        // Approximate dB-domain compression for improved line separation.
        const float db = 20.0 * log (max (x, 1.0e-12)) / log (10.0);
        m = clamp ((db + 100.0) / 100.0, 0.0, 1.0);
    }
    // More intense => brighter pixel.
    float alpha = pow (m, uAlphaPower);
    // Suppress low-energy speckle/noise.
    alpha = (alpha < uAlphaThreshold) ? 0.0 : alpha;
    // Monochrome waterfall: constant white RGB, intensity controls brightness via alpha.
    gl_FragColor = vec4 (1.0, 1.0, 1.0, alpha);
}
)";
#else
static constexpr char kLineVert[] = R"(#version 150 core
in vec2 position;
void main()
{
    gl_Position = vec4(position, 0.0, 1.0);
}
)";
static constexpr char kLineFrag[] = R"(#version 150 core
uniform vec4 color;
out vec4 fragColor;
void main()
{
    fragColor = color;
}
)";
static constexpr char kTexVert[] = R"(#version 150 core
in vec2 position;
in vec2 uv;
in vec4 color;
out vec2 vUV;
out vec4 vCol;
void main()
{
    gl_Position = vec4(position, 0.0, 1.0);
    vUV = uv;
    vCol = color;
}
)";
static constexpr char kTexFrag[] = R"(#version 150 core
uniform sampler2D tex;
in vec2 vUV;
in vec4 vCol;
out vec4 fragColor;
void main()
{
    fragColor = texture(tex, vUV) * vCol;
}
)";

static constexpr char kWaterfallTexFrag[] = R"(#version 150 core
uniform sampler2D tex;
uniform float uEnergyScale;
uniform float uAlphaPower;
uniform float uAlphaThreshold;
uniform float uDisplayCurveMode;
in vec2 vUV;
in vec4 vCol;
out vec4 fragColor;
void main()
{
    float e = texture (tex, vec2 (vUV.y, vUV.x)).r;
    float x = max (e, 0.0);
    float m = clamp (x * uEnergyScale, 0.0, 1.0);
    if (uDisplayCurveMode > 0.5 && uDisplayCurveMode < 1.5)
        m = clamp (sqrt (x) * uEnergyScale, 0.0, 1.0);
    else if (uDisplayCurveMode >= 1.5)
    {
        float db = 20.0 * log (max (x, 1.0e-12)) / log (10.0);
        m = clamp ((db + 100.0) / 100.0, 0.0, 1.0);
    }
    // PitchLab-style compositing: intensity drives alpha, not an always-on lane-color underlay.
    // Compress low energies so empty background stays near-black.
    float alpha = pow (m, uAlphaPower);
    // Suppress low-energy speckle/noise.
    alpha = (alpha < uAlphaThreshold) ? 0.0 : alpha;
    // Monochrome waterfall: constant white RGB, intensity controls brightness via alpha.
    fragColor = vec4 (1.0, 1.0, 1.0, alpha);
}
)";
#endif
} // namespace

OpenGLVisualizerHost::OpenGLVisualizerHost()
{
    openGLContext_.setOpenGLVersionRequired (juce::OpenGLContext::openGL3_2);
    openGLContext_.setComponentPaintingEnabled (false);
    openGLContext_.setRenderer (this);
    openGLContext_.attachTo (*this);
    openGLContext_.setContinuousRepainting (true);

    lineScratch_.resize (static_cast<std::size_t> (pitchlab::RenderFrameData::kWaveformSamples * 2));
}

OpenGLVisualizerHost::~OpenGLVisualizerHost()
{
    openGLContext_.detach();
}

void OpenGLVisualizerHost::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void OpenGLVisualizerHost::resized()
{
    axisOverlayDirty_ = true;
    rebuildAxisOverlayImage();
}

void OpenGLVisualizerHost::setRenderFrame (const pitchlab::RenderFrameData& frame) noexcept
{
    const juce::ScopedLock sl (frameLock_);
    latestFrame_ = frame;
}

void OpenGLVisualizerHost::newOpenGLContextCreated()
{
    backendHealthy_ = false;
    lineShader_ = std::make_unique<juce::OpenGLShaderProgram> (openGLContext_);
    const bool lineVsOk = lineShader_->addVertexShader (kLineVert);
    const bool lineFsOk = lineShader_->addFragmentShader (kLineFrag);
    const bool lineLinkOk = lineShader_->link();

    linePosAttrib_ = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*lineShader_, "position");
    lineColorUniform_ = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*lineShader_, "color");

    texShader_ = std::make_unique<juce::OpenGLShaderProgram> (openGLContext_);
    const bool texVsOk = texShader_->addVertexShader (kTexVert);
    const bool texFsOk = texShader_->addFragmentShader (kTexFrag);
    const bool texLinkOk = texShader_->link();

    texPosAttrib_ = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*texShader_, "position");
    texUvAttrib_ = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*texShader_, "uv");
    texColorAttrib_ = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*texShader_, "color");

    texSamplerUniform_ = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*texShader_, "tex");

    waterfallTexShader_ = std::make_unique<juce::OpenGLShaderProgram> (openGLContext_);
    const bool waterfallVsOk = waterfallTexShader_->addVertexShader (kTexVert);
    const bool waterfallFsOk = waterfallTexShader_->addFragmentShader (kWaterfallTexFrag);
    const bool waterfallLinkOk = waterfallTexShader_->link();

    waterfallTexPosAttrib_ = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*waterfallTexShader_, "position");
    waterfallTexUvAttrib_ = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*waterfallTexShader_, "uv");
    waterfallTexColorAttrib_ = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*waterfallTexShader_, "color");
    waterfallTexSamplerUniform_ = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*waterfallTexShader_, "tex");

    waterfallEnergyScaleUniform_ = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*waterfallTexShader_, "uEnergyScale");
    waterfallAlphaPowerUniform_ = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*waterfallTexShader_, "uAlphaPower");
    waterfallAlphaThresholdUniform_ = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*waterfallTexShader_, "uAlphaThreshold");
    waterfallDisplayCurveModeUniform_ = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*waterfallTexShader_, "uDisplayCurveMode");

    auto& gl = openGLContext_.extensions;
    gl.glGenBuffers (1, &lineVbo_);
    gl.glGenBuffers (1, &waterfallVbo_);
    gl.glGenBuffers (1, &dynamicVbo_);
    gl.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, waterfallVbo_);
    gl.glBufferData (juce::gl::GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr> (sizeof (float) * static_cast<std::size_t> (12 * 6 * 8)),
                     nullptr,
                     juce::gl::GL_DYNAMIC_DRAW);
    gl.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, 0);

    backendHealthy_ = lineVsOk && lineFsOk && lineLinkOk
                      && texVsOk && texFsOk && texLinkOk
                      && waterfallVsOk && waterfallFsOk && waterfallLinkOk
                      && linePosAttrib_ != nullptr && lineColorUniform_ != nullptr
                      && texPosAttrib_ != nullptr && texUvAttrib_ != nullptr
                      && texColorAttrib_ != nullptr && texSamplerUniform_ != nullptr
                      && waterfallTexPosAttrib_ != nullptr && waterfallTexUvAttrib_ != nullptr
                      && waterfallTexColorAttrib_ != nullptr && waterfallTexSamplerUniform_ != nullptr
                      && waterfallEnergyScaleUniform_ != nullptr && waterfallAlphaPowerUniform_ != nullptr
                      && waterfallAlphaThresholdUniform_ != nullptr && waterfallDisplayCurveModeUniform_ != nullptr
                      && lineVbo_ != 0 && waterfallVbo_ != 0 && dynamicVbo_ != 0;
}

void OpenGLVisualizerHost::setWaterfallEnergyScale (float s) noexcept
{
    waterfallEnergyScale_.store (juce::jmax (0.0f, s), std::memory_order_relaxed);
}

void OpenGLVisualizerHost::setWaterfallAlphaPower (float p) noexcept
{
    waterfallAlphaPower_.store (juce::jmax (0.0f, p), std::memory_order_relaxed);
}

void OpenGLVisualizerHost::setWaterfallAlphaThreshold (float t) noexcept
{
    waterfallAlphaThreshold_.store (juce::jmax (0.0f, t), std::memory_order_relaxed);
}

void OpenGLVisualizerHost::setWaterfallDisplayCurveMode (pitchlab::WaterfallDisplayCurveMode m) noexcept
{
    waterfallDisplayCurveModeRaw_.store (static_cast<std::uint8_t> (m), std::memory_order_relaxed);
}

void OpenGLVisualizerHost::setWaterfallTextureFilterMode (pitchlab::WaterfallTextureFilterMode m) noexcept
{
    waterfallTextureFilterModeRaw_.store (static_cast<std::uint8_t> (m), std::memory_order_relaxed);
}

void OpenGLVisualizerHost::openGLContextClosing()
{
    auto& gl = openGLContext_.extensions;
    if (lineVbo_ != 0)
    {
        gl.glDeleteBuffers (1, &lineVbo_);
        lineVbo_ = 0;
    }

    if (waterfallVbo_ != 0)
    {
        gl.glDeleteBuffers (1, &waterfallVbo_);
        waterfallVbo_ = 0;
    }
    if (dynamicVbo_ != 0)
    {
        gl.glDeleteBuffers (1, &dynamicVbo_);
        dynamicVbo_ = 0;
    }

    if (whiteTex_ != 0)
    {
        using namespace juce::gl;
        glDeleteTextures (1, &whiteTex_);
        whiteTex_ = 0;
    }

    if (strobeTex_ != 0)
    {
        using namespace juce::gl;
        glDeleteTextures (1, &strobeTex_);
        strobeTex_ = 0;
    }

    if (waterfallTex_ != 0)
    {
        using namespace juce::gl;
        glDeleteTextures (1, &waterfallTex_);
        waterfallTex_ = 0;
    }

    if (axisOverlayTex_ != 0)
    {
        using namespace juce::gl;
        glDeleteTextures (1, &axisOverlayTex_);
        axisOverlayTex_ = 0;
    }

    lineShader_.reset();
    linePosAttrib_.reset();
    lineColorUniform_.reset();
    texShader_.reset();
    texPosAttrib_.reset();
    texUvAttrib_.reset();
    texColorAttrib_.reset();
    texSamplerUniform_.reset();

    waterfallTexShader_.reset();
    waterfallTexPosAttrib_.reset();
    waterfallTexUvAttrib_.reset();
    waterfallTexColorAttrib_.reset();
    waterfallTexSamplerUniform_.reset();
    waterfallEnergyScaleUniform_.reset();
    waterfallAlphaPowerUniform_.reset();
    waterfallAlphaThresholdUniform_.reset();
    waterfallDisplayCurveModeUniform_.reset();
}

void OpenGLVisualizerHost::pushWaterfallRow (std::span<const float> row384)
{
    waterfallRing_.pushRow (row384);
}

void OpenGLVisualizerHost::commitWaterfallGrid384 (std::span<const float> rowMajor384x384) noexcept
{
    constexpr int kCells = SharedWaterfallRing::kRowBins * SharedWaterfallRing::kRows;
    if (rowMajor384x384.size() < static_cast<std::size_t> (kCells))
        return;

    {
        const juce::ScopedLock sl (waterfallBulkLock_);
        waterfallBulkPending_.assign (rowMajor384x384.begin(),
                                      rowMajor384x384.begin() + static_cast<std::ptrdiff_t> (kCells));
    }
    waterfallBulkUploadPending_.store (true, std::memory_order_release);
}

void OpenGLVisualizerHost::createFilmTextureIfNeeded()
{
    if (waterfallTex_ != 0)
        return;

    using namespace juce::gl;
    glGenTextures (1, &waterfallTex_);
    glBindTexture (GL_TEXTURE_2D, waterfallTex_);
    const auto filter = static_cast<pitchlab::WaterfallTextureFilterMode> (waterfallTextureFilterModeRaw_.load (std::memory_order_relaxed));
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter == pitchlab::WaterfallTextureFilterMode::Nearest ? GL_NEAREST : GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter == pitchlab::WaterfallTextureFilterMode::Nearest ? GL_NEAREST : GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    std::vector<float> zeros (static_cast<std::size_t> (kFilmWidth * kFilmHeight), 0.0f);
    glTexImage2D (GL_TEXTURE_2D,
                  0,
#if JUCE_OPENGL_ES
                  GL_LUMINANCE,
#else
                  GL_R32F,
#endif
                  kFilmWidth,
                  kFilmHeight,
                  0,
#if JUCE_OPENGL_ES
                  GL_LUMINANCE,
#else
                  GL_RED,
#endif
                  GL_FLOAT,
                  zeros.data());

    glBindTexture (GL_TEXTURE_2D, 0);
    waterfallWriteY_ = waterfallRing_.writeY();
}

void OpenGLVisualizerHost::renderOpenGL()
{
    if (! backendHealthy_)
        return;

    using namespace juce::gl;
    // Let JUCE manage the OpenGL viewport (it accounts for HiDPI scaling).

    // Black background per grayscale waterfall requirements.
    glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
    glClear (GL_COLOR_BUFFER_BIT);

    if (waterfallBulkUploadPending_.exchange (false, std::memory_order_acq_rel))
    {
        createFilmTextureIfNeeded();
        std::vector<float> bulk;
        {
            const juce::ScopedLock sl (waterfallBulkLock_);
            bulk.swap (waterfallBulkPending_);
        }

        if (waterfallTex_ != 0 && static_cast<int> (bulk.size()) >= SharedWaterfallRing::kRowBins * SharedWaterfallRing::kRows)
        {
            using namespace juce::gl;
            glBindTexture (GL_TEXTURE_2D, waterfallTex_);
            for (int y = 0; y < SharedWaterfallRing::kRows; ++y)
            {
                glTexSubImage2D (GL_TEXTURE_2D,
                                 0,
                                 0,
                                 y,
                                 SharedWaterfallRing::kRowBins,
                                 1,
#if JUCE_OPENGL_ES
                                 GL_LUMINANCE,
#else
                                 GL_RED,
#endif
                                 GL_FLOAT,
                                 bulk.data() + static_cast<std::size_t> (y) * static_cast<std::size_t> (SharedWaterfallRing::kRowBins));
            }
            glBindTexture (GL_TEXTURE_2D, 0);
            waterfallRing_.syncWriteHeadAfterBulkStaticFill();
        }
    }

    std::array<float, 384> row {};
    int rowY = 0;
    if (waterfallRing_.consumePendingRow (row, rowY) && waterfallTex_ != 0)
    {
        using namespace juce::gl;
        glBindTexture (GL_TEXTURE_2D, waterfallTex_);
        glTexSubImage2D (GL_TEXTURE_2D,
                         0,
                         0,
                         rowY,
                         384,
                         1,
#if JUCE_OPENGL_ES
                         GL_LUMINANCE,
#else
                         GL_RED,
#endif
                         GL_FLOAT,
                         row.data());
        glBindTexture (GL_TEXTURE_2D, 0);
    }
    waterfallWriteY_ = waterfallRing_.writeY();

    if (mode_ == VisualizationMode::Waterfall)
        createFilmTextureIfNeeded();

    switch (mode_)
    {
        case VisualizationMode::Waveform:
            renderWaveform();
            break;
        case VisualizationMode::Waterfall:
            renderWaterfall();
            break;
        case VisualizationMode::Needle:
            renderNeedle();
            break;
        case VisualizationMode::StrobeRadial:
            renderStrobeComposite();
            break;
        case VisualizationMode::ChordMatrix:
            renderChordMatrix();
            break;
        default:
            renderWaveform();
            break;
    }
}

void OpenGLVisualizerHost::renderWaveform()
{
    if (lineShader_ == nullptr || linePosAttrib_ == nullptr
        || lineColorUniform_ == nullptr || lineVbo_ == 0)
        return;

    pitchlab::RenderFrameData frame;
    {
        const juce::ScopedLock sl (frameLock_);
        frame = latestFrame_;
    }

    const int n = pitchlab::RenderFrameData::kWaveformSamples;
    if (n < 2)
        return;

    for (int i = 0; i < n; ++i)
    {
        const float t = static_cast<float> (i) / static_cast<float> (n - 1);
        lineScratch_[static_cast<std::size_t> (i * 2 + 0)] = -1.0f + 2.0f * t;
        const float amp = static_cast<float> (frame.waveform[static_cast<std::size_t> (i)]) * (1.0f / 32768.0f);
        lineScratch_[static_cast<std::size_t> (i * 2 + 1)] = juce::jlimit (-1.0f, 1.0f, amp * 0.9f);
    }

    using namespace juce::gl;

    auto& gl = openGLContext_.extensions;
    gl.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, lineVbo_);
    gl.glBufferData (juce::gl::GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr> (sizeof (float) * static_cast<std::size_t> (n * 2)),
                     lineScratch_.data(),
                     juce::gl::GL_STREAM_DRAW);

    lineShader_->use();
    lineColorUniform_->set (0.35f, 0.85f, 1.0f, 1.0f);

    gl.glEnableVertexAttribArray ((GLuint) linePosAttrib_->attributeID);
    gl.glVertexAttribPointer ((GLuint) linePosAttrib_->attributeID,
                              2,
                              juce::gl::GL_FLOAT,
                              juce::gl::GL_FALSE,
                              0,
                              nullptr);
    juce::gl::glDrawArrays (juce::gl::GL_LINE_STRIP, 0, n);
    gl.glDisableVertexAttribArray ((GLuint) linePosAttrib_->attributeID);
    gl.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, 0);
}

void OpenGLVisualizerHost::renderWaterfall()
{
    createFilmTextureIfNeeded();

    if (waterfallTexShader_ == nullptr || waterfallTexPosAttrib_ == nullptr || waterfallTexUvAttrib_ == nullptr
        || waterfallTexColorAttrib_ == nullptr || waterfallTexSamplerUniform_ == nullptr || waterfallTex_ == 0
        || waterfallVbo_ == 0)
        return;

    LegacyGLBatcher batch;
    const float uSpan = kChromaTexWidthFraction;
    const int newestRowIndex = (waterfallRing_.writeY() - 1 + SharedWaterfallRing::kRows) % SharedWaterfallRing::kRows;
    const float tNewest = static_cast<float> (newestRowIndex) / static_cast<float> (kFilmHeight);
    const float tTexStep = 1.0f / static_cast<float> (kFilmHeight);
    // Make leftmost and rightmost texels sample different history rows (avoid GL_REPEAT edge duplication).
    const float tLeft = tNewest - 1.0f + tTexStep;
    const float tRight = tNewest;

    for (int note = 0; note < 12; ++note)
    {
        const float y0 = -1.0f + static_cast<float> (note) * (2.0f / 12.0f);
        const float y1 = -1.0f + static_cast<float> (note + 1) * (2.0f / 12.0f);
        // Chroma bin folding: texture U slice for this note lane.
        const float s0 = (static_cast<float> (note) / 12.0f) * uSpan;
        const float s1 = (static_cast<float> (note + 1) / 12.0f) * uSpan;
        // With the waterfall shader UV-transpose: vertex UV.x -> texture V (time/history), vertex UV.y -> texture U (chroma).
        // Monochrome mode: quads are white; shader alpha controls actual brightness.
        const juce::Colour laneColor = juce::Colours::white.withAlpha (1.0f);
        batch.addQuad (-1.0f, y0, 2.0f, y1 - y0, tLeft, s0, tRight, s1, laneColor);
    }

    if (batch.empty())
        return;

    using namespace juce::gl;

    // Waterfall uses alpha intensity pixels over a dark background.
    juce::gl::glEnable (GL_BLEND);
    juce::gl::glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    juce::gl::glActiveTexture (juce::gl::GL_TEXTURE0);
    juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, waterfallTex_);
    {
        const auto filter = static_cast<pitchlab::WaterfallTextureFilterMode> (waterfallTextureFilterModeRaw_.load (std::memory_order_relaxed));
        juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MIN_FILTER,
                                   filter == pitchlab::WaterfallTextureFilterMode::Nearest ? juce::gl::GL_NEAREST : juce::gl::GL_LINEAR);
        juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MAG_FILTER,
                                   filter == pitchlab::WaterfallTextureFilterMode::Nearest ? juce::gl::GL_NEAREST : juce::gl::GL_LINEAR);
    }
    waterfallTexShader_->use();
    waterfallTexSamplerUniform_->set (0);
    waterfallEnergyScaleUniform_->set (waterfallEnergyScale_.load (std::memory_order_relaxed));
    waterfallAlphaPowerUniform_->set (waterfallAlphaPower_.load (std::memory_order_relaxed));
    waterfallAlphaThresholdUniform_->set (waterfallAlphaThreshold_.load (std::memory_order_relaxed));
    waterfallDisplayCurveModeUniform_->set (static_cast<float> (waterfallDisplayCurveModeRaw_.load (std::memory_order_relaxed)));

    auto& gl = openGLContext_.extensions;
    gl.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, waterfallVbo_);
    gl.glBufferSubData (juce::gl::GL_ARRAY_BUFFER,
                        0,
                        static_cast<GLsizeiptr> (sizeof (float) * static_cast<std::size_t> (batch.numFloats())),
                        batch.vertexData());

    const GLsizei stride = 8 * sizeof (float);

    gl.glEnableVertexAttribArray ((GLuint) waterfallTexPosAttrib_->attributeID);
    gl.glVertexAttribPointer ((GLuint) waterfallTexPosAttrib_->attributeID, 2, juce::gl::GL_FLOAT, juce::gl::GL_FALSE, stride, nullptr);

    gl.glEnableVertexAttribArray ((GLuint) waterfallTexUvAttrib_->attributeID);
    gl.glVertexAttribPointer ((GLuint) waterfallTexUvAttrib_->attributeID,
                              2,
                              juce::gl::GL_FLOAT,
                              juce::gl::GL_FALSE,
                              stride,
                              reinterpret_cast<const void*> (sizeof (float) * 2));

    gl.glEnableVertexAttribArray ((GLuint) waterfallTexColorAttrib_->attributeID);
    gl.glVertexAttribPointer ((GLuint) waterfallTexColorAttrib_->attributeID,
                              4,
                              juce::gl::GL_FLOAT,
                              juce::gl::GL_FALSE,
                              stride,
                              reinterpret_cast<const void*> (sizeof (float) * 4));

    juce::gl::glDrawArrays (juce::gl::GL_TRIANGLES, 0, batch.numVertices());

    gl.glDisableVertexAttribArray ((GLuint) waterfallTexPosAttrib_->attributeID);
    gl.glDisableVertexAttribArray ((GLuint) waterfallTexUvAttrib_->attributeID);
    gl.glDisableVertexAttribArray ((GLuint) waterfallTexColorAttrib_->attributeID);
    gl.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, 0);
    juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, 0);

    // Draw 12-note axis overlay (grid lines + labels) on top.
    if (texShader_ != nullptr && texPosAttrib_ != nullptr && texUvAttrib_ != nullptr && texColorAttrib_ != nullptr
        && texSamplerUniform_ != nullptr && dynamicVbo_ != 0 && axisOverlayImage_.isValid())
    {
        uploadAxisOverlayTextureIfNeeded();
        if (axisOverlayTex_ != 0)
        {
            LegacyGLBatcher axisBatch;
            axisBatch.addQuad (-1.0f,
                                -1.0f,
                                2.0f,
                                2.0f,
                                0.0f,
                                1.0f,
                                1.0f,
                                0.0f,
                                juce::Colours::white);

            // Standard alpha blending so the overlay doesn't brighten/tint the waterfall.
            juce::gl::glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            juce::gl::glActiveTexture (juce::gl::GL_TEXTURE0);
            juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, axisOverlayTex_);
            texShader_->use();
            texSamplerUniform_->set (0);

            gl.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, dynamicVbo_);
            gl.glBufferData (juce::gl::GL_ARRAY_BUFFER,
                              static_cast<GLsizeiptr> (sizeof (float) * static_cast<std::size_t> (axisBatch.numFloats())),
                              axisBatch.vertexData(),
                              juce::gl::GL_STREAM_DRAW);

            const GLsizei axisStride = 8 * sizeof (float);
            gl.glEnableVertexAttribArray ((GLuint) texPosAttrib_->attributeID);
            gl.glVertexAttribPointer ((GLuint) texPosAttrib_->attributeID, 2, juce::gl::GL_FLOAT, juce::gl::GL_FALSE, axisStride, nullptr);

            gl.glEnableVertexAttribArray ((GLuint) texUvAttrib_->attributeID);
            gl.glVertexAttribPointer ((GLuint) texUvAttrib_->attributeID,
                                      2,
                                      juce::gl::GL_FLOAT,
                                      juce::gl::GL_FALSE,
                                      axisStride,
                                      reinterpret_cast<const void*> (sizeof (float) * 2));

            gl.glEnableVertexAttribArray ((GLuint) texColorAttrib_->attributeID);
            gl.glVertexAttribPointer ((GLuint) texColorAttrib_->attributeID,
                                      4,
                                      juce::gl::GL_FLOAT,
                                      juce::gl::GL_FALSE,
                                      axisStride,
                                      reinterpret_cast<const void*> (sizeof (float) * 4));

            juce::gl::glDrawArrays (juce::gl::GL_TRIANGLES, 0, axisBatch.numVertices());

            gl.glDisableVertexAttribArray ((GLuint) texPosAttrib_->attributeID);
            gl.glDisableVertexAttribArray ((GLuint) texUvAttrib_->attributeID);
            gl.glDisableVertexAttribArray ((GLuint) texColorAttrib_->attributeID);
            gl.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, 0);
            juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, 0);
        }
    }

    juce::gl::glDisable (GL_BLEND);
}

void OpenGLVisualizerHost::ensureWhiteTexture()
{
    if (whiteTex_ != 0)
        return;

    using namespace juce::gl;
    glGenTextures (1, &whiteTex_);
    glBindTexture (GL_TEXTURE_2D, whiteTex_);
    const std::uint8_t px[] = { 255, 255, 255, 255 };
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
#if JUCE_OPENGL_ES
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
#else
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
#endif
    glBindTexture (GL_TEXTURE_2D, 0);
}

void OpenGLVisualizerHost::ensureStrobeTexture()
{
    if (strobeTex_ != 0 || staticTables_ == nullptr)
        return;

    std::vector<std::uint8_t> lut (staticTables_->strobeSize());
    for (std::size_t i = 0; i < lut.size(); ++i)
        lut[i] = staticTables_->strobe (i);

    using namespace juce::gl;
    glGenTextures (1, &strobeTex_);
    glBindTexture (GL_TEXTURE_2D, strobeTex_);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
#if JUCE_OPENGL_ES
    glTexImage2D (GL_TEXTURE_2D,
                  0,
                  GL_LUMINANCE,
                  static_cast<GLsizei> (lut.size()),
                  1,
                  0,
                  GL_LUMINANCE,
                  GL_UNSIGNED_BYTE,
                  lut.data());
#else
    glTexImage2D (GL_TEXTURE_2D,
                  0,
                  GL_R8,
                  static_cast<GLsizei> (lut.size()),
                  1,
                  0,
                  GL_RED,
                  GL_UNSIGNED_BYTE,
                  lut.data());
#endif
    glBindTexture (GL_TEXTURE_2D, 0);
}

void OpenGLVisualizerHost::rebuildAxisOverlayImage()
{
    const int w = juce::jmax (1, getWidth());
    const int h = juce::jmax (1, getHeight());

    axisOverlayImage_ = juce::Image (juce::Image::ARGB, w, h, true);

    juce::Graphics g (axisOverlayImage_);
    g.fillAll (juce::Colours::transparentBlack);

    const float laneH = static_cast<float> (h) / 12.0f;
    if (laneH <= 0.0f)
        return;

    // Notes: lane 0 is C at the bottom, lane 11 is B at the top.
    constexpr const char* labels[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    // Grid lines between lanes.
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    for (int note = 0; note < 11; ++note)
    {
        const float y = static_cast<float> (h) - static_cast<float> (note + 1) * laneH;
        g.drawLine (0.0f, y, static_cast<float> (w), y, 1.2f);
    }

    // Labels on the left.
    const float fontSize = juce::jlimit (10.0f, 18.0f, laneH * 0.42f);
    g.setFont (juce::FontOptions { fontSize, juce::Font::bold });
    g.setColour (juce::Colours::white.withAlpha (0.85f));

    for (int note = 0; note < 12; ++note)
    {
        const float centerY = static_cast<float> (h) - (static_cast<float> (note) + 0.5f) * laneH;
        const int y0 = static_cast<int> (centerY - fontSize * 0.6f);
        const int labelW = juce::jmin (70, w);
        const juce::Rectangle<int> r (6, y0, labelW, static_cast<int> (fontSize * 1.2f));

        // Optional shadow for readability.
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawText (labels[note], r.translated (1, 1), juce::Justification::centredLeft, false);
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.drawText (labels[note], r, juce::Justification::centredLeft, false);
    }

    axisOverlayDirty_ = true;
}

void OpenGLVisualizerHost::uploadAxisOverlayTextureIfNeeded()
{
    if (! axisOverlayImage_.isValid() || ! axisOverlayDirty_)
        return;

    const int w = axisOverlayImage_.getWidth();
    const int h = axisOverlayImage_.getHeight();
    if (w <= 0 || h <= 0)
        return;

    if (axisOverlayTex_ == 0)
    {
        using namespace juce::gl;
        glGenTextures (1, &axisOverlayTex_);
    }

    std::vector<std::uint8_t> rgba;
    rgba.resize (static_cast<std::size_t> (w) * static_cast<std::size_t> (h) * 4u);

    juce::Image::BitmapData bd (axisOverlayImage_, juce::Image::BitmapData::readOnly);
    if (bd.pixelStride <= 0 || bd.pixelStride < 4)
        return;

    for (int y = 0; y < h; ++y)
    {
        const std::uint8_t* line = bd.getLinePointer (y);
        for (int x = 0; x < w; ++x)
        {
            const std::uint8_t* p = line + static_cast<std::size_t> (x) * static_cast<std::size_t> (bd.pixelStride);
            // PixelARGB memory order is: a, r, g, b.
            const std::uint8_t a = p[0];
            const std::uint8_t r = p[1];
            const std::uint8_t g = p[2];
            const std::uint8_t b = p[3];

            const std::size_t idx = (static_cast<std::size_t> (y) * static_cast<std::size_t> (w) + static_cast<std::size_t> (x)) * 4u;
            rgba[idx + 0] = r;
            rgba[idx + 1] = g;
            rgba[idx + 2] = b;
            rgba[idx + 3] = a;
        }
    }

    using namespace juce::gl;
    glBindTexture (GL_TEXTURE_2D, axisOverlayTex_);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D (GL_TEXTURE_2D,
                  0,
                  GL_RGBA8,
                  static_cast<GLsizei> (w),
                  static_cast<GLsizei> (h),
                  0,
                  GL_RGBA,
                  GL_UNSIGNED_BYTE,
                  rgba.data());
    glBindTexture (GL_TEXTURE_2D, 0);

    axisOverlayDirty_ = false;
}

void OpenGLVisualizerHost::renderNeedle()
{
    if (lineShader_ == nullptr || linePosAttrib_ == nullptr || lineColorUniform_ == nullptr || lineVbo_ == 0)
        return;

    constexpr float kPi = 3.14159265f;
    constexpr float kRadPerCent = 0.020944f; // New Plan §5.3

    pitchlab::RenderFrameData frame;
    {
        const juce::ScopedLock sl (frameLock_);
        frame = latestFrame_;
    }
    const float cents = frame.tuningError;
    const float cx = 0.0f;
    const float cy = -0.55f;
    const float arcR = 0.48f;
    const float needleLen = 0.42f;

    int o = 0;
    constexpr int kArcPts = 65;
    for (int i = 0; i < kArcPts; ++i)
    {
        const float t = static_cast<float> (i) / static_cast<float> (kArcPts - 1);
        const float a = kPi * 0.5f - 1.05f + t * 2.1f;
        lineScratch_[static_cast<std::size_t> (o++)] = cx + arcR * std::cos (a);
        lineScratch_[static_cast<std::size_t> (o++)] = cy + arcR * std::sin (a);
    }

    const float needleA = kPi * 0.5f - cents * kRadPerCent;
    const float nx = cx + needleLen * std::cos (needleA);
    const float ny = cy + needleLen * std::sin (needleA);
    lineScratch_[static_cast<std::size_t> (o++)] = cx;
    lineScratch_[static_cast<std::size_t> (o++)] = cy;
    lineScratch_[static_cast<std::size_t> (o++)] = nx;
    lineScratch_[static_cast<std::size_t> (o++)] = ny;

    auto& gl = openGLContext_.extensions;
    gl.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, lineVbo_);
    gl.glBufferData (juce::gl::GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr> (sizeof (float) * static_cast<std::size_t> (o)),
                     lineScratch_.data(),
                     juce::gl::GL_STREAM_DRAW);

    lineShader_->use();
    lineColorUniform_->set (0.45f, 0.5f, 0.55f, 1.0f);
    gl.glEnableVertexAttribArray ((GLuint) linePosAttrib_->attributeID);
    gl.glVertexAttribPointer ((GLuint) linePosAttrib_->attributeID,
                              2,
                              juce::gl::GL_FLOAT,
                              juce::gl::GL_FALSE,
                              0,
                              nullptr);
    juce::gl::glDrawArrays (juce::gl::GL_LINE_STRIP, 0, kArcPts);

    lineColorUniform_->set (1.0f, 0.35f, 0.25f, 1.0f);
    juce::gl::glDrawArrays (juce::gl::GL_LINES, kArcPts, 2);
    gl.glDisableVertexAttribArray ((GLuint) linePosAttrib_->attributeID);
    gl.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, 0);
}

void OpenGLVisualizerHost::renderStrobeComposite()
{
    if (texShader_ == nullptr || texPosAttrib_ == nullptr || texUvAttrib_ == nullptr
        || texColorAttrib_ == nullptr || texSamplerUniform_ == nullptr)
        return;

    ensureWhiteTexture();
    ensureStrobeTexture();

    if (strobeTex_ == 0)
    {
        if (lineShader_ != nullptr && linePosAttrib_ != nullptr && lineColorUniform_ != nullptr && lineVbo_ != 0)
        {
            const float cross[] = {
                -0.5f, 0.0f, 0.5f, 0.0f,
                0.0f, -0.5f, 0.0f, 0.5f
            };
            auto& gl = openGLContext_.extensions;
            gl.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, lineVbo_);
            gl.glBufferData (juce::gl::GL_ARRAY_BUFFER, sizeof (cross), cross, juce::gl::GL_STREAM_DRAW);
            lineShader_->use();
            lineColorUniform_->set (0.5f, 0.5f, 0.55f, 1.0f);
            gl.glEnableVertexAttribArray ((GLuint) linePosAttrib_->attributeID);
            gl.glVertexAttribPointer ((GLuint) linePosAttrib_->attributeID,
                                      2,
                                      juce::gl::GL_FLOAT,
                                      juce::gl::GL_FALSE,
                                      0,
                                      nullptr);
            juce::gl::glDrawArrays (juce::gl::GL_LINES, 0, 4);
            gl.glDisableVertexAttribArray ((GLuint) linePosAttrib_->attributeID);
            gl.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, 0);
        }
        return;
    }

    pitchlab::RenderFrameData frame;
    {
        const juce::ScopedLock sl (frameLock_);
        frame = latestFrame_;
    }
    const float phase = frame.strobePhase;
    constexpr float kTwoPi = 6.2831855f;
    const float phaseU = phase / kTwoPi;

    std::array<float, 12> noteEnergy {};
    float maxEnergy = 0.0f;
    float sumEnergy = 0.0f;
    for (int pc = 0; pc < 12; ++pc)
    {
        float m = 0.0f;
        const int base = pc * 32;
        for (int i = 0; i < 32; ++i)
        {
            const float e = frame.chromaRow[static_cast<std::size_t> (base + i)];
            m = std::max (m, e);
        }
        noteEnergy[static_cast<std::size_t> (pc)] = m;
        maxEnergy = std::max (maxEnergy, m);
        sumEnergy += m;
    }

    const float invMaxE = (maxEnergy > 1.0e-9f) ? (1.0f / maxEnergy) : 0.0f;
    const float gateGlobal = (maxEnergy > 1.0e-9f) ? ((sumEnergy / 12.0f) * invMaxE) : 0.0f;
    const juce::Colour ringColor = juce::Colour::fromFloatRGBA (0.25f + 0.75f * gateGlobal,
                                                               0.25f + 0.75f * gateGlobal,
                                                               0.35f + 0.65f * gateGlobal,
                                                               1.0f);

    LegacyGLBatcher batch;
    constexpr int kSeg = 40;
    const float r0 = 0.22f;
    const float r1 = 0.52f;

    for (int i = 0; i < kSeg; ++i)
    {
        const float t0 = static_cast<float> (i) / static_cast<float> (kSeg);
        const float t1 = static_cast<float> (i + 1) / static_cast<float> (kSeg);
        const float a0 = kTwoPi * t0 + phase;
        const float a1 = kTwoPi * t1 + phase;
        const float x00 = r0 * std::cos (a0);
        const float y00 = r0 * std::sin (a0);
        const float x10 = r0 * std::cos (a1);
        const float y10 = r0 * std::sin (a1);
        const float x11 = r1 * std::cos (a1);
        const float y11 = r1 * std::sin (a1);
        const float x01 = r1 * std::cos (a0);
        const float y01 = r1 * std::sin (a0);
        const float u0 = t0 + phaseU;
        const float u1 = t1 + phaseU;
        batch.addSkewedTexturedQuad (x00, y00, x10, y10, x11, y11, x01, y01,
                                      u0,
                                      0.0f,
                                      u1,
                                      1.0f,
                                      ringColor);
    }

    const float du = phaseU * 2.0f;
    for (int s = 0; s < 6; ++s)
    {
        const float y0 = -1.0f + static_cast<float> (s) * 0.14f;
        const float y1 = y0 + 0.12f;
        const float uA = du + static_cast<float> (s) * 0.07f;
        const float uB = uA + 1.5f;
        const int pc0 = s * 2;
        const int pc1 = pc0 + 1;
        const float gate = std::max (noteEnergy[static_cast<std::size_t> (pc0)],
                                      noteEnergy[static_cast<std::size_t> (pc1)]);
        const float gateN = gate * invMaxE;
        const juce::Colour c = juce::Colour::fromFloatRGBA (gateN,
                                                             0.2f + 0.8f * gateN,
                                                             0.05f + 0.95f * gateN,
                                                             1.0f);
        batch.addQuad (-1.0f, y0, 2.0f, y1 - y0, uA, 0.0f, uB, 1.0f, c);
    }

    if (batch.empty())
        return;

    using namespace juce::gl;
    juce::gl::glActiveTexture (juce::gl::GL_TEXTURE0);
    juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, strobeTex_);
    texShader_->use();
    texSamplerUniform_->set (0);

    auto& gl = openGLContext_.extensions;
    gl.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, dynamicVbo_);
    gl.glBufferData (juce::gl::GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr> (sizeof (float) * static_cast<std::size_t> (batch.numFloats())),
                     batch.vertexData(),
                     juce::gl::GL_STREAM_DRAW);

    const GLsizei stride = 8 * sizeof (float);
    gl.glEnableVertexAttribArray ((GLuint) texPosAttrib_->attributeID);
    gl.glVertexAttribPointer ((GLuint) texPosAttrib_->attributeID, 2, juce::gl::GL_FLOAT, juce::gl::GL_FALSE, stride, nullptr);
    gl.glEnableVertexAttribArray ((GLuint) texUvAttrib_->attributeID);
    gl.glVertexAttribPointer ((GLuint) texUvAttrib_->attributeID,
                              2,
                              juce::gl::GL_FLOAT,
                              juce::gl::GL_FALSE,
                              stride,
                              reinterpret_cast<const void*> (sizeof (float) * 2));
    gl.glEnableVertexAttribArray ((GLuint) texColorAttrib_->attributeID);
    gl.glVertexAttribPointer ((GLuint) texColorAttrib_->attributeID,
                              4,
                              juce::gl::GL_FLOAT,
                              juce::gl::GL_FALSE,
                              stride,
                              reinterpret_cast<const void*> (sizeof (float) * 4));
    juce::gl::glDrawArrays (juce::gl::GL_TRIANGLES, 0, batch.numVertices());
    gl.glDisableVertexAttribArray ((GLuint) texPosAttrib_->attributeID);
    gl.glDisableVertexAttribArray ((GLuint) texUvAttrib_->attributeID);
    gl.glDisableVertexAttribArray ((GLuint) texColorAttrib_->attributeID);
    gl.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, 0);
    juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, 0);
}

void OpenGLVisualizerHost::renderChordMatrix()
{
    if (texShader_ == nullptr || texPosAttrib_ == nullptr || texUvAttrib_ == nullptr
        || texColorAttrib_ == nullptr || texSamplerUniform_ == nullptr)
        return;

    pitchlab::RenderFrameData frame;
    {
        const juce::ScopedLock sl (frameLock_);
        frame = latestFrame_;
    }

    ensureWhiteTexture();
    if (whiteTex_ == 0)
        return;

    constexpr int kRoots = 12;
    constexpr int kTypes = 7;
    const float cellW = 1.9f / static_cast<float> (kRoots);
    const float cellH = 1.75f / static_cast<float> (kTypes);

    LegacyGLBatcher batch;
    for (int type = 0; type < kTypes; ++type)
    {
        for (int root = 0; root < kRoots; ++root)
        {
            const float p = frame.chordProbabilities[static_cast<std::size_t> (root + type * kRoots)];
            const float x = -0.95f + static_cast<float> (root) * cellW;
            const float y = 0.88f - static_cast<float> (type + 1) * cellH;
            const juce::Colour c = juce::Colour::fromFloatRGBA (p, p * 0.85f + 0.05f, p * 0.4f + 0.1f, 1.0f);
            batch.addQuad (x, y, cellW * 0.96f, cellH * 0.92f, 0.0f, 0.0f, 1.0f, 1.0f, c);
        }
    }

    using namespace juce::gl;
    juce::gl::glActiveTexture (juce::gl::GL_TEXTURE0);
    juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, whiteTex_);
    texShader_->use();
    texSamplerUniform_->set (0);

    auto& gl = openGLContext_.extensions;
    gl.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, dynamicVbo_);
    gl.glBufferData (juce::gl::GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr> (sizeof (float) * static_cast<std::size_t> (batch.numFloats())),
                     batch.vertexData(),
                     juce::gl::GL_STREAM_DRAW);

    const GLsizei stride = 8 * sizeof (float);
    gl.glEnableVertexAttribArray ((GLuint) texPosAttrib_->attributeID);
    gl.glVertexAttribPointer ((GLuint) texPosAttrib_->attributeID, 2, juce::gl::GL_FLOAT, juce::gl::GL_FALSE, stride, nullptr);
    gl.glEnableVertexAttribArray ((GLuint) texUvAttrib_->attributeID);
    gl.glVertexAttribPointer ((GLuint) texUvAttrib_->attributeID,
                              2,
                              juce::gl::GL_FLOAT,
                              juce::gl::GL_FALSE,
                              stride,
                              reinterpret_cast<const void*> (sizeof (float) * 2));
    gl.glEnableVertexAttribArray ((GLuint) texColorAttrib_->attributeID);
    gl.glVertexAttribPointer ((GLuint) texColorAttrib_->attributeID,
                              4,
                              juce::gl::GL_FLOAT,
                              juce::gl::GL_FALSE,
                              stride,
                              reinterpret_cast<const void*> (sizeof (float) * 4));
    juce::gl::glDrawArrays (juce::gl::GL_TRIANGLES, 0, batch.numVertices());
    gl.glDisableVertexAttribArray ((GLuint) texPosAttrib_->attributeID);
    gl.glDisableVertexAttribArray ((GLuint) texUvAttrib_->attributeID);
    gl.glDisableVertexAttribArray ((GLuint) texColorAttrib_->attributeID);
    gl.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, 0);
    juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, 0);
}
