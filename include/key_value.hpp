#ifndef QUICKHEAP_KEY_VALUE_HPP
#define QUICKHEAP_KEY_VALUE_HPP

#include "export.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace quickheap {

/// Min-priority queue of matching integer keys and payloads.
/// Equal-key payload order is unspecified.
template <class T>
class QUICKHEAP_API KvSimdQuickHeap {
    static_assert(std::is_same_v<T, std::int32_t> ||
                      std::is_same_v<T, std::uint32_t> ||
                      std::is_same_v<T, std::int64_t> ||
                      std::is_same_v<T, std::uint64_t>,
                  "KvSimdQuickHeap requires a 32-bit or 64-bit integer type");

public:
    KvSimdQuickHeap();
    ~KvSimdQuickHeap();
    KvSimdQuickHeap(const KvSimdQuickHeap&) = delete;
    KvSimdQuickHeap& operator=(const KvSimdQuickHeap&) = delete;
    KvSimdQuickHeap(KvSimdQuickHeap&&) noexcept;
    KvSimdQuickHeap& operator=(KvSimdQuickHeap&&) noexcept;

    void push(T key, T value);
    std::optional<std::pair<T, T>> try_pop();

    /// Return the minimum-key entry. Requires !empty().
    std::pair<T, T> pop();
    std::size_t size() const;
    bool empty() const;

    /// Total allocated element slots across key and payload buckets.
    std::size_t capacity() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

extern template class KvSimdQuickHeap<std::int32_t>;
extern template class KvSimdQuickHeap<std::uint32_t>;
extern template class KvSimdQuickHeap<std::int64_t>;
extern template class KvSimdQuickHeap<std::uint64_t>;

}

#endif
