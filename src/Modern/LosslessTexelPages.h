#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <vector>
#if defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(__SSE2__)
#include <emmintrin.h>
#endif
#ifdef _WIN32
#include <windows.h>
#include <compressapi.h>
#endif

namespace hwmodern
{
/* CPU backing for immutable material texels, not a GPU texture format. Completed
   pages are byte-shuffled and losslessly compressed; GPU uploads recover the
   original bits. A bounded cache serves load-time emissive sampling and uploads.
   A reference/span is borrowed until the NEXT read/append, unlike PagedArray's
   stable addresses. Callers must consume it before asking for another page.
   Incompressible pages stay raw, so the cache bound is not a total RAM budget.
   This container, like the renderer's material registry, is single-threaded. */
template<typename T, std::size_t PageElements = 65536,
         std::size_t DecodedPages = 257>
class LosslessTexelPages
{
    static_assert(std::is_trivially_copyable<T>::value,
                  "Texel compression must preserve a trivial object representation");
    static_assert(PageElements != 0 && DecodedPages != 0,
                  "Texel pages and decoded cache must be nonempty");
    static_assert(PageElements <= std::numeric_limits<std::size_t>::max() / sizeof(T),
                  "Texel page byte count must fit size_t");
    static constexpr std::size_t PageBytes = PageElements * sizeof(T);
    struct Page
    {
        std::unique_ptr<T[]> raw;
        std::vector<unsigned char> compressed;
    };
    struct CachedPage
    {
        std::size_t index = std::numeric_limits<std::size_t>::max();
        std::uint64_t lastUse = 0;
        std::unique_ptr<T[]> texels;
    };

public:
    struct ConstSpan { const T *data; std::size_t size; };

    explicit LosslessTexelPages(std::size_t initialSize = 0)
        : decoded_(DecodedPages)
    {
        if (initialSize > max_size())
            throw std::length_error("Texel array initial size exceeds max_size");
        while (size_ < initialSize) push_back(T{});
    }
    ~LosslessTexelPages()
    {
#ifdef _WIN32
        if (compressor_) CloseCompressor(compressor_);
        if (decompressor_) CloseDecompressor(decompressor_);
#endif
    }
    LosslessTexelPages(const LosslessTexelPages &) = delete;
    LosslessTexelPages &operator=(const LosslessTexelPages &) = delete;

    std::size_t size() const noexcept { return size_; }
    std::size_t max_size() const noexcept
    {
        return std::min(pages_.max_size(),
            std::numeric_limits<std::size_t>::max() / PageBytes) * PageElements;
    }
    const T &operator[](std::size_t index) const
    {
        return pageData(index / PageElements)[index % PageElements];
    }
    ConstSpan span(std::size_t index, std::size_t requested) const
    {
        if (index >= size_ || requested == 0) return {nullptr, 0};
        const std::size_t available = std::min(requested,
            std::min(PageElements - index % PageElements, size_ - index));
        return {&(*this)[index], available};
    }
    void push_back(const T &value)
    {
        // Also supports appending a value borrowed from our own current page.
        T copy;
        std::memcpy(&copy, &value, sizeof(T));
        if (size_ == max_size())
            throw std::length_error("Texel array size exceeds max_size");
        if (size_ % PageElements == 0)
        {
            // Allocate the next tail before sealing the old one. Allocation
            // failure leaves all previous values readable and size unchanged.
            Page next;
            next.raw.reset(new T[PageElements]);
            if (!pages_.empty()) sealPage(pages_.back());
            pages_.push_back(std::move(next));
        }
        std::memcpy(&pages_.back().raw[size_ % PageElements], &copy, sizeof(T));
        ++size_;
    }
    void clear() noexcept
    {
        pages_.clear();
        for (auto &entry : decoded_)
        {
            entry.texels.reset();
            entry.index = std::numeric_limits<std::size_t>::max();
            entry.lastUse = 0;
        }
        shuffled_.reset();
        size_ = 0;
        clock_ = 0;
        decompressions_ = 0;
        lastIndex_ = std::numeric_limits<std::size_t>::max();
        lastData_ = nullptr;
    }
    std::size_t storageBytes() const noexcept
    {
        std::size_t bytes = shuffled_ ? PageBytes : 0;
        for (const auto &page : pages_)
            bytes += page.raw ? PageBytes : page.compressed.size();
        for (const auto &entry : decoded_)
            if (entry.texels) bytes += PageBytes;
        return bytes;
    }
    std::size_t compressedPages() const noexcept
    {
        std::size_t count = 0;
        for (const auto &page : pages_) count += !page.compressed.empty();
        return count;
    }
    std::size_t cachedPages() const noexcept
    {
        std::size_t count = 0;
        for (const auto &entry : decoded_) count += !!entry.texels;
        return count;
    }
    std::uint64_t decompressions() const noexcept { return decompressions_; }

private:
#if defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(__SSE2__)

