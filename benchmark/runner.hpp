#ifndef QUICKHEAP_BENCH_RUNNER_HPP
#define QUICKHEAP_BENCH_RUNNER_HPP

#include "workloads.hpp"
#include "perf_counters.hpp"

#include <chrono>
#include <ostream>

namespace quickheap::bench {

template <typename Q, typename T, typename Factory, typename Measure>
auto prepared_trial(const Plan<T> &plan, Factory factory, Measure measure) {
    auto run = prepare<Q>(plan, factory(plan));
    const auto result = measure(run);
    verify(plan, run);
    return result;
}

template <typename Q, typename T, typename Factory>
std::int64_t timed_trial(const Plan<T> &plan, Factory factory, bool perf = false,
                         std::optional<HardwareCounts> *hardware = nullptr) {
    return prepared_trial<Q>(plan, factory, [&](auto &run) {
        PerfCounters counters(perf);
        counters.start();
        const auto start = std::chrono::steady_clock::now();
        run.execute(plan);
        const auto stop = std::chrono::steady_clock::now();
        const auto counts = counters.stop();
        if (hardware) *hardware = counts;
        return std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count();
    });
}

inline constexpr std::string_view timing_csv_header =
    "elem,heap,n,workload,ops,repeat,nanos,push_comparisons,pop_comparisons,"
    "branch_misses,l1_cache_misses,hw_cache_misses,hw_cache_references,l3_cache_misses";
inline constexpr std::string_view memory_csv_header =
    "elem,heap,n,workload,size,capacity,capacity_ratio";

inline void csv_field(std::ostream &out, std::string_view value) {
    if (value.find_first_of(",\"\r\n") == std::string_view::npos) {
        out << value;
        return;
    }
    out << '"';
    for (const char ch : value) {
        if (ch == '"') out << '"';
        out << ch;
    }
    out << '"';
}

inline void flush_csv(std::ostream &out) {
    out.flush();
    if (!out) throw std::runtime_error("failed to write benchmark CSV");
}

template <typename T>
void trial_csv_prefix(std::ostream &out, std::string_view heap, std::size_t n,
                      std::string_view workload, const Plan<T> &plan, std::size_t repeat) {
    const auto pushes = plan.timed.pushes.size();
    const auto pops = plan.timed.expected_pops.size();
    if (pushes != pops) throw std::runtime_error("CSV ops requires balanced push/pop pairs");
    out << key_name<T>() << ',';
    csv_field(out, heap);
    out << ',' << n << ',';
    csv_field(out, workload);
    out << ',' << pops << ',' << repeat << ',';
}

template <typename T>
void timing_csv_row(std::ostream &out, std::string_view heap, std::size_t n,
                    std::string_view workload, const Plan<T> &plan, std::size_t repeat,
                    std::int64_t nanos, std::optional<HardwareCounts> hardware = std::nullopt) {
    trial_csv_prefix(out, heap, n, workload, plan, repeat);
    out << nanos << ",,,";
    if (hardware) out << hardware->branch_misses;
    out << ",,";
    if (hardware) out << hardware->cache_misses;
    out << ',';
    if (hardware) out << hardware->cache_references;
    out << ",\n";
    flush_csv(out);
}

struct ComparisonResult { std::uint64_t pushes; std::uint64_t pops; };

template <typename Q>
concept ComparisonQueue = requires(Q &queue) {
    queue.reset_comparisons();
    queue.push_comparisons();
    queue.pop_comparisons();
} || requires(Q &queue) {
    queue.reset_counters();
    queue.counters().push_comparisons;
    queue.counters().pop_comparisons;
    queue.counters().comparisons_exact;
};

template <typename Q, typename T, typename Factory>
ComparisonResult comparison_trial(const Plan<T> &plan, Factory factory) {
    return prepared_trial<Q>(plan, factory, [&](auto &run) {
        if constexpr (requires { run.queue.reset_comparisons(); }) {
            run.queue.reset_comparisons();
            run.execute(plan);
            return ComparisonResult{run.queue.push_comparisons(), run.queue.pop_comparisons()};
        } else {
            run.queue.reset_counters();
            if (!run.queue.counters().comparisons_exact)
                throw std::runtime_error("queue configuration cannot count exact comparisons");
            run.execute(plan);
            return ComparisonResult{run.queue.counters().push_comparisons,
                                    run.queue.counters().pop_comparisons};
        }
    });
}

template <typename T>
void comparison_csv_row(std::ostream &out, std::string_view heap, std::size_t n,
                        std::string_view workload, const Plan<T> &plan, std::size_t repeat,
                        ComparisonResult result) {
    trial_csv_prefix(out, heap, n, workload, plan, repeat);
    out << ',' << result.pushes << ',' << result.pops << ",,,,,\n";
    flush_csv(out);
}

struct MemoryResult { std::size_t size; std::size_t capacity; };

template <typename Q, typename T, typename Factory>
MemoryResult memory_trial(const Plan<T> &plan, Factory factory) {
    return prepared_trial<Q>(plan, factory, [&](auto &run) {
        run.execute(plan);
        return MemoryResult{run.queue.size(), run.queue.capacity()};
    });
}

template <typename T>
void memory_csv_row(std::ostream &out, std::string_view heap, std::size_t n,
                    std::string_view workload, MemoryResult result) {
    out << key_name<T>() << ',';
    csv_field(out, heap);
    out << ',' << n << ',';
    csv_field(out, workload);
    out << ',' << result.size << ',' << result.capacity << ',';
    if (n != 0) out << static_cast<double>(result.capacity) / n;
    out << '\n';
    flush_csv(out);
}

} // namespace quickheap::bench

#endif
