#include "benchmark_workloads.hpp"

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

namespace {
using namespace quickheap::test_bench;

template <typename Q, typename T>
void run_benchmark(const std::string &name, const Plan<T> &plan) {
    BENCHMARK_ADVANCED(std::string(name))(Catch::Benchmark::Chronometer meter) {
        std::vector<Run<Q, T>> runs;
        runs.reserve(static_cast<std::size_t>(meter.runs()));
        for (int i = 0; i < meter.runs(); ++i) runs.push_back(prepare<Q>(plan));
        meter.measure([&](int index) {
            auto &run = runs[static_cast<std::size_t>(index)];
            run.execute(plan);
            return run.replay_ok;
        });
        for (auto &run : runs) REQUIRE_NOTHROW(verify(plan, run));
    };
}

template <typename T> void run_size(const WorkloadInfo &workload, std::size_t n) {
    const Plan<T> plan = make_plan<T>(workload.workload, n);
    INFO(plan.error);
    REQUIRE(plan.error.empty());
    const std::string prefix = std::string(workload.name) + "/" + std::string(key_name<T>()) + "/";
    const std::string suffix = "/" + std::to_string(n) +
        " pushes=" + std::to_string(plan.timed.pushes.size()) +
        " pops=" + std::to_string(plan.timed.expected_pops.size());
    run_benchmark<KeyQuickHeap<T>>(prefix + "quickheap" + suffix, plan);
    run_benchmark<KvQuickHeap<T>>(prefix + "kv_quickheap" + suffix, plan);
}

#define REGISTER_WORKLOAD(kind)                                                \
    TEMPLATE_TEST_CASE(#kind, "[!benchmark]", std::int32_t, std::int64_t,         \
                       std::uint32_t, std::uint64_t) {                          \
        const auto &workload = workloads[static_cast<std::size_t>(Workload::kind)]; \
        for (std::size_t n = 1 << 10;                                          \
             n <= (std::size_t{1} << workload.catch_max_log2); n *= 4) {         \
            DYNAMIC_SECTION(std::to_string(n)) { run_size<TestType>(workload, n); } \
        }                                                                     \
    }

REGISTER_WORKLOAD(HeapSort)
REGISTER_WORKLOAD(MonotoneConstantSize)
REGISTER_WORKLOAD(RandomConstantSize)
REGISTER_WORKLOAD(MonotoneWiggle)
REGISTER_WORKLOAD(RandomWiggle)
REGISTER_WORKLOAD(GeometricMonotoneWiggle)
REGISTER_WORKLOAD(WorstCaseDescending)
REGISTER_WORKLOAD(MostlyPushDecreasing)

#undef REGISTER_WORKLOAD

}