    static void transpose16(const unsigned char *source, std::size_t sourceStride,
                            unsigned char *destination, std::size_t destinationStride)
    {
        __m128i a[16], b[16];
        for (unsigned i=0; i<16; ++i)
            a[i] = _mm_loadu_si128(reinterpret_cast<const __m128i *>(source+i*sourceStride));
        for (unsigned i=0; i<16; i+=2) {
            b[i] = _mm_unpacklo_epi8(a[i],a[i+1]);
            b[i+1] = _mm_unpackhi_epi8(a[i],a[i+1]);
        }
        for (unsigned i=0; i<16; i+=4) {
            a[i] = _mm_unpacklo_epi16(b[i],b[i+2]);
            a[i+1] = _mm_unpackhi_epi16(b[i],b[i+2]);
            a[i+2] = _mm_unpacklo_epi16(b[i+1],b[i+3]);
            a[i+3] = _mm_unpackhi_epi16(b[i+1],b[i+3]);
        }
        for (unsigned i=0; i<16; i+=8) {
            b[i] = _mm_unpacklo_epi32(a[i],a[i+4]);
            b[i+1] = _mm_unpackhi_epi32(a[i],a[i+4]);
            b[i+2] = _mm_unpacklo_epi32(a[i+1],a[i+5]);
            b[i+3] = _mm_unpackhi_epi32(a[i+1],a[i+5]);
            b[i+4] = _mm_unpacklo_epi32(a[i+2],a[i+6]);
            b[i+5] = _mm_unpackhi_epi32(a[i+2],a[i+6]);
            b[i+6] = _mm_unpacklo_epi32(a[i+3],a[i+7]);
            b[i+7] = _mm_unpackhi_epi32(a[i+3],a[i+7]);
        }
        for (unsigned i=0; i<8; ++i) {
            const auto low = _mm_unpacklo_epi64(b[i],b[i+8]);
            const auto high = _mm_unpackhi_epi64(b[i],b[i+8]);
            _mm_storeu_si128(reinterpret_cast<__m128i *>(destination+(i*2)*destinationStride),low);
            _mm_storeu_si128(reinterpret_cast<__m128i *>(destination+(i*2+1)*destinationStride),high);
        }
    }
#endif

