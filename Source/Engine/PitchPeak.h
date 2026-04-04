#pragma once

namespace pitchlab
{

struct PitchPeak
{
    static constexpr int kMaxPeaksCap = 64;

    float frequencyHz = 0.0f;
    float magnitude = 0.0f;
    float midiPitch = 0.0f;
};

} // namespace pitchlab
