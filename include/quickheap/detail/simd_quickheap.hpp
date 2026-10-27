#pragma once

#include "partitioning.hpp"
#include "pivots.hpp"
#include "simd.hpp"
#include "sort.hpp"
#include "vec.hpp"

#include <cassert>
#include <cstddef>
#include <new>
#include <utility>
#include <vector>

namespace quickheap::detail {

// Mirrors `SimdQuickHeap` from the Rust crate's lib.rs. `pivots` decrease; `buckets[i]`
// holds elements between pivots i-1 and i. The terminal layer `buckets[pivots.size()]`
// stays sorted in decreasing order whenever it holds at most N elements.
template <class T, class Backend = DefaultBackend>
class SimdQuickHeap {
    static constexpr std::size_t N = 16;
    static constexpr std::size_t L = SimdElem<T, Backend>::L;

    Vec<T> pivots_ = Vec<T>::with_capacity(128);
    std::vector<Vec<T>> buckets_ = std::vector<Vec<T>>(128);
    std::size_t size_ = 0;

    static void sort_descending(Vec<T>& layer) noexcept {
        sort_unstable_small(layer.data(), layer.size(), [](T a, T b) { return a > b; });
    }

    [[gnu::noinline]] void partition() {
        const auto total_layers = pivots_.size() + 1;

        if (total_layers + 2 * L >= pivots_.capacity()) {
            pivots_.reserve(L);
        }

        if (total_layers == buckets_.size()) {
            buckets_.emplace_back();
        }

        auto& cur_layer = buckets_[pivots_.size()];
        auto& next_layer = buckets_[pivots_.size() + 1];

        const auto [pivot, pivot_pos] = MedianOfM<3>::pick(cur_layer.data(), cur_layer.size());

        pivots_.push(pivot);

        next_layer.clear();

        // Rust aborts on allocation failure; C++ unwinds, so drop the pivot to keep the heap valid.
        try {
            VecPartitioning<T, Backend>::partition(cur_layer, next_layer, pivot, pivot_pos);
        } catch (...) {
            pivots_.pop();
            throw;
        }

        if (next_layer.empty()) {
            pivots_.pop();
        }

        if (cur_layer.empty()) {
            cur_layer.swap(next_layer);
            pivots_.pop();
        }
    }

public:
    std::size_t capacity() const noexcept {
        std::size_t total = 0;
        for (const auto& bucket : buckets_) {
            total += bucket.capacity();
        }
        return total;
    }

    std::size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }

    void push(T t) {
        const auto target_layer = push_position<T, Backend>(pivots_, t);

        auto& layer = buckets_[target_layer];

        if (target_layer == pivots_.size() && layer.size() < N) {
            const auto pos = partition_point(layer.data(), layer.size(), [t](T x) { return x > t; });
            layer.insert(pos, t);
        } else {
            layer.push(t);
        }

        ++size_;
    }

    // Rust returns `Option<T>` in registers. GCC returns `std::optional<T>` via a stack
    // spill whose reload stalls store forwarding on every pop.
    bool pop_into(T& out) {
        if (size_ == 0) {
            return false;
        }

        if (buckets_[pivots_.size()].size() > N) {
            while (buckets_[pivots_.size()].size() > N) {
                partition();
            }
            sort_descending(buckets_[pivots_.size()]);
        }

        auto& layer = buckets_[pivots_.size()];
        assert(!layer.empty());

        out = layer.pop();

        if (layer.empty() && !pivots_.empty()) {
            pivots_.pop();

            if (buckets_[pivots_.size()].size() <= N) {
                sort_descending(buckets_[pivots_.size()]);
            }
        }

        --size_;

        return true;
    }
};

// Mirrors `KvSimdQuickHeap` from lib.rs: keys and values live in parallel layers.
template <class T, class Backend = DefaultBackend>
class KvSimdQuickHeap {
    static constexpr std::size_t N = 16;
    static constexpr std::size_t L = SimdElem<T, Backend>::L;

    Vec<T> pivots_ = Vec<T>::with_capacity(128);
    std::vector<Vec<T>> keys_ = std::vector<Vec<T>>(128);
    std::vector<Vec<T>> values_ = std::vector<Vec<T>>(128);
    std::size_t size_ = 0;

