#pragma once

#include "partitioning.hpp"
#include "pivots.hpp"
#include "simd.hpp"
#include "sort.hpp"
#include "vec.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <functional>
#include <new>
#include <span>
#include <utility>
#include <vector>

namespace quickheap::detail {

inline constexpr unsigned opt_terminal_shortcut = 1;
inline constexpr unsigned opt_cold_terminal = 2;
inline constexpr unsigned opt_one_count = 4;
inline constexpr unsigned opt_scan_early_exit = 8;
inline constexpr unsigned opt_sort_network = 16;
inline constexpr unsigned opt_sample_growth = 64;
inline constexpr unsigned opt_terminal_32 = 128;
inline constexpr unsigned opt_rebalance = 256;
inline constexpr unsigned opt_four_way = 512;

// Mirrors `SimdQuickHeap` from the Rust crate's lib.rs. `pivots` decrease; `buckets[i]`
// holds elements between pivots i-1 and i. The terminal layer `buckets[pivots.size()]`
// stays sorted in decreasing order whenever it holds at most N elements.
template <class T, class Backend = DefaultBackend, unsigned Opts = 0>
class SimdQuickHeap {
    static constexpr std::size_t N = (Opts & opt_terminal_32) != 0 ? 32 : 16;
    static constexpr std::size_t L = SimdElem<T, Backend>::L;

    Vec<T> pivots_ = Vec<T>::with_capacity(128);
    std::vector<Vec<T>> buckets_ = std::vector<Vec<T>>(128);
    std::size_t size_ = 0;
    std::size_t pops_since_rebalance_ = 0;

    static void sort_descending(Vec<T>& layer) noexcept {
        const auto greater = [](T a, T b) { return a > b; };
        if constexpr ((Opts & opt_sort_network) != 0) {
            if (layer.size() <= 16) {
                sort_terminal_descending(layer.data(), layer.size());
                return;
            }
        }
        sort_unstable_small(layer.data(), layer.size(), greater);
    }

    std::size_t push_target(T t) const noexcept {
        if constexpr ((Opts & opt_terminal_shortcut) != 0) {
            if (pivots_.empty() || t < pivots_[pivots_.size() - 1]) {
                return pivots_.size();
            }
        }
        return push_position<T, Backend, (Opts & opt_scan_early_exit) != 0>(pivots_, t);
    }

    static std::pair<T, std::size_t> pick_pivot(const T* layer, std::size_t n) noexcept {
        if constexpr ((Opts & opt_sample_growth) != 0) {
            if (n >= 16384) {
                return MedianOfM<15>::pick(layer, n);
            }
            if (n >= 1024) {
                return MedianOfM<7>::pick(layer, n);
            }
        }
        return MedianOfM<3>::pick(layer, n);
    }

    bool try_partition4() {
        const auto p = pivots_.size();
        const auto n = buckets_[p].size();

        std::array<std::pair<T, std::size_t>, 7> samples;
        for (auto& sample : samples) {
            const auto pos = random_range(n);
            sample = {buckets_[p][pos], pos};
        }
        insertion_sort_shift_left(samples.data(), samples.size(), 1, std::less<>{});
        const std::array bounds = {samples[3], samples[1], samples[0]};

        pivots_.reserve(4 + 2 * L);
        while (buckets_.size() < p + 4) {
            buckets_.emplace_back();
        }
        for (std::size_t k = 1; k < 4; ++k) {
            buckets_[p + k].clear();
        }

        const std::span<Vec<T>, 3> outs(&buckets_[p + 1], 3);
        VecPartitioning<T, Backend>::partition4(buckets_[p], outs, bounds);

        std::size_t populated = 0;
        for (std::size_t k = 0; k < 4; ++k) {
            populated += !buckets_[p + k].empty();
        }
        if (populated < 2) {
            for (std::size_t k = 1; k < 4; ++k) {
                if (!buckets_[p + k].empty()) {
                    buckets_[p].swap(buckets_[p + k]);
                }
            }
            return false;
        }

        std::size_t placed = 0;
        for (std::size_t k = 0; k < 4; ++k) {
            if (buckets_[p + k].empty()) {
                continue;
            }
            if (placed != 0) {
                pivots_.push(bounds[k - 1].first);
            }
            buckets_[p + placed].swap(buckets_[p + k]);
            ++placed;
        }
        return true;
    }

    [[gnu::noinline]] void partition() {
        if constexpr ((Opts & opt_four_way) != 0) {
            if (buckets_[pivots_.size()].size() > 128 && try_partition4()) {
                return;
            }
        }

        const auto total_layers = pivots_.size() + 1;

        if (total_layers + 2 * L >= pivots_.capacity()) {
            pivots_.reserve(L);
        }

        if (total_layers == buckets_.size()) {
            buckets_.emplace_back();
        }

        auto& cur_layer = buckets_[pivots_.size()];
        auto& next_layer = buckets_[pivots_.size() + 1];

        const auto [pivot, pivot_pos] = pick_pivot(cur_layer.data(), cur_layer.size());

        pivots_.push(pivot);

        next_layer.clear();

        // Rust aborts on allocation failure; C++ unwinds, so drop the pivot to keep the heap valid.
        try {
            VecPartitioning<T, Backend>::template partition<(Opts & opt_one_count) != 0>(
                cur_layer, next_layer, pivot, pivot_pos);
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

    [[gnu::noinline]] void refill_terminal() {
        while (buckets_[pivots_.size()].size() > N) {
            partition();
        }
        sort_descending(buckets_[pivots_.size()]);
    }

    void merge_layers(std::size_t upper) {
        auto* dst = &buckets_[upper];
        auto* src = &buckets_[upper + 1];
        if (dst->size() < src->size()) {
            std::swap(dst, src);
        }
        dst->extend_from_slice(src->data(), src->size());
        src->clear();
        buckets_[upper].swap(*dst);
        std::rotate(buckets_.begin() + static_cast<std::ptrdiff_t>(upper) + 1,
                    buckets_.begin() + static_cast<std::ptrdiff_t>(upper) + 2, buckets_.end());
        pivots_.remove(upper);
    }

    [[gnu::noinline]] void rebalance() {
        std::size_t total = 0;
        for (auto layer = pivots_.size(); layer != 0; --layer) {
            if (buckets_[layer].size() + buckets_[layer - 1].size() < 2 * total) {
                merge_layers(layer - 1);
            } else {
                total += buckets_[layer].size();
            }
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
        const auto target_layer = push_target(t);

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

        if constexpr ((Opts & opt_rebalance) != 0) {
            if (++pops_since_rebalance_ == 2048) {
                pops_since_rebalance_ = 0;
                rebalance();
            }
        }

        if (buckets_[pivots_.size()].size() > N) {
            if constexpr ((Opts & opt_cold_terminal) != 0) {
                refill_terminal();
            } else {
                while (buckets_[pivots_.size()].size() > N) {
                    partition();
                }
                sort_descending(buckets_[pivots_.size()]);
            }
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
