#pragma once

#include "simd.hpp"
#include "vec.hpp"

#include <cassert>
#include <cstddef>

namespace quickheap::detail {

constexpr std::size_t next_multiple_of(std::size_t n, std::size_t m) noexcept {
    return (n + m - 1) / m * m;
}

constexpr std::size_t saturating_sub(std::size_t a, std::size_t b) noexcept {
    return a > b ? a - b : 0;
}

// Mirrors `partitioning::VecPartitioning`: splits `cur` around `pivot` in place, moving
// the small side to the empty `next`. Full-register stores land in L spare slots.
template <class T, class Backend>
struct VecPartitioning {
    using S = SimdElem<T, Backend>;
    static constexpr std::size_t L = S::L;

    static void partition(Vec<T>& cur_layer, Vec<T>& next_layer, T pivot, std::size_t pivot_pos) {
        assert(next_layer.empty());
        const auto n = cur_layer.size();

        cur_layer.reserve(L);
        next_layer.reserve(n + L);

        cur_layer.set_len(n + L);
        next_layer.set_len(n + L);

        const auto n2 = saturating_sub(next_multiple_of(n, L), L);

        std::size_t cur_len = 0;
        std::size_t next_len = 0;
        const auto threshold = S::splat(pivot);

        T* const cur_ptr = cur_layer.data();
        T* const next_ptr = next_layer.data();

        for (std::size_t i = 0; i < n2; i += L) {
            S::partition_fast(S::load(cur_ptr + i), threshold, pivot_pos,
                              cur_ptr, cur_len, next_ptr, next_len);
        }

        if (n2 < n) {
            S::partition_slow(S::load(cur_ptr + n2), n - n2, threshold, pivot_pos,
                              cur_ptr, cur_len, next_ptr, next_len);
        }

        cur_layer.set_len(cur_len);
        next_layer.set_len(next_len);
    }

    static void partition_kv(Vec<T>& cur_keys, Vec<T>& cur_vals, Vec<T>& next_keys,
                             Vec<T>& next_vals, T pivot, std::size_t pivot_pos) {
        assert(cur_keys.size() == cur_vals.size());
        assert(next_keys.empty() && next_vals.empty());
        const auto n = cur_keys.size();

        cur_keys.reserve(L);
        cur_vals.reserve(L);
        next_keys.reserve(n + L);
        next_vals.reserve(n + L);

        cur_keys.set_len(n + L);
        cur_vals.set_len(n + L);
        next_keys.set_len(n + L);
        next_vals.set_len(n + L);

        const auto n2 = saturating_sub(next_multiple_of(n, L), L);

        std::size_t cur_len = 0;
        std::size_t next_len = 0;
        const auto threshold = S::splat(pivot);

        T* const cur_key_ptr = cur_keys.data();
        T* const next_key_ptr = next_keys.data();
        T* const cur_val_ptr = cur_vals.data();
        T* const next_val_ptr = next_vals.data();

        for (std::size_t i = 0; i < n2; i += L) {
            S::partition_fast_kv(S::load(cur_key_ptr + i), S::load(cur_val_ptr + i), threshold,
                                 pivot_pos, cur_key_ptr, cur_val_ptr, cur_len,
                                 next_key_ptr, next_val_ptr, next_len);
        }

        if (n2 < n) {
            S::partition_slow_kv(S::load(cur_key_ptr + n2), S::load(cur_val_ptr + n2), n - n2,
                                 threshold, pivot_pos, cur_key_ptr, cur_val_ptr, cur_len,
                                 next_key_ptr, next_val_ptr, next_len);
        }

        cur_keys.set_len(cur_len);
        cur_vals.set_len(cur_len);
        next_keys.set_len(next_len);
        next_vals.set_len(next_len);
    }
};

}