    [[gnu::noinline]] void partition() {
        const auto total_layers = pivots_.size() + 1;

        if (total_layers + 2 * L >= pivots_.capacity()) {
            pivots_.reserve(L);
        }

        if (total_layers == keys_.size()) {
            keys_.reserve(keys_.size() + 1);
            values_.reserve(values_.size() + 1);
            keys_.emplace_back();
            values_.emplace_back();
        }

        auto& cur_key_layer = keys_[pivots_.size()];
        auto& next_key_layer = keys_[pivots_.size() + 1];
        auto& cur_val_layer = values_[pivots_.size()];
        auto& next_val_layer = values_[pivots_.size() + 1];

        const auto [pivot, pivot_pos] =
            MedianOfM<3>::pick(cur_key_layer.data(), cur_key_layer.size());

        pivots_.push(pivot);

        next_key_layer.clear();
        next_val_layer.clear();

        // Rust aborts on allocation failure; C++ unwinds, so drop the pivot to keep the heap valid.
        try {
            VecPartitioning<T, Backend>::partition_kv(cur_key_layer, cur_val_layer, next_key_layer,
                                                      next_val_layer, pivot, pivot_pos);
        } catch (...) {
            pivots_.pop();
            throw;
        }

        if (next_key_layer.empty()) {
            pivots_.pop();
        }

        if (cur_key_layer.empty()) {
            cur_key_layer.swap(next_key_layer);
            cur_val_layer.swap(next_val_layer);
            pivots_.pop();
        }
    }

    // Sorts by decreasing key through a heap-allocated index permutation, as Rust does.
    // A failed allocation falls back to a stack buffer so the terminal layer still ends
    // sorted; callers only sort layers of at most N elements.
    static void sort_key_and_values(Vec<T>& keys, Vec<T>& vals) noexcept {
        const auto n = keys.size();
        if (n == 0) {
            return;
        }
        assert(n <= N);
        std::size_t stack_idx[N];
        auto* heap_idx = static_cast<std::size_t*>(::operator new(n * sizeof(std::size_t), std::nothrow));
        auto* idx = heap_idx != nullptr ? heap_idx : stack_idx;
        for (std::size_t i = 0; i < n; ++i) {
            idx[i] = i;
        }
        const T* key_data = keys.data();
        sort_unstable_small(idx, n, [key_data](std::size_t a, std::size_t b) {
            return key_data[a] > key_data[b];
        });

        for (std::size_t i = 0; i < n; ++i) {
            if (idx[i] == i) {
                continue;
            }
            const T tk = keys[i];
            const T tv = vals[i];
            auto cur = i;
            while (true) {
                const auto next = idx[cur];
                idx[cur] = cur;
                if (next == i) {
                    keys[cur] = tk;
                    vals[cur] = tv;
                    break;
                }
                keys[cur] = keys[next];
                vals[cur] = vals[next];
                cur = next;
            }
        }
        ::operator delete(heap_idx);
    }

public:
    std::size_t capacity() const noexcept {
        std::size_t total = 0;
        for (const auto& bucket : keys_) {
            total += bucket.capacity();
        }
        for (const auto& bucket : values_) {
            total += bucket.capacity();
        }
        return total;
    }

    std::size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }

    void push(T key, T value) {
        const auto target_layer = push_position<T, Backend>(pivots_, key);

        auto& key_layer = keys_[target_layer];
        auto& value_layer = values_[target_layer];

        // Rust aborts on allocation failure; growing both layers first keeps them paired if
        // either allocation throws.
        key_layer.reserve(1);
        value_layer.reserve(1);

        if (target_layer == pivots_.size() && key_layer.size() < N) {
            const auto pos =
                partition_point(key_layer.data(), key_layer.size(), [key](T x) { return x > key; });
            key_layer.insert(pos, key);
            value_layer.insert(pos, value);
        } else {
            key_layer.push(key);
            value_layer.push(value);
        }

        ++size_;
    }

    bool pop_into(T& key, T& value) {
        if (size_ == 0) {
            return false;
        }

        if (keys_[pivots_.size()].size() > N) {
            while (keys_[pivots_.size()].size() > N) {
                partition();
            }
            sort_key_and_values(keys_[pivots_.size()], values_[pivots_.size()]);
        }

        auto& key_layer = keys_[pivots_.size()];
        auto& val_layer = values_[pivots_.size()];
        assert(!key_layer.empty());

        key = key_layer.pop();
        value = val_layer.pop();

        if (key_layer.empty() && !pivots_.empty()) {
            assert(val_layer.empty());
            pivots_.pop();

            if (keys_[pivots_.size()].size() <= N) {
                sort_key_and_values(keys_[pivots_.size()], values_[pivots_.size()]);
            }
        }

        --size_;

        return true;
    }
};

}
