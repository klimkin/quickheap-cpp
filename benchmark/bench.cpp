#include "heap_catalog.hpp"
#include "runner.hpp"
#include "direct_workloads.hpp"
#include "build_info.hpp"
#include "backend.hpp"
#include "scalar_trace.hpp"
#include "diagnostics.hpp"

#include <charconv>
#include <iostream>
#include <string_view>

namespace {
using namespace quickheap::bench;

struct Options {
    int min = 10;
    int max = 25;
    int step = 1;
    std::size_t repeat = 3;
    std::size_t warmup = 1;
    std::array<bool, 4> types{};
    std::vector<Workload> selected_workloads;
    std::vector<QueueKind> selected_heaps;
    bool explicit_heaps = false;
    bool explicit_workloads = false;
    bool table = false;
    bool perf = false;
    bool memory = false;
    bool comparisons = false;
    bool ablation = false;
    bool list = false;
    bool help = false;
    std::filesystem::path fixture;
#ifdef QUICKHEAP_BENCH_FIXED_BACKEND
    std::string backend = QUICKHEAP_BENCH_FIXED_BACKEND;
#else
    std::string backend = "native";
#endif
    bool verify_only = false;
    bool scalar_trace = false;
    bool diagnostics = false;
    std::vector<std::uint64_t> samples;
};

int number(std::string_view text, std::string_view flag) {
    int value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size())
        throw std::runtime_error(std::string(flag) + " requires an integer");
    return value;
}

template <typename Catalog, typename Select>
void filter(std::string_view text, const Catalog &catalog, Select select) {
    do {
        const auto comma = text.find(',');
        const auto name = text.substr(0, comma);
        const auto found = std::find_if(catalog.begin(), catalog.end(),
                                       [&](const auto &entry) { return entry.name == name; });
        if (found == catalog.end()) throw std::runtime_error("unknown filter: " + std::string(name));
        select(*found);
        if (comma == std::string_view::npos) break;
        text.remove_prefix(comma + 1);
    } while (true);
}

