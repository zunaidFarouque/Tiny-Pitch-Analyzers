#include "VizCpuRenderer.h"

#include "StaticTables.h"
#include "WaterfallMapping.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace pitchlab
{
namespace
{
constexpr float kPi = 3.14159265359f;

[[nodiscard]] juce::Colour sampleStrobeLut (const StaticTables* tables, float u) noexcept
{
    if (tables == nullptr || tables->strobeSize() == 0)
        return juce::Colour::fromFloatRGBA (0.5f, 0.5f, 0.55f, 1.0f);

    const float n = static_cast<float> (tables->strobeSize());
    float x = u - std::floor (u);
    if (x < 0.0f)
        x += 1.0f;
    const auto idx = static_cast<std::size_t> (std::clamp (x * (n - 1.0f), 0.0f, n - 1.0f));
    const float v = static_cast<float> (tables->strobe (idx)) / 255.0f;
    return juce::Colour::fromFloatRGBA (v, v, v, 1.0f);
}

[[nodiscard]] juce::String modeToken (VizMode m)
{
    switch (m)
    {
        case VizMode::Waveform: return "waveform";
        case VizMode::Waterfall: return "waterfall";
        case VizMode::Needle: return "needle";
        case VizMode::StrobeRadial: return "stroberadial";
        case VizMode::ChordMatrix: return "chordmatrix";
        default: return {};
    }
}
} // namespace

VizCpuRenderer::VizCpuRenderer (int width, int height)
    : width_ (std::max (16, width))
    , height_ (std::max (16, height))
{
}

void VizCpuRenderer::setWaterfallRenderParams (const WaterfallRenderParams& p) noexcept
{
    waterfallParams_ = p;
}

juce::Image VizCpuRenderer::render (VizMode mode, const VizFrameData& frame, const StaticTables* tables) const
{
    switch (mode)
    {
        case VizMode::Waveform: return renderWaveform (frame);
        case VizMode::Waterfall: return renderWaterfall (frame);
        case VizMode::Needle: return renderNeedle (frame);
        case VizMode::StrobeRadial: return renderStrobe (frame, tables);
        case VizMode::ChordMatrix: return renderChordMatrix (frame);
        default: return renderWaveform (frame);
    }
}

juce::Image VizCpuRenderer::renderWaveform (const VizFrameData& frame) const
{
    juce::Image img (juce::Image::ARGB, width_, height_, true);
    juce::Graphics g (img);
    g.fillAll (juce::Colour::fromRGB (20, 23, 30));
    g.setColour (juce::Colour::fromFloatRGBA (0.35f, 0.85f, 1.0f, 1.0f));

    juce::Path p;
    const int n = static_cast<int> (frame.waveform.size());
    for (int i = 0; i < n; ++i)
    {
        const float x = static_cast<float> (i) / static_cast<float> (n - 1) * static_cast<float> (width_ - 1);
        const float yNorm = std::clamp (static_cast<float> (frame.waveform[static_cast<std::size_t> (i)]) / 32768.0f, -1.0f, 1.0f);
        const float y = static_cast<float> (height_) * (0.5f - 0.45f * yNorm);
        if (i == 0)
            p.startNewSubPath (x, y);
        else
            p.lineTo (x, y);
    }

    g.strokePath (p, juce::PathStrokeType (2.0f));
    return img;
}

juce::Image VizCpuRenderer::renderWaterfall (const VizFrameData& frame) const
{
    juce::Image img (juce::Image::ARGB, width_, height_, true);
    juce::Image::BitmapData px (img, juce::Image::BitmapData::writeOnly);

    for (int y = 0; y < height_; ++y)
    {
        for (int x = 0; x < width_; ++x)
        {
            // CPU short-term policy: we only have one 384-bin chroma row, so we treat that row as repeated
            // across all time/history rows. The mapping still matters (direction + folding).
            const waterfall::WaterfallSample s = waterfall::mapWaterfallPixel (x, y, width_, height_, frame.waterfallWriteY);
            const float v = (frame.waterfallGrid384 != nullptr)
                                ? frame.waterfallGrid384[static_cast<std::size_t> (s.timeIndex) * waterfall::kChromaBins
                                                          + static_cast<std::size_t> (s.chromaIndex)]
                                : frame.chromaRow[static_cast<std::size_t> (s.chromaIndex)];

            const float xv = std::max (0.0f, v);
            float m = std::clamp (xv * waterfallParams_.energyScale, 0.0f, 1.0f);
            if (waterfallParams_.curveMode == WaterfallDisplayCurveMode::Sqrt)
                m = std::clamp (std::sqrt (xv) * waterfallParams_.energyScale, 0.0f, 1.0f);
            else if (waterfallParams_.curveMode == WaterfallDisplayCurveMode::LogDb)
            {
                const float db = 20.0f * std::log10 (std::max (xv, 1.0e-12f));
                m = std::clamp ((db + 100.0f) / 100.0f, 0.0f, 1.0f);
            }
            float a = std::pow (m, std::max (0.0f, waterfallParams_.alphaPower));
            if (a < std::max (0.0f, waterfallParams_.alphaThreshold))
                a = 0.0f;
            const std::uint8_t c = static_cast<std::uint8_t> (std::clamp (a, 0.0f, 1.0f) * 255.0f);
            px.setPixelColour (x, y, juce::Colour::fromRGB (c, c, c));
        }
    }

    return img;
}

juce::Image VizCpuRenderer::renderNeedle (const VizFrameData& frame) const
{
    juce::Image img (juce::Image::ARGB, width_, height_, true);
    juce::Graphics g (img);
    g.fillAll (juce::Colour::fromRGB (20, 23, 30));

    constexpr float kRadPerCent = 0.020944f;
    const float cx = 0.0f;
    const float cy = -0.55f;
    const float arcR = 0.48f;
    const float needleLen = 0.42f;

    auto mapX = [this] (float x) { return (x + 1.0f) * 0.5f * static_cast<float> (width_); };
    auto mapY = [this] (float y) { return (1.0f - (y + 1.0f) * 0.5f) * static_cast<float> (height_); };

    juce::Path arc;
    constexpr int kArcPts = 65;
    for (int i = 0; i < kArcPts; ++i)
    {
        const float t = static_cast<float> (i) / static_cast<float> (kArcPts - 1);
        const float a = kPi * 0.5f - 1.05f + t * 2.1f;
        const float x = cx + arcR * std::cos (a);
        const float y = cy + arcR * std::sin (a);
        if (i == 0)
            arc.startNewSubPath (mapX (x), mapY (y));
        else
            arc.lineTo (mapX (x), mapY (y));
    }

    g.setColour (juce::Colour::fromFloatRGBA (0.45f, 0.5f, 0.55f, 1.0f));
    g.strokePath (arc, juce::PathStrokeType (2.0f));

    const float a = kPi * 0.5f - frame.tuningError * kRadPerCent;
    const float nx = cx + needleLen * std::cos (a);
    const float ny = cy + needleLen * std::sin (a);

    g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.35f, 0.25f, 1.0f));
    g.drawLine (mapX (cx), mapY (cy), mapX (nx), mapY (ny), 2.5f);
    return img;
}

