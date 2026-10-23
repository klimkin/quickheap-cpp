#include "backend.hpp"
#include "build_info.hpp"
#include "direct_workloads.hpp"
#include "experimental_heap.hpp"
#include "perf_counters.hpp"
#include "runner.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace qb = quickheap::bench;

namespace {

enum class Mode { Variants, Ablation, Comparisons, Rebalancing, Partition };
enum class Baseline { Experimental, QuickHeap, StdPriorityQueue };

struct Options {
    Mode mode = Mode::Variants;
    int minimum = 10;
    int maximum = 20;
    int step = 2;
    int repeats = 3;
    std::array<bool, 4> types = {false, true, false, false};
    bool type_selected = false;
    bool perf = false;
    bool list = false;
    bool diagnostics = false;
    std::string fixture;
    std::string backend = "native";
    std::string workload;
    std::string variant;
};

struct Variant {
    std::string name;
    qb::ExperimentalConfig config;
    Baseline baseline = Baseline::Experimental;
    bool blocked = false;
};

int integer(std::string_view value, std::string_view flag) {
    int parsed = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (error != std::errc{} || end != value.data() + value.size()) {
        throw std::invalid_argument(std::string(flag) + " requires an integer");
    }
    return parsed;
}

Options options(int argc, char** argv) {
    Options result;
    for (int i = 1; i < argc; ++i) {
        const std::string_view flag(argv[i]);
        const auto value = [&]() -> std::string_view {
            if (++i == argc) throw std::invalid_argument(std::string(flag) + " requires a value");
            return argv[i];
        };
        if (flag == "--help") {
            std::cout << "Usage: quickheap_experiments [--mode variants|ablation|comparisons|rebalancing|partition]\n"
                         "  [--min LOG2] [--max LOG2] [--step LOG2] [--repeat COUNT]\n"
                         "  [--i32] [--i64] [--u32] [--u64] [--workload NAME] [--variant NAME]\n"
                         "  [--fixture FILE] [--backend native|scalar|avx2|avx512] [--diagnostics] [--perf]\n"
                         "Partition workloads: random, duplicates, ascending. Variants: scalar, simd.\n"
                         "Rebalancing workloads: best, worst, flat. Variants: none, pivot_forgetting, naive_log.\n"
                         "Other modes use synthetic workload names; --list prints their variants.\n";
            std::exit(0);
        } else if (flag == "--mode") {
            const auto name = value();
            if (name == "variants") result.mode = Mode::Variants;
            else if (name == "ablation") result.mode = Mode::Ablation;
            else if (name == "comparisons") result.mode = Mode::Comparisons;
            else if (name == "rebalancing") result.mode = Mode::Rebalancing;
            else if (name == "partition") result.mode = Mode::Partition;
            else throw std::invalid_argument("unknown experiment mode " + std::string(name));
        } else if (flag == "--min") result.minimum = integer(value(), flag);
        else if (flag == "--max") result.maximum = integer(value(), flag);
        else if (flag == "--step") result.step = integer(value(), flag);
        else if (flag == "--repeat") result.repeats = integer(value(), flag);
        else if (flag == "--workload") result.workload = value();
        else if (flag == "--variant") result.variant = value();
        else if (flag == "--fixture") result.fixture = value();
        else if (flag == "--backend") result.backend = value();
        else if (flag == "--diagnostics") result.diagnostics = true;
        else if (flag == "--perf") { result.perf = true; result.diagnostics = true; }
        else if (flag == "--list") result.list = true;
        else if (flag == "--i32" || flag == "--i64" || flag == "--u32" || flag == "--u64") {
            if (!result.type_selected) result.types.fill(false);
            result.type_selected = true;
            result.types[flag == "--i32" ? 0 : flag == "--i64" ? 1 : flag == "--u32" ? 2 : 3] = true;
        } else throw std::invalid_argument("unknown option " + std::string(flag));
    }
    if (result.minimum < 1 || result.maximum > 25 || result.maximum < result.minimum ||
        result.step <= 0 || result.step > 25 || result.repeats <= 0) {
        throw std::invalid_argument("require 1 <= --min <= --max <= 25, --step in [1,25], --repeat > 0");
    }
    if (!result.fixture.empty() &&
        (result.mode == Mode::Partition || result.mode == Mode::Rebalancing))
        throw std::invalid_argument("--fixture requires variants, ablation, or comparisons mode");
    return result;
}

std::vector<Variant> variants(Mode mode) {
    std::vector<Variant> result;
    const auto add = [&](std::string name, qb::ExperimentalConfig config) {
        result.push_back({std::move(name), config});
    };
    if (mode == Mode::Rebalancing) {
        qb::ExperimentalConfig config;
        config.search = qb::Search::Simd;
        config.partition = qb::Partition::Simd;
        add("none", config);
        config.rebalance = qb::Rebalance::PivotForgetting;
        add("pivot_forgetting", config);
        config.rebalance = qb::Rebalance::NaiveLog;
        add("naive_log", config);
        return result;
    }
    if (mode == Mode::Partition) return {{"scalar", {}}, {"simd", {}}};
    if (mode == Mode::Variants) {
        result.push_back({"quickheap", {}, Baseline::QuickHeap});
        result.push_back({"std_priority_queue", {}, Baseline::StdPriorityQueue});
    }
    const std::array pivots = {
        std::pair{qb::Pivot::Random, "median1"}, std::pair{qb::Pivot::Median3, "median3"},
        std::pair{qb::Pivot::Median5, "median5"}, std::pair{qb::Pivot::Perfect, "perfect"},
        std::pair{qb::Pivot::Log2, "log2"}, std::pair{qb::Pivot::Cbrt, "cbrt"},
        std::pair{qb::Pivot::Table, "table"}};
    for (const auto& [pivot, name] : pivots) {
        if (mode == Mode::Ablation && pivot != qb::Pivot::Random &&
            pivot != qb::Pivot::Median3 && pivot != qb::Pivot::Median5) continue;
        if (mode == Mode::Comparisons && pivot != qb::Pivot::Random &&
            pivot != qb::Pivot::Median3 && pivot != qb::Pivot::Perfect) continue;
        const std::array searches = {
            std::pair{qb::Search::Binary, "binary"}, std::pair{qb::Search::Linear, "linear"},
            std::pair{qb::Search::Simd, "simd"}};
        for (const auto& [search, search_name] : searches) {
            if (mode == Mode::Ablation && search == qb::Search::Linear) continue;
            if (mode == Mode::Comparisons && search == qb::Search::Simd) continue;
            qb::ExperimentalConfig config;
            config.pivot = pivot;
            config.search = search;
            config.partition = search == qb::Search::Simd ? qb::Partition::Simd : qb::Partition::Scalar;
            add(std::string(name) + "_" + search_name, config);
        }
    }
    if (mode == Mode::Ablation || mode == Mode::Variants) {
        for (const auto& [pivot, name] : pivots) {
            if (pivot != qb::Pivot::Random && pivot != qb::Pivot::Median3 &&
                pivot != qb::Pivot::Median5) continue;
            qb::ExperimentalConfig config;
            config.pivot = pivot;
            config.search = qb::Search::Simd;
            add(std::string(name) + "_simd_search", config);
            config.search = qb::Search::Binary;
            config.partition = qb::Partition::Simd;
            add(std::string(name) + "_simd_partition", config);
        }
    }
    if (mode == Mode::Variants) {
        for (const auto count : {2U, 3U, 4U}) {
            qb::ExperimentalConfig config;
            config.search = qb::Search::Simd;
            config.partition = qb::Partition::Simd;
            config.partition_count = count;
            if (count != 2) add("median3_partition" + std::to_string(count), config);
            result.push_back({"blocked112_partition" + std::to_string(count), config,
                              Baseline::Experimental, true});
        }
        for (const std::size_t terminal : {1U, 8U, 16U}) {
            qb::ExperimentalConfig config;
            config.terminal_size = terminal;
            add("median3_terminal" + std::to_string(terminal), config);
        }
        for (const auto& [rebalance, name] : {
                std::pair{qb::Rebalance::PivotForgetting, "pivot_forgetting"},
                std::pair{qb::Rebalance::NaiveLog, "naive_log"}}) {
            qb::ExperimentalConfig config;
            config.search = qb::Search::Simd;
            config.partition = qb::Partition::Simd;
            config.rebalance = rebalance;
            add(name, config);
        }
    }
    return result;
}

bool simd(const Variant& variant) {
    return variant.baseline == Baseline::QuickHeap ||
           (variant.baseline == Baseline::Experimental &&
            (variant.config.search == qb::Search::Simd || variant.config.partition == qb::Partition::Simd));
}

struct Measurement {
    std::int64_t nanos;
    std::optional<qb::HardwareCounts> hardware;
};

template <typename Q, typename T>
Measurement measure(const qb::Plan<T>& plan, qb::Run<Q, T>& run, bool perf) {
    qb::PerfCounters counters(perf);
    counters.start();
    const auto start = std::chrono::steady_clock::now();
    run.execute(plan);
    const auto stop = std::chrono::steady_clock::now();
    const auto hardware = counters.stop();
    qb::verify(plan, run);
    return {std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count(), hardware};
}

template <typename T>
void legacy_row(const Variant& variant, std::size_t n, std::string_view workload, const qb::Plan<T>& plan,
         int repeat, Measurement measured, std::optional<qb::ExperimentalCounters> counters) {
    std::cout << qb::key_name<T>() << ',' << variant.name << ',' << n << ',' << workload << ','
              << plan.timed.expected_pops.size() << ',' << repeat << ',' << measured.nanos << ',';
    if (counters && counters->comparisons_exact) std::cout << counters->push_comparisons;
    std::cout << ',';
    if (counters && counters->comparisons_exact) std::cout << counters->pop_comparisons;
    std::cout << ',';
    if (measured.hardware) std::cout << measured.hardware->branch_misses;
    std::cout << ",,";
    if (measured.hardware) std::cout << measured.hardware->cache_misses;
    std::cout << ',';
    if (measured.hardware) std::cout << measured.hardware->cache_references;
    std::cout << ",,";
    if (counters) std::cout << counters->partitions;
    std::cout << ',';
    if (counters) std::cout << counters->elements_partitioned;
    std::cout << ',';
    if (counters) std::cout << counters->rebalances;
    std::cout << ',' << (simd(variant) ? qb::simd_backend() : "scalar") << ',';
    if (counters) std::cout << counters->multi_partitions;
    std::cout << ',';
    if (counters) std::cout << counters->blocks_partitioned;
    std::cout << '\n';
    std::cout.flush();
    if (!std::cout) throw std::runtime_error("failed to write experiment CSV");
}

inline constexpr std::string_view paired_csv_header =
    "elem,heap,n,workload,ops,repeat,nanos,protocol,build,backend,fixture_digest,"
    "reservation,timer,repeats,warmup,push_comparisons,pop_comparisons";
inline constexpr std::string_view diagnostic_csv_header =
    ",partitions,elements_partitioned,rebalances,multi_partitions,blocks_partitioned,"
    "branch_misses,hw_cache_misses,hw_cache_references";

std::string implementation_name(const Variant& variant) {
    return variant.baseline == Baseline::Experimental ? "substitute_" + variant.name : variant.name;
}

template <typename T>
void fixture_row(const qb::Fixture<T>& fixture, const Variant& variant, const Options& opts,
                 int repeat, const Measurement& measured,
                 const std::optional<qb::ExperimentalCounters>& counters) {
    std::cout << qb::key_name<T>() << ',' << implementation_name(variant) << ',' << fixture.n << ','
              << qb::fixture_workload_name(fixture.workload) << ','
              << qb::fixture_shape(fixture.workload, fixture.n).timed_count << ',' << repeat << ','
              << measured.nanos << ',' << qb::fixture_protocol << ',';
    qb::csv_field(std::cout, std::string(qb::build_description) + "/" + qb::build_options +
                  "/source=" + qb::source_digest + "/dependencies=" + qb::dependency_digest);
    std::cout << ',' << (simd(variant) ? qb::simd_backend() : "scalar") << ',' << fixture.digest
              << ",backend-default,workload-only-raii"
              << ',' << opts.repeats << ',' << (opts.mode == Mode::Comparisons ? 0 : 1) << ',';
    if (counters && counters->comparisons_exact) std::cout << counters->push_comparisons;
    std::cout << ',';
    if (counters && counters->comparisons_exact) std::cout << counters->pop_comparisons;
    if (opts.diagnostics) {
        for (const auto count : {&qb::ExperimentalCounters::partitions,
                                 &qb::ExperimentalCounters::elements_partitioned,
                                 &qb::ExperimentalCounters::rebalances,
                                 &qb::ExperimentalCounters::multi_partitions,
                                 &qb::ExperimentalCounters::blocks_partitioned}) {
            std::cout << ',';
            if (counters) std::cout << (*counters).*count;
        }
        std::cout << ',';
        if (measured.hardware) std::cout << measured.hardware->branch_misses;
        std::cout << ',';
        if (measured.hardware) std::cout << measured.hardware->cache_misses;
        std::cout << ',';
        if (measured.hardware) std::cout << measured.hardware->cache_references;
    }
    std::cout << '\n';
    std::cout.flush();
    if (!std::cout) throw std::runtime_error("failed to write experiment CSV");
}

template <typename T, typename Factory>
Measurement fixture_measure(const qb::Fixture<T>& fixture, Factory&& factory, bool perf) {
    if (!perf) {
        const auto duration = qb::direct_trial(fixture, factory, [] {
            return std::chrono::steady_clock::now();
        });
        return {std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count(), std::nullopt};
    }
    qb::PerfCounters counters(true);
    std::optional<qb::HardwareCounts> hardware;
    bool started = false;
    const auto duration = qb::direct_trial(fixture, factory, [&] {
        if (!started) {
            counters.start();
            started = true;
            return std::chrono::steady_clock::now();
        }
        const auto stop = std::chrono::steady_clock::now();
        hardware = counters.stop();
        return stop;
    });
    return {std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count(), hardware};
}

template <typename T, typename Factory, typename CountFactory>
void fixture_trial(const qb::Fixture<T>& fixture, const Variant& variant, const Options& opts,
                   Factory&& factory, CountFactory&& count_factory) {
    qb::validate_fixture(fixture, factory);
    if (opts.mode != Mode::Comparisons) fixture_measure(fixture, factory, false);
    std::optional<qb::ExperimentalCounters> counts;
    if (opts.diagnostics || opts.mode == Mode::Comparisons) {
        auto counted = count_factory();
        if constexpr (requires { counted.reset_counters(); counted.counters(); }) {
            qb::setup_workload(fixture, counted);
            counted.reset_counters();
            qb::dispatch_workload(fixture.workload, [&]<qb::Workload W>() {
                qb::execute_workload<W>(fixture, counted);
            });
            counts = counted.counters();
        }
    }
    for (int repeat = 0; repeat < opts.repeats; ++repeat) {
        const auto measured = opts.mode == Mode::Comparisons
            ? Measurement{0, std::nullopt}
            : fixture_measure(fixture, factory, opts.perf && opts.diagnostics);
        fixture_row(fixture, variant, opts, repeat, measured, counts);
    }
}

template <class T, bool Count> using FlatHeap = qb::ExperimentalHeap<T, Count>;
template <class T, bool Count> using BlockHeap = qb::BlockQuickHeap<T, Count>;

template <typename T, template <class, bool> class Heap>
void experimental_fixture(const qb::Fixture<T>& fixture, const Variant& variant,
                          const Options& opts) {
    fixture_trial(fixture, variant, opts,
                  [&] { return Heap<T, false>(variant.config); },
                  [&] { return Heap<T, true>(variant.config); });
}

template <typename T>
void experiment(const qb::Fixture<T>& fixture, const Variant& variant, const Options& opts) {
    if (variant.baseline == Baseline::QuickHeap) {
        const auto factory = [] { return qb::KeyQuickHeap<T>{}; };
        fixture_trial(fixture, variant, opts, factory, factory);
    } else if (variant.baseline == Baseline::StdPriorityQueue) {
        const auto factory = [] { return qb::StdKeyHeap<T>{}; };
        fixture_trial(fixture, variant, opts, factory, factory);
    } else if (variant.blocked) {
        experimental_fixture<T, BlockHeap>(fixture, variant, opts);
    } else {
        experimental_fixture<T, FlatHeap>(fixture, variant, opts);
    }
}

template <typename T>
std::vector<std::vector<T>> layers(const std::vector<T>& sorted, std::string_view layout) {
    std::mt19937_64 rng(0x6c6179657273ULL ^ sorted.size());
    std::vector<std::vector<T>> result;
    if (layout == "flat") {
        result.push_back(sorted);
        std::shuffle(result[0].begin(), result[0].end(), rng);
        return result;
    }
    std::size_t count = 16;
    for (std::size_t offset = 0; offset < sorted.size();) {
        const auto end = offset + std::min(count, sorted.size() - offset);
        result.emplace_back(sorted.begin() + offset, sorted.begin() + end);
        std::shuffle(result.back().begin(), result.back().end(), rng);
        offset = end;
        if (layout == "best") count *= 2;
    }
    std::reverse(result.begin(), result.end());
    return result;
}

template <typename T>
void rebalancing(std::size_t n, const std::vector<Variant>& configs, const Options& opts) {
    std::mt19937_64 rng(0x726562616c616e63ULL ^ n);
    std::vector<T> keys(n);
    for (auto& key : keys) key = qb::key_from_bits<T>(static_cast<std::make_unsigned_t<T>>(rng()));
    std::sort(keys.begin(), keys.end());
    qb::PlanBuilder<T> builder;
    for (const auto key : keys) builder.push(key);
    builder.timed();
    for (std::size_t i = 0; i < std::max(std::size_t{1}, n / 8); ++i) {
        builder.pop();
        builder.push(qb::key_from_bits<T>(static_cast<std::make_unsigned_t<T>>(rng())));
    }
    const auto plan = builder.finish();
    for (const std::string_view layout : {"best", "worst", "flat"}) {
        if (!opts.workload.empty() && opts.workload != layout) continue;
        const auto initial = layers(keys, layout);
        for (const auto& variant : configs) {
            const auto make_run = [&]<bool Count>() {
                using Heap = qb::ExperimentalHeap<T, Count>;
                auto queue = Heap::from_layers(initial, variant.config);
                if (queue.size() != plan.setup_size || !queue.valid()) {
                    throw std::runtime_error("controlled rebalancing layout invalid");
                }
                return qb::Run<Heap, T>{std::move(queue), std::vector<T>(plan.timed.expected_pops.size()), false};
            };
            {
                auto warmup = make_run.template operator()<false>();
                measure(plan, warmup, false);
            }
            const auto counts = [&] {
                auto counted = make_run.template operator()<true>();
                counted.execute(plan);
                const auto counts = counted.queue.counters();
                qb::verify(plan, counted);
                return counts;
            }();
            for (int repeat = 0; repeat < opts.repeats; ++repeat) {
                auto run = make_run.template operator()<false>();
                legacy_row(variant, n, layout, plan, repeat, measure(plan, run, opts.perf), counts);
            }
        }
    }
}

template <typename T>
qb::SplitResult scalar_partition(std::vector<T>& upper,
                                 std::vector<T>& lower, T pivot) {
    std::size_t high = 0;
    std::size_t low = 0;
    for (std::size_t i = 0; i < upper.size(); ++i) {
        const auto key = upper[i];
        if (key < pivot || (key == pivot && (i & 1) == 0)) lower[low++] = key;
        else upper[high++] = key;
    }
    if (low == 0) {
        const auto position = static_cast<std::size_t>(std::find(upper.begin(), upper.begin() + high, pivot) - upper.begin());
        lower[low++] = upper[position];
        upper[position] = upper[--high];
    } else if (high == 0) {
        const auto position = static_cast<std::size_t>(std::find(lower.begin(), lower.begin() + low, pivot) - lower.begin());
        upper[high++] = lower[position];
        lower[position] = lower[--low];
    }
    return {high, low};
}

template <typename T>
void partition(std::size_t n, const std::vector<Variant>& configs, const Options& opts) {
    for (const std::string_view workload : {"random", "duplicates", "ascending"}) {
        if (!opts.workload.empty() && opts.workload != workload) continue;
        std::mt19937_64 rng(0x706172746974696fULL ^ n);
        std::vector<T> input(n);
        for (std::size_t i = 0; i < n; ++i) {
            input[i] = workload == "duplicates" ? T{7} : workload == "ascending" ? static_cast<T>(i)
                : qb::key_from_bits<T>(static_cast<std::make_unsigned_t<T>>(rng()));
        }
        auto sorted = input;
        std::sort(sorted.begin(), sorted.end());
        const T pivot = sorted[(n - 1) / 2];
        std::vector<T> upper(n);
        std::vector<T> lower(n);
        std::vector<T> combined;
        combined.reserve(n);
        for (const auto& variant : configs) {
            for (int repeat = -1; repeat < opts.repeats; ++repeat) {
                std::copy(input.begin(), input.end(), upper.begin());
                qb::PerfCounters hardware(opts.perf && repeat >= 0);
                hardware.start();
                const auto start = std::chrono::steady_clock::now();
                const auto split = variant.name == "simd"
                    ? qb::partition(upper.data(), static_cast<T*>(nullptr), lower.data(),
                                    static_cast<T*>(nullptr), n, pivot, 0)
                    : scalar_partition(upper, lower, pivot);
                const auto stop = std::chrono::steady_clock::now();
                const auto counts = hardware.stop();
                if (split.upper == 0 || split.lower == 0 || split.upper + split.lower != n ||
                    std::any_of(upper.begin(), upper.begin() + split.upper, [pivot](T key) { return key < pivot; }) ||
                    std::any_of(lower.begin(), lower.begin() + split.lower, [pivot](T key) { return key > pivot; })) {
                    throw std::runtime_error("partition boundary or count invalid");
                }
                combined.assign(upper.begin(), upper.begin() + split.upper);
                combined.insert(combined.end(), lower.begin(), lower.begin() + split.lower);
                std::sort(combined.begin(), combined.end());
                if (combined != sorted) throw std::runtime_error("partition key multiset changed");
                if (repeat < 0) continue;
                const auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count();
                std::cout << qb::key_name<T>() << ',' << variant.name << ','
                          << (variant.name == "simd" ? qb::simd_backend() : "scalar") << ','
                          << n << ',' << workload << ',' << repeat << ',' << nanos << ','
                          << split.upper << ',' << split.lower << ',';
                if (counts) std::cout << counts->branch_misses;
                std::cout << ',';
                if (counts) std::cout << counts->cache_misses;
                std::cout << ',';
                if (counts) std::cout << counts->cache_references;
                std::cout << '\n';
                std::cout.flush();
                if (!std::cout) throw std::runtime_error("failed to write partition CSV");
            }
        }
    }
}

template <typename T>
void run(const Options& opts, const std::vector<Variant>& configs) {
    for (int logarithm = opts.minimum; logarithm <= opts.maximum; logarithm += opts.step) {
        const auto n = std::size_t{1} << logarithm;
        if (opts.mode == Mode::Partition) partition<T>(n, configs, opts);
        else if (opts.mode == Mode::Rebalancing) rebalancing<T>(n, configs, opts);
        else {
            for (const auto& workload : qb::workloads) {
                if (!opts.workload.empty() && opts.workload != workload.name) continue;
                const auto fixture = qb::generate_fixture<T>(workload.workload, n);
                for (const auto& config : configs) experiment(fixture, config, opts);
            }
        }
    }
}

}

