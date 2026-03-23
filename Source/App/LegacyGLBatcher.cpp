#include "LegacyGLBatcher.h"

void LegacyGLBatcher::addQuad (float x, float y, float w, float h,
                               float u0, float v0, float u1, float v1,
                               juce::Colour c) noexcept
{
    const float r = c.getFloatRed();
    const float g = c.getFloatGreen();
    const float b = c.getFloatBlue();
    const float a = c.getFloatAlpha();

    const float x1 = x + w;
    const float y1 = y + h;

    const auto pushV = [&] (float px, float py, float u, float v) {
        vertices_.push_back (px);
        vertices_.push_back (py);
        vertices_.push_back (u);
        vertices_.push_back (v);
        vertices_.push_back (r);
        vertices_.push_back (g);
        vertices_.push_back (b);
        vertices_.push_back (a);
    };

    pushV (x, y, u0, v0);
    pushV (x1, y, u1, v0);
    pushV (x, y1, u0, v1);

    pushV (x1, y, u1, v0);
    pushV (x1, y1, u1, v1);
    pushV (x, y1, u0, v1);
}

void LegacyGLBatcher::addSkewedTexturedQuad (float x00, float y00, float x10, float y10,
                                             float x11, float y11, float x01, float y01,
                                             float u0, float v0, float u1, float v1,
                                             juce::Colour c) noexcept
{
    const float r = c.getFloatRed();
    const float g = c.getFloatGreen();
    const float b = c.getFloatBlue();
    const float a = c.getFloatAlpha();

    const auto pushV = [&] (float px, float py, float u, float v) {
        vertices_.push_back (px);
        vertices_.push_back (py);
        vertices_.push_back (u);
        vertices_.push_back (v);
        vertices_.push_back (r);
        vertices_.push_back (g);
        vertices_.push_back (b);
        vertices_.push_back (a);
    };

    pushV (x00, y00, u0, v0);
    pushV (x10, y10, u1, v0);
    pushV (x01, y01, u0, v1);

    pushV (x10, y10, u1, v0);
    pushV (x11, y11, u1, v1);
    pushV (x01, y01, u0, v1);
}
