#include "external_heaps.hpp"

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <limits>
#include <queue>
#include <random>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

template <class T> T random_key(std::mt19937_64& random) {
    if constexpr (std::is_signed_v<T>) {
        return static_cast<T>(static_cast<std::int64_t>(random() % 1000000) - 500000);
    } else {
        return static_cast<T>(1 + random() % 1000000);
    }
}

template <class Queue> void compare_schedule() {
    using T = typename Queue::Item;
    Queue prepared(1U << 17);
    Queue queue(std::move(prepared));
    std::priority_queue<T, std::vector<T>, std::greater<T>> expected;
    std::mt19937_64 random(42);
    REQUIRE_FALSE(queue.try_pop());
    for (unsigned i = 0; i != 20000; ++i) {
        if (expected.empty() || random() % 3 != 0) {
            const T key = random_key<T>(random);
            queue.push(key);
            expected.push(key);
        } else {
            REQUIRE(queue.try_pop() == expected.top());
            expected.pop();
        }
        REQUIRE(queue.size() == expected.size());
    }
    for (unsigned i = 0; i != 50000; ++i) {
        const T key = random_key<T>(random);
        queue.push(key);
        expected.push(key);
    }
    while (!expected.empty()) {
        REQUIRE(queue.try_pop() == expected.top());
        expected.pop();
    }
    REQUIRE(queue.empty());
    REQUIRE(queue.valid());
    REQUIRE_FALSE(queue.try_pop());
}

} // namespace

TEMPLATE_TEST_CASE("External queues preserve minimum and size through mixed schedules",
                   "[benchmark][external]", std::int32_t, std::int64_t,
                   std::uint32_t, std::uint64_t) {
    SECTION("SequenceHeap") { compare_schedule<quickheap::bench::SequenceHeap<TestType>>(); }
    SECTION("S3Q") { compare_schedule<quickheap::bench::S3QHeap<TestType>>(); }
    SECTION("Original Quickheap") { compare_schedule<quickheap::bench::OriginalQuickHeap<TestType>>(); }
}

TEMPLATE_TEST_CASE("External queue boundaries reject unsupported input",
                   "[benchmark][external]", std::int32_t, std::int64_t,
                   std::uint32_t, std::uint64_t) {
    quickheap::bench::SequenceHeap<TestType> sequence;
    quickheap::bench::S3QHeap<TestType> s3q;
    for (const TestType key : {std::numeric_limits<TestType>::lowest(),
                              std::numeric_limits<TestType>::max()}) {
        REQUIRE_THROWS_AS(sequence.push(key), std::domain_error);
        REQUIRE_THROWS_AS(s3q.push(key), std::domain_error);
    }
    REQUIRE(sequence.empty());
    REQUIRE(s3q.empty());
    quickheap::bench::OriginalQuickHeap<TestType> original(8);
    for (unsigned i = 0; i != 8; ++i) original.push(static_cast<TestType>(i));
    REQUIRE_THROWS_AS(original.push(static_cast<TestType>(9)), std::length_error);
    for (unsigned i = 0; i != 8; ++i) REQUIRE(original.try_pop() == static_cast<TestType>(i));
    REQUIRE_THROWS_AS(quickheap::bench::OriginalQuickHeap<TestType>(1U << 30), std::length_error);
}