Options parse(int argc, char **argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view flag = argv[i];
        const auto value = [&]() -> std::string_view {
            if (++i >= argc) throw std::runtime_error("missing value for " + std::string(flag));
            return argv[i];
        };
        if (flag == "--min") options.min = number(value(), flag);
        else if (flag == "--fixture") options.fixture = value();
        else if (flag == "--backend") options.backend = value();
        else if (flag == "--verify-only") options.verify_only = true;
        else if (flag == "--scalar-trace") options.scalar_trace = true;
        else if (flag == "--diagnostics") options.diagnostics = true;
        else if (flag == "--samples") {
            auto text = value();
            do {
                const auto comma = text.find(',');
                const auto sample = number(text.substr(0, comma), flag);
                if (sample < 0) throw std::runtime_error("negative pivot sample");
                options.samples.push_back(static_cast<std::uint64_t>(sample));
                if (comma == std::string_view::npos) break;
                text.remove_prefix(comma + 1);
            } while (true);
        }
        else if (flag == "--max") options.max = number(value(), flag);
        else if (flag == "--step") options.step = number(value(), flag);
        else if (flag == "--repeat" || flag == "--warmup") {
            const int count = number(value(), flag);
            if (count < (flag == "--repeat" ? 1 : 0))
                throw std::runtime_error(std::string(flag) + " has an invalid trial count");
            (flag == "--repeat" ? options.repeat : options.warmup) = static_cast<std::size_t>(count);
        } else if (flag == "--i32") options.types[0] = true;
        else if (flag == "--i64") options.types[1] = true;
        else if (flag == "--u32") options.types[2] = true;
        else if (flag == "--u64") options.types[3] = true;
        else if (flag == "--workload") {
            options.explicit_workloads = true;
            filter(value(), workloads, [&](const auto &entry) {
                options.selected_workloads.push_back(entry.workload);
            });
        } else if (flag == "--heap") {
            options.explicit_heaps = true;
            filter(value(), queue_catalog, [&](const auto &entry) {
                options.selected_heaps.push_back(entry.kind);
            });
        } else if (flag == "--table") options.table = true;
        else if (flag == "--perf") options.perf = true;
        else if (flag == "--memory") options.memory = true;
        else if (flag == "--comparisons") options.comparisons = true;
        else if (flag == "--ablation") options.ablation = true;
        else if (flag == "--list") options.list = true;
        else if (flag == "--help" || flag == "-h") options.help = true;
        else throw std::runtime_error("unsupported option: " + std::string(flag));
    }
    if (options.min < 0 || options.max > 25 || options.min > options.max)
        throw std::runtime_error("require 0 <= --min <= --max <= 25");
    if (options.step < 1 || options.step > 25) throw std::runtime_error("--step must be in [1, 25]");
    if (options.memory && options.comparisons)
        throw std::runtime_error("--memory and --comparisons cannot be combined");
    if (options.verify_only && options.fixture.empty())
        throw std::runtime_error("--verify-only requires --fixture");
    if (options.scalar_trace && options.fixture.empty()) throw std::runtime_error("--scalar-trace requires --fixture");
    if (options.diagnostics && options.fixture.empty()) throw std::runtime_error("--diagnostics requires --fixture");
    if (!options.samples.empty() && !options.scalar_trace) throw std::runtime_error("--samples requires --scalar-trace");
    if (!options.fixture.empty() && (options.memory || options.ablation))
        throw std::runtime_error("--fixture currently requires timing or --verify-only mode");
    if (!options.fixture.empty() && options.perf)
        throw std::runtime_error("--fixture hardware counters are not available yet");
    if (options.perf && (options.memory || options.comparisons))
        throw std::runtime_error("--perf requires timing mode");
    if (options.ablation && (options.memory || options.comparisons || options.explicit_heaps))
        throw std::runtime_error("--ablation cannot combine with --heap, --memory, or --comparisons");
    if (std::none_of(options.types.begin(), options.types.end(), [](bool enabled) { return enabled; }))
        options.types[0] = options.types[1] = true;
    if (options.table) {
        options.selected_workloads = {Workload::MonotoneConstantSize};
        options.step = 5;
    }
    if (options.selected_workloads.empty()) {
        for (const auto &entry : workloads) {
            if (options.memory || options.comparisons || options.ablation ||
                entry.workload == Workload::HeapSort || entry.workload == Workload::MonotoneConstantSize ||
                entry.workload == Workload::MonotoneWiggle || entry.workload == Workload::RandomWiggle)
                options.selected_workloads.push_back(entry.workload);
        }
    }
    if (options.selected_heaps.empty()) {
        for (const auto &entry : queue_catalog) {
            if ((!options.memory || entry.exact_capacity) &&
                (!options.comparisons || entry.exact_comparisons))
                options.selected_heaps.push_back(entry.kind);
        }
    }
    for (const auto &entry : queue_catalog) {
        if (std::find(options.selected_heaps.begin(), options.selected_heaps.end(), entry.kind) ==
            options.selected_heaps.end()) continue;
        if (options.memory && !entry.exact_capacity)
            throw std::runtime_error("--memory unsupported for " + std::string(entry.name));
        if (options.comparisons && !entry.exact_comparisons)
            throw std::runtime_error("--comparisons unsupported for " + std::string(entry.name));
        if (entry.monotone && options.explicit_heaps && options.explicit_workloads) {
            for (const auto workload : options.selected_workloads)
                if (!monotone_workload(workload))
                    throw std::runtime_error(std::string(entry.name) + " requires monotone workloads");
        }
    }
    return options;
}

