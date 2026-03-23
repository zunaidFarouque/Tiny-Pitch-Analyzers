#include "SharedWaterfallRing.h"

#include <algorithm>

void SharedWaterfallRing::pushRow (std::span<const float> row384)
{
    const juce::ScopedLock sl (lock_);
    const int n = juce::jmin (kRowBins, static_cast<int> (row384.size()));
    for (int i = 0; i < n; ++i)
    {
        pendingRow_[static_cast<std::size_t> (i)] = row384[static_cast<std::size_t> (i)];
        latestRow_[static_cast<std::size_t> (i)] = row384[static_cast<std::size_t> (i)];
    }
    for (int i = n; i < kRowBins; ++i)
    {
        pendingRow_[static_cast<std::size_t> (i)] = 0.0f;
        latestRow_[static_cast<std::size_t> (i)] = 0.0f;
    }
    hasPending_ = true;
}

bool SharedWaterfallRing::consumePendingRow (std::array<float, kRowBins>& outRow, int& outWriteY)
{
    const juce::ScopedLock sl (lock_);
    if (! hasPending_)
        return false;

    outRow = pendingRow_;
    outWriteY = writeY_;
    writeY_ = (writeY_ + 1) % kRows;
    hasPending_ = false;
    return true;
}

void SharedWaterfallRing::latestRow (std::array<float, kRowBins>& outRow) const
{
    const juce::ScopedLock sl (lock_);
    outRow = latestRow_;
}

int SharedWaterfallRing::writeY() const noexcept
{
    const juce::ScopedLock sl (lock_);
    return writeY_;
}

