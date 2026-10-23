#pragma once

#include "direct_workloads.hpp"
#include "experimental_heap.hpp"
#include "scalar_heap.hpp"

#include <algorithm>
#include <optional>
#include <ostream>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace quickheap::bench {

inline constexpr std::string_view diagnostic_memory_scope =
    "scalar-slot-capacity-excludes-pivots-metadata-allocator";

struct AllocationDiagnostics {
    std::size_t capacity;
    std::size_t allocated_key_bytes;
    std::size_t spare_slots;
};

struct BucketDiagnostics {
    std::size_t active_buckets;
    std::optional<std::size_t> largest_bucket;
    std::optional<std::size_t> terminal_size;
    std::optional<std::vector<std::size_t>> layer_sizes;
};

struct CounterDiagnostics {
    std::optional<std::uint64_t> push_comparisons;
    std::optional<std::uint64_t> pop_comparisons;
    std::optional<std::uint64_t> partitions;
    std::optional<std::uint64_t> elements_partitioned;
    std::optional<std::uint64_t> retries;
    std::optional<std::uint64_t> samples;
    std::optional<std::uint64_t> rebalances;
    std::optional<std::uint64_t> multi_partitions;
    std::optional<std::uint64_t> blocks_partitioned;
};

struct FixtureDiagnostics {
    std::size_t size;
    std::optional<AllocationDiagnostics> allocation;
    std::optional<BucketDiagnostics> buckets;
    CounterDiagnostics counters;
    std::optional<std::size_t> peak_size;
    std::optional<std::uint64_t> pushes;
    std::optional<std::uint64_t> pops;
};

template <typename Q> struct DiagnosticTraits {
    static constexpr std::size_t columns = 1;
    static constexpr bool counted = [] {
        if constexpr (requires { Q::comparison_counted; }) return Q::comparison_counted;
        else return false;
    }();
};

template <typename T> struct DiagnosticTraits<KvQuickHeap<T>> {
    static constexpr std::size_t columns = 2;
    static constexpr bool counted = false;
};

template <typename T, std::size_t M, bool Perfect, bool Linear, bool Count>
struct DiagnosticTraits<ScalarHeap<T, M, Perfect, Linear, Count>> {
    static constexpr std::size_t columns = 1;
    static constexpr bool counted = Count;
};

template <typename T, bool Count, typename Bucket>
struct DiagnosticTraits<ExperimentalHeap<T, Count, Bucket>> {
    static constexpr std::size_t columns = 1;
    static constexpr bool counted = Count;
};

template <typename Q> FixtureDiagnostics diagnostics(const Q &queue) {
    FixtureDiagnostics result{queue.size(), {}, {}, {}, {}, {}, {}};
    if constexpr (requires { queue.capacity(); }) {
        using Key = decltype(key_of(std::declval<typename Q::Item>()));
        const auto capacity = queue.capacity();
        const auto occupied_slots = queue.size() * DiagnosticTraits<Q>::columns;
        result.allocation = AllocationDiagnostics{
            capacity, capacity * sizeof(Key), capacity - occupied_slots};
    }
    if constexpr (requires { queue.buckets(); }) {
        const auto buckets = queue.buckets();
        BucketDiagnostics shape{buckets.size(), std::size_t{0}, std::size_t{0},
                                std::vector<std::size_t>{}};
        shape.layer_sizes->reserve(buckets.size());
        for (const auto &bucket : buckets) {
            shape.layer_sizes->push_back(bucket.size());
            shape.largest_bucket = std::max(*shape.largest_bucket, bucket.size());
        }
        if (!buckets.empty()) shape.terminal_size = buckets.back().size();
        result.buckets = std::move(shape);
    } else if constexpr (requires { queue.layers(); }) {
        result.buckets = BucketDiagnostics{queue.layers(), {}, {}, {}};
    }
    if constexpr (DiagnosticTraits<Q>::counted) {
        if constexpr (requires { queue.counters(); }) {
            const auto &counts = queue.counters();
            if (counts.comparisons_exact) {
                result.counters.push_comparisons = counts.push_comparisons;
                result.counters.pop_comparisons = counts.pop_comparisons;
            }
            result.counters.partitions = counts.partitions;
            result.counters.elements_partitioned = counts.elements_partitioned;
            if constexpr (requires { counts.retries; }) result.counters.retries = counts.retries;
            if constexpr (requires { counts.samples; }) result.counters.samples = counts.samples;
            if constexpr (requires { counts.rebalances; }) result.counters.rebalances = counts.rebalances;
            if constexpr (requires { counts.multi_partitions; }) result.counters.multi_partitions = counts.multi_partitions;
            if constexpr (requires { counts.blocks_partitioned; }) result.counters.blocks_partitioned = counts.blocks_partitioned;
        } else if constexpr (requires { queue.push_comparisons(); queue.pop_comparisons(); }) {
            result.counters.push_comparisons = queue.push_comparisons();
            result.counters.pop_comparisons = queue.pop_comparisons();
        }
    }
    return result;
}

