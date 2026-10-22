#include "quickheap.hpp"

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <queue>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

enum class Workload {
    HeapSort,
    MonotoneConstantSize,
    RandomConstantSize,
    MonotoneWiggle,
    RandomWiggle,
    GeometricMonotoneWiggle,
    WorstCaseDescending,
    MostlyPushDecreasing,
};

constexpr std::uint8_t kPush = 1;
constexpr std::uint8_t kPop = 0;

template <typename T> struct Script {
    std::vector<std::uint8_t> operations;
    std::vector<T> pushes;
    std::vector<T> expected_pops;
};

template <typename T> struct Plan {
    Script<T> setup;
    Script<T> timed;
    std::size_t setup_size = 0;
    std::size_t final_size = 0;
    std::string error;
};

template <typename T> class PlanBuilder {
  public:
    void timed() {
        plan_.setup_size = reference_.size();
        current_ = &plan_.timed;
    }

    void push(T key) {
        current_->operations.push_back(kPush);
        current_->pushes.push_back(key);
        reference_.push(key);
    }

    bool push_sum(T base, std::uint64_t offset) {
        if (offset > static_cast<std::uint64_t>(std::numeric_limits<T>::max() - base)) {
            plan_.error = "monotone key overflow";
            return false;
        }
        push(static_cast<T>(base + offset));
        return true;
    }

    T pop() {
        if (reference_.empty()) {
            plan_.error = "workload popped an empty heap";
            return T{};
        }
        const T key = reference_.top();
        reference_.pop();
        current_->operations.push_back(kPop);
        current_->expected_pops.push_back(key);
        return key;
    }

    Plan<T> finish() {
        plan_.final_size = reference_.size();
        return std::move(plan_);
    }

  private:
    Plan<T> plan_;
    Script<T> *current_ = &plan_.setup;
    std::priority_queue<T, std::vector<T>, std::greater<T>> reference_;
};

template <typename T> constexpr std::uint64_t stride() {
    return sizeof(T) == 4 ? (std::uint64_t{1} << 24)
                          : (std::uint64_t{1} << 48);
}

template <typename T> Plan<T> make_plan(Workload workload, std::size_t n) {
    PlanBuilder<T> b;
    std::mt19937_64 rng(0x51484C42ULL ^ (static_cast<std::uint64_t>(workload) << 56) ^
                        (static_cast<std::uint64_t>(sizeof(T)) << 48) ^ n);
    const auto offset = [&rng](std::uint64_t bound) {
        return std::uniform_int_distribution<std::uint64_t>(0, bound - 1)(rng);
    };
    const auto full_key = [&rng]() {
        T key = static_cast<T>(rng());
        if (key == std::numeric_limits<T>::min()) ++key;
        if (key == std::numeric_limits<T>::max()) --key;
        return key;
    };

    switch (workload) {
    case Workload::HeapSort:
        b.timed();
        for (std::size_t i = 0; i < n; ++i) b.push(full_key());
        for (std::size_t i = 0; i < n; ++i) b.pop();
        break;

    case Workload::MonotoneConstantSize: {
        T last = 0;
        for (std::size_t i = 0; i < n + 50; ++i) {
            if (!b.push_sum(last, offset(stride<T>()))) return b.finish();
            last = b.pop();
            if (!b.push_sum(last, offset(stride<T>()))) return b.finish();
        }
        b.timed();
        for (std::size_t i = 0; i < 10 * n; ++i) {
            last = b.pop();
            if (!b.push_sum(last, offset(stride<T>()))) return b.finish();
        }
        break;
    }

    case Workload::RandomConstantSize:
        for (std::size_t i = 0; i < n + 50; ++i) {
            b.push(static_cast<T>(offset(1U << 15)));
            b.pop();
            b.push(static_cast<T>(offset(1U << 15)));
        }
        b.timed();
        for (std::size_t i = 0; i < 10 * n; ++i) {
            b.pop();
            b.push(static_cast<T>(offset(1U << 15)));
        }
        break;

    case Workload::MonotoneWiggle:
    case Workload::GeometricMonotoneWiggle: {
        std::geometric_distribution<std::uint64_t> geometric(
            1.0 / static_cast<double>(stride<T>()));
        const auto next_offset = [&]() {
            return workload == Workload::MonotoneWiggle ? offset(stride<T>())
                                                         : geometric(rng);
        };
        T last = 0;
        b.timed();
        for (std::size_t i = 0; i < n; ++i) {
            if (!b.push_sum(last, next_offset())) return b.finish();
            last = b.pop();
            if (!b.push_sum(last, next_offset())) return b.finish();
        }
        for (std::size_t i = 0; i < n; ++i) {
            last = b.pop();
            if (!b.push_sum(last, next_offset())) return b.finish();
            b.pop();
        }
        break;
    }

    case Workload::RandomWiggle:
        b.timed();
        for (std::size_t i = 0; i < n; ++i) {
            b.push(static_cast<T>(offset(stride<T>())));
            b.pop();
            b.push(static_cast<T>(offset(stride<T>())));
        }
        for (std::size_t i = 0; i < n; ++i) {
            b.pop();
            b.push(static_cast<T>(offset(stride<T>())));
            b.pop();
        }
        break;

    case Workload::WorstCaseDescending: {
        constexpr std::size_t k = 16;
        constexpr std::size_t pushes_per_round = k / 2 + 1;
        const std::size_t total_pushes = n * pushes_per_round;
        std::bernoulli_distribution random_key(0.5);
        b.timed();
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j < pushes_per_round; ++j) {
                const auto descending = total_pushes - 1 - i * pushes_per_round - j;
                b.push(random_key(rng) ? static_cast<T>(rng())
                                       : static_cast<T>(descending));
            }
            b.pop();
        }
        for (std::size_t i = 0; i < n * (k / 2); ++i) b.pop();
        break;
    }

    case Workload::MostlyPushDecreasing: {
        std::vector<T> values;
        values.reserve(10 * n);
        for (std::size_t i = 0; i < 10 * n; ++i)
            values.push_back(static_cast<T>(offset(stride<T>())));
        std::sort(values.begin(), values.end(), std::greater<T>());
        std::size_t index = 0;
        b.timed();
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j < 4; ++j) b.push(values[index++]);
            b.pop();
            for (std::size_t j = 0; j < 6; ++j) b.push(values[index++]);
            b.pop();
        }
        for (std::size_t i = 0; i < 8 * n; ++i) b.pop();
        break;
    }
    }
    return b.finish();
}

