#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "experiment_simd.cpp"
#include "hwy/foreach_target.h"
#include "hwy/highway.h"

#include "experiment_simd.hpp"

HWY_BEFORE_NAMESPACE();
namespace quickheap::bench {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;

template <class T>
MultiSplitCounts Chunk(const T* keys, std::size_t count, const T* pivots,
                       std::size_t partitions, T* const* outputs, std::size_t salt) {
    const hn::ScalableTag<T> d;
    const auto lanes = hn::Lanes(d);
    const auto one = hn::Set(d, T{1});
    MultiSplitCounts counts{};
    std::size_t i = 0;
    for (; count - i >= lanes; i += lanes) {
        const auto keys_v = hn::LoadU(d, keys + i);
        const auto even = hn::Eq(hn::And(hn::Iota(d, static_cast<T>((i ^ salt) & 1)), one),
                                hn::Zero(d));
        auto remaining = hn::Eq(keys_v, keys_v);
        for (std::size_t bucket = 0; bucket + 1 < partitions; ++bucket) {
            const auto threshold = hn::Set(d, pivots[bucket]);
            const auto small = hn::Or(hn::Lt(keys_v, threshold),
                                     hn::And(hn::Eq(keys_v, threshold), even));
            const auto selected = hn::And(remaining, hn::Not(small));
            counts[bucket] += hn::CompressStore(keys_v, selected, d,
                                                outputs[bucket] + counts[bucket]);
            remaining = hn::And(remaining, small);
        }
        counts[partitions - 1] += hn::CompressStore(keys_v, remaining, d,
            outputs[partitions - 1] + counts[partitions - 1]);
    }
    for (; i < count; ++i) {
        const auto key = keys[i];
        std::size_t bucket = 0;
        while (bucket + 1 < partitions &&
               (key < pivots[bucket] || (key == pivots[bucket] && ((i ^ salt) & 1) == 0))) {
            ++bucket;
        }
        outputs[bucket][counts[bucket]++] = key;
    }
    return counts;
}

#define QUICKHEAP_CHUNK_TARGET(T, Suffix) \
    MultiSplitCounts Chunk##Suffix(const T* keys, std::size_t count, const T* pivots, \
                                   std::size_t partitions, T* const* outputs, std::size_t salt) { \
        return Chunk(keys, count, pivots, partitions, outputs, salt); \
    }

QUICKHEAP_CHUNK_TARGET(std::int32_t, I32)
QUICKHEAP_CHUNK_TARGET(std::uint32_t, U32)
QUICKHEAP_CHUNK_TARGET(std::int64_t, I64)
QUICKHEAP_CHUNK_TARGET(std::uint64_t, U64)

#undef QUICKHEAP_CHUNK_TARGET
}
}
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace quickheap::bench {
#define QUICKHEAP_CHUNK_DISPATCH(T, Suffix) \
    HWY_EXPORT(Chunk##Suffix); \
    MultiSplitCounts partition_chunk(const T* keys, std::size_t count, const T* pivots, \
                                    std::size_t partitions, T* const* outputs, std::size_t salt) noexcept { \
        return HWY_DYNAMIC_DISPATCH(Chunk##Suffix)(keys, count, pivots, partitions, outputs, salt); \
    }

QUICKHEAP_CHUNK_DISPATCH(std::int32_t, I32)
QUICKHEAP_CHUNK_DISPATCH(std::uint32_t, U32)
QUICKHEAP_CHUNK_DISPATCH(std::int64_t, I64)
QUICKHEAP_CHUNK_DISPATCH(std::uint64_t, U64)

#undef QUICKHEAP_CHUNK_DISPATCH
}
#endif
