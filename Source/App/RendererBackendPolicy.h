#pragma once

enum class RenderBackendPolicy
{
    Auto,
    ForceGpu,
    ForceCpu
};

[[nodiscard]] inline bool shouldUseCpuBackend (RenderBackendPolicy policy, bool gpuHealthy) noexcept
{
    switch (policy)
    {
        case RenderBackendPolicy::ForceCpu: return true;
        case RenderBackendPolicy::ForceGpu: return false;
        case RenderBackendPolicy::Auto: return ! gpuHealthy;
        default: return ! gpuHealthy;
    }
}

