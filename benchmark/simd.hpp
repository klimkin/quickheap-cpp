#pragma once

#include <cstddef>
#include <cstdint>

namespace quickheap::bench {

struct SplitResult {
    std::size_t upper;
    std::size_t lower;
};

#define QUICKHEAP_BENCH_KERNELS(T)                                                   \
    std::size_t push_position(const T* pivots, std::size_t count, T key) noexcept; \
    SplitResult partition(T* keys, T* values, T* lower_keys, T* lower_values,    \
                          std::size_t count, T pivot, std::size_t salt) noexcept;

QUICKHEAP_BENCH_KERNELS(std::int32_t)
QUICKHEAP_BENCH_KERNELS(std::uint32_t)
QUICKHEAP_BENCH_KERNELS(std::int64_t)
QUICKHEAP_BENCH_KERNELS(std::uint64_t)

#undef QUICKHEAP_BENCH_KERNELS

}
