#pragma once

#include <JuceHeader.h>

#include <vector>

/**
    Immediate-mode style quad batch for GL 3.2 (New Plan §4.2 LegacyGLBatcher shim).
 */
class LegacyGLBatcher
{
public:
    void clear() noexcept { vertices_.clear(); }

    void addQuad (float x, float y, float w, float h,
                  float u0, float v0, float u1, float v1,
                  juce::Colour c) noexcept;

    /** Two triangles; UV corners map (x00,y00)→(u0,v0), (x10,y10)→(u1,v0), (x01,y01)→(u0,v1), (x11,y11)→(u1,v1). */
    void addSkewedTexturedQuad (float x00, float y00, float x10, float y10,
                                float x11, float y11, float x01, float y01,
                                float u0, float v0, float u1, float v1,
                                juce::Colour c) noexcept;

    [[nodiscard]] bool empty() const noexcept { return vertices_.empty(); }
    [[nodiscard]] const float* vertexData() const noexcept { return vertices_.data(); }
    [[nodiscard]] int numFloats() const noexcept { return static_cast<int> (vertices_.size()); }
    /** 8 floats per vertex: xy, uv, rgba */
    [[nodiscard]] int numVertices() const noexcept { return numFloats() / 8; }

private:
    std::vector<float> vertices_;
};
