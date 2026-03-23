#include "FftMagnitudes.h"

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>

namespace pitchlab
{

struct FftMagnitudes::Pimpl
{
    explicit Pimpl (int orderIn)
        : fft (orderIn)
        , order (orderIn)
    {
        buf_.assign (static_cast<std::size_t> (2 * fft.getSize()), 0.0f);
    }

    juce::dsp::FFT fft;
    int order = 0;
    std::vector<float> buf_;
};

FftMagnitudes::FftMagnitudes (int fftOrder)
    : pimpl_ (std::make_unique<Pimpl> (fftOrder))
{
}

FftMagnitudes::~FftMagnitudes() = default;

int FftMagnitudes::fftSize() const noexcept
{
    return pimpl_->fft.getSize();
}

void FftMagnitudes::computePowerSpectrum (std::span<const float> timeDomainInterleavedReal,
                                          std::span<float> magOut) noexcept
{
    auto& fft = pimpl_->fft;
    const int n = fft.getSize();
    auto& buf = pimpl_->buf_;

    if ((int) timeDomainInterleavedReal.size() < n || (int) magOut.size() < n / 2 + 1)
        return;

    std::fill (buf.begin(), buf.end(), 0.0f);
    for (int i = 0; i < n; ++i)
        buf[static_cast<std::size_t> (i)] = timeDomainInterleavedReal[static_cast<std::size_t> (i)];

    fft.performFrequencyOnlyForwardTransform (buf.data(), true);

    const int outBins = n / 2 + 1;
    for (int k = 0; k < outBins; ++k)
        magOut[static_cast<std::size_t> (k)] = buf[static_cast<std::size_t> (k)];
}

} // namespace pitchlab
