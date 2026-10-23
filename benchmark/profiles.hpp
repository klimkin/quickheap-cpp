#pragma once

#include "heap_catalog.hpp"
#include "original_profile.hpp"

#include <cmath>

namespace quickheap::bench {

enum class BenchmarkProfile { OriginalRust, ControlledOperations };

inline BenchmarkProfile parse_profile(std::string_view name) {
    if (name == "original-rust") return BenchmarkProfile::OriginalRust;
    if (name == "controlled-operations") return BenchmarkProfile::ControlledOperations;
    throw std::runtime_error("unknown benchmark profile: " + std::string(name));
}

inline constexpr std::array original_workloads{
    Workload::HeapSort, Workload::MonotoneConstantSize,
    Workload::MonotoneWiggle, Workload::RandomWiggle,
};

inline constexpr std::array original_heaps{
    QueueKind::QuickHeap, QueueKind::ScalarM3Linear,
#ifdef QUICKHEAP_BENCH_EXTERNAL
    QueueKind::Sequence, QueueKind::S3Q, QueueKind::Original, QueueKind::ExternalRadix,
#endif
    QueueKind::StdPriorityQueue, QueueKind::Dary8, QueueKind::Weak,
};

inline bool original_cutoff_exceeded(std::int64_t median_nanos, std::size_t n,
                                     std::size_t ops, bool table) {
    return static_cast<double>(median_nanos) / (static_cast<double>(ops) * std::log2(n)) >
           (table ? 128.0 : 32.0);
}

template <typename Q> Q make_original_queue(std::size_t peak_size) {
    if constexpr (std::is_same_v<Q, StdKeyHeap<typename Q::Item>>) {
        return Q(original_std_capacity);
#ifdef QUICKHEAP_BENCH_EXTERNAL
    } else if constexpr (std::is_same_v<Q, OriginalQuickHeap<typename Q::Item>>) {
        return Q(std::size_t{1} << 28);
#endif
    } else {
        return make_queue<Q>(peak_size);
    }
}

inline std::string_view original_reservation(QueueKind kind) {
    if (kind == QueueKind::StdPriorityQueue) return "reserve:1048576";
#ifdef QUICKHEAP_BENCH_EXTERNAL
    if (kind == QueueKind::Original) return "reserve:268435456";
#endif
    return "backend-default";
}

}
