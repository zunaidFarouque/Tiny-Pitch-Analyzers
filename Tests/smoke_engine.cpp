#include <cstring>
#include <gtest/gtest.h>

#include "PitchLabEngine.h"

TEST(PitchLabEngine, VersionStringIsNonEmpty)
{
    const char* v = pitchlab::engineVersionString();
    ASSERT_NE(v, nullptr);
    EXPECT_GT(std::strlen(v), 0u);
}
