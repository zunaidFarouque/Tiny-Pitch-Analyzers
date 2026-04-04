#include "Decimator4x.h"

#include <juce_dsp/juce_dsp.h>

namespace pitchlab
{

void Decimator4x::prepare (double sampleRate)
{
    filters_.clear();
    sampleCounter_ = 0;

    if (sampleRate <= 0.0)
        return;

    const float cutoffHz = static_cast<float> (5000.0 * sampleRate / 44100.0);
    auto coeffs = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod (
        cutoffHz, sampleRate, 8);

    filters_.reserve (static_cast<std::size_t> (coeffs.size()));
    for (int i = 0; i < coeffs.size(); ++i)
    {
        juce::dsp::IIR::Filter<float> f;
        f.coefficients = coeffs.getUnchecked (i);
        filters_.push_back (std::move (f));
    }

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = 2048;
    spec.numChannels = 1;

    for (auto& f : filters_)
    {
        f.prepare (spec);
        f.reset();
    }
}

void Decimator4x::reset() noexcept
{
    sampleCounter_ = 0;
    for (auto& f : filters_)
        f.reset();
}

int Decimator4x::processBlock (std::span<const float> in, std::span<float> out) noexcept
{
    if (filters_.empty() || out.empty())
        return 0;

    const int maxPossible = static_cast<int> (in.size() / 4);
    const int cap = static_cast<int> (out.size());
    int w = 0;

    for (float x : in)
    {
        for (auto& f : filters_)
            x = f.processSample (x);

        ++sampleCounter_;
        if ((sampleCounter_ % 4ULL) == 0ULL && w < cap && w < maxPossible)
            out[static_cast<std::size_t> (w++)] = x;
    }

    return w;
}

} // namespace pitchlab
