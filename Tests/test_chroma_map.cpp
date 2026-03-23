#include <gtest/gtest.h>

#include "ChromaMap.h"

TEST(ChromaMap, GoldenStartBins44100_4096)
{
    pitchlab::ChromaMap m;
    m.rebuild (44100.0, 4096);

    EXPECT_EQ(m.startBin (0), 6);  // C2 ~65.4 Hz
    EXPECT_EQ(m.startBin (9), 10); // A2 ~110 Hz (MIDI 36+9)
}

TEST(ChromaMap, RebuildZerosWhenInvalid)
{
    pitchlab::ChromaMap m;
    m.rebuild (44100.0, 4096);
    m.rebuild (0.0, 4096);
    EXPECT_EQ(m.startBin (0), 0);
}
