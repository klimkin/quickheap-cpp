#ifndef QUICKHEAP_BENCH_DIRECT_WORKLOADS_HPP
#define QUICKHEAP_BENCH_DIRECT_WORKLOADS_HPP

#include "fixtures.hpp"

#include <atomic>
#include <concepts>
#include <ostream>

namespace quickheap::bench {

template <typename Q> inline void observe_queue(Q &queue) {
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" : : "g"(&queue) : "memory");
#else
    static_cast<void>(queue);
    std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}

template <typename Function> decltype(auto) dispatch_workload(Workload workload, Function &&function) {
    switch (workload) {
    case Workload::HeapSort:
        return function.template operator()<Workload::HeapSort>();
    case Workload::MonotoneConstantSize:
        return function.template operator()<Workload::MonotoneConstantSize>();
    case Workload::RandomConstantSize:
        return function.template operator()<Workload::RandomConstantSize>();
    case Workload::MonotoneWiggle:
        return function.template operator()<Workload::MonotoneWiggle>();
    case Workload::RandomWiggle:
        return function.template operator()<Workload::RandomWiggle>();
    case Workload::GeometricMonotoneWiggle:
        return function.template operator()<Workload::GeometricMonotoneWiggle>();
    case Workload::WorstCaseDescending:
        return function.template operator()<Workload::WorstCaseDescending>();
    case Workload::MostlyPushDecreasing:
        return function.template operator()<Workload::MostlyPushDecreasing>();
    }
    throw std::runtime_error("unknown direct workload");
}

template <typename Q> typename Q::Item pop_item(Q &queue) {
    if constexpr (requires { { queue.pop() } -> std::same_as<typename Q::Item>; }) {
        return queue.pop();
    } else {
        const auto item = queue.try_pop();
        if (!item) throw std::runtime_error("workload popped an empty heap");
        return *item;
    }
}

template <typename Q> auto pop_key(Q &queue) { return key_of(pop_item(queue)); }

template <bool Checked, typename T> inline T offset_key(T base, std::uint64_t offset) {
    if constexpr (Checked) {
#if defined(__GNUC__) || defined(__clang__)
        T key;
        if (__builtin_add_overflow(base, offset, &key))
            throw std::runtime_error("monotone key overflow; reduce --max or select a wider type");
        return key;
#else
        const auto key = add_positive(base, offset);
        if (!key) throw std::runtime_error("monotone key overflow; reduce --max or select a wider type");
        return *key;
#endif
    } else {
        using U = std::make_unsigned_t<T>;
        return key_from_bits<T>(static_cast<U>(static_cast<U>(base) + static_cast<U>(offset)));
    }
}

template <bool Checked = false, typename T, typename Q>
void setup_workload(const Fixture<T> &fixture, Q &queue) {
    if (fixture.workload == Workload::MonotoneConstantSize) {
        T last = 0;
        std::size_t input = 0;
        for (std::size_t i = 0; i < fixture.n + 50; ++i) {
            queue.push(offset_key<Checked>(last, fixture.setup_offsets[input++]));
            last = pop_key(queue);
            queue.push(offset_key<Checked>(last, fixture.setup_offsets[input++]));
        }
    } else if (fixture.workload == Workload::RandomConstantSize) {
        std::size_t input = 0;
        for (std::size_t i = 0; i < fixture.n + 50; ++i) {
            queue.push(fixture.setup_keys[input++]);
            pop_key(queue);
            queue.push(fixture.setup_keys[input++]);
        }
    }
}

template <Workload W, bool Checked = false, typename T, typename Q>
void execute_workload(const Fixture<T> &fixture, Q &queue) {
    const std::size_t n = fixture.n;
    if constexpr (W == Workload::HeapSort) {
        for (T key : fixture.keys) queue.push(key);
        for (std::size_t i = 0; i < n; ++i) pop_key(queue);
    } else if constexpr (W == Workload::MonotoneConstantSize) {
        for (std::size_t i = 0; i < 10 * n; ++i) {
            const T last = pop_key(queue);
            queue.push(offset_key<Checked>(last, fixture.offsets[i]));
        }
    } else if constexpr (W == Workload::RandomConstantSize) {
        for (std::size_t i = 0; i < 10 * n; ++i) {
            pop_key(queue);
            queue.push(fixture.keys[i]);
        }
    } else if constexpr (W == Workload::MonotoneWiggle ||
                         W == Workload::GeometricMonotoneWiggle) {
        T last = 0;
        std::size_t input = 0;
        for (std::size_t i = 0; i < n; ++i) {
            queue.push(offset_key<Checked>(last, fixture.offsets[input++]));
            last = pop_key(queue);
            queue.push(offset_key<Checked>(last, fixture.offsets[input++]));
        }
        for (std::size_t i = 0; i < n; ++i) {
            last = pop_key(queue);
            queue.push(offset_key<Checked>(last, fixture.offsets[input++]));
            pop_key(queue);
        }
    } else if constexpr (W == Workload::RandomWiggle) {
        std::size_t input = 0;
        for (std::size_t i = 0; i < n; ++i) {
            queue.push(fixture.keys[input++]);
            pop_key(queue);
            queue.push(fixture.keys[input++]);
        }
        for (std::size_t i = 0; i < n; ++i) {
            pop_key(queue);
            queue.push(fixture.keys[input++]);
            pop_key(queue);
        }
    } else if constexpr (W == Workload::WorstCaseDescending) {
        std::size_t input = 0;
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j < 9; ++j) queue.push(fixture.keys[input++]);
            pop_key(queue);
        }
        for (std::size_t i = 0; i < 8 * n; ++i) pop_key(queue);
    } else if constexpr (W == Workload::MostlyPushDecreasing) {
        std::size_t input = 0;
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j < 4; ++j) queue.push(fixture.keys[input++]);
            pop_key(queue);
            for (std::size_t j = 0; j < 6; ++j) queue.push(fixture.keys[input++]);
            pop_key(queue);
        }
        for (std::size_t i = 0; i < 8 * n; ++i) pop_key(queue);
    }
}

