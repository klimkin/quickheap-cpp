#ifndef QUICKHEAP_BENCH_WORKLOADS_HPP
#define QUICKHEAP_BENCH_WORKLOADS_HPP

#include "quickheap.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <queue>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace quickheap::bench {

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

struct WorkloadInfo {
    Workload workload;
    std::string_view name;
};
inline constexpr std::array workloads = {
    WorkloadInfo{Workload::HeapSort, "HeapSort"},
    WorkloadInfo{Workload::MonotoneConstantSize, "MonotoneConstantSize"},
    WorkloadInfo{Workload::RandomConstantSize, "RandomConstantSize"},
    WorkloadInfo{Workload::MonotoneWiggle, "MonotoneWiggle"},
    WorkloadInfo{Workload::RandomWiggle, "RandomWiggle"},
    WorkloadInfo{Workload::GeometricMonotoneWiggle, "GeometricMonotoneWiggle"},
    WorkloadInfo{Workload::WorstCaseDescending, "WorstCaseDescending"},
    WorkloadInfo{Workload::MostlyPushDecreasing, "MostlyPushDecreasing"},
};

template <typename T> constexpr std::string_view key_name() {
    if constexpr (std::is_same_v<T, std::int32_t>) return "i32";
    if constexpr (std::is_same_v<T, std::int64_t>) return "i64";
    if constexpr (std::is_same_v<T, std::uint32_t>) return "u32";
    return "u64";
}

template <typename T> constexpr T key_from_bits(std::make_unsigned_t<T> bits) {
    if constexpr (std::is_unsigned_v<T>) {
        return bits;
    } else {
        using U = std::make_unsigned_t<T>;
        constexpr U sign = U{1} << (std::numeric_limits<U>::digits - 1);
        if (bits < sign) return static_cast<T>(bits);
        return static_cast<T>(std::numeric_limits<T>::min() + static_cast<T>(bits - sign));
    }
}

template <typename T> constexpr std::optional<T> add_positive(T base, std::uint64_t offset) {
    using U = std::make_unsigned_t<T>;
    const U room = static_cast<U>(std::numeric_limits<T>::max()) - static_cast<U>(base);
    if (offset > static_cast<std::uint64_t>(room)) return std::nullopt;
    return key_from_bits<T>(static_cast<U>(static_cast<U>(base) + static_cast<U>(offset)));
}

template <typename T> constexpr T payload(T key) {
    using U = std::make_unsigned_t<T>;
    return key_from_bits<T>(static_cast<U>(~static_cast<U>(key)));
}

