#pragma once

#include <JuceHeader.h>
#include <PitchLabEngine.h>

#include <array>
#include <cstddef>
#include <span>

/**
    Threading: updateFromEngine() on audio thread; readFloats() on OpenGL thread.
    Short CriticalSection around memcpy only (New Plan D0).
 */
class WaveformSnapshotFeed
{
public:
    static constexpr int kNumSamples = 2048;

    void updateFromEngine (const pitchlab::PitchLabEngine& engine)
    {
        std::array<std::int16_t, kNumSamples> i16 {};
        engine.copyIngressLatest (i16);

        const juce::ScopedLock sl (lock_);
        for (std::size_t i = 0; i < static_cast<std::size_t> (kNumSamples); ++i)
            samples_[i] = static_cast<float> (i16[i]) * (1.0f / 32767.0f);
    }

    void readFloats (std::span<float> dst) const
    {
        const juce::ScopedLock sl (lock_);
        const int n = juce::jmin (static_cast<int> (dst.size()), kNumSamples);
        for (int i = 0; i < n; ++i)
            dst[static_cast<std::size_t> (i)] = samples_[static_cast<std::size_t> (i)];

        for (int i = n; i < static_cast<int> (dst.size()); ++i)
            dst[static_cast<std::size_t> (i)] = 0.0f;
    }

private:
    std::array<float, static_cast<std::size_t> (kNumSamples)> samples_ {};
    mutable juce::CriticalSection lock_;
};
