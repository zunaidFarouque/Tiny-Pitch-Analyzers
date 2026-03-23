#include <gtest/gtest.h>

#include "RendererBackendPolicy.h"

TEST(RendererBackendPolicy, AutoUsesCpuWhenGpuUnhealthy)
{
    EXPECT_TRUE(shouldUseCpuBackend (RenderBackendPolicy::Auto, false));
    EXPECT_FALSE(shouldUseCpuBackend (RenderBackendPolicy::Auto, true));
}

TEST(RendererBackendPolicy, ForcedPoliciesOverrideHealth)
{
    EXPECT_TRUE(shouldUseCpuBackend (RenderBackendPolicy::ForceCpu, true));
    EXPECT_TRUE(shouldUseCpuBackend (RenderBackendPolicy::ForceCpu, false));
    EXPECT_FALSE(shouldUseCpuBackend (RenderBackendPolicy::ForceGpu, true));
    EXPECT_FALSE(shouldUseCpuBackend (RenderBackendPolicy::ForceGpu, false));
}

