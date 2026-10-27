#pragma once

#include "sort.hpp"
#include "vec.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#if defined(__AVX2__) && !defined(QUICKHEAP_FORCE_SCALAR)
#define QUICKHEAP_AVX2_BACKEND
#include <immintrin.h>
#endif

namespace quickheap::detail {

// Mirrors quickheap/src/simd.rs from the Rust crate (AVX2 backend), with a scalar
// backend of identical lane semantics for builds without AVX2.

constexpr std::uint64_t mix(std::uint64_t x) noexcept {
    x += UINT64_C(0x9E3779B97F4A7C15);
    x = (x ^ (x >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94D049BB133111EB);
    return x ^ (x >> 31);
}

// Mask with one selected lane in every adjacent pair.
constexpr std::uint64_t balanced(std::uint64_t x) noexcept {
    const auto even = x & UINT64_C(0x5555555555555555);
    return even | ((~even & UINT64_C(0x5555555555555555)) << 1);
}

inline constexpr auto equal_masks = [] {
    std::array<std::uint64_t, 16> table{};
    std::uint64_t state = UINT64_C(0x123456789ABCDEF0);
    for (auto& mask : table) {
        state = mix(state);
        mask = balanced(state);
    }
    return table;
}();

// Row `drop` packs the lanes whose bit is clear in `drop` to the front, as 32-bit
// permute indices (two per 64-bit lane).
template <std::size_t Lanes>
constexpr auto make_uniqshuf() {
    constexpr std::size_t words_per_lane = 8 / Lanes;
    std::array<std::array<std::int32_t, 8>, std::size_t{1} << Lanes> table{};
    for (std::size_t drop = 0; drop < table.size(); ++drop) {
        std::size_t out = 0;
        for (std::size_t lane = 0; lane < Lanes; ++lane) {
            if (((drop >> lane) & 1) == 0) {
                for (std::size_t word = 0; word < words_per_lane; ++word) {
                    table[drop][out++] = static_cast<std::int32_t>(lane * words_per_lane + word);
                }
            }
        }
    }
    return table;
}

alignas(32) inline constexpr auto uniqshuf32 = make_uniqshuf<8>();
alignas(32) inline constexpr auto uniqshuf64 = make_uniqshuf<4>();

struct Avx2;
struct Scalar;

template <class T, class Backend>
struct Lanes;

#if defined(QUICKHEAP_AVX2_BACKEND)

template <class T>
struct Lanes<T, Avx2> {
    static_assert(std::is_integral_v<T> && (sizeof(T) == 4 || sizeof(T) == 8));
    static constexpr std::size_t L = 32 / sizeof(T);
    static constexpr unsigned full = (1u << L) - 1;
    using Simd = __m256i;

    static Simd splat(T value) noexcept {
        if constexpr (sizeof(T) == 4) {
            return _mm256_set1_epi32(static_cast<std::int32_t>(value));
        } else {
            return _mm256_set1_epi64x(static_cast<std::int64_t>(value));
        }
    }

    static Simd load(const T* ptr) noexcept {
        return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr));
    }

    static Simd to_signed_order(Simd value) noexcept {
        if constexpr (std::is_unsigned_v<T>) {
            return _mm256_xor_si256(value, splat(static_cast<T>(T{1} << (sizeof(T) * 8 - 1))));
        } else {
            return value;
        }
    }

    static unsigned movemask(Simd mask) noexcept {
        if constexpr (sizeof(T) == 4) {
            return static_cast<unsigned>(_mm256_movemask_ps(_mm256_castsi256_ps(mask)));
        } else {
            return static_cast<unsigned>(_mm256_movemask_pd(_mm256_castsi256_pd(mask)));
        }
    }

    static Simd signed_gt(Simd a, Simd b) noexcept {
        if constexpr (sizeof(T) == 4) {
            return _mm256_cmpgt_epi32(a, b);
        } else {
            return _mm256_cmpgt_epi64(a, b);
        }
    }