template <typename T> class KeyQuickHeap {
  public:
    void push(T key) { heap_.push(key); }
    std::optional<T> try_pop() { return heap_.try_pop(); }
    std::size_t size() const { return heap_.size(); }
    bool valid() const { return true; }

  private:
    quickheap::Heap<T> heap_;
};

template <typename T> class KvQuickHeap {
  public:
    void push(T key) { heap_.push(key, static_cast<T>(~key)); }
    std::optional<T> try_pop() {
        auto item = heap_.try_pop();
        if (!item) return std::nullopt;
        if (item->second != static_cast<T>(~item->first)) valid_ = false;
        return item->first;
    }
    std::size_t size() const { return heap_.size(); }
    bool valid() const { return valid_; }

  private:
    quickheap::KvSimdQuickHeap<T> heap_;
    bool valid_ = true;
};

template <typename T> class StdKeyHeap {
  public:
    void push(T key) { heap_.push(key); }
    std::optional<T> try_pop() {
        if (heap_.empty()) return std::nullopt;
        T key = heap_.top();
        heap_.pop();
        return key;
    }
    std::size_t size() const { return heap_.size(); }
    bool valid() const { return true; }

  private:
    std::priority_queue<T, std::vector<T>, std::greater<T>> heap_;
};

template <typename T> class StdKvHeap {
    using Item = std::pair<T, T>;
    struct GreaterKey {
        bool operator()(const Item &a, const Item &b) const {
            return a.first > b.first;
        }
    };

  public:
    void push(T key) { heap_.emplace(key, static_cast<T>(~key)); }
    std::optional<T> try_pop() {
        if (heap_.empty()) return std::nullopt;
        const Item item = heap_.top();
        heap_.pop();
        if (item.second != static_cast<T>(~item.first)) valid_ = false;
        return item.first;
    }
    std::size_t size() const { return heap_.size(); }
    bool valid() const { return valid_; }

  private:
    std::priority_queue<Item, std::vector<Item>, GreaterKey> heap_;
    bool valid_ = true;
};

template <typename Q, typename T>
bool replay(const Script<T> &script, Q &queue, std::vector<T> *outputs) {
    std::size_t push_index = 0;
    std::size_t pop_index = 0;
    for (const std::uint8_t operation : script.operations) {
        if (operation == kPush) {
            queue.push(script.pushes[push_index++]);
        } else {
            const auto key = queue.try_pop();
            if (!key) return false;
            if (outputs) {
                (*outputs)[pop_index] = *key;
            } else if (*key != script.expected_pops[pop_index]) {
                return false;
            }
            ++pop_index;
        }
    }
    return queue.valid() && push_index == script.pushes.size() &&
           pop_index == script.expected_pops.size();
}

