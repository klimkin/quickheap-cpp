#include "diagnostics.hpp"
#include "competitor_heaps.hpp"

#include <catch2/catch_test_macros.hpp>

#include <sstream>

namespace {
using namespace quickheap::bench;

TEST_CASE("Diagnostics report allocated scalar slots and retained occupancy", "[diagnostics][memory]") {
    ScalarHeap<std::int64_t, 1, false, false, true> queue;
    queue.reserve(64);
    for (const auto key : {8, 2, 5, 3}) queue.push(key);
    REQUIRE(queue.try_pop() == 2);
    const auto stats = diagnostics(queue);
    REQUIRE(stats.allocation);
    CHECK(stats.size == 3);
    CHECK(stats.allocation->capacity == queue.capacity());
    CHECK(stats.allocation->allocated_key_bytes == queue.capacity() * sizeof(std::int64_t));
    CHECK(stats.allocation->spare_slots == queue.capacity() - queue.size());
    REQUIRE(stats.buckets);
    CHECK(stats.buckets->active_buckets == queue.buckets().size());
    REQUIRE(stats.buckets->layer_sizes);
    std::size_t occupied = 0;
    std::size_t largest = 0;
    for (const auto size : *stats.buckets->layer_sizes) {
        occupied += size;
        largest = std::max(largest, size);
    }
    CHECK(occupied == queue.size());
    CHECK(stats.buckets->largest_bucket == largest);
    CHECK(stats.buckets->terminal_size == queue.buckets().back().size());
    CHECK(stats.counters.partitions == queue.counters().partitions);
    CHECK(stats.counters.elements_partitioned == queue.counters().elements_partitioned);
    CHECK(stats.counters.retries == queue.counters().retries);
    CHECK(stats.counters.samples == queue.counters().samples);
    const auto again = diagnostics(queue);
    CHECK(again.counters.partitions == stats.counters.partitions);
    CHECK(again.size == stats.size);
}

TEST_CASE("Unsupported and disabled diagnostics remain absent", "[diagnostics][availability]") {
    StdKeyHeap<std::int32_t> standard;
    standard.push(3);
    const auto std_stats = diagnostics(standard);
    CHECK(std_stats.size == 1);
    CHECK_FALSE(std_stats.allocation);
    CHECK_FALSE(std_stats.buckets);
    CHECK_FALSE(std_stats.counters.push_comparisons);
    CHECK_FALSE(std_stats.peak_size);

    ScalarHeap<std::int32_t> uncounted;
    uncounted.push(8);
    uncounted.push(1);
    REQUIRE(uncounted.try_pop() == 1);
    const auto scalar_stats = diagnostics(uncounted);
    CHECK_FALSE(scalar_stats.counters.push_comparisons);
    CHECK_FALSE(scalar_stats.counters.partitions);
    CHECK_FALSE(scalar_stats.counters.samples);
}

TEST_CASE("Counted competitors report supported zero comparisons", "[diagnostics][availability]") {
    DaryHeap<std::int32_t, 4, true> queue;
    queue.push(3);
    const auto stats = diagnostics(queue);
    CHECK(stats.counters.push_comparisons == 0);
    CHECK(stats.counters.pop_comparisons == 0);
    CHECK_FALSE(stats.counters.partitions);
    CHECK_FALSE(stats.buckets);
}

TEST_CASE("SIMD diagnostics retain partition counts without claiming exact comparisons", "[diagnostics][availability]") {
    ExperimentalConfig config;
    config.search = Search::Simd;
    ExperimentalHeap<std::int32_t, true> queue(config);
    const auto stats = diagnostics(queue);
    CHECK_FALSE(stats.counters.push_comparisons);
    CHECK_FALSE(stats.counters.pop_comparisons);
    CHECK(stats.counters.partitions == 0);
    CHECK(stats.counters.rebalances == 0);
    REQUIRE(stats.buckets);
    CHECK(stats.buckets->active_buckets == 1);
    CHECK_FALSE(stats.buckets->largest_bucket);
    CHECK_FALSE(stats.buckets->layer_sizes);
}

TEST_CASE("Key value allocations use scalar width for both columns", "[diagnostics][memory]") {
    KvQuickHeap<std::int64_t> queue;
    queue.push(3);
    queue.push(8);
    const auto stats = diagnostics(queue);
    REQUIRE(stats.allocation);
    CHECK(stats.allocation->capacity == queue.capacity());
    CHECK(stats.allocation->allocated_key_bytes == queue.capacity() * sizeof(std::int64_t));
    CHECK(stats.allocation->spare_slots == queue.capacity() - 2 * queue.size());
}

TEST_CASE("Diagnostic replay counts workload operations and retains setup peak", "[diagnostics][workload]") {
    for (const auto &entry : workloads) {
        CAPTURE(entry.name);
        const std::size_t n = 8;
        const auto fixture = generate_fixture<std::int32_t>(entry.workload, n);
        const auto stats = collect_fixture_diagnostics(fixture, [] {
            return ScalarHeap<std::int32_t, 1, false, false, true>{};
        });
        const bool constant_size = entry.workload == Workload::MonotoneConstantSize ||
                                   entry.workload == Workload::RandomConstantSize;
        CHECK(stats.size == (constant_size ? n + 50 : 0));
        std::uint64_t operations = n;
        std::size_t peak = n;
        switch (entry.workload) {
        case Workload::HeapSort: break;
        case Workload::MonotoneConstantSize:
        case Workload::RandomConstantSize:
            operations = 10 * n;
            peak = n + 50;
            break;
        case Workload::MonotoneWiggle:
        case Workload::GeometricMonotoneWiggle:
        case Workload::RandomWiggle:
            operations = 3 * n;
            break;
        case Workload::WorstCaseDescending:
            operations = 9 * n;
            peak = 8 * n + 1;
            break;
        case Workload::MostlyPushDecreasing:
            operations = 10 * n;
            peak = 8 * n + 1;
            break;
        }
        CHECK(stats.pushes == operations);
        CHECK(stats.pops == operations);
        CHECK(stats.peak_size == peak);
        CHECK(stats.counters.push_comparisons.has_value());
        CHECK(stats.counters.pop_comparisons.has_value());
    }
}

TEST_CASE("Diagnostic counters exclude setup comparisons", "[diagnostics][workload]") {
    const auto fixture = generate_fixture<std::int32_t>(Workload::RandomConstantSize, 4);
    const auto stats = collect_fixture_diagnostics(fixture, [] {
        return DaryHeap<std::int32_t, 4, true>{};
    });
    DaryHeap<std::int32_t, 4, true> reference;
    setup_workload(fixture, reference);
    reference.reset_comparisons();
    execute_workload<Workload::RandomConstantSize>(fixture, reference);
    CHECK(stats.counters.push_comparisons == reference.push_comparisons());
    CHECK(stats.counters.pop_comparisons == reference.pop_comparisons());
}

TEST_CASE("Diagnostic CSV distinguishes unsupported fields from observed zeros", "[diagnostics][csv]") {
    FixtureDiagnostics missing{0, {}, {}, {}, {}, {}, {}};
    std::ostringstream absent;
    write_diagnostics(absent, missing);
    const auto absent_csv = absent.str();
    CHECK(absent_csv == "0,,,,,,,,,,,,,,,,,,");
    CHECK(std::count(absent_csv.begin(), absent_csv.end(), ',') ==
          std::count(diagnostics_csv_header.begin(), diagnostics_csv_header.end(), ','));

    ScalarHeap<std::int32_t, 1, false, false, true> queue;
    const auto stats = diagnostics(queue);
    std::ostringstream supported;
    write_diagnostics(supported, stats);
    CHECK(supported.str().ends_with(diagnostic_memory_scope));
    CHECK(supported.str().find(",0,0,0,0,") != std::string::npos);
}

}
