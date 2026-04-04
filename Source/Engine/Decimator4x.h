#pragma once

#include <span>
#include <vector>

#include <juce_dsp/juce_dsp.h>

namespace pitchlab
{

/** 4× decimation: anti-alias low-pass (~5 kHz @ 44.1 kHz, scaled with sample rate) then keep every 4th sample.
    All allocation in prepare(); processBlock is allocation-free. */
class Decimator4x
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;

    /** Filters `in` and writes floor(in.size()/4) samples to `out`. Returns count written (== in.size()/4).
        Requires out.size() >= in.size()/4. If in.size() is not a multiple of 4, trailing samples are processed
        for filter state but produce no extra output. */
    int processBlock (std::span<const float> in, std::span<float> out) noexcept;

private:
    std::vector<juce::dsp::IIR::Filter<float>> filters_;
    std::uint64_t sampleCounter_ = 0;
};

} // namespace pitchlab