void help() {
    std::cout << "Usage: quickheap_bench [options]\n"
                 "  --min N --max N --step N  powers of two, defaults 10/25/1, max 25\n"
                 "  --repeat N               measured trials, default 3, positive\n"
                 "  --warmup N               fresh unreported trials, default 1, nonnegative\n"
                 "  --i32 --i64 --u32 --u64   key filters, default i32 and i64\n"
                 "  --workload NAME[,NAME]   select synthetic workloads\n"
                 "  --heap NAME[,NAME]       select available queue variants\n"
                 "  --table                  MonotoneConstantSize with exponent step 5\n"
                 "  --memory                 final exact queue capacity, unsupported heaps error\n"
                 "  --comparisons            exact counts at --max; unsupported heaps error\n"
                 "  --ablation               scalar/SIMD median1/3/5 variants\n"
                 "  --list --help\n"
                 "  --perf                   Linux hardware events during timed trials\n"
                 "  --backend NAME           native, scalar, avx2, avx512\n"
                 "  --fixture FILE           shared inputs under quickheap-cpp-v1\n"
                 "  --verify-only            full untimed fixture trace\n"
                 "  --scalar-trace           scalar bucket transitions and counts\n"
                 "  --samples I[,I]          scripted pivots in scalar trace\n"
                 "  --diagnostics            untimed fixture memory and counters\n"
                 "Timing excludes preparation, validation, and destruction.\n"
                 "CSV ops counts push/pop pairs. Unmeasured counters stay blank.\n"
                 "Radix heap runs monotone workloads only; incompatible defaults are skipped.\n"
                 "Memory capacity counts key slots; kv includes both columns.\n"
                 "Memory ratio divides capacity by workload n.\n";
}

void list() {
    for (const auto &entry : workloads) std::cout << "workload," << entry.name << '\n';
    for (const auto &entry : queue_catalog) std::cout << "heap," << entry.name << '\n';
    for (const auto &entry : ablation_catalog) std::cout << "ablation," << entry.name << '\n';
    for (const auto &backend : backend_options())
        std::cout << "backend," << backend.name << ",compiled=" << backend.compiled
                  << ",supported=" << backend.host_supported << '\n';
    std::cout << "elem,i32\nelem,i64\nelem,u32\nelem,u64\n";
}

inline constexpr std::string_view paired_csv_header =
    "elem,heap,n,workload,ops,repeat,nanos,protocol,build,backend,fixture_digest,"
    "reservation,timer,repeats,warmup,push_comparisons,pop_comparisons";

template <typename T> void run_fixture(const Options &options, const Fixture<T> &fixture) {
    const auto shape = fixture_shape(fixture.workload, fixture.n);
    const auto peak = [&] {
        switch (fixture.workload) {
        case Workload::MonotoneConstantSize:
        case Workload::RandomConstantSize: return fixture.n + 50;
        case Workload::WorstCaseDescending: return 8 * fixture.n + 1;
        case Workload::MostlyPushDecreasing: return 8 * fixture.n + 1;
        case Workload::HeapSort: return fixture.n;
        default: return fixture.n + 1;
        }
    }();
    for (const auto &heap : queue_catalog) {
        if (std::find(options.selected_heaps.begin(), options.selected_heaps.end(), heap.kind) ==
            options.selected_heaps.end()) continue;
        if (heap.monotone && !monotone_workload(fixture.workload)) {
            if (options.explicit_heaps) throw std::runtime_error("heap requires monotone fixture");
            continue;
        }
        const auto visit = [&]<typename Q>() {
            const auto factory = [&] { return make_queue<Q>(peak); };
#ifdef QUICKHEAP_BENCH_EXTERNAL
            if (heap.kind == QueueKind::Sequence || heap.kind == QueueKind::S3Q) {
                const auto reserved = [](T key) {
                    return key == std::numeric_limits<T>::lowest() || key == std::numeric_limits<T>::max();
                };
                if (std::any_of(fixture.keys.begin(), fixture.keys.end(), reserved) ||
                    std::any_of(fixture.setup_keys.begin(), fixture.setup_keys.end(), reserved)) {
                    if (options.explicit_heaps) throw std::runtime_error("fixture contains reserved sentinel keys");
                    return;
                }
            }
#endif
            validate_fixture(fixture, factory, options.verify_only ? &std::cout : nullptr);
            if (options.verify_only) return;
            if (options.scalar_trace) {
                if constexpr (requires(Q &queue) { queue.pivots(); queue.buckets(); }) {
                    scalar_trace<Q>(fixture, std::cout, options.samples);
                    return;
                } else throw std::runtime_error("--scalar-trace requires paired scalar heap");
            }
            for (std::size_t trial = 0; trial < options.warmup + options.repeat; ++trial) {
                std::uint64_t pushes = 0, pops = 0;
                std::int64_t nanos = 0;
                std::optional<FixtureDiagnostics> diagnostic;
                if (options.diagnostics) {
                    diagnostic = collect_fixture_diagnostics(fixture, factory);
                    pushes = diagnostic->counters.push_comparisons.value_or(0);
                    pops = diagnostic->counters.pop_comparisons.value_or(0);
                } else if (options.comparisons) {
                    if constexpr (requires(Q &queue) { queue.reset_comparisons(); queue.push_comparisons(); queue.pop_comparisons(); }) {
                        auto queue = factory();
                        setup_workload(fixture, queue);
                        queue.reset_comparisons();
                        dispatch_workload(fixture.workload, [&]<Workload W>() { execute_workload<W>(fixture, queue); });
                        pushes = queue.push_comparisons();
                        pops = queue.pop_comparisons();
                    } else throw std::runtime_error("fixture comparison counts unavailable");
                } else {
                    const auto duration = direct_trial(fixture, factory, [] { return std::chrono::steady_clock::now(); });
                    nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
                }
                if (trial < options.warmup) continue;
                std::cout << key_name<T>() << ',' << heap.name << ',' << fixture.n << ','
                          << fixture_workload_name(fixture.workload) << ',' << shape.timed_count << ','
                          << trial - options.warmup << ',' << nanos << ',' << fixture_protocol << ',';
                csv_field(std::cout, std::string(build_description) + "/" + build_options + "/source=" + source_digest + "/dependencies=" + dependency_digest);
                std::cout << ',' << (heap.kind == QueueKind::QuickHeap || heap.kind == QueueKind::KvQuickHeap
                    ? simd_backend() : "scalar") << ',' << fixture.digest << ',';
                std::string reservation = "backend-default";
#ifdef QUICKHEAP_BENCH_EXTERNAL
                if (heap.kind == QueueKind::Original) reservation = "required-capacity:" + std::to_string(peak);
#endif
                csv_field(std::cout, reservation);
                std::cout << ",workload-only-raii," << options.repeat << ',' << options.warmup << ',';
                if (options.comparisons || (diagnostic && diagnostic->counters.push_comparisons)) std::cout << pushes;
                std::cout << ',';
                if (options.comparisons || (diagnostic && diagnostic->counters.pop_comparisons)) std::cout << pops;
                if (diagnostic) {
                    std::cout << ',';
                    write_diagnostics(std::cout, *diagnostic);
                }
                std::cout << '\n';
                flush_csv(std::cout);
            }
        };
        if (options.comparisons || options.scalar_trace || options.diagnostics) visit_queue<T, true>(heap.kind, visit);
        else visit_queue<T, false>(heap.kind, visit);
    }
}