template <typename Q> class ObservationQueue {
public:
    using Item = typename Q::Item;

    explicit ObservationQueue(Q &queue) : queue_(queue), peak_size_(queue.size()) {}

    template <typename T> void push(T key) {
        queue_.push(key);
        ++pushes_;
        peak_size_ = std::max(peak_size_, queue_.size());
    }

    std::optional<Item> try_pop() {
        auto item = queue_.try_pop();
        if (item) ++pops_;
        return item;
    }

    std::size_t size() const { return queue_.size(); }

    void reset_operations() noexcept {
        pushes_ = 0;
        pops_ = 0;
    }

    FixtureDiagnostics snapshot() const {
        auto result = diagnostics(queue_);
        result.peak_size = peak_size_;
        result.pushes = pushes_;
        result.pops = pops_;
        return result;
    }

private:
    Q &queue_;
    std::size_t peak_size_;
    std::uint64_t pushes_ = 0;
    std::uint64_t pops_ = 0;
};

template <typename T, typename Factory>
FixtureDiagnostics collect_fixture_diagnostics(const Fixture<T> &fixture, Factory factory) {
    auto queue = factory();
    ObservationQueue observed(queue);
    setup_workload<true>(fixture, observed);
    observed.reset_operations();
    if constexpr (requires { queue.reset_counters(); }) queue.reset_counters();
    else if constexpr (requires { queue.reset_comparisons(); }) queue.reset_comparisons();
    dispatch_workload(fixture.workload, [&]<Workload W>() {
        execute_workload<W, true>(fixture, observed);
    });
    return observed.snapshot();
}

inline constexpr std::string_view diagnostics_csv_header =
    "size,capacity,allocated_key_bytes,spare_slots,peak_size,pushes,pops,"
    "partitions,elements_partitioned,retries,samples,rebalances,multi_partitions,"
    "blocks_partitioned,active_buckets,largest_bucket,terminal_size,layer_sizes,memory_scope";

inline void write_diagnostics(std::ostream &out, const FixtureDiagnostics &stats) {
    const auto field = [&](const auto &value) {
        if (value) out << *value;
        out << ',';
    };
    out << stats.size << ',';
    if (stats.allocation) {
        out << stats.allocation->capacity << ',' << stats.allocation->allocated_key_bytes
            << ',' << stats.allocation->spare_slots << ',';
    } else out << ",,,";
    field(stats.peak_size);
    field(stats.pushes);
    field(stats.pops);
    field(stats.counters.partitions);
    field(stats.counters.elements_partitioned);
    field(stats.counters.retries);
    field(stats.counters.samples);
    field(stats.counters.rebalances);
    field(stats.counters.multi_partitions);
    field(stats.counters.blocks_partitioned);
    if (stats.buckets) {
        out << stats.buckets->active_buckets << ',';
        field(stats.buckets->largest_bucket);
        field(stats.buckets->terminal_size);
        if (stats.buckets->layer_sizes) {
            bool first = true;
            for (const auto size : *stats.buckets->layer_sizes) {
                if (!first) out << ';';
                out << size;
                first = false;
            }
        }
        out << ',';
    } else out << ",,,,";
    if (stats.allocation) out << diagnostic_memory_scope;
}

}
