#ifndef QUICKHEAP_BENCH_HEAP_CATALOG_HPP
#define QUICKHEAP_BENCH_HEAP_CATALOG_HPP

#include "competitor_heaps.hpp"
#include "experimental_heap.hpp"
#include "scalar_heap.hpp"
#include "workloads.hpp"
#ifdef QUICKHEAP_BENCH_EXTERNAL
#include "external_heaps.hpp"
#include "radix_external.hpp"
#endif

namespace quickheap::bench {

enum class QueueKind {
    QuickHeap, StdPriorityQueue, KvQuickHeap, KvStdPriorityQueue,
    CustomBinary, Dary4, Dary8, Dary16, CustomDary4, CustomDary8, Weak, Radix, Scalar, Blocked,
    ScalarM1Linear, ScalarM3Linear, ScalarPerfectLinear,
    ScalarM1Binary, ScalarM3Binary, ScalarPerfectBinary,
#ifdef QUICKHEAP_BENCH_BOOST
    BoostDary4, BoostFibonacci, BoostPairing, BoostBinomial, BoostSkew,
#endif
#ifdef QUICKHEAP_BENCH_EXTERNAL
    Sequence, S3Q, Original, ExternalRadix,
#endif
    FirstQuickHeapVariant,
};

struct QueueInfo {
    QueueKind kind;
    std::string_view name;
    bool exact_capacity;
    bool exact_comparisons;
    bool monotone;
};

inline constexpr std::array base_queue_catalog = {
    QueueInfo{QueueKind::QuickHeap, "quickheap", true, false, false},
    QueueInfo{QueueKind::StdPriorityQueue, "std_priority_queue", false, false, false},
    QueueInfo{QueueKind::KvQuickHeap, "kv_quickheap", true, false, false},
    QueueInfo{QueueKind::KvStdPriorityQueue, "kv_std_priority_queue", false, false, false},
    QueueInfo{QueueKind::CustomBinary, "custom_binary_heap", true, true, false},
    QueueInfo{QueueKind::Dary4, "dary4", true, true, false},
    QueueInfo{QueueKind::Dary8, "dary8", true, true, false},
    QueueInfo{QueueKind::Dary16, "dary16", true, true, false},
    QueueInfo{QueueKind::CustomDary4, "custom_dary4", true, true, false},
    QueueInfo{QueueKind::CustomDary8, "custom_dary8", true, true, false},
    QueueInfo{QueueKind::Weak, "weak_heap", true, true, false},
    QueueInfo{QueueKind::Radix, "radix_heap", true, false, true},
    QueueInfo{QueueKind::Scalar, "scalar_quickheap", true, true, false},
    QueueInfo{QueueKind::Blocked, "blocked_quickheap", true, true, false},
    QueueInfo{QueueKind::ScalarM1Linear, "scalar_median1_linear", true, true, false},
    QueueInfo{QueueKind::ScalarM3Linear, "scalar_median3_linear", true, true, false},
    QueueInfo{QueueKind::ScalarPerfectLinear, "scalar_perfect_linear", true, true, false},
    QueueInfo{QueueKind::ScalarM1Binary, "scalar_median1_binary", true, true, false},
    QueueInfo{QueueKind::ScalarM3Binary, "scalar_median3_binary", true, true, false},
    QueueInfo{QueueKind::ScalarPerfectBinary, "scalar_perfect_binary", true, true, false},
#ifdef QUICKHEAP_BENCH_BOOST
    QueueInfo{QueueKind::BoostDary4, "boost_dary4", false, true, false},
    QueueInfo{QueueKind::BoostFibonacci, "boost_fibonacci", false, true, false},
    QueueInfo{QueueKind::BoostPairing, "boost_pairing", false, true, false},
    QueueInfo{QueueKind::BoostBinomial, "boost_binomial", false, true, false},
    QueueInfo{QueueKind::BoostSkew, "boost_skew", false, true, false},
#endif
#ifdef QUICKHEAP_BENCH_EXTERNAL
    QueueInfo{QueueKind::Sequence, "sequence_heap", false, false, false},
    QueueInfo{QueueKind::S3Q, "s3q", false, false, false},
    QueueInfo{QueueKind::Original, "original_quickheap", false, false, false},
    QueueInfo{QueueKind::ExternalRadix, "external_radix_heap", false, false, true},
#endif
};

inline constexpr unsigned a1 = quickheap::detail::opt_terminal_shortcut;
inline constexpr unsigned a2 = quickheap::detail::opt_cold_terminal;
inline constexpr unsigned a3 = quickheap::detail::opt_one_count;
inline constexpr unsigned a4 = quickheap::detail::opt_scan_early_exit;
inline constexpr unsigned a5 = quickheap::detail::opt_sort_network;
inline constexpr unsigned a6 = quickheap::detail::opt_sample_growth;
inline constexpr unsigned a7 = quickheap::detail::opt_terminal_32;
inline constexpr unsigned a8 = quickheap::detail::opt_rebalance;
inline constexpr unsigned a9 = quickheap::detail::opt_four_way;
inline constexpr unsigned aa_unread_bit_twin_of_quickheap = 32;

struct QuickHeapVariant { std::string_view name; unsigned opts; };
inline constexpr std::array quickheap_variants = {
    QuickHeapVariant{"quickheap_a1", a1},
    QuickHeapVariant{"quickheap_a2", a2},
    QuickHeapVariant{"quickheap_a3", a3},
    QuickHeapVariant{"quickheap_a4", a4},
    QuickHeapVariant{"quickheap_a5", a5},
    QuickHeapVariant{"quickheap_a6", a6},
    QuickHeapVariant{"quickheap_a7", a7},
    QuickHeapVariant{"quickheap_a8", a8},
    QuickHeapVariant{"quickheap_a9", a9},
    QuickHeapVariant{"quickheap_a12", a1 | a2},
    QuickHeapVariant{"quickheap_a123", a1 | a2 | a3},
    QuickHeapVariant{"quickheap_a1234", a1 | a2 | a3 | a4},
    QuickHeapVariant{"quickheap_a12345", a1 | a2 | a3 | a4 | a5},
    QuickHeapVariant{"quickheap_aa", aa_unread_bit_twin_of_quickheap},
    QuickHeapVariant{"quickheap_o944", 944},
    QuickHeapVariant{"quickheap_o872", 872},
    QuickHeapVariant{"quickheap_o216", 216},
    QuickHeapVariant{"quickheap_o740", 740},
    QuickHeapVariant{"quickheap_o340", 340},
    QuickHeapVariant{"quickheap_o396", 396},
    QuickHeapVariant{"quickheap_o572", 572},
    QuickHeapVariant{"quickheap_o482", 482},
    QuickHeapVariant{"quickheap_o594", 594},
    QuickHeapVariant{"quickheap_o650", 650},
    QuickHeapVariant{"quickheap_o314", 314},
    QuickHeapVariant{"quickheap_o774", 774},
    QuickHeapVariant{"quickheap_o182", 182},
    QuickHeapVariant{"quickheap_o110", 110},
    QuickHeapVariant{"quickheap_o990", 990},
    QuickHeapVariant{"quickheap_o961", 961},
    QuickHeapVariant{"quickheap_o113", 113},
    QuickHeapVariant{"quickheap_o169", 169},
    QuickHeapVariant{"quickheap_o793", 793},
    QuickHeapVariant{"quickheap_o293", 293},
    QuickHeapVariant{"quickheap_o661", 661},
    QuickHeapVariant{"quickheap_o589", 589},
    QuickHeapVariant{"quickheap_o509", 509},
    QuickHeapVariant{"quickheap_o547", 547},
    QuickHeapVariant{"quickheap_o403", 403},
    QuickHeapVariant{"quickheap_o331", 331},
    QuickHeapVariant{"quickheap_o763", 763},
    QuickHeapVariant{"quickheap_o199", 199},
    QuickHeapVariant{"quickheap_o887", 887},
    QuickHeapVariant{"quickheap_o943", 943},
    QuickHeapVariant{"quickheap_o192", 192},
    QuickHeapVariant{"quickheap_o144", 144},
    QuickHeapVariant{"quickheap_o80", 80},
    QuickHeapVariant{"quickheap_o136", 136},
    QuickHeapVariant{"quickheap_o72", 72},
    QuickHeapVariant{"quickheap_o24", 24},
    QuickHeapVariant{"quickheap_o130", 130},
    QuickHeapVariant{"quickheap_o66", 66},
    QuickHeapVariant{"quickheap_o18", 18},
    QuickHeapVariant{"quickheap_o210", 210},
    QuickHeapVariant{"quickheap_o10", 10},
    QuickHeapVariant{"quickheap_o202", 202},
    QuickHeapVariant{"quickheap_o154", 154},
    QuickHeapVariant{"quickheap_o90", 90},
    QuickHeapVariant{"quickheap_o129", 129},
    QuickHeapVariant{"quickheap_o65", 65},
    QuickHeapVariant{"quickheap_o17", 17},
    QuickHeapVariant{"quickheap_o209", 209},
    QuickHeapVariant{"quickheap_o9", 9},
    QuickHeapVariant{"quickheap_o201", 201},
    QuickHeapVariant{"quickheap_o153", 153},
    QuickHeapVariant{"quickheap_o89", 89},
    QuickHeapVariant{"quickheap_o195", 195},
    QuickHeapVariant{"quickheap_o147", 147},
    QuickHeapVariant{"quickheap_o83", 83},
    QuickHeapVariant{"quickheap_o139", 139},
    QuickHeapVariant{"quickheap_o75", 75},
    QuickHeapVariant{"quickheap_o27", 27},
    QuickHeapVariant{"quickheap_o219", 219},
    QuickHeapVariant{"quickheap_o203", 203},
    QuickHeapVariant{"quickheap_o207", 207},
    QuickHeapVariant{"quickheap_o217", 217},
    QuickHeapVariant{"quickheap_o221", 221},
};

constexpr bool quickheap_variant(QueueKind kind) { return kind >= QueueKind::FirstQuickHeapVariant; }

inline constexpr auto queue_catalog = [] {
    std::array<QueueInfo, base_queue_catalog.size() + quickheap_variants.size()> catalog{};
    std::copy(base_queue_catalog.begin(), base_queue_catalog.end(), catalog.begin());
    for (std::size_t i = 0; i < quickheap_variants.size(); ++i) {
        catalog[base_queue_catalog.size() + i] = QueueInfo{
            static_cast<QueueKind>(static_cast<std::size_t>(QueueKind::FirstQuickHeapVariant) + i),
            quickheap_variants[i].name, true, false, false};
    }
    return catalog;
}();

constexpr bool monotone_workload(Workload workload) {
    return workload == Workload::HeapSort || workload == Workload::MonotoneConstantSize ||
           workload == Workload::MonotoneWiggle || workload == Workload::GeometricMonotoneWiggle;
}

template <typename T, typename Visitor, std::size_t... I>
void visit_quickheap_variant(QueueKind kind, Visitor &visit, std::index_sequence<I...>) {
    const auto index = static_cast<std::size_t>(kind) - static_cast<std::size_t>(QueueKind::FirstQuickHeapVariant);
    static_cast<void>(((index == I && (visit.template operator()<KeyQuickHeap<T, quickheap_variants[I].opts>>(), true)) || ...));
}

template <typename T, bool Count, typename Visitor>
void visit_queue(QueueKind kind, Visitor &&visit) {
    switch (kind) {
    case QueueKind::QuickHeap: visit.template operator()<KeyQuickHeap<T>>(); break;
    case QueueKind::StdPriorityQueue: visit.template operator()<StdKeyHeap<T>>(); break;
    case QueueKind::KvQuickHeap: visit.template operator()<KvQuickHeap<T>>(); break;
    case QueueKind::KvStdPriorityQueue: visit.template operator()<StdKvHeap<T>>(); break;
    case QueueKind::CustomBinary: visit.template operator()<CustomBinaryHeap<T, Count>>(); break;
    case QueueKind::Dary4: visit.template operator()<DaryHeap<T, 4, Count>>(); break;
    case QueueKind::Dary8: visit.template operator()<DaryHeap<T, 8, Count>>(); break;
    case QueueKind::Dary16: visit.template operator()<DaryHeap<T, 16, Count>>(); break;
    case QueueKind::CustomDary4: visit.template operator()<CustomDaryHeap<T, 4, Count>>(); break;
    case QueueKind::CustomDary8: visit.template operator()<CustomDaryHeap<T, 8, Count>>(); break;
    case QueueKind::Weak: visit.template operator()<WeakHeap<T, Count>>(); break;
    case QueueKind::Radix: visit.template operator()<RadixHeap<T>>(); break;
    case QueueKind::Scalar: visit.template operator()<ExperimentalHeap<T, Count>>(); break;
    case QueueKind::Blocked: visit.template operator()<BlockQuickHeap<T, Count>>(); break;
    case QueueKind::ScalarM1Linear: visit.template operator()<ScalarHeap<T, 1, false, true, Count>>(); break;
    case QueueKind::ScalarM3Linear: visit.template operator()<ScalarHeap<T, 3, false, true, Count>>(); break;
    case QueueKind::ScalarPerfectLinear: visit.template operator()<ScalarHeap<T, 1, true, true, Count>>(); break;
    case QueueKind::ScalarM1Binary: visit.template operator()<ScalarHeap<T, 1, false, false, Count>>(); break;
    case QueueKind::ScalarM3Binary: visit.template operator()<ScalarHeap<T, 3, false, false, Count>>(); break;
    case QueueKind::ScalarPerfectBinary: visit.template operator()<ScalarHeap<T, 1, true, false, Count>>(); break;
#ifdef QUICKHEAP_BENCH_BOOST
    case QueueKind::BoostDary4: visit.template operator()<BoostDaryHeap<T, 4, Count>>(); break;
    case QueueKind::BoostFibonacci: visit.template operator()<BoostFibonacciHeap<T, Count>>(); break;
    case QueueKind::BoostPairing: visit.template operator()<BoostPairingHeap<T, Count>>(); break;
    case QueueKind::BoostBinomial: visit.template operator()<BoostBinomialHeap<T, Count>>(); break;
    case QueueKind::BoostSkew: visit.template operator()<BoostSkewHeap<T, Count>>(); break;
#endif
#ifdef QUICKHEAP_BENCH_EXTERNAL
    case QueueKind::Sequence: visit.template operator()<SequenceHeap<T>>(); break;
    case QueueKind::S3Q: visit.template operator()<S3QHeap<T>>(); break;
    case QueueKind::Original: visit.template operator()<OriginalQuickHeap<T>>(); break;
    case QueueKind::ExternalRadix: visit.template operator()<ExternalRadixHeap<T>>(); break;
#endif
    default: visit_quickheap_variant<T>(kind, visit, std::make_index_sequence<quickheap_variants.size()>{});
    }
}

constexpr bool key_only_queue(QueueKind kind) {
    return kind != QueueKind::KvQuickHeap && kind != QueueKind::KvStdPriorityQueue;
}

template <typename Q> Q make_queue(std::size_t peak_size) {
#ifdef QUICKHEAP_BENCH_EXTERNAL
    if constexpr (std::is_same_v<Q, SequenceHeap<typename Q::Item>> ||
                  std::is_same_v<Q, S3QHeap<typename Q::Item>> ||
                  std::is_same_v<Q, OriginalQuickHeap<typename Q::Item>>) {
        return Q(std::max(std::size_t{1}, peak_size));
    } else
#endif
    {
        static_cast<void>(peak_size);
        return Q{};
    }
}

template <typename Q, typename T> Q make_queue(const Plan<T> &plan) {
#ifdef QUICKHEAP_BENCH_EXTERNAL
    if constexpr (std::is_same_v<Q, SequenceHeap<T>> || std::is_same_v<Q, S3QHeap<T>>) {
        const auto sentinel = [](T key) {
            return key == std::numeric_limits<T>::lowest() || key == std::numeric_limits<T>::max();
        };
        if (std::any_of(plan.setup.pushes.begin(), plan.setup.pushes.end(), sentinel) ||
            std::any_of(plan.timed.pushes.begin(), plan.timed.pushes.end(), sentinel))
            throw std::runtime_error("external heap workload contains reserved sentinel keys");
    }
#endif
    return make_queue<Q>(plan.peak_size);
}

struct AblationInfo { std::string_view name; Pivot pivot; bool simd; };
inline constexpr std::array ablation_catalog = {
    AblationInfo{"scalar_median1", Pivot::Random, false},
    AblationInfo{"scalar_median3", Pivot::Median3, false},
    AblationInfo{"scalar_median5", Pivot::Median5, false},
    AblationInfo{"simd_median1", Pivot::Random, true},
    AblationInfo{"simd_median3", Pivot::Median3, true},
    AblationInfo{"simd_median5", Pivot::Median5, true},
};

} // namespace quickheap::bench

#endif
