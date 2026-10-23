#pragma once

#include "direct_workloads.hpp"
#include "scalar_heap.hpp"

namespace quickheap::bench {

template <class Keys> void trace_keys(std::ostream &out, const Keys &keys) {
    bool separator = false;
    for (const auto key : keys) {
        if (separator) out << ':';
        out << static_cast<std::make_unsigned_t<decltype(key)>>(key);
        separator = true;
    }
}

template <class Q> class ScalarTraceQueue {
public:
    using Item = typename Q::Item;
    Q queue;
    std::ostream &out;

    ScalarTraceQueue(std::ostream &stream, std::span<const std::uint64_t> samples)
        : queue(samples), out(stream) {}

    void push(Item key) {
        queue.push(key);
        operation("push", key);
    }

    std::optional<Item> try_pop() {
        const auto item = queue.try_pop([&](const auto &event) {
            out << "partition," << static_cast<std::make_unsigned_t<Item>>(event.pivot) << ','
                << event.pivot_index << ',' << event.retry << ',' << event.push_comparisons << ','
                << event.pop_comparisons << ',';
            trace_keys(out, event.upper);
            out << ',';
            trace_keys(out, event.lower);
            out << '\n';
        });
        if (item) operation("pop", *item);
        return item;
    }

    std::size_t size() const { return queue.size(); }

private:
    void operation(std::string_view name, Item key) {
        out << name << ',' << static_cast<std::make_unsigned_t<Item>>(key) << ','
            << queue.push_comparisons() << ',' << queue.pop_comparisons() << ',';
        trace_keys(out, queue.pivots());
        out << ',';
        bool separator = false;
        for (const auto &bucket : queue.buckets()) {
            if (separator) out << ';';
            trace_keys(out, bucket);
            separator = true;
        }
        out << '\n';
    }
};

template <class Q, class T>
void scalar_trace(const Fixture<T> &fixture, std::ostream &out, std::span<const std::uint64_t> samples) {
    ScalarTraceQueue<Q> traced(out, samples);
    setup_workload(fixture, traced);
    traced.queue.reset_comparisons();
    out << "reset,0,0\n";
    dispatch_workload(fixture.workload, [&]<Workload W>() { execute_workload<W>(fixture, traced); });
    out << "final," << traced.size() << ',' << traced.queue.push_comparisons() << ','
        << traced.queue.pop_comparisons() << '\n';
    while (traced.try_pop()) {}
}

}