template <typename Q, typename T, typename Factory>
void run_case(const Options &options, const Plan<T> &plan, std::string_view heap,
              std::string_view workload, std::size_t n, Factory factory) {
    if (options.memory) {
        if constexpr (requires(Q &queue) { queue.capacity(); })
            memory_csv_row<T>(std::cout, heap, n, workload, memory_trial<Q>(plan, factory));
        else throw std::runtime_error("queue cannot report exact capacity");
        return;
    }
    if (options.comparisons) {
        if constexpr (ComparisonQueue<Q>) {
            for (std::size_t repeat = 0; repeat < options.repeat; ++repeat)
                comparison_csv_row(std::cout, heap, n, workload, plan, repeat,
                                   comparison_trial<Q>(plan, factory));
        } else throw std::runtime_error("queue cannot report exact comparisons");
        return;
    }
    for (std::size_t warmup = 0; warmup < options.warmup; ++warmup)
        static_cast<void>(timed_trial<Q>(plan, factory));
    for (std::size_t repeat = 0; repeat < options.repeat; ++repeat) {
        std::optional<HardwareCounts> hardware;
        const auto nanos = timed_trial<Q>(plan, factory, options.perf, &hardware);
        timing_csv_row(std::cout, heap, n, workload, plan, repeat, nanos, hardware);
    }
}