juce::Image VizCpuRenderer::renderStrobe (const VizFrameData& frame, const StaticTables* tables) const
{
    juce::Image img (juce::Image::ARGB, width_, height_, true);
    juce::Graphics g (img);
    g.fillAll (juce::Colour::fromRGB (20, 23, 30));

    std::array<float, 12> noteEnergy {};
    float maxEnergy = 0.0f;
    float sumEnergy = 0.0f;
    for (int pc = 0; pc < 12; ++pc)
    {
        float m = 0.0f;
        const int base = pc * 32;
        for (int i = 0; i < 32; ++i)
            m = std::max (m, frame.chromaRow[static_cast<std::size_t> (base + i)]);
        noteEnergy[static_cast<std::size_t> (pc)] = m;
        maxEnergy = std::max (maxEnergy, m);
        sumEnergy += m;
    }

    const float invMaxE = (maxEnergy > 1.0e-9f) ? (1.0f / maxEnergy) : 0.0f;
    const float gateGlobal = (maxEnergy > 1.0e-9f) ? ((sumEnergy / 12.0f) * invMaxE) : 0.0f;

    auto mapX = [this] (float x) { return (x + 1.0f) * 0.5f * static_cast<float> (width_); };
    auto mapY = [this] (float y) { return (1.0f - (y + 1.0f) * 0.5f) * static_cast<float> (height_); };

    const float phase = frame.strobePhase;
    constexpr float kTwoPi = 6.2831855f;
    const float phaseU = phase / kTwoPi;
    constexpr int kSeg = 40;
    const float r0 = 0.22f;
    const float r1 = 0.52f;

    for (int i = 0; i < kSeg; ++i)
    {
        const float t0 = static_cast<float> (i) / static_cast<float> (kSeg);
        const float t1 = static_cast<float> (i + 1) / static_cast<float> (kSeg);
        const float a0 = kTwoPi * t0 + phase;
        const float a1 = kTwoPi * t1 + phase;
        const juce::Colour lutC = sampleStrobeLut (tables, 0.5f * (t0 + t1) + phaseU);
        const float glow = 0.25f + 0.75f * gateGlobal;
        const juce::Colour c = juce::Colour::fromFloatRGBA (lutC.getFloatRed() * glow,
                                                             lutC.getFloatGreen() * glow,
                                                             lutC.getFloatBlue() * (0.35f + 0.65f * gateGlobal),
                                                             1.0f);

        juce::Path quad;
        quad.startNewSubPath (mapX (r0 * std::cos (a0)), mapY (r0 * std::sin (a0)));
        quad.lineTo (mapX (r0 * std::cos (a1)), mapY (r0 * std::sin (a1)));
        quad.lineTo (mapX (r1 * std::cos (a1)), mapY (r1 * std::sin (a1)));
        quad.lineTo (mapX (r1 * std::cos (a0)), mapY (r1 * std::sin (a0)));
        quad.closeSubPath();
        g.setColour (c);
        g.fillPath (quad);
    }

    const float du = phaseU * 2.0f;
    for (int s = 0; s < 6; ++s)
    {
        const float y0 = -1.0f + static_cast<float> (s) * 0.14f;
        const float y1 = y0 + 0.12f;
        const int pc0 = s * 2;
        const int pc1 = pc0 + 1;
        const float gate = std::max (noteEnergy[static_cast<std::size_t> (pc0)],
                                     noteEnergy[static_cast<std::size_t> (pc1)]);
        const float gateN = gate * invMaxE;
        const juce::Colour lutC = sampleStrobeLut (tables, du + static_cast<float> (s) * 0.07f);
        const juce::Colour c = juce::Colour::fromFloatRGBA (lutC.getFloatRed() * gateN,
                                                             (0.2f + 0.8f * gateN) * lutC.getFloatGreen(),
                                                             (0.05f + 0.95f * gateN) * lutC.getFloatBlue(),
                                                             1.0f);
        g.setColour (c);
        g.fillRect (juce::Rectangle<float> (mapX (-1.0f), mapY (y1), mapX (1.0f) - mapX (-1.0f), mapY (y0) - mapY (y1)));
    }

    return img;
}