    // Bit i = a[i] < b[i].
    static unsigned lt_bitmask(Simd a, Simd b) noexcept {
        return movemask(signed_gt(to_signed_order(b), to_signed_order(a)));
    }

    static unsigned eq_bitmask(Simd a, Simd b) noexcept {
        if constexpr (sizeof(T) == 4) {
            return movemask(_mm256_cmpeq_epi32(a, b));
        } else {
            return movemask(_mm256_cmpeq_epi64(a, b));
        }
    }

    // Bit i = len > i, computed as a vector compare against the lane indices.
    static unsigned in_range(std::size_t len) noexcept {
        if constexpr (sizeof(T) == 4) {
            return movemask(_mm256_cmpgt_epi32(_mm256_set1_epi32(static_cast<std::int32_t>(len)),
                                               _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7)));
        } else {
            return movemask(_mm256_cmpgt_epi64(_mm256_set1_epi64x(static_cast<std::int64_t>(len)),
                                               _mm256_setr_epi64x(0, 1, 2, 3)));
        }
    }

    // Full-register store of the lanes whose bit is clear in `drop`, packed to the front.
    static void compress_store(T* dst, Simd value, unsigned drop) noexcept {
        const auto* row = sizeof(T) == 4 ? uniqshuf32[drop].data() : uniqshuf64[drop].data();
        const auto shuffle = _mm256_load_si256(reinterpret_cast<const __m256i*>(row));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst),
                            _mm256_permutevar8x32_epi32(value, shuffle));
    }
};

using DefaultBackend = Avx2;

#else

using DefaultBackend = Scalar;

#endif

template <class T>
struct Lanes<T, Scalar> {
    static_assert(std::is_integral_v<T> && (sizeof(T) == 4 || sizeof(T) == 8));
    static constexpr std::size_t L = 32 / sizeof(T);
    static constexpr unsigned full = (1u << L) - 1;
    using Simd = std::array<T, L>;

    static Simd splat(T value) noexcept {
        Simd result;
        result.fill(value);
        return result;
    }

    static Simd load(const T* ptr) noexcept {
        Simd result;
        std::copy_n(ptr, L, result.begin());
        return result;
    }

    static unsigned lt_bitmask(const Simd& a, const Simd& b) noexcept {
        unsigned mask = 0;
        for (std::size_t lane = 0; lane < L; ++lane) {
            mask |= static_cast<unsigned>(a[lane] < b[lane]) << lane;
        }
        return mask;
    }

    static unsigned eq_bitmask(const Simd& a, const Simd& b) noexcept {
        unsigned mask = 0;
        for (std::size_t lane = 0; lane < L; ++lane) {
            mask |= static_cast<unsigned>(a[lane] == b[lane]) << lane;
        }
        return mask;
    }

    static unsigned in_range(std::size_t len) noexcept {
        return len >= L ? full : (1u << len) - 1;
    }

    // Writes all L slots like the vector store, so callers see the same spare-capacity use.
    static void compress_store(T* dst, const Simd& value, unsigned drop) noexcept {
        std::size_t out = 0;
        for (std::size_t lane = 0; lane < L; ++lane) {
            if (((drop >> lane) & 1) == 0) {
                dst[out++] = value[lane];
            }
        }
        for (; out < L; ++out) {
            dst[out] = value[0];
        }
    }
};

// Mirrors the `SimdElem` partition kernels in SPLIT_PIVOTS_CUR_AND_NEXT mode: lanes below
// the pivot, plus pivot-equal lanes selected by `EQUAL_MASKS[pivot_pos % 16]`, go to
// `next`; the rest stay in `cur`.
template <class T, class Backend>
struct SimdElem : Lanes<T, Backend> {
    using V = Lanes<T, Backend>;
    using Simd = typename V::Simd;
    using V::full;
    using V::L;

    static unsigned small_mask(Simd keys, Simd threshold, std::size_t pivot_pos) noexcept {
        const auto lt = V::lt_bitmask(keys, threshold);
        const auto eq = V::eq_bitmask(keys, threshold);
        return (lt | (eq & static_cast<unsigned>(equal_masks[pivot_pos % 16]))) & full;
    }

