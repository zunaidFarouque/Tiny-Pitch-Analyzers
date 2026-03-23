#include "CircularInt16Buffer.h"

#include <algorithm>
#include <cstring>

namespace pitchlab
{

CircularInt16Buffer::CircularInt16Buffer (std::size_t capacity)
    : storage_ (capacity, 0)
{
}

void CircularInt16Buffer::reset()
{
    writeHead_ = 0;
    std::fill (storage_.begin(), storage_.end(), static_cast<std::int16_t> (0));
}

void CircularInt16Buffer::push (std::span<const std::int16_t> samples)
{
    if (samples.empty() || storage_.empty())
        return;

    const std::size_t cap = storage_.size();
    std::size_t head = writeHead_;
    const std::size_t n = samples.size();

    const std::size_t spaceEnd = cap - head;

    if (n <= spaceEnd)
    {
        std::memcpy (storage_.data() + head, samples.data(), n * sizeof (std::int16_t));
        head += n;
    }
    else
    {
        std::memcpy (storage_.data() + head, samples.data(), spaceEnd * sizeof (std::int16_t));
        const std::size_t rem = n - spaceEnd;
        std::memcpy (storage_.data(), samples.data() + spaceEnd, rem * sizeof (std::int16_t));
        head = rem;
    }

    if (head >= cap)
        head -= cap;

    writeHead_ = head;
}

std::int16_t CircularInt16Buffer::rawAt (std::size_t index) const noexcept
{
    if (storage_.empty())
        return 0;
    return storage_[index % storage_.size()];
}

void CircularInt16Buffer::copyLatestInto (std::span<std::int16_t> dst) const noexcept
{
    if (dst.empty() || storage_.empty())
        return;

    const auto cap = storage_.size();
    const auto n = std::min (dst.size(), cap);
    const auto skip = dst.size() - n;

    if (skip > 0)
        std::fill (dst.begin(), dst.begin() + static_cast<std::ptrdiff_t> (skip), static_cast<std::int16_t> (0));

    const std::size_t start = (writeHead_ + cap - n) % cap;

    for (std::size_t i = 0; i < n; ++i)
        dst[static_cast<std::size_t> (skip) + i] = storage_[(start + i) % cap];
}

} // namespace pitchlab
