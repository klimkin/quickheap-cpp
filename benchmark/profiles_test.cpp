#include "profiles.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace quickheap::bench;

TEST_CASE("Profiles reject unknown contracts", "[profile]") {
    CHECK(parse_profile("original-rust") == BenchmarkProfile::OriginalRust);
    CHECK(parse_profile("controlled-operations") == BenchmarkProfile::ControlledOperations);
    CHECK_THROWS(parse_profile("original"));
}

TEST_CASE("Original cutoff includes workload factor once", "[profile]") {
    for (const auto workload : original_workloads) {
        const auto ops = fixture_shape(workload, 8).timed_count;
        CHECK_FALSE(original_cutoff_exceeded(32 * 3 * ops, 8, ops, false));
        CHECK(original_cutoff_exceeded(32 * 3 * ops + 1, 8, ops, false));
        CHECK_FALSE(original_cutoff_exceeded(128 * 3 * ops, 8, ops, true));
        CHECK(original_cutoff_exceeded(128 * 3 * ops + 1, 8, ops, true));
    }
}

TEST_CASE("Original registrations exclude extra competitors", "[profile]") {
    CHECK(original_heaps.front() == QueueKind::QuickHeap);
    CHECK(original_heaps[1] == QueueKind::ScalarM3Linear);
    CHECK(std::find(original_heaps.begin(), original_heaps.end(), QueueKind::KvQuickHeap) == original_heaps.end());
    auto queue = make_original_queue<StdKeyHeap<std::int32_t>>(8);
    CHECK(queue.size() == 0);
}
