#pragma once

#include <cstddef>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace hwmodern
{
/* Append-only CPU storage for texture texels. Pages never move, so growing a
   multi-gigabyte collection neither copies existing texels nor temporarily
   allocates a second copy. This is intentionally not a contiguous container.
   65536 sixteen-byte texels occupy one 1 MiB page. */
template<typename T, std::size_t PageElements = 65536>
class PagedArray
{
    static_assert(PageElements != 0 &&
                  (PageElements & (PageElements - 1)) == 0,
                  "PagedArray page size must be a power of two");
    static_assert(PageElements <= std::numeric_limits<std::size_t>::max() / sizeof(T),
                  "PagedArray page allocation must fit size_t");

    static constexpr unsigned int pageShift()
    {
        std::size_t remainingPageElements = PageElements;
        unsigned int shift = 0;
        while (remainingPageElements > 1) { remainingPageElements >>= 1; ++shift; }
        return shift;
    }

public:
    struct Span { T *data; std::size_t size; };
    struct ConstSpan { const T *data; std::size_t size; };

    explicit PagedArray(std::size_t initialSize = 0)
    {
        if (initialSize > max_size())
            throw std::length_error("PagedArray initial size exceeds max_size");
        while (size_ < initialSize) push_back(T{});
    }

    PagedArray(const PagedArray &) = delete;
    PagedArray &operator=(const PagedArray &) = delete;

    std::size_t size() const noexcept { return size_; }
    std::size_t capacity() const noexcept { return pages_.size() * PageElements; }
    std::size_t max_size() const noexcept
    {
        const std::size_t elementLimit =
            std::numeric_limits<std::size_t>::max() / sizeof(T);
        const std::size_t pageLimit = elementLimit / PageElements;
        return (pages_.max_size() < pageLimit ? pages_.max_size() : pageLimit) *
            PageElements;
    }

    T &operator[](std::size_t index) noexcept
    {
        return pages_[index >> pageShift()][index & (PageElements - 1)];
    }
    const T &operator[](std::size_t index) const noexcept
    {
        return pages_[index >> pageShift()][index & (PageElements - 1)];
    }

    // Sequential conversion can use ordinary contiguous inner loops within a
    // page. This preserves vectorization and hoists the page lookup out of
    // each texel access; spans never cross a page boundary.
    Span span(std::size_t index, std::size_t requested) noexcept
    {
        const auto view = static_cast<const PagedArray &>(*this).span(index, requested);
        return {const_cast<T *>(view.data), view.size};
    }
    ConstSpan span(std::size_t index, std::size_t requested) const noexcept
    {
        if (index >= size_ || requested == 0) return {nullptr, 0};
        std::size_t available = PageElements - (index & (PageElements - 1));
        if (available > size_ - index) available = size_ - index;
        if (available > requested) available = requested;
        return {&(*this)[index], available};
    }

    void push_back(const T &value)
    {
        prepareAppend();
        (*this)[size_] = value;
        ++size_;
    }
    void push_back(T &&value)
    {
        prepareAppend();
        (*this)[size_] = std::move(value);
        ++size_;
    }

    void clear() noexcept
    {
        pages_.clear();
        size_ = 0;
    }

private:
    void prepareAppend()
    {
        if (size_ == max_size())
            throw std::length_error("PagedArray size exceeds max_size");
        if (size_ == capacity())
        {
            // Allocate exactly one page. An allocation failure leaves all
            // existing records and their addresses unchanged.
            std::unique_ptr<T[]> next(new T[PageElements]);
            pages_.push_back(std::move(next));
        }
    }

    std::vector<std::unique_ptr<T[]>> pages_;
    std::size_t size_ = 0;
};
}
