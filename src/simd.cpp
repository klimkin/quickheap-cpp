#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "simd.cpp"
#include "hwy/foreach_target.h"
#include "hwy/highway.h"

#include "simd.hpp"

#include <algorithm>
#include <utility>

HWY_BEFORE_NAMESPACE();
namespace quickheap::detail {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

template <class T>
std::size_t Position(const T* pivots, std::size_t count, T key) {
    if (count > 64) {
        std::size_t first = 0;
        std::size_t last = count;
        while (first < last) {
            const auto mid = first + (last - first) / 2;
            if (pivots[mid] > key) {
                first = mid + 1;
            } else {
                last = mid;
            }
        }
        return first;
    }
    const hn::ScalableTag<T> d;
    const auto lanes = hn::Lanes(d);
    const auto threshold = hn::Set(d, key);
    std::size_t position = 0;
    std::size_t i = 0;
    for (; count - i >= lanes; i += lanes) {
        position += hn::CountTrue(d, hn::Gt(hn::LoadU(d, pivots + i), threshold));
    }
    for (; i < count; ++i) {
        position += pivots[i] > key;
    }
    return position;
}

template <class T, bool Paired>
SplitResult Partition(T* keys, T* values, T* lower_keys, T* lower_values,
                      std::size_t count, T pivot, std::size_t salt) {
    const hn::ScalableTag<T> d;
    const auto lanes = hn::Lanes(d);
    const auto threshold = hn::Set(d, pivot);
    const auto one = hn::Set(d, T{1});
    std::size_t upper = 0;
    std::size_t lower = 0;
    std::size_t i = 0;
    for (; count - i >= lanes; i += lanes) {
        const auto key = hn::LoadU(d, keys + i);
        auto value = hn::Zero(d);
        if constexpr (Paired) {
            value = hn::LoadU(d, values + i);
        }
        const auto even = hn::Eq(hn::And(hn::Iota(d, static_cast<T>((i ^ salt) & 1)), one),
                                 hn::Zero(d));
        const auto small = hn::Or(hn::Lt(key, threshold), hn::And(hn::Eq(key, threshold), even));
        const auto large = hn::Not(small);
        // Both cursors are <= i, so full-register stores stay inside count slots.
        const auto low_count = hn::CompressStore(key, small, d, lower_keys + lower);
        const auto high_count = hn::CompressStore(key, large, d, keys + upper);
        if constexpr (Paired) {
            hn::CompressStore(value, small, d, lower_values + lower);
            hn::CompressStore(value, large, d, values + upper);
        }
        lower += low_count;
        upper += high_count;
    }
    for (; i < count; ++i) {
        const T key = keys[i];
        T value{};
        if constexpr (Paired) {
            value = values[i];
        }
        if (key < pivot || (key == pivot && ((i ^ salt) & 1) == 0)) {
            lower_keys[lower] = key;
            if constexpr (Paired) {
                lower_values[lower] = value;
            }
            ++lower;
        } else {
            keys[upper] = key;
            if constexpr (Paired) {
                values[upper] = value;
            }
            ++upper;
        }
    }
    if (lower == 0) {
        const auto pos = static_cast<std::size_t>(std::find(keys, keys + upper, pivot) - keys);
        lower_keys[0] = keys[pos];
        keys[pos] = keys[--upper];
        if constexpr (Paired) {
            lower_values[0] = values[pos];
            values[pos] = values[upper];
        }
        lower = 1;
    } else if (upper == 0) {
        const auto pos = static_cast<std::size_t>(
            std::find(lower_keys, lower_keys + lower, pivot) - lower_keys);
        keys[0] = lower_keys[pos];
        lower_keys[pos] = lower_keys[--lower];
        if constexpr (Paired) {
            values[0] = lower_values[pos];
            lower_values[pos] = lower_values[lower];
        }
        upper = 1;
    }
    return {upper, lower};
}

#define QUICKHEAP_TARGET(T, Suffix)                                           \
    std::size_t Position##Suffix(const T* pivots, std::size_t count, T key) {   \
        return Position(pivots, count, key);                                 \
    }                                                                        \
    SplitResult Partition##Suffix(T* keys, T* values, T* lower_keys,            \
                                  T* lower_values, std::size_t count,          \
                                  T pivot, std::size_t salt) {                \
        if (values != nullptr) {                                              \
            return Partition<T, true>(keys, values, lower_keys, lower_values,  \
                                       count, pivot, salt);                    \
        }                                                                     \
        return Partition<T, false>(keys, values, lower_keys, lower_values,     \
                                    count, pivot, salt);                      \
    }

QUICKHEAP_TARGET(std::int32_t, I32)
QUICKHEAP_TARGET(std::uint32_t, U32)
QUICKHEAP_TARGET(std::int64_t, I64)
QUICKHEAP_TARGET(std::uint64_t, U64)

#undef QUICKHEAP_TARGET

}
}
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace quickheap::detail {

#define QUICKHEAP_DISPATCH(T, Suffix)                                         \
    HWY_EXPORT(Position##Suffix);                                            \
    HWY_EXPORT(Partition##Suffix);                                           \
    std::size_t push_position(const T* pivots, std::size_t count, T key) noexcept { \
        return HWY_DYNAMIC_DISPATCH(Position##Suffix)(pivots, count, key);     \
    }                                                                        \
    SplitResult partition(T* keys, T* values, T* lower_keys, T* lower_values,   \
                          std::size_t count, T pivot, std::size_t salt) noexcept { \
        return HWY_DYNAMIC_DISPATCH(Partition##Suffix)(                       \
            keys, values, lower_keys, lower_values, count, pivot, salt);       \
    }

QUICKHEAP_DISPATCH(std::int32_t, I32)
QUICKHEAP_DISPATCH(std::uint32_t, U32)
QUICKHEAP_DISPATCH(std::int64_t, I64)
QUICKHEAP_DISPATCH(std::uint64_t, U64)

#undef QUICKHEAP_DISPATCH

}
#endif
