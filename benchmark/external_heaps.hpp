#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>

namespace quickheap::bench {

template <class T>
inline constexpr bool external_key = std::is_same_v<T, std::int32_t> || std::is_same_v<T, std::int64_t> ||
                                     std::is_same_v<T, std::uint32_t> || std::is_same_v<T, std::uint64_t>;

template <class T> class SequenceHeap {
public:
    static_assert(external_key<T>);
    using Item = T;
    explicit SequenceHeap(std::size_t capacity = 0);
    ~SequenceHeap();
    SequenceHeap(SequenceHeap&&) noexcept;
    SequenceHeap& operator=(SequenceHeap&&) noexcept;
    SequenceHeap(const SequenceHeap&) = delete;
    SequenceHeap& operator=(const SequenceHeap&) = delete;
    void push(T key);
    std::optional<T> try_pop();
    std::size_t size() const;
    bool valid() const { return true; }
    bool empty() const { return size() == 0; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

template <class T> class S3QHeap {
public:
    static_assert(external_key<T>);
    using Item = T;
    explicit S3QHeap(std::size_t capacity = 0);
    ~S3QHeap();
    S3QHeap(S3QHeap&&) noexcept;
    S3QHeap& operator=(S3QHeap&&) noexcept;
    S3QHeap(const S3QHeap&) = delete;
    S3QHeap& operator=(const S3QHeap&) = delete;
    void push(T key);
    std::optional<T> try_pop();
    std::size_t size() const;
    bool valid() const { return true; }
    bool empty() const { return size() == 0; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

template <class T> class OriginalQuickHeap {
public:
    static_assert(external_key<T>);
    using Item = T;
    explicit OriginalQuickHeap(std::size_t capacity = 1U << 20);
    ~OriginalQuickHeap();
    OriginalQuickHeap(OriginalQuickHeap&&) noexcept;
    OriginalQuickHeap& operator=(OriginalQuickHeap&&) noexcept;
    OriginalQuickHeap(const OriginalQuickHeap&) = delete;
    OriginalQuickHeap& operator=(const OriginalQuickHeap&) = delete;
    void push(T key);
    std::optional<T> try_pop();
    std::size_t size() const;
    bool valid() const { return true; }
    bool empty() const { return size() == 0; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace quickheap::bench
