#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>

namespace quickheap::bench {

// Quicksort for integral keys. Lomuto partitioning turns the comparison into an
// arithmetic update, so random data costs no branch misses.
namespace branchless_sort_detail {

inline constexpr std::size_t insertion_limit = 20;

template <class T>
void insertion_sort(T* v, std::size_t n) {
    for (std::size_t i = 1; i < n; ++i) {
        const T key = v[i];
        auto j = i;
        for (; j != 0 && key < v[j - 1]; --j) v[j] = v[j - 1];
        v[j] = key;
    }
}

template <class T>
T median3(T a, T b, T c) {
    const auto lo = std::min(a, b);
    const auto hi = std::max(a, b);
    return std::max(lo, std::min(hi, c));
}

template <class T>
std::size_t choose_pivot(const T* v, std::size_t n) {
    const auto step = n / 8;
    const auto a = step, b = step * 4, c = step * 7;
    if (n < 64) {
        const auto lo = v[a] < v[b] ? a : b;
        const auto hi = v[a] < v[b] ? b : a;
        return v[hi] < v[c] ? hi : (v[lo] < v[c] ? c : lo);
    }
    // Ninther: median of three medians, found by value then located by index.
    const auto value = median3(median3(v[a - 1], v[a], v[a + 1]),
                               median3(v[b - 1], v[b], v[b + 1]),
                               median3(v[c - 1], v[c], v[c + 1]));
    for (auto i : {a - 1, a, a + 1, b - 1, b, b + 1, c - 1, c, c + 1})
        if (v[i] == value) return i;
    return b;
}

// Moves keys before the pivot (< or <=) to the front and returns the pivot's final index.
template <bool OrEqual, class T>
std::size_t partition(T* v, std::size_t n, std::size_t pivot_index) {
    std::swap(v[0], v[pivot_index]);
    const T pivot = v[0];
    std::size_t lt = 0;
    for (std::size_t i = 1; i < n; ++i) {
        const T x = v[i];
        v[i] = v[lt + 1];
        v[lt + 1] = x;
        lt += OrEqual ? x <= pivot : x < pivot;
    }
    std::swap(v[0], v[lt]);
    return lt;
}

template <class T>
void sort(T* v, std::size_t n, std::optional<T> ancestor, unsigned limit) {
    while (n > insertion_limit) {
        if (limit-- == 0) {
            std::make_heap(v, v + n);
            std::sort_heap(v, v + n);
            return;
        }
        const auto p = choose_pivot(v, n);
        if (ancestor && !(*ancestor < v[p])) {
            // Pivot equals the ancestor, so everything <= pivot is already in place.
            const auto mid = partition<true>(v, n, p);
            v += mid + 1;
            n -= mid + 1;
            ancestor.reset();
            continue;
        }
        const T pivot = v[p];
        const auto mid = partition<false>(v, n, p);
        sort(v, mid, ancestor, limit);
        v += mid + 1;
        n -= mid + 1;
        ancestor = pivot;
    }
    insertion_sort(v, n);
}

}  // namespace branchless_sort_detail

template <class T>
void branchless_sort(T* first, std::size_t n) {
    static_assert(std::is_integral_v<T>);
    branchless_sort_detail::sort<T>(first, n, std::nullopt, 2 * std::bit_width(n | 1));
}

}  // namespace quickheap::bench