template <typename Q, typename T>
void run_benchmark(const std::string &name, const Plan<T> &plan) {
    BENCHMARK_ADVANCED(std::string(name))(Catch::Benchmark::Chronometer meter) {
        struct Run {
            Q queue;
            std::vector<T> outputs;
            bool replay_ok = false;
        };
        std::vector<Run> runs(static_cast<std::size_t>(meter.runs()));
        for (auto &run : runs) {
            run.outputs.resize(plan.timed.expected_pops.size());
            REQUIRE(replay(plan.setup, run.queue, static_cast<std::vector<T> *>(nullptr)));
            REQUIRE(run.queue.size() == plan.setup_size);
        }

        meter.measure([&](int index) {
            auto &run = runs[static_cast<std::size_t>(index)];
            run.replay_ok = replay(plan.timed, run.queue, &run.outputs);
            return run.replay_ok;
        });

        for (const auto &run : runs) {
            REQUIRE(run.replay_ok);
            REQUIRE(run.queue.valid());
            REQUIRE(run.queue.size() == plan.final_size);
            REQUIRE(run.outputs == plan.timed.expected_pops);
        }
    };
}

template <typename T>
void run_workload(const char *name, Workload workload, int max_log2) {
    for (std::size_t n = 1 << 10; n <= (std::size_t{1} << max_log2); n *= 4) {
        DYNAMIC_SECTION(std::to_string(n)) {
            const Plan<T> plan = make_plan<T>(workload, n);
            INFO(plan.error);
            REQUIRE(plan.error.empty());
            const std::string prefix = std::string(name) + "/" +
                                       (sizeof(T) == 4 ? "u32" : "u64") + "/";
            const std::string suffix = "/" + std::to_string(n) +
                " pushes=" + std::to_string(plan.timed.pushes.size()) +
                " pops=" + std::to_string(plan.timed.expected_pops.size());
            run_benchmark<KeyQuickHeap<T>>(prefix + "quickheap" + suffix, plan);
            run_benchmark<StdKeyHeap<T>>(prefix + "std_priority_queue" + suffix, plan);
            run_benchmark<KvQuickHeap<T>>(prefix + "kv_quickheap" + suffix, plan);
            run_benchmark<StdKvHeap<T>>(prefix + "kv_std_priority_queue" + suffix, plan);
        }
    }
}

TEMPLATE_TEST_CASE("HeapSort", "[benchmark]", std::uint32_t, std::uint64_t) {
    run_workload<TestType>("HeapSort", Workload::HeapSort, 20);
}

TEMPLATE_TEST_CASE("MonotoneConstantSize", "[benchmark]", std::uint32_t, std::uint64_t) {
    run_workload<TestType>("MonotoneConstantSize", Workload::MonotoneConstantSize, 20);
}

TEMPLATE_TEST_CASE("RandomConstantSize", "[benchmark]", std::uint32_t, std::uint64_t) {
    run_workload<TestType>("RandomConstantSize", Workload::RandomConstantSize, 20);
}

TEMPLATE_TEST_CASE("MonotoneWiggle", "[benchmark]", std::uint32_t, std::uint64_t) {
    run_workload<TestType>("MonotoneWiggle", Workload::MonotoneWiggle, 18);
}

TEMPLATE_TEST_CASE("RandomWiggle", "[benchmark]", std::uint32_t, std::uint64_t) {
    run_workload<TestType>("RandomWiggle", Workload::RandomWiggle, 18);
}

TEMPLATE_TEST_CASE("GeometricMonotoneWiggle", "[benchmark]", std::uint32_t, std::uint64_t) {
    run_workload<TestType>("GeometricMonotoneWiggle", Workload::GeometricMonotoneWiggle, 18);
}

TEMPLATE_TEST_CASE("WorstCaseDescending", "[benchmark]", std::uint32_t, std::uint64_t) {
    run_workload<TestType>("WorstCaseDescending", Workload::WorstCaseDescending, 20);
}

TEMPLATE_TEST_CASE("MostlyPushDecreasing", "[benchmark]", std::uint32_t, std::uint64_t) {
    run_workload<TestType>("MostlyPushDecreasing", Workload::MostlyPushDecreasing, 20);
}

}
