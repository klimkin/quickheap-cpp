#include "runner.hpp"

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <sstream>

namespace {
using namespace quickheap::bench;

template <typename Q, typename T> void check_queue(const Plan<T> &plan) {
    auto run = prepare<Q>(plan);
    run.execute(plan);
    REQUIRE_NOTHROW(verify(plan, run));
}

TEMPLATE_TEST_CASE("Synthetic schedules retain operation contracts", "[workloads]",
                   std::int32_t, std::int64_t, std::uint32_t, std::uint64_t) {
    constexpr std::size_t n = 32;
    for (const auto &entry : workloads) {
        CAPTURE(entry.name);
        const auto plan = make_plan<TestType>(entry.workload, n);
        REQUIRE(plan.error.empty());
        std::size_t pairs = 0;
        std::size_t setup_size = 0;
        switch (entry.workload) {
        case Workload::HeapSort: pairs = n; break;
        case Workload::MonotoneConstantSize:
        case Workload::RandomConstantSize: pairs = 10 * n; setup_size = n + 50; break;
        case Workload::MonotoneWiggle:
        case Workload::RandomWiggle:
        case Workload::GeometricMonotoneWiggle: pairs = 3 * n; break;
        case Workload::WorstCaseDescending: pairs = 9 * n; break;
        case Workload::MostlyPushDecreasing: pairs = 10 * n; break;
        }
        CHECK(plan.timed.pushes.size() == pairs);
        CHECK(plan.timed.expected_pops.size() == pairs);
        CHECK(plan.timed.operations.size() == 2 * pairs);
        CHECK(plan.setup_size == setup_size);
        CHECK(plan.final_size == setup_size);
        check_queue<KeyQuickHeap<TestType>>(plan);
        check_queue<KvQuickHeap<TestType>>(plan);
        check_queue<StdKeyHeap<TestType>>(plan);
        check_queue<StdKvHeap<TestType>>(plan);
    }
}

TEMPLATE_TEST_CASE("Signed boundaries preserve keys and payloads", "[workloads]",
                   std::int32_t, std::int64_t) {
    using T = TestType;
    const T low = std::numeric_limits<T>::min();
    const T high = std::numeric_limits<T>::max();
    CHECK(add_positive(low, 0) == low);
    CHECK(add_positive(low, 1) == low + 1);
    const auto full_range = static_cast<std::uint64_t>(static_cast<std::make_unsigned_t<T>>(high) -
                                                     static_cast<std::make_unsigned_t<T>>(low));
    CHECK(add_positive(low, full_range) == high);
    CHECK(add_positive(high, 1) == std::nullopt);
    CHECK(add_positive(T{-1}, 1) == T{0});
    CHECK(payload(low) == high);
    CHECK(payload(high) == low);
    PlanBuilder<T> builder;
    builder.timed();
    for (const T key : {high, low, T{-1}, T{0}, T{1}, low, high}) builder.push(key);
    for (int i = 0; i < 5; ++i) builder.pop();
    const auto plan = builder.finish();
    REQUIRE(plan.timed.expected_pops == std::vector<T>{low, low, -1, 0, 1});
    REQUIRE(plan.expected_final == std::vector<T>{high, high});
    check_queue<KeyQuickHeap<T>>(plan);
    check_queue<KvQuickHeap<T>>(plan);
    check_queue<StdKeyHeap<T>>(plan);
    check_queue<StdKvHeap<T>>(plan);
}

TEST_CASE("Monotone range exhaustion produces actionable error", "[workloads]") {
    PlanBuilder<std::int32_t> builder;
    REQUIRE_FALSE(builder.push_sum(std::numeric_limits<std::int32_t>::max(), 1));
    REQUIRE_FALSE(builder.finish().error.empty());
    REQUIRE_FALSE(make_plan<std::uint32_t>(Workload::HeapSort, 0).error.empty());
    REQUIRE_FALSE(make_plan<std::uint32_t>(Workload::HeapSort, (std::size_t{1} << 25) + 1).error.empty());
}

TEST_CASE("Verification rejects wrong popped and retained keys", "[workloads]") {
    auto plan = make_plan<std::int32_t>(Workload::RandomConstantSize, 8);
    auto run = prepare<StdKeyHeap<std::int32_t>>(plan);
    run.execute(plan);
    ++run.outputs[0];
    REQUIRE_THROWS(verify(plan, run));
    auto retained = prepare<StdKeyHeap<std::int32_t>>(plan);
    retained.execute(plan);
    ++plan.expected_final[0];
    REQUIRE_THROWS(verify(plan, retained));
}

TEST_CASE("Verification rejects corrupted payloads", "[workloads]") {
    const auto plan = make_plan<std::int64_t>(Workload::HeapSort, 8);
    auto run = prepare<StdKvHeap<std::int64_t>>(plan);
    run.execute(plan);
    ++run.outputs[0].second;
    REQUIRE_THROWS(verify(plan, run));
}

TEST_CASE("CSV keeps unmeasured counters empty and reports operation pairs", "[workloads]") {
    const auto plan = make_plan<std::uint32_t>(Workload::HeapSort, 4);
    std::ostringstream csv;
    timing_csv_row(csv, "quickheap", 4, "HeapSort", plan, 2, 99);
    CHECK(csv.str() == "u32,quickheap,4,HeapSort,4,2,99,,,,,,,\n");
    std::ostringstream memory;
    memory_csv_row<std::uint32_t>(memory, "kv_quickheap", 4, "HeapSort", {0, 16});
    CHECK(memory.str() == "u32,kv_quickheap,4,HeapSort,0,16,4\n");
}

} // namespace