template <typename T> constexpr T key_of(T item) { return item; }
template <typename T> constexpr T key_of(const std::pair<T, T> &item) { return item.first; }
template <typename T> constexpr bool valid_payload(T) { return true; }
template <typename T> constexpr bool valid_payload(const std::pair<T, T> &item) {
    return item.second == payload(item.first);
}

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
    std::size_t peak_size = 0;
    std::size_t final_size = 0;
    std::vector<T> expected_final;
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
        plan_.peak_size = std::max(plan_.peak_size, reference_.size());
    }

    bool push_sum(T base, std::uint64_t offset) {
        const auto key = add_positive(base, offset);
        if (!key) {
            plan_.error = "monotone key overflow; reduce --max or select a wider type";
            return false;
        }
        push(*key);
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
        while (!reference_.empty()) {
            plan_.expected_final.push_back(reference_.top());
            reference_.pop();
        }
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
    if (n == 0 || n > (std::size_t{1} << 25)) {
        Plan<T> plan;
        plan.error = "workload size must be in [1, 2^25]";
        return plan;
    }
    std::mt19937_64 rng(0x51484C42ULL ^ (static_cast<std::uint64_t>(workload) << 56) ^
                        (static_cast<std::uint64_t>(sizeof(T)) << 48) ^ n);
    const auto offset = [&rng](std::uint64_t bound) {
        return std::uniform_int_distribution<std::uint64_t>(0, bound - 1)(rng);
    };
    const auto full_key = [&rng]() {
        T key = key_from_bits<T>(static_cast<std::make_unsigned_t<T>>(rng()));
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
                b.push(random_key(rng) ? key_from_bits<T>(static_cast<std::make_unsigned_t<T>>(rng()))
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
    using Item = T;
    void push(T key) { heap_.push(key); }
    T pop() { return heap_.pop(); }
    std::optional<T> try_pop() { return heap_.try_pop(); }
    std::size_t size() const { return heap_.size(); }
    std::size_t capacity() const { return heap_.capacity(); }

  private:
    quickheap::Heap<T> heap_;
};

template <typename T> class KvQuickHeap {
  public:
    using Item = std::pair<T, T>;
    void push(T key) { heap_.push(key, payload(key)); }
    std::optional<Item> try_pop() { return heap_.try_pop(); }
    std::size_t size() const { return heap_.size(); }
    std::size_t capacity() const { return heap_.capacity(); }

  private:
    quickheap::KvSimdQuickHeap<T> heap_;
};

template <typename T> class StdKeyHeap {
  public:
    using Item = T;
    StdKeyHeap() = default;
    explicit StdKeyHeap(std::size_t capacity) : heap_(std::greater<T>{}, reserved(capacity)) {}
    void push(T key) { heap_.push(key); }
    T pop() {
        if (heap_.empty()) throw std::runtime_error("workload popped an empty heap");
        T key = heap_.top();
        heap_.pop();
        return key;
    }
    std::optional<T> try_pop() {
        if (heap_.empty()) return std::nullopt;
        return pop();
    }
    std::size_t size() const { return heap_.size(); }

  private:
    static std::vector<T> reserved(std::size_t capacity) {
        std::vector<T> keys;
        keys.reserve(capacity);
        return keys;
    }
    std::priority_queue<T, std::vector<T>, std::greater<T>> heap_;
};

template <typename T> class StdKvHeap {
  public:
    using Item = std::pair<T, T>;
  private:
    struct GreaterKey {
        bool operator()(const Item &a, const Item &b) const {
            return a.first > b.first;
        }
    };

  public:
    void push(T key) { heap_.emplace(key, payload(key)); }
    std::optional<Item> try_pop() {
        if (heap_.empty()) return std::nullopt;
        const Item item = heap_.top();
        heap_.pop();
        return item;
    }
    std::size_t size() const { return heap_.size(); }

  private:
    std::priority_queue<Item, std::vector<Item>, GreaterKey> heap_;
};

template <typename Q, typename T>
bool replay(const Script<T> &script, Q &queue, std::vector<typename Q::Item> *outputs) {
    std::size_t push_index = 0;
    std::size_t pop_index = 0;
    for (const std::uint8_t operation : script.operations) {
        if (operation == kPush) {
            queue.push(script.pushes[push_index++]);
        } else {
            const auto item = queue.try_pop();
            if (!item) return false;
            if (outputs) {
                (*outputs)[pop_index] = *item;
            } else if (key_of(*item) != script.expected_pops[pop_index] || !valid_payload(*item)) {
                return false;
            }
            ++pop_index;
        }
    }
    return push_index == script.pushes.size() && pop_index == script.expected_pops.size();
}

template <typename Q, typename T> struct Run {
    Q queue;
    std::vector<typename Q::Item> outputs;
    bool replay_ok = false;

    void execute(const Plan<T> &plan) { replay_ok = replay(plan.timed, queue, &outputs); }
};

template <typename Q, typename T> Run<Q, T> prepare(const Plan<T> &plan, Q queue = {}) {
    if (!plan.error.empty()) throw std::runtime_error(plan.error);
    Run<Q, T> run{std::move(queue), std::vector<typename Q::Item>(plan.timed.expected_pops.size()), false};
    if (!replay(plan.setup, run.queue, static_cast<std::vector<typename Q::Item> *>(nullptr)) ||
        run.queue.size() != plan.setup_size) {
        throw std::runtime_error("heap disagrees with workload setup");
    }
    return run;
}

template <typename Q, typename T> void verify(const Plan<T> &plan, Run<Q, T> &run) {
    if (!run.replay_ok || run.queue.size() != plan.final_size) {
        throw std::runtime_error("heap replay or final size disagrees with workload");
    }
    for (std::size_t i = 0; i < run.outputs.size(); ++i) {
        if (key_of(run.outputs[i]) != plan.timed.expected_pops[i] || !valid_payload(run.outputs[i])) {
            throw std::runtime_error("heap popped key or payload disagrees with workload");
        }
    }
    for (const T expected : plan.expected_final) {
        const auto item = run.queue.try_pop();
        if (!item || key_of(*item) != expected || !valid_payload(*item)) {
            throw std::runtime_error("heap final key or payload disagrees with workload");
        }
    }
    if (run.queue.size() != 0) throw std::runtime_error("heap retained unexpected final keys");
}

} // namespace quickheap::bench

#endif
