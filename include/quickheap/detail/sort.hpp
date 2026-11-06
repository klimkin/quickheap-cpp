#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <limits>
#include <utility>

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

template <class T, class Less>
void swap_if_less(T* v, std::size_t a, std::size_t b, Less is_less) noexcept {
    const bool should_swap = is_less(v[b], v[a]);
    const T va = v[a];
    const T vb = v[b];
    v[a] = should_swap ? vb : va;
    v[b] = should_swap ? va : vb;
}

using SwapPair = std::array<unsigned char, 2>;

inline constexpr SwapPair sort9_pairs[] = {
    {0, 3}, {1, 7}, {2, 5}, {4, 8}, {0, 7}, {2, 4}, {3, 8}, {5, 6}, {0, 2}, {1, 3}, {4, 5}, {7, 8}, {1, 4},
    {3, 6}, {5, 7}, {0, 1}, {2, 4}, {3, 5}, {6, 8}, {2, 3}, {4, 5}, {6, 7}, {1, 2}, {3, 4}, {5, 6}};

inline constexpr SwapPair sort13_pairs[] = {
    {0, 12}, {1, 10}, {2, 9},  {3, 7},  {5, 11}, {6, 8},  {1, 6},  {2, 3},   {4, 11}, {7, 9},  {8, 10}, {0, 4},
    {1, 2},  {3, 6},  {7, 8},  {9, 10}, {11, 12}, {4, 6}, {5, 9},  {8, 11},  {10, 12}, {0, 5}, {3, 8},  {4, 7},
    {6, 11}, {9, 10}, {0, 1},  {2, 5},  {6, 9},  {7, 8},  {10, 11}, {1, 3},  {2, 4},  {5, 6},  {9, 10}, {1, 2},
    {3, 4},  {5, 7},  {6, 8},  {2, 3},  {4, 5},  {6, 7},  {8, 9},  {3, 4},   {5, 6}};

template <const auto& Pairs, class T, class Less>
void sort_network(T* v, Less is_less) noexcept {
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        (swap_if_less(v, Pairs[I][0], Pairs[I][1], is_less), ...);
    }(std::make_index_sequence<std::size(Pairs)>{});
}

// Mirrors core::slice::sort::shared::smallsort::bidirectional_merge.
template <class T, class Less>
void bidirectional_merge(const T* src, std::size_t len, T* dst, Less is_less) noexcept {
    const auto half = len / 2;
    const T* left = src;
    const T* right = src + half;
    const T* left_rev = src + half - 1;
    const T* right_rev = src + len - 1;
    T* dst_rev = dst + len - 1;
    for (std::size_t i = 0; i < half; ++i) {
        const bool up_left = !is_less(*right, *left);
        *dst++ = up_left ? *left : *right;
        left += up_left;
        right += !up_left;

        const bool down_right = !is_less(*right_rev, *left_rev);
        *dst_rev-- = down_right ? *right_rev : *left_rev;
        right_rev -= down_right;
        left_rev -= !down_right;
    }
    if (len % 2 != 0) {
        const bool left_nonempty = left <= left_rev;
        *dst = left_nonempty ? *left : *right;
    }
}

// Mirrors core::slice::sort::shared::smallsort::small_sort_network.
template <class T, class Less>
void small_sort_network(T* v, std::size_t len, Less is_less) noexcept {
    const auto half = len / 2;
    const bool no_merge = len < 18;
    const auto sort_region = [&](T* region, std::size_t n) {
        std::size_t presorted = 1;
        if (n >= 13) {
            sort_network<sort13_pairs>(region, is_less);
            presorted = 13;
        } else if (n >= 9) {
            sort_network<sort9_pairs>(region, is_less);
            presorted = 9;
        }
        insertion_sort_shift_left(region, n, presorted, is_less);
    };
    if (no_merge) {
        sort_region(v, len);
        return;
    }
    sort_region(v, half);
    sort_region(v + half, len - half);
    std::array<T, 32> scratch;
    bidirectional_merge(v, len, scratch.data(), is_less);
    std::copy_n(scratch.data(), len, v);
}

// Mirrors `slice::sort_unstable_by` for Copy keys of at most 8 bytes and the lengths quickheap
// sorts: insertion sort up to 20, then ipnsort's run detection and the 32-key network sort.
template <class T, class Less>
void sort_unstable_small(T* v, std::size_t len, Less is_less) noexcept {
    assert(len <= 32);
    if (len < 2) {
        return;
    }
    if (len <= 20) {
        insertion_sort_shift_left(v, len, 1, is_less);
        return;
    }
    std::size_t run = 2;
    const bool strictly_descending = is_less(v[1], v[0]);
    if (strictly_descending) {
        while (run < len && is_less(v[run], v[run - 1])) {
            ++run;
        }
    } else {
        while (run < len && !is_less(v[run], v[run - 1])) {
            ++run;
        }
    }
    if (run == len) {
        if (strictly_descending) {
            std::reverse(v, v + len);
        }
        return;
    }
    small_sort_network(v, len, is_less);
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
