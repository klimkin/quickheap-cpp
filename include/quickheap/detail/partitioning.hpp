#pragma once

#include "simd.hpp"
#include "vec.hpp"

#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <span>
#include <utility>

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

    template <bool OneCount = false>
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
            S::template partition_fast<OneCount>(S::load(cur_ptr + i), threshold, pivot_pos,
                                                 cur_ptr, cur_len, next_ptr, next_len);
        }

        if (n2 < n) {
            S::template partition_slow<OneCount>(S::load(cur_ptr + n2), n - n2, threshold,
                                                 pivot_pos, cur_ptr, cur_len, next_ptr, next_len);
        }

        cur_layer.set_len(cur_len);
        next_layer.set_len(next_len);
    }

    static void partition4(Vec<T>& cur_layer, std::span<Vec<T>, 3> outs,
                           const std::array<std::pair<T, std::size_t>, 3>& bounds) {
        const auto n = cur_layer.size();

        cur_layer.reserve(L);
        for (std::size_t k = 0; k < 3; ++k) {
            assert(outs[k].empty());
            outs[k].reserve(n + L);
        }

        const auto n2 = saturating_sub(next_multiple_of(n, L), L);
        const std::array thresholds = {S::splat(bounds[0].first), S::splat(bounds[1].first),
                                       S::splat(bounds[2].first)};

        T* const cur_ptr = cur_layer.data();
        const std::array out_ptrs = {outs[0].data(), outs[1].data(), outs[2].data()};
        std::size_t cur_len = 0;
        std::array<std::size_t, 3> out_lens{};

        const auto split = [&](typename S::Simd vals, unsigned live) {
            const auto m0 = S::small_mask(vals, thresholds[0], bounds[0].second) & live;
            const auto m1 = S::small_mask(vals, thresholds[1], bounds[1].second) & live;
            const auto m2 = S::small_mask(vals, thresholds[2], bounds[2].second) & live;
            const auto any = m0 | m1 | m2;
            const auto odd = m0 ^ m1 ^ m2;
            const auto all_three = m0 & m1 & m2;
            const auto none = live & ~any;
            const std::array<unsigned, 3> masks = {odd & ~all_three, any & ~odd, all_three};
            S::compress_store(cur_ptr + cur_len, vals, none ^ S::full);
            cur_len += static_cast<std::size_t>(std::popcount(none));
            for (std::size_t k = 0; k < 3; ++k) {
                S::compress_store(out_ptrs[k] + out_lens[k], vals, masks[k] ^ S::full);
                out_lens[k] += static_cast<std::size_t>(std::popcount(masks[k]));
            }
        };

        for (std::size_t i = 0; i < n2; i += L) {
            split(S::load(cur_ptr + i), S::full);
        }

        if (n2 < n) {
            split(S::load(cur_ptr + n2), S::in_range(n - n2));
        }

        cur_layer.set_len(cur_len);
        for (std::size_t k = 0; k < 3; ++k) {
            outs[k].set_len(out_lens[k]);
        }
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
