#include <cmath>

#include <gtest/gtest.h>

#include "PitchLabEngine.h"
#include "RenderFrameData.h"

TEST (PitchLabEngine, MultiResProducesFiniteRenderFrame)
{
    pitchlab::PitchLabEngine eng;
    eng.state().setSpectralBackendMode (pitchlab::SpectralBackendMode::MultiResSTFT_v1_0);
    eng.prepareToPlay (44100.0, 512);

    std::vector<float> buf (512, 0.02f);
    const float* ch[] = { buf.data() };
    for (int i = 0; i < 32; ++i)
        eng.processAudioInterleaved (ch, 1, 512);

    pitchlab::RenderFrameData frame;
    eng.copyLatestRenderFrame (frame);
    EXPECT_TRUE (std::isfinite (frame.currentHz));
    EXPECT_TRUE (std::isfinite (frame.tuningError));
}
