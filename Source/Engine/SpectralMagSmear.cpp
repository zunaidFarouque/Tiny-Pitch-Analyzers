#include "SpectralMagSmear.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace pitchlab
{

void buildMagForFold (std::span<const float> magIn,
                      std::span<float> magOut,
                      double sampleRate,
                      int fftSize,
                      SpectralBackendMode backend) noexcept
{
    if (magIn.empty() || magOut.size() < magIn.size() || fftSize <= 0 || sampleRate <= 0.0)
        return;

    const int B = static_cast<int> (magIn.size());

    if (backend == SpectralBackendMode::STFT_v1_0
        || backend == SpectralBackendMode::MultiResSTFT_v1_0)
    {
        std::copy (magIn.begin(), magIn.end(), magOut.begin());
        return;
    }

    const double binHz = sampleRate / static_cast<double> (fftSize);
    constexpr float Q0 = 28.0f;
    constexpr double fref = 440.0;
    constexpr float beta = 0.25f;

    magOut[0] = magIn[0];
    magOut[static_cast<std::size_t> (B - 1)] = magIn[static_cast<std::size_t> (B - 1)];

    for (int b = 1; b < B - 1; ++b)
    {
        const double hz = static_cast<double> (b) * binHz;
        float Qeff = Q0;
        if (backend == SpectralBackendMode::VariableQApprox_v0_1 && hz > 1.0e-6)
            Qeff = Q0 * std::pow (static_cast<float> (hz / fref), beta);

        const double bw = hz / static_cast<double> (Qeff);
        int halfW = static_cast<int> (std::lround (bw / binHz));
        halfW = std::clamp (halfW, 1, 96);

        double acc = 0.0;
        double sumW = 0.0;
        for (int d = -halfW; d <= halfW; ++d)
        {
            const int j = b + d;
            if (j < 0 || j >= B)
                continue;
            const float tri = 1.0f - static_cast<float> (std::abs (d)) / static_cast<float> (halfW + 1);
            const float wgt = std::max (0.0f, tri);
            acc += static_cast<double> (magIn[static_cast<std::size_t> (j)]) * static_cast<double> (wgt);
            sumW += static_cast<double> (wgt);
        }
        magOut[static_cast<std::size_t> (b)] = sumW > 1.0e-12 ? static_cast<float> (acc / sumW) : 0.0f;
    }
}

} // namespace pitchlab
