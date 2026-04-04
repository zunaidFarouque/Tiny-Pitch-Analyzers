#include <gtest/gtest.h>

#include "WaterfallFreqAxis.h"
#include "WaterfallPeakRow.h"

#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

TEST (WaterfallPeakRow, EmptyPeaksZerosRow)
{
    std::vector<float> row (384, 1.0f);
    pitchlab::fillWaterfallRowFromPeaks (std::span<const pitchlab::PitchPeak> {},
                                         std::span<float> { row.data(), row.size() },
                                         pitchlab::WaterfallFreqAxis::kVisMinHz,
                                         pitchlab::WaterfallFreqAxis::kVisMaxHz);
    for (float v : row)
        EXPECT_EQ (v, 0.0f);
}

TEST (WaterfallPeakRow, WrongSpanSizeNoOp)
{
    std::vector<float> row (100, 1.0f);
    pitchlab::PitchPeak pk;
    pk.frequencyHz = 440.0f;
    pk.magnitude = 2.0f;
    const std::vector<pitchlab::PitchPeak> peaks { pk };
    pitchlab::fillWaterfallRowFromPeaks (std::span<const pitchlab::PitchPeak> { peaks.data(), peaks.size() },
                                         std::span<float> { row.data(), row.size() },
                                         pitchlab::WaterfallFreqAxis::kVisMinHz,
                                         pitchlab::WaterfallFreqAxis::kVisMaxHz);
    EXPECT_EQ (row[0], 1.0f);
}

TEST (WaterfallPeakRow, SinglePeak440MapsToExpectedBin)
{
    std::vector<float> row (384, 0.0f);
    pitchlab::PitchPeak pk;
    pk.frequencyHz = 440.0f;
    pk.magnitude = 5.0f;

    const std::vector<pitchlab::PitchPeak> peaks { pk };
    pitchlab::fillWaterfallRowFromPeaks (std::span<const pitchlab::PitchPeak> { peaks.data(), peaks.size() },
                                         std::span<float> { row.data(), row.size() },
                                         pitchlab::WaterfallFreqAxis::kVisMinHz,
                                         pitchlab::WaterfallFreqAxis::kVisMaxHz);

    int nonZero = 0;
    int idx = -1;
    for (int i = 0; i < 384; ++i)
    {
        if (row[static_cast<std::size_t> (i)] > 0.0f)
        {
            ++nonZero;
            idx = i;
        }
    }
    ASSERT_EQ (nonZero, 1);
    EXPECT_EQ (row[static_cast<std::size_t> (idx)], 5.0f);

    const double logMin = std::log2 (static_cast<double> (pitchlab::WaterfallFreqAxis::kVisMinHz));
    const double logDen = std::log2 (static_cast<double> (pitchlab::WaterfallFreqAxis::kVisMaxHz)) - logMin;
    const double t = (std::log2 (440.0) - logMin) / logDen;
    const int expectedK = static_cast<int> (std::lround (t * 383.0));
    EXPECT_EQ (idx, std::clamp (expectedK, 0, 383));
}

TEST (WaterfallPeakRow, TwoPeaksFarApartTwoBins)
{
    std::vector<float> row (384, 0.0f);
    pitchlab::PitchPeak a;
    a.frequencyHz = 200.0f;
    a.magnitude = 1.0f;
    pitchlab::PitchPeak b;
    b.frequencyHz = 4000.0f;
    b.magnitude = 2.0f;

    const std::vector<pitchlab::PitchPeak> peaks { a, b };
    pitchlab::fillWaterfallRowFromPeaks (std::span<const pitchlab::PitchPeak> { peaks.data(), peaks.size() },
                                         std::span<float> { row.data(), row.size() },
                                         pitchlab::WaterfallFreqAxis::kVisMinHz,
                                         pitchlab::WaterfallFreqAxis::kVisMaxHz);

    int count = 0;
    for (float v : row)
        if (v > 0.0f)
            ++count;
    EXPECT_EQ (count, 2);
}

TEST (WaterfallPeakRow, OutsideBandIgnored)
{
    std::vector<float> row (384, 0.0f);
    pitchlab::PitchPeak pk;
    pk.frequencyHz = 30.0f;
    pk.magnitude = 99.0f;

    const std::vector<pitchlab::PitchPeak> peaks { pk };
    pitchlab::fillWaterfallRowFromPeaks (std::span<const pitchlab::PitchPeak> { peaks.data(), peaks.size() },
                                         std::span<float> { row.data(), row.size() },
                                         pitchlab::WaterfallFreqAxis::kVisMinHz,
                                         pitchlab::WaterfallFreqAxis::kVisMaxHz);

    for (float v : row)
        EXPECT_EQ (v, 0.0f);
}

TEST (WaterfallPeakRow, SameBinMaxMagnitude)
{
    std::vector<float> row (384, 0.0f);
    pitchlab::PitchPeak a;
    a.frequencyHz = 440.0f;
    a.magnitude = 1.0f;
    pitchlab::PitchPeak b;
    b.frequencyHz = 441.0f;
    b.magnitude = 3.0f;

    const std::vector<pitchlab::PitchPeak> peaks { a, b };
    pitchlab::fillWaterfallRowFromPeaks (std::span<const pitchlab::PitchPeak> { peaks.data(), peaks.size() },
                                         std::span<float> { row.data(), row.size() },
                                         pitchlab::WaterfallFreqAxis::kVisMinHz,
                                         pitchlab::WaterfallFreqAxis::kVisMaxHz);

    int nonZero = 0;
    float mag = 0.0f;
    for (float v : row)
    {
        if (v > 0.0f)
        {
            ++nonZero;
            mag = v;
        }
    }
    EXPECT_EQ (nonZero, 1);
    EXPECT_EQ (mag, 3.0f);
}
