#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace quickheap::bench {

using MultiSplitCounts = std::array<std::size_t, 4>;

#define QUICKHEAP_EXPERIMENT_KERNEL(T) \
    MultiSplitCounts partition_chunk(const T* keys, std::size_t count, \
                                    const T* pivots, std::size_t partitions, \
                                    T* const* outputs, std::size_t salt) noexcept;

QUICKHEAP_EXPERIMENT_KERNEL(std::int32_t)
QUICKHEAP_EXPERIMENT_KERNEL(std::uint32_t)
QUICKHEAP_EXPERIMENT_KERNEL(std::int64_t)
QUICKHEAP_EXPERIMENT_KERNEL(std::uint64_t)

#undef QUICKHEAP_EXPERIMENT_KERNEL

}