    void ensureScratch() const
    {
        if (!shuffled_) shuffled_.reset(new unsigned char[PageBytes]);
    }
    void sealPage(Page &page)
    {
#ifdef _WIN32
        if (!page.raw) return; // A previous append allocation may have failed.
        if (!codecAttempted_)
        {
            codecAttempted_ = true;
            if (!CreateCompressor(COMPRESS_ALGORITHM_XPRESS, nullptr, &compressor_))
                return;
            if (!CreateDecompressor(COMPRESS_ALGORITHM_XPRESS, nullptr, &decompressor_))
            {
                CloseCompressor(compressor_);
                compressor_ = nullptr;
                return;
            }
        }
        if (!compressor_ || !decompressor_) return; // Raw fallback is exact.
        ensureScratch();
        const auto *source = reinterpret_cast<const unsigned char *>(page.raw.get());
#if defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(__SSE2__)
        if constexpr (sizeof(T) == 16 && PageElements % 16 == 0)
        {
            // Unaligned loads/stores accept allocator and byte-plane offsets.
            for (std::size_t i = 0; i < PageElements; i += 16)
                transpose16(source + i * 16, 16,
                            shuffled_.get() + i, PageElements);
        }
        else
#endif
        {
        for (std::size_t byte = 0; byte < sizeof(T); ++byte)
            for (std::size_t element = 0; element < PageElements; ++element)
                shuffled_[byte * PageElements + element] = source[element * sizeof(T) + byte];
        }
        std::vector<unsigned char> output(PageBytes);
        SIZE_T written = 0;
        if (!Compress(compressor_, shuffled_.get(), PageBytes,
                      output.data(), output.size(), &written) ||
            written == 0 || written >= PageBytes)
            return;
        // Keep only the encoded bytes, not output's original 1 MiB capacity.
        page.compressed.assign(output.data(), output.data() + written);
        lastIndex_ = std::numeric_limits<std::size_t>::max();
        lastData_ = nullptr;
        page.raw.reset();
#else
        (void)page;
#endif
    }
    const T *pageData(std::size_t index) const
    {
        if (lastIndex_ == index) return lastData_;
        const Page &page = pages_[index];
        if (page.raw)
        {
            lastIndex_ = index;
            lastData_ = page.raw.get();
            return lastData_;
        }
        for (auto &entry : decoded_)
        {
            if (entry.index == index)
            {
                entry.lastUse = ++clock_;
                lastIndex_ = index;
                lastData_ = entry.texels.get();
                return entry.texels.get();
            }
        }
        auto entry = std::min_element(decoded_.begin(), decoded_.end(),
            [](const CachedPage &a, const CachedPage &b) { return a.lastUse < b.lastUse; });
        if (!entry->texels) entry->texels.reset(new T[PageElements]);
        // Invalidate before decoding: an exception must never expose partially
        // replaced bytes as the former cached page on a subsequent lookup.
        entry->index = std::numeric_limits<std::size_t>::max();
        lastIndex_ = std::numeric_limits<std::size_t>::max();
        lastData_ = nullptr;
        ensureScratch();
#ifdef _WIN32
        SIZE_T written = 0;
        if (!Decompress(decompressor_, page.compressed.data(), page.compressed.size(),
                        shuffled_.get(), PageBytes, &written) || written != PageBytes)
            throw std::runtime_error("Lossless material texel decompression failed");
        auto *destination = reinterpret_cast<unsigned char *>(entry->texels.get());
#if defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(__SSE2__)
        if constexpr (sizeof(T) == 16 && PageElements % 16 == 0)
        {
            // Transposing the byte planes again restores the exact object bits.
            for (std::size_t i = 0; i < PageElements; i += 16)
                transpose16(shuffled_.get() + i, PageElements,
                            destination + i * 16, 16);
        }
        else
#endif
        {
        for (std::size_t byte = 0; byte < sizeof(T); ++byte)
            for (std::size_t element = 0; element < PageElements; ++element)
                destination[element * sizeof(T) + byte] = shuffled_[byte * PageElements + element];
        }
#else
        throw std::runtime_error("Compressed texel page unavailable on this platform");
#endif
        entry->index = index;
        entry->lastUse = ++clock_;
        ++decompressions_;
        lastIndex_ = index;
        lastData_ = entry->texels.get();
        return entry->texels.get();
    }

    std::vector<Page> pages_;
    mutable std::vector<CachedPage> decoded_;
    mutable std::unique_ptr<unsigned char[]> shuffled_;
    std::size_t size_ = 0;
    mutable std::uint64_t clock_ = 0;
    mutable std::uint64_t decompressions_ = 0;
    mutable std::size_t lastIndex_ = std::numeric_limits<std::size_t>::max();
    mutable const T *lastData_ = nullptr;
#ifdef _WIN32
    bool codecAttempted_ = false;
    COMPRESSOR_HANDLE compressor_ = nullptr;
    DECOMPRESSOR_HANDLE decompressor_ = nullptr;
#endif
};
}
