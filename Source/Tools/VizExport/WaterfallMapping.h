#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace pitchlab
{
namespace waterfall
{
static constexpr int kNotes = 12;
static constexpr int kSubBinsPerNote = 32;
static constexpr int kChromaBins = kNotes * kSubBinsPerNote; // 384

static constexpr int kFilmHeight = 384;

struct WaterfallSample
{
    int timeIndex = 0;   // texture V row index (0..kFilmHeight-1)
    int chromaIndex = 0; // texture U sample index (0..kChromaBins-1)
};

[[nodiscard]] inline int clampInt (int v, int lo, int hi) noexcept
{
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

/**
    CPU waterfall mapping helper.

    Interprets the waterfall film reel as:
    - Texture U (384 samples) = chroma cents bins (note*32 + sub)
    - Texture V (384 samples) = time/history row index (circular ring)
    - Newest time row is derived from `waterfallWriteY` (next write head),
      so newestRow = waterfallWriteY - 1.
 */
[[nodiscard]] inline WaterfallSample mapWaterfallPixel (int screenX,
                                                         int screenY,
                                                         int width,
                                                         int height,
                                                         int waterfallWriteY) noexcept
{
    const int newestRow = (waterfallWriteY - 1 + kFilmHeight) % kFilmHeight;

    const float fx = (width <= 1) ? 0.0f : (static_cast<float> (screenX) / static_cast<float> (width - 1));
    const float fy = (height <= 1) ? 0.0f : (static_cast<float> (screenY) / static_cast<float> (height - 1));

    // ScreenY is split into 12 stacked note lanes. Within a lane, we map bottom->top to sub=0..31.
    const float yNdc = 1.0f - 2.0f * fy; // OpenGL NDC: +1 at top, -1 at bottom.
    const float stripH = 2.0f / static_cast<float> (kNotes);
    const int note = clampInt (static_cast<int> (std::floor ((yNdc + 1.0f) / stripH)), 0, kNotes - 1);
    const float y0 = -1.0f + static_cast<float> (note) * stripH;
    const float laneLocal = (stripH <= 0.0f) ? 0.0f : ((yNdc - y0) / stripH); // 0..1
    const int sub = clampInt (static_cast<int> (std::floor (laneLocal * static_cast<float> (kSubBinsPerNote))),
                                0,
                                kSubBinsPerNote - 1);
    const int chromaIndex = note * kSubBinsPerNote + sub;

    // In the GPU, waterfall shader uses a UV-transpose. So vertex UV.x encodes texture V.
    // We mirror the same endpoint behavior used in OpenGL:
    //   tNewest = newestRow / filmHeight
    //   tLeft   = tNewest - 1 + 1/filmHeight  (avoid GL_REPEAT edge duplication)
    //   tRight  = tNewest
    const float tNewest = static_cast<float> (newestRow) / static_cast<float> (kFilmHeight);
    const float tStep = 1.0f / static_cast<float> (kFilmHeight);
    const float tLeft = tNewest - 1.0f + tStep;
    const float tRight = tNewest;
    const float tCoord = tLeft + fx * (tRight - tLeft);

    // Convert texture coordinate back into a discrete row index.
    float frac = tCoord - std::floor (tCoord); // [0,1)
    if (frac < 0.0f)
        frac = 0.0f;
    const int timeIndex = clampInt (static_cast<int> (std::floor (frac * static_cast<float> (kFilmHeight))),
                                      0,
                                      kFilmHeight - 1);

    return WaterfallSample { timeIndex, chromaIndex };
}

} // namespace waterfall
} // namespace pitchlab

