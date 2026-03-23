#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace pitchlab
{

/** Fixed-capacity int16 ring for ingress (New Plan §2.2). No heap alloc in push. */
class CircularInt16Buffer
{
public:
    explicit CircularInt16Buffer (std::size_t capacity);

    void reset();

    /** Append samples; uses split memcpy and conditional subtract for head wrap (no %). */
    void push (std::span<const std::int16_t> samples);

    [[nodiscard]] std::size_t capacity() const noexcept { return storage_.size(); }
    [[nodiscard]] std::size_t writeHead() const noexcept { return writeHead_; }

    /** Direct sample read for tests (index in [0, capacity)). */
    [[nodiscard]] std::int16_t rawAt (std::size_t index) const noexcept;

    /** Last `dst.size()` samples in chronological order (oldest at dst.front()). If dst is longer than capacity, leading samples are zero-padded. */
    void copyLatestInto (std::span<std::int16_t> dst) const noexcept;

private:
    std::vector<std::int16_t> storage_;
    std::size_t writeHead_ = 0;
};

} // namespace pitchlab
