#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <limits>

namespace quickheap::detail {

// Mirrors core::slice::sort::shared::smallsort::insertion_sort_shift_left.
template <class T, class Less>
void insertion_sort_shift_left(T* v, std::size_t len, std::size_t offset, Less is_less) noexcept {
    assert(offset != 0 && offset <= len);
    for (auto* tail = v + offset; tail != v + len; ++tail) {
        auto* sift = tail - 1;
        if (!is_less(*tail, *sift)) {
            continue;
        }
        const T tmp = *tail;
        auto* gap = tail;
        while (true) {
            *gap = *sift;
            gap = sift;
            if (sift == v) {
                break;
            }
            --sift;
            if (!is_less(tmp, *sift)) {
                break;
            }
        }
        *gap = tmp;
    }
}

// Mirrors `slice::sort_unstable_by` for the lengths quickheap sorts (at most 20, where std
// always runs insertion sort).
template <class T, class Less>
void sort_unstable_small(T* v, std::size_t len, Less is_less) noexcept {
    assert(len <= 20);
    if (len < 2) {
        return;
    }
    insertion_sort_shift_left(v, len, 1, is_less);
}

// Mirrors `slice::partition_point`, built on std's branchless `binary_search_by`.
template <class T, class Pred>
std::size_t partition_point(const T* v, std::size_t len, Pred pred) noexcept {
    if (len == 0) {
        return 0;
    }
    std::size_t size = len;
    std::size_t base = 0;
    while (size > 1) {
        const auto half = size / 2;
        const auto mid = base + half;
        base = pred(v[mid]) ? mid : base;
        size -= half;
    }
    return base + static_cast<std::size_t>(pred(v[base]));
}


// Mirrors `sort_terminal_descending` from the Rust crate (commit 9f0428a): above 8 keys, a
// 16-wide bitonic network padded with MAX sorts without data-dependent branches.
template <class T>
[[gnu::noinline]] void sort_terminal_descending(T* layer, std::size_t n) noexcept {
    constexpr std::size_t W = 16;
    assert(n <= W);
    if (n <= 8) {
        for (std::size_t i = 1; i < n; ++i) {
            const T t = layer[i];
            std::size_t pos = i;
            while (pos != 0 && layer[pos - 1] < t) {
                layer[pos] = layer[pos - 1];
                --pos;
            }
            layer[pos] = t;
        }
        return;
    }
    std::array<T, W> v;
    v.fill(std::numeric_limits<T>::max());
    std::copy_n(layer, n, v.data() + (W - n));
    for (std::size_t width = 2; width <= W; width *= 2) {
        for (std::size_t distance = width / 2; distance != 0; distance /= 2) {
            for (std::size_t i = 0; i < W; ++i) {
                const auto j = i ^ distance;
                if (j > i) {
                    const T hi = std::max(v[i], v[j]);
                    const T lo = std::min(v[i], v[j]);
                    const bool up = (i & width) == 0;
                    v[i] = up ? hi : lo;
                    v[j] = up ? lo : hi;
                }
            }
        }
    }
    std::copy_n(v.data() + (W - n), n, layer);
}

}
