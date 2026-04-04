#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <cstdint>
#include <span>

/**
    Shared temporal waterfall model (1024x384 texture policy; 384 chroma values per row).
    This ring is backend-agnostic and can be consumed by GPU and CPU renderers.
 */
class SharedWaterfallRing
{
public:
    static constexpr int kRowBins = 384;
    static constexpr int kRows = 384;

    void pushRow (std::span<const float> row384);

    /** Consume one pending row; returns false if none. outWriteY is target row index before increment. */
    bool consumePendingRow (std::array<float, kRowBins>& outRow, int& outWriteY);

    void latestRow (std::array<float, kRowBins>& outRow) const;
    [[nodiscard]] int writeY() const noexcept;

    /** After uploading rows 0..kRows-1 with oldest at 0 and newest at kRows-1, set head so UV math matches. */
    void syncWriteHeadAfterBulkStaticFill() noexcept;

private:
    mutable juce::CriticalSection lock_;
    std::array<float, kRowBins> latestRow_ {};
    std::array<float, kRowBins> pendingRow_ {};
    int writeY_ = 0;
    bool hasPending_ = false;
};

