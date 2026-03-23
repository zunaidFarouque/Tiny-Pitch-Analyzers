#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

namespace pitchlab
{

/**
    Real FFT magnitudes (JUCE dsp::FFT) for power-of-two sizes (New Plan §3.3).
    Scaling matches juce::dsp::FFT::performFrequencyOnlyForwardTransform (linear magnitude bins);
    peak picking / null tests should use relative peaks on this curve (§8), not absolute SI units.
 */
class FftMagnitudes
{
public:
    explicit FftMagnitudes (int fftOrder);
    ~FftMagnitudes();

    [[nodiscard]] int fftSize() const noexcept;
    /** magOut length must be fftSize/2 + 1 */
    void computePowerSpectrum (std::span<const float> timeDomainInterleavedReal,
                               std::span<float> magOut) noexcept;

private:
    struct Pimpl;
    std::unique_ptr<Pimpl> pimpl_;
};

} // namespace pitchlab
