#ifndef QUICKHEAP_BENCH_ORIGINAL_PROFILE_HPP
#define QUICKHEAP_BENCH_ORIGINAL_PROFILE_HPP

#include "direct_workloads.hpp"

namespace quickheap::bench {

inline constexpr std::string_view original_protocol = "quickheap-original-rust-a9f2518-v1";
inline constexpr std::size_t original_std_capacity = std::size_t{1} << 20;

template <typename T, typename Rng>
Fixture<T> generate_original_fixture(Workload workload, std::size_t n, Rng &rng) {
    if (workload != Workload::HeapSort && workload != Workload::MonotoneConstantSize &&
        workload != Workload::MonotoneWiggle && workload != Workload::RandomWiggle)
        throw std::runtime_error("workload is not active in original Rust profile");
    const auto shape = fixture_shape(workload, n);
    Fixture<T> input{workload, n, {}, {}, {}, {}, {}};
    std::uniform_int_distribution<std::uint64_t> bounded(0, stride<T>() - 1);
    std::uniform_int_distribution<std::uint64_t> full;
    const auto next_key = [&] {
        if (workload == Workload::RandomWiggle) return static_cast<T>(bounded(rng));
        T key = key_from_bits<T>(static_cast<std::make_unsigned_t<T>>(full(rng)));
        if (workload == Workload::HeapSort) {
            if (key == std::numeric_limits<T>::min()) ++key;
            if (key == std::numeric_limits<T>::max()) --key;
        }
        return key;
    };
    if (shape.offsets) {
        input.setup_offsets.reserve(shape.setup_count);
        input.offsets.reserve(shape.timed_count);
        for (std::size_t i = 0; i < shape.setup_count; ++i)
            input.setup_offsets.push_back(bounded(rng));
        for (std::size_t i = 0; i < shape.timed_count; ++i)
            input.offsets.push_back(bounded(rng));
    } else {
        input.setup_keys.reserve(shape.setup_count);
        input.keys.reserve(shape.timed_count);
        for (std::size_t i = 0; i < shape.setup_count; ++i)
            input.setup_keys.push_back(next_key());
        for (std::size_t i = 0; i < shape.timed_count; ++i)
            input.keys.push_back(next_key());
    }
    input.digest = fixture_digest(encode_fixture(input));
    return input;
}

template <Workload W, typename T, typename Q>
void execute_original_workload(Fixture<T> &input, Q &queue) {
    if constexpr (W == Workload::HeapSort) {
        for (T key : input.keys) queue.push(key);
        std::vector<T>().swap(input.keys);
        for (std::size_t i = 0; i < input.n; ++i) pop_key(queue);
    } else {
        execute_workload<W, true>(input, queue);
        std::vector<T>().swap(input.keys);
        std::vector<std::uint64_t>().swap(input.offsets);
    }
}

template <typename T, typename Factory, typename Clock>
auto original_trial(Fixture<T> input, Factory &&factory, Clock &&clock) {
    return dispatch_workload(input.workload, [&]<Workload W>() {
        decltype(clock()) start;
        {
            auto queue = factory();
            auto owned_input = std::move(input);
            setup_workload<true>(owned_input, queue);
            std::vector<T>().swap(owned_input.setup_keys);
            std::vector<std::uint64_t>().swap(owned_input.setup_offsets);
            observe_queue(queue);
            start = clock();
            execute_original_workload<W>(owned_input, queue);
            observe_queue(queue);
        }
        const auto stop = clock();
        return stop - start;
    });
}

}

#endif
