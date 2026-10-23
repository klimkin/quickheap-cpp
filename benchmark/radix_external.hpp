#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <type_traits>

#include <radix_heap.h>

namespace quickheap::bench {

template <class T> class ExternalRadixHeap {
    static_assert(std::is_same_v<T, std::int32_t> || std::is_same_v<T, std::int64_t> ||
                  std::is_same_v<T, std::uint32_t> || std::is_same_v<T, std::uint64_t>);

  public:
    using Item = T;
    static constexpr bool monotone = true;
    static constexpr std::size_t columns = 1;
    static constexpr bool comparison_counted = false;

    void push(T key) {
        if (key < lower_bound_)
            throw std::invalid_argument("external radix heap insertion precedes last pop");
        queue_.push(key);
    }

    std::optional<T> try_pop() {
        if (queue_.empty()) return std::nullopt;
        const T key = queue_.top();
        queue_.pop();
        lower_bound_ = key;
        return key;
    }

    bool empty() const { return queue_.empty(); }
    std::size_t size() const { return queue_.size(); }

    void clear() {
        queue_.clear();
        lower_bound_ = std::numeric_limits<T>::lowest();
    }

  private:
    radix_heap::radix_heap<T> queue_;
    T lower_bound_ = std::numeric_limits<T>::lowest();
};

} // namespace quickheap::bench