juce::Image VizCpuRenderer::renderChordMatrix (const VizFrameData& frame) const
{
    juce::Image img (juce::Image::ARGB, width_, height_, true);
    juce::Graphics g (img);
    g.fillAll (juce::Colour::fromRGB (20, 23, 30));

    constexpr int kRoots = 12;
    constexpr int kTypes = 7;
    const float cellW = 1.9f / static_cast<float> (kRoots);
    const float cellH = 1.75f / static_cast<float> (kTypes);
    auto mapX = [this] (float x) { return (x + 1.0f) * 0.5f * static_cast<float> (width_); };
    auto mapY = [this] (float y) { return (1.0f - (y + 1.0f) * 0.5f) * static_cast<float> (height_); };

    for (int type = 0; type < kTypes; ++type)
    {
        for (int root = 0; root < kRoots; ++root)
        {
            const float p = frame.chordProbabilities[static_cast<std::size_t> (root + type * kRoots)];
            const float x = -0.95f + static_cast<float> (root) * cellW;
            const float y = 0.88f - static_cast<float> (type + 1) * cellH;
            const juce::Colour c = juce::Colour::fromFloatRGBA (p, p * 0.85f + 0.05f, p * 0.4f + 0.1f, 1.0f);

            const float px = mapX (x);
            const float pyTop = mapY (y + cellH * 0.92f);
            const float pyBottom = mapY (y);
            const float pw = mapX (x + cellW * 0.96f) - px;
            const float ph = pyBottom - pyTop;
            g.setColour (c);
            g.fillRect (juce::Rectangle<float> (px, pyTop, pw, ph));
        }
    }

    return img;
}

const char* modeToName (VizMode mode) noexcept
{
    switch (mode)
    {
        case VizMode::Waveform: return "waveform";
        case VizMode::Waterfall: return "waterfall";
        case VizMode::Needle: return "needle";
        case VizMode::StrobeRadial: return "stroberadial";
        case VizMode::ChordMatrix: return "chordmatrix";
        default: return "unknown";
    }
}

bool parseModeList (const juce::String& csv, juce::Array<VizMode>& outModes)
{
    outModes.clear();
    juce::StringArray toks;
    toks.addTokens (csv, ",", "\"");
    toks.trim();
    toks.removeEmptyStrings();

    for (const auto& raw : toks)
    {
        const auto t = raw.toLowerCase();
        if (t == "all")
        {
            outModes.addArray ({ VizMode::Waveform, VizMode::Waterfall, VizMode::Needle, VizMode::StrobeRadial, VizMode::ChordMatrix });
            return true;
        }
        if (t == "waveform") outModes.add (VizMode::Waveform);
        else if (t == "waterfall") outModes.add (VizMode::Waterfall);
        else if (t == "needle") outModes.add (VizMode::Needle);
        else if (t == "stroberadial" || t == "strobe") outModes.add (VizMode::StrobeRadial);
        else if (t == "chordmatrix" || t == "chord") outModes.add (VizMode::ChordMatrix);
        else return false;
    }

    return outModes.size() > 0;
}

} // namespace pitchlab