template <typename T, typename Q> class ValidationQueue {
  public:
    using Item = typename Q::Item;

    ValidationQueue(Q &queue, std::ostream *trace) : queue_(queue), trace_(trace) {}

    void push(T key) {
        queue_.push(key);
        reference_.push(key);
        record("push", key);
    }

    std::optional<Item> try_pop() {
        const auto expected = reference_.try_pop();
        if (!expected || queue_.size() == 0)
            throw std::runtime_error("heap popped key or payload disagrees with fixture");
        const auto item = pop_item(queue_);
        if (key_of(item) != *expected || !valid_payload(item))
            throw std::runtime_error("heap popped key or payload disagrees with fixture");
        record("pop", key_of(item));
        return item;
    }

    void finish(std::size_t expected_size) {
        if (queue_.size() != expected_size || reference_.size() != expected_size)
            throw std::runtime_error("heap final size disagrees with fixture");
        if (trace_) *trace_ << "final," << queue_.size() << '\n';
        while (const auto expected = reference_.try_pop()) {
            if (queue_.size() == 0)
                throw std::runtime_error("heap retained key or payload disagrees with fixture");
            const auto item = pop_item(queue_);
            if (key_of(item) != *expected || !valid_payload(item))
                throw std::runtime_error("heap retained key or payload disagrees with fixture");
            record("drain", key_of(item));
        }
        if (queue_.size() != 0 || queue_.try_pop())
            throw std::runtime_error("heap retained unexpected final keys");
    }

  private:
    void record(std::string_view operation, T key) {
        if (trace_) *trace_ << operation << ',' << static_cast<std::make_unsigned_t<T>>(key) << '\n';
    }

    Q &queue_;
    StdKeyHeap<T> reference_;
    std::ostream *trace_;
};

template <typename T, typename Factory>
void validate_fixture(const Fixture<T> &fixture, Factory &&factory, std::ostream *trace = nullptr) {
    const auto shape = fixture_shape(fixture.workload, fixture.n);
    if (shape.offsets) {
        if (!fixture.setup_keys.empty() || !fixture.keys.empty() ||
            fixture.setup_offsets.size() != shape.setup_count || fixture.offsets.size() != shape.timed_count)
            throw std::runtime_error("wrong offset fixture region length");
    } else if (!fixture.setup_offsets.empty() || !fixture.offsets.empty() ||
               fixture.setup_keys.size() != shape.setup_count || fixture.keys.size() != shape.timed_count) {
        throw std::runtime_error("wrong key fixture region length");
    }
    auto queue = factory();
    ValidationQueue<T, decltype(queue)> validation(queue, trace);
    setup_workload<true>(fixture, validation);
    dispatch_workload(fixture.workload, [&]<Workload W>() { execute_workload<W, true>(fixture, validation); });
    const bool retains_setup = fixture.workload == Workload::MonotoneConstantSize ||
                               fixture.workload == Workload::RandomConstantSize;
    validation.finish(retains_setup ? fixture.n + 50 : 0);
}

template <typename T, typename Factory, typename Clock>
auto direct_trial(const Fixture<T> &fixture, Factory &&factory, Clock &&clock) {
    return dispatch_workload(fixture.workload, [&]<Workload W>() {
        auto queue = factory();
        setup_workload(fixture, queue);
        observe_queue(queue);
        const auto start = clock();
        execute_workload<W>(fixture, queue);
        observe_queue(queue);
        const auto stop = clock();
        return stop - start;
    });
}

} // namespace quickheap::bench

#endif