template <typename T> void run_type(const Options &options) {
    for (const auto &workload : workloads) {
        if (std::find(options.selected_workloads.begin(), options.selected_workloads.end(),
                      workload.workload) == options.selected_workloads.end()) continue;
        for (int exponent = options.comparisons ? options.max : options.min;
             exponent <= options.max; exponent += options.step) {
            const auto n = std::size_t{1} << exponent;
            if (!options.memory && !options.comparisons && !options.ablation && !options.perf) {
                run_fixture(options, generate_fixture<T>(workload.workload, n));
                continue;
            }
            const auto plan = make_plan<T>(workload.workload, n);
            if (!plan.error.empty())
                throw std::runtime_error(std::string(workload.name) + "/" + std::string(key_name<T>()) +
                                         "/" + std::to_string(n) + ": " + plan.error);
            if (options.ablation) {
                for (const auto &variant : ablation_catalog) {
                    ExperimentalConfig config;
                    config.pivot = variant.pivot;
                    config.search = variant.simd ? Search::Simd : Search::Binary;
                    config.partition = variant.simd ? Partition::Simd : Partition::Scalar;
                    run_case<ExperimentalHeap<T>>(options, plan, variant.name, workload.name, n,
                        [config](const auto &) { return ExperimentalHeap<T>(config); });
                }
                continue;
            }
            for (const auto &heap : queue_catalog) {
                if (std::find(options.selected_heaps.begin(), options.selected_heaps.end(), heap.kind) ==
                    options.selected_heaps.end()) continue;
                if (heap.monotone && !monotone_workload(workload.workload)) {
                    std::cerr << "skip " << heap.name << '/' << workload.name << ": nonmonotone workload\n";
                    continue;
                }
#ifdef QUICKHEAP_BENCH_EXTERNAL
                if (heap.kind == QueueKind::Sequence || heap.kind == QueueKind::S3Q) {
                    const auto sentinel = [](T key) {
                        return key == std::numeric_limits<T>::lowest() || key == std::numeric_limits<T>::max();
                    };
                    if (std::any_of(plan.setup.pushes.begin(), plan.setup.pushes.end(), sentinel) ||
                        std::any_of(plan.timed.pushes.begin(), plan.timed.pushes.end(), sentinel)) {
                        if (options.explicit_heaps)
                            throw std::runtime_error(std::string(heap.name) + " workload contains reserved sentinel keys");
                        std::cerr << "skip " << heap.name << '/' << workload.name << '/' << key_name<T>()
                                  << ": reserved sentinel keys\n";
                        continue;
                    }
                }
#endif
                const auto visit = [&]<typename Q>() {
                    run_case<Q>(options, plan, heap.name, workload.name, n,
                                [](const auto &prepared) { return make_queue<Q>(prepared); });
                };
                if (options.comparisons) visit_queue<T, true>(heap.kind, visit);
                else visit_queue<T, false>(heap.kind, visit);
            }
        }
    }
}

} // namespace

int main(int argc, char **argv) {
    try {
        const auto options = parse(argc, argv);
        if (options.help) { help(); return 0; }
#ifdef QUICKHEAP_BENCH_FIXED_BACKEND
        if (options.backend != QUICKHEAP_BENCH_FIXED_BACKEND)
            throw std::runtime_error("independent executable has a fixed SIMD backend");
#endif
        select_backend(options.backend);
        if (options.list) { list(); return 0; }
        if (!options.fixture.empty()) {
            if (!options.verify_only && !options.scalar_trace) {
                std::cout << paired_csv_header;
                if (options.diagnostics) std::cout << ',' << diagnostics_csv_header;
                std::cout << '\n';
            }
            const auto raw = load_raw_fixture(options.fixture);
            if (raw.elem == "i32") run_fixture(options, typed_fixture<std::int32_t>(raw));
            else if (raw.elem == "i64") run_fixture(options, typed_fixture<std::int64_t>(raw));
            else if (raw.elem == "u32") run_fixture(options, typed_fixture<std::uint32_t>(raw));
            else run_fixture(options, typed_fixture<std::uint64_t>(raw));
            return 0;
        }
        std::cout << (options.memory ? memory_csv_header :
            !options.comparisons && !options.ablation && !options.perf ? paired_csv_header : timing_csv_header) << '\n';
        if (options.types[0]) run_type<std::int32_t>(options);
        if (options.types[1]) run_type<std::int64_t>(options);
        if (options.types[2]) run_type<std::uint32_t>(options);
        if (options.types[3]) run_type<std::uint64_t>(options);
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "quickheap_bench: " << error.what() << '\n';
        return 1;
    }
}
