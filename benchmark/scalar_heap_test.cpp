#include "scalar_heap.hpp"

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <queue>
#include <random>
#include <tuple>
#include <vector>

namespace quickheap::bench {
namespace {

template <class T>
T scalar_key_bits(std::uint64_t bits) {
    using U = std::make_unsigned_t<T>;
    return std::bit_cast<T>(static_cast<U>(bits));
}

template <class Heap>
void check_scalar_order() {
    using T = typename Heap::Item;
    Heap heap;
    std::priority_queue<T, std::vector<T>, std::greater<T>> reference;
    std::mt19937_64 random(991);
    REQUIRE_FALSE(heap.try_pop());
    for (std::size_t i = 0; i < 2048; ++i) {
        const auto key = i % 5 == 0 ? std::numeric_limits<T>::min()
            : i % 5 == 1 ? std::numeric_limits<T>::max()
            : i % 5 == 2 ? T{} : scalar_key_bits<T>(random());
        heap.push(key);
        reference.push(key);
    }
    for (std::size_t i = 0; i < 4096; ++i) {
        if (i % 3 != 0) {
            REQUIRE(heap.try_pop() == reference.top());
            reference.pop();
        } else {
            const auto key = i % 7 == 0 ? T{} : scalar_key_bits<T>(random());
            heap.push(key);
            reference.push(key);
        }
        REQUIRE(heap.size() == reference.size());
        REQUIRE(heap.valid());
    }
    while (!reference.empty()) {
        REQUIRE(heap.try_pop() == reference.top());
        reference.pop();
    }
    REQUIRE(heap.valid());
    REQUIRE(heap.empty());
    REQUIRE_FALSE(heap.try_pop());
    REQUIRE(heap.counters().partitions > 0);
    REQUIRE(heap.counters().elements_partitioned >= heap.counters().partitions * 2);
    REQUIRE(heap.counters().comparisons_exact);
}

TEMPLATE_TEST_CASE("paired scalar configurations preserve queue order", "[scalar-contract]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    check_scalar_order<ScalarHeap<TestType, 1, false, false, true>>();
    check_scalar_order<ScalarHeap<TestType, 1, false, true, true>>();
    check_scalar_order<ScalarHeap<TestType, 3, false, false, true>>();
    check_scalar_order<ScalarHeap<TestType, 3, false, true, true>>();
    check_scalar_order<ScalarHeap<TestType, 1, true, false, true>>();
    check_scalar_order<ScalarHeap<TestType, 1, true, true, true>>();
}

TEMPLATE_TEST_CASE("branchless sort matches std::sort", "[scalar-contract]",
                   std::int8_t, std::uint16_t, std::int64_t, std::uint64_t) {
    std::mt19937_64 random(7);
    for (const std::size_t n : {0u, 1u, 2u, 20u, 21u, 63u, 64u, 65u, 1000u, 20000u}) {
        for (const auto shape : {0, 1, 2, 3, 4}) {
            std::vector<TestType> keys(n);
            for (std::size_t i = 0; i < n; ++i) {
                keys[i] = shape == 0 ? scalar_key_bits<TestType>(random())
                    : shape == 1 ? static_cast<TestType>(random() % 3)
                    : shape == 2 ? static_cast<TestType>(i)
                    : shape == 3 ? static_cast<TestType>(n - i)
                    : static_cast<TestType>(i % 17);
            }
            auto expected = keys;
            std::sort(expected.begin(), expected.end());
            branchless_sort(keys.data(), keys.size());
            REQUIRE(keys == expected);
        }
    }
}

TEST_CASE("scripted duplicate pivot defines singleton split counts", "[scalar-contract]") {
    constexpr std::array<std::uint64_t, 1> script = {0};
    ScalarHeap<std::int64_t, 1, false, false, true> heap(script);
    for (auto key : {4, 4, 4, 4}) heap.push(key);
    std::size_t events = 0;
    REQUIRE(heap.try_pop([&](const auto& event) {
        ++events;
        CHECK(event.pivot == 4);
        CHECK(event.pivot_index == 0);
        CHECK_FALSE(event.retry);
        CHECK(std::vector<std::int64_t>(event.upper.begin(), event.upper.end()) ==
              std::vector<std::int64_t>{4, 4, 4});
        CHECK(std::vector<std::int64_t>(event.lower.begin(), event.lower.end()) ==
              std::vector<std::int64_t>{4});
        CHECK(event.pop_comparisons == 4);
    }) == 4);
    REQUIRE(events == 1);
    CHECK(heap.push_comparisons() == 0);
    CHECK(heap.pop_comparisons() == 4);
    CHECK(heap.counters().samples == 1);
    CHECK(heap.counters().partitions == 1);
    CHECK(heap.counters().elements_partitioned == 4);
    CHECK(heap.counters().retries == 0);
    CHECK(heap.valid());
}

TEST_CASE("empty upper retries retain reordered lower and count work", "[scalar-contract]") {
    constexpr std::array<std::uint64_t, 2> script = {3, 0};
    ScalarHeap<std::int64_t, 1, false, false, true> heap(script);
    for (auto key : {4, 4, 4, 4}) heap.push(key);
    std::size_t events = 0;
    REQUIRE(heap.try_pop([&](const auto& event) {
        CHECK(event.pivot == 4);
        CHECK(event.pop_comparisons == 4 * (events + 1));
        if (events++ == 0) {
            CHECK(event.pivot_index == 3);
            CHECK(event.retry);
            CHECK(event.upper.empty());
            CHECK(std::vector<std::int64_t>(event.lower.begin(), event.lower.end()) ==
                  std::vector<std::int64_t>{4, 4, 4, 4});
        } else {
            CHECK(event.pivot_index == 0);
            CHECK_FALSE(event.retry);
            CHECK(event.upper.size() == 3);
            CHECK(event.lower.size() == 1);
        }
    }) == 4);
    CHECK(events == 2);
    CHECK(heap.pop_comparisons() == 8);
    CHECK(heap.counters().partitions == 2);
    CHECK(heap.counters().elements_partitioned == 8);
    CHECK(heap.counters().retries == 1);
    CHECK(heap.counters().samples == 2);
    CHECK(heap.valid());
}

TEST_CASE("median samples count one key comparison with index ties", "[scalar-contract]") {
    constexpr std::array<std::uint64_t, 6> script = {2, 2, 0, 0, 0, 0};
    ScalarHeap<std::int64_t, 3, false, false, true> heap(script);
    for (auto key : {4, 4, 4, 4}) heap.push(key);
    std::size_t events = 0;
    REQUIRE(heap.try_pop([&](const auto& event) {
        if (events++ == 0) {
            CHECK(event.pivot_index == 2);
            CHECK(event.upper.size() == 1);
            CHECK(event.lower.size() == 3);
            CHECK(event.pop_comparisons == 7);
        } else {
            CHECK(event.pivot_index == 0);
            CHECK(event.pop_comparisons == 12);
        }
    }) == 4);
    CHECK(events == 2);
    CHECK(heap.counters().samples == 6);
    CHECK(heap.pop_comparisons() == 12);
}

TEST_CASE("retry preserves distinct key order for next scripted pivot", "[scalar-contract]") {
    constexpr std::array<std::uint64_t, 2> script = {3, 0};
    ScalarHeap<std::int64_t, 1, false, false, true> heap(script);
    for (auto key : {1, 1, 2, 3}) heap.push(key);
    std::size_t events = 0;
    REQUIRE(heap.try_pop([&](const auto& event) {
        if (events++ == 0) {
            CHECK(event.pivot == 3);
            CHECK(event.retry);
            CHECK(event.upper.empty());
            CHECK(std::vector<std::int64_t>(event.lower.begin(), event.lower.end()) ==
                  std::vector<std::int64_t>{1, 1, 2, 3});
        } else {
            CHECK(event.pivot == 1);
            CHECK_FALSE(event.retry);
            CHECK(std::vector<std::int64_t>(event.upper.begin(), event.upper.end()) ==
                  std::vector<std::int64_t>{1, 2, 3});
            CHECK(std::vector<std::int64_t>(event.lower.begin(), event.lower.end()) ==
                  std::vector<std::int64_t>{1});
        }
    }) == 1);
    CHECK(events == 2);
    CHECK(heap.pop_comparisons() == 8);
    CHECK(heap.counters().retries == 1);
    CHECK(heap.counters().samples == 2);
}

TEST_CASE("perfect pivot oracle excludes selection comparisons", "[scalar-contract]") {
    ScalarHeap<std::int64_t, 1, true, false, true> heap;
    for (auto key : {3, 0, 2, 1}) heap.push(key);
    std::size_t events = 0;
    REQUIRE(heap.try_pop([&](const auto& event) {
        if (events++ == 0) {
            CHECK(event.pivot == 1);
            CHECK(event.pivot_index == 1);
            CHECK(std::vector<std::int64_t>(event.upper.begin(), event.upper.end()) ==
                  std::vector<std::int64_t>{2, 3});
            CHECK(std::vector<std::int64_t>(event.lower.begin(), event.lower.end()) ==
                  std::vector<std::int64_t>{0, 1});
            CHECK(event.pop_comparisons == 4);
        }
    }) == 0);
    CHECK(events == 2);
    CHECK(heap.pop_comparisons() == 6);
    CHECK(heap.counters().samples == 0);
    CHECK(heap.counters().retries == 0);
}

template <class T, bool Linear>
void check_perfect_split_trace() {
    ScalarHeap<T, 1, true, Linear, true> counted;
    ScalarHeap<T, 1, true, Linear, false> uncounted;
    ScalarHeap<T, 1, true, Linear, false> unobserved;
    std::mt19937_64 random(42);
    using Event = std::tuple<T, std::size_t, bool, std::vector<T>, std::vector<T>>;
    const auto push = [&](T key) {
        counted.push(key);
        uncounted.push(key);
        unobserved.push(key);
    };
    const auto pop = [&] {
        std::vector<Event> expected;
        std::vector<Event> actual;
        const auto capture = [](auto& events) {
            return [&](const auto& event) {
                events.emplace_back(event.pivot, event.pivot_index, event.retry,
                    std::vector<T>(event.upper.begin(), event.upper.end()),
                    std::vector<T>(event.lower.begin(), event.lower.end()));
            };
        };
        const auto key = counted.try_pop(capture(expected));
        REQUIRE(uncounted.try_pop(capture(actual)) == key);
        REQUIRE(unobserved.try_pop() == key);
        REQUIRE(actual == expected);
        REQUIRE(uncounted.valid());
        REQUIRE(uncounted.size() == counted.size());
        REQUIRE(std::equal(uncounted.pivots().begin(), uncounted.pivots().end(),
                           counted.pivots().begin(), counted.pivots().end()));
        REQUIRE(std::equal(uncounted.buckets().begin(), uncounted.buckets().end(),
                           counted.buckets().begin(), counted.buckets().end()));
        REQUIRE(std::equal(unobserved.pivots().begin(), unobserved.pivots().end(),
                           counted.pivots().begin(), counted.pivots().end()));
        REQUIRE(std::equal(unobserved.buckets().begin(), unobserved.buckets().end(),
                           counted.buckets().begin(), counted.buckets().end()));
    };
    for (std::size_t i = 0; i < 256; ++i) {
        push(i % 3 == 0 ? std::numeric_limits<T>::min()
            : i % 3 == 1 ? std::numeric_limits<T>::max() : T{});
    }
    for (std::size_t i = 0; i < 1024; ++i) {
        pop();
        push(i % 5 == 0 ? T{} : scalar_key_bits<T>(random()));
    }
    while (!counted.empty()) pop();
    CHECK(uncounted.counters().comparisons() == 0);
}

TEMPLATE_TEST_CASE("uncounted perfect splits match the counted oracle", "[scalar-contract]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    check_perfect_split_trace<TestType, false>();
    check_perfect_split_trace<TestType, true>();
}

template <bool Linear>
void check_equality_routing() {
    constexpr std::array<std::uint64_t, 3> script = {2, 1, 0};
    ScalarHeap<std::int64_t, 1, false, Linear, true> heap(script);
    for (auto key : {0, 1, 2, 3}) heap.push(key);
    REQUIRE(heap.try_pop() == 0);
    REQUIRE(std::vector<std::int64_t>(heap.pivots().begin(), heap.pivots().end()) ==
            std::vector<std::int64_t>{2, 1});
    heap.reset_comparisons();
    heap.push(2);
    CHECK(heap.push_comparisons() == 2);
    REQUIRE(heap.buckets()[1] == std::vector<std::int64_t>{2, 2});
    heap.push(1);
    CHECK(heap.push_comparisons() == (Linear ? 4 : 3));
    REQUIRE(heap.buckets()[2] == std::vector<std::int64_t>{1, 1});
    CHECK(heap.pop_comparisons() == 0);
    CHECK(heap.valid());
}

TEST_CASE("both scalar searches route equality below pivot", "[scalar-contract]") {
    check_equality_routing<false>();
    check_equality_routing<true>();
}

TEST_CASE("uncounted scalar queue preserves trace without counter writes", "[scalar-contract]") {
    constexpr std::array<std::uint64_t, 1> script = {0};
    ScalarHeap<std::int64_t> heap(script);
    heap.reserve(16);
    const auto initial_capacity = heap.capacity();
    for (auto key : {1, 1}) heap.push(key);
    std::size_t events = 0;
    REQUIRE(heap.try_pop([&](const auto& event) {
        ++events;
        CHECK(event.pop_comparisons == 0);
    }) == 1);
    CHECK(events == 1);
    CHECK(heap.counters().comparisons() == 0);
    CHECK(heap.counters().partitions == 0);
    CHECK_FALSE(heap.counters().comparisons_exact);
    REQUIRE(heap.try_pop() == 1);
    CHECK(heap.capacity() >= initial_capacity);
    CHECK(heap.valid());
    heap.push(2);
    REQUIRE(heap.try_pop() == 2);
    CHECK(heap.valid());
}

TEST_CASE("invalid scalar sample fails at diagnostic boundary", "[scalar-contract]") {
    constexpr std::array<std::uint64_t, 1> script = {2};
    ScalarHeap<std::int64_t> heap(script);
    heap.push(1);
    heap.push(2);
    CHECK_THROWS_AS(heap.try_pop(), std::invalid_argument);
    CHECK(heap.valid());
}

}
}