    static void partition_fast(Simd vals, Simd threshold, std::size_t pivot_pos,
                               T* cur_write, std::size_t& cur_write_cnt,
                               T* next_write, std::size_t& next_write_cnt) noexcept {
        const auto small = small_mask(vals, threshold, pivot_pos);
        const auto large = small ^ full;
        V::compress_store(cur_write + cur_write_cnt, vals, small);
        cur_write_cnt += static_cast<std::size_t>(std::popcount(large));
        V::compress_store(next_write + next_write_cnt, vals, large);
        next_write_cnt += static_cast<std::size_t>(std::popcount(small));
    }

    static void partition_slow(Simd vals, std::size_t len, Simd threshold, std::size_t pivot_pos,
                               T* cur_write, std::size_t& cur_write_cnt,
                               T* next_write, std::size_t& next_write_cnt) noexcept {
        const auto in_range = V::in_range(len);
        const auto small = small_mask(vals, threshold, pivot_pos) & in_range;
        const auto large = (small ^ full) & in_range;
        V::compress_store(cur_write + cur_write_cnt, vals, large ^ full);
        cur_write_cnt += static_cast<std::size_t>(std::popcount(large));
        V::compress_store(next_write + next_write_cnt, vals, small ^ full);
        next_write_cnt += static_cast<std::size_t>(std::popcount(small));
    }

    static void partition_fast_kv(Simd keys, Simd vals, Simd threshold, std::size_t pivot_pos,
                                  T* cur_keys, T* cur_vals, std::size_t& cur_write_cnt,
                                  T* next_keys, T* next_vals, std::size_t& next_write_cnt) noexcept {
        const auto small = small_mask(keys, threshold, pivot_pos);
        const auto large = small ^ full;
        V::compress_store(cur_keys + cur_write_cnt, keys, small);
        V::compress_store(cur_vals + cur_write_cnt, vals, small);
        cur_write_cnt += static_cast<std::size_t>(std::popcount(large));
        V::compress_store(next_keys + next_write_cnt, keys, large);
        V::compress_store(next_vals + next_write_cnt, vals, large);
        next_write_cnt += static_cast<std::size_t>(std::popcount(small));
    }

    static void partition_slow_kv(Simd keys, Simd vals, std::size_t len, Simd threshold,
                                  std::size_t pivot_pos, T* cur_keys, T* cur_vals,
                                  std::size_t& cur_write_cnt, T* next_keys, T* next_vals,
                                  std::size_t& next_write_cnt) noexcept {
        const auto in_range = V::in_range(len);
        const auto small = small_mask(keys, threshold, pivot_pos) & in_range;
        const auto large = (small ^ full) & in_range;
        V::compress_store(cur_keys + cur_write_cnt, keys, large ^ full);
        V::compress_store(cur_vals + cur_write_cnt, vals, large ^ full);
        cur_write_cnt += static_cast<std::size_t>(std::popcount(large));
        V::compress_store(next_keys + next_write_cnt, keys, small ^ full);
        V::compress_store(next_vals + next_write_cnt, vals, small ^ full);
        next_write_cnt += static_cast<std::size_t>(std::popcount(small));
    }
};

// `partition_point(|p| t < p)` over the decreasing pivots. Up to 64 pivots it uses
// full-register loads that may read past `len`; the heap keeps 2 * L spare slots so those
// reads stay inside the allocation.
template <class T, class Backend>
std::size_t push_position(const Vec<T>& pivots, T t) noexcept {
    using S = SimdElem<T, Backend>;
    const auto len = pivots.size();
    if (len <= 64) {
        const auto t_simd = S::splat(t);
        std::size_t target_layer = 0;
        for (std::size_t i = 0; i < len; i += S::L) {
            const auto vals = S::load(pivots.data() + i);
            target_layer += static_cast<std::size_t>(std::countr_one(S::lt_bitmask(t_simd, vals)));
        }
        return std::min(target_layer, len);
    }
    return partition_point(pivots.data(), len, [t](T p) { return t < p; });
}

}