int main(int argc, char** argv) {
    try {
        const auto opts = options(argc, argv);
        qb::select_backend(opts.backend);
        auto configs = variants(opts.mode);
        if (opts.list) {
            for (const auto& variant : configs) std::cout << variant.name << '\n';
            return 0;
        }
        if (!opts.variant.empty()) {
            std::erase_if(configs, [&opts](const auto& variant) { return variant.name != opts.variant; });
            if (configs.empty()) throw std::invalid_argument("unknown variant " + opts.variant);
        }
        if (!opts.workload.empty()) {
            const bool valid = opts.mode == Mode::Partition
                ? opts.workload == "random" || opts.workload == "duplicates" || opts.workload == "ascending"
                : opts.mode == Mode::Rebalancing
                ? opts.workload == "best" || opts.workload == "worst" || opts.workload == "flat"
                : std::any_of(qb::workloads.begin(), qb::workloads.end(), [&opts](const auto& workload) {
                    return workload.name == opts.workload;
                });
            if (!valid) throw std::invalid_argument("unknown workload " + opts.workload);
        }
        if (opts.mode == Mode::Partition) {
            std::cout << "elem,variant,backend,n,workload,repeat,nanos,upper,lower,branch_misses,hw_cache_misses,hw_cache_references\n";
        } else if (opts.mode == Mode::Rebalancing) {
            std::cout << qb::timing_csv_header << ",partitions,elements_partitioned,rebalances,backend,multi_partitions,blocks_partitioned\n";
        } else {
            std::cout << paired_csv_header;
            if (opts.diagnostics) std::cout << diagnostic_csv_header;
            std::cout << '\n';
        }
        if (!opts.fixture.empty()) {
            auto raw = qb::load_raw_fixture(opts.fixture);
            if (!opts.workload.empty() && opts.workload != qb::fixture_workload_name(raw.workload))
                throw std::invalid_argument("--workload disagrees with fixture");
            const auto consume = [&]<typename T>() {
                const auto fixture = qb::typed_fixture<T>(std::move(raw));
                for (const auto& config : configs) experiment(fixture, config, opts);
            };
            if (raw.elem == "i32") consume.template operator()<std::int32_t>();
            else if (raw.elem == "i64") consume.template operator()<std::int64_t>();
            else if (raw.elem == "u32") consume.template operator()<std::uint32_t>();
            else consume.template operator()<std::uint64_t>();
            return 0;
        }
        if (opts.types[0]) run<std::int32_t>(opts, configs);
        if (opts.types[1]) run<std::int64_t>(opts, configs);
        if (opts.types[2]) run<std::uint32_t>(opts, configs);
        if (opts.types[3]) run<std::uint64_t>(opts, configs);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "quickheap_experiments: " << error.what() << '\n';
        return 1;
    }
}
