#include "competitor_heaps.hpp"

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <queue>
#include <random>
#include <type_traits>
#include <vector>

namespace {
using namespace quickheap::bench;

template <typename T> T random_key(std::mt19937_64 &rng) {
    const auto bits = static_cast<std::make_unsigned_t<T>>(rng());
    if constexpr (std::is_signed_v<T>) return std::bit_cast<T>(bits);
    else return bits;
}

template <typename Heap, typename T> void check_mixed() {
    Heap heap;
    std::priority_queue<T, std::vector<T>, std::greater<T>> reference;
    std::mt19937_64 rng(0x5148434f4d504554ULL);
    REQUIRE(heap.try_pop() == std::nullopt);
    for (std::size_t i = 0; i < 12000; ++i) {
        if (reference.empty() || rng() % 5 < 3) {
            const auto key = i % 17 == 0 ? std::numeric_limits<T>::min()
                : i % 19 == 0 ? std::numeric_limits<T>::max()
                : i % 3 == 0 ? static_cast<T>(rng() % 9)
                : random_key<T>(rng);
            heap.push(key);
            reference.push(key);
        } else {
            REQUIRE(heap.try_pop() == std::optional<T>(reference.top()));
            reference.pop();
        }
        REQUIRE(heap.size() == reference.size());
        if (i % 127 == 0) REQUIRE(heap.valid());
    }
    while (!reference.empty()) {
        REQUIRE(heap.try_pop() == std::optional<T>(reference.top()));
        reference.pop();
    }
    CHECK(heap.try_pop() == std::nullopt);
    CHECK(heap.valid());
}

template <typename T> void check_radix() {
    RadixHeap<T> heap;
    std::mt19937_64 rng(0x5241444958484541ULL);
    std::vector<T> keys{std::numeric_limits<T>::min(), std::numeric_limits<T>::max(),
                        std::numeric_limits<T>::min(), std::numeric_limits<T>::max(), T{0}};
    for (std::size_t i = 0; i < 5000; ++i)
        keys.push_back(i % 3 == 0 ? static_cast<T>(rng() % 9) : random_key<T>(rng));
    for (const auto key : keys) heap.push(key);
    std::sort(keys.begin(), keys.end());
    for (const auto key : keys) {
        REQUIRE(heap.try_pop() == std::optional<T>(key));
        heap.push(key);
        REQUIRE(heap.try_pop() == std::optional<T>(key));
    }
    CHECK(heap.valid());
    CHECK(heap.size() == 0);
    CHECK(heap.try_pop() == std::nullopt);
    CHECK_THROWS_AS(heap.push(std::numeric_limits<T>::min()), std::invalid_argument);
    heap.push(std::numeric_limits<T>::max());
    CHECK(heap.try_pop() == std::optional<T>(std::numeric_limits<T>::max()));

    RadixHeap<T> increasing;
    std::priority_queue<T, std::vector<T>, std::greater<T>> reference;
    T last = 0;
    for (std::size_t i = 0; i < 10000; ++i) {
        if (reference.empty() || rng() % 5 < 3) {
            const T key = static_cast<T>(last + static_cast<T>(rng() % 100));
            increasing.push(key);
            reference.push(key);
        } else {
            last = reference.top();
            REQUIRE(increasing.try_pop() == std::optional<T>(last));
            reference.pop();
        }
        if (i % 127 == 0) REQUIRE(increasing.valid());
    }
    while (!reference.empty()) {
        REQUIRE(increasing.try_pop() == std::optional<T>(reference.top()));
        reference.pop();
    }
    CHECK(increasing.try_pop() == std::nullopt);
}

TEMPLATE_TEST_CASE("Competitor heaps preserve mixed operations and duplicates", "[competitors]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    check_mixed<DaryHeap<TestType, 2>, TestType>();
    check_mixed<DaryHeap<TestType, 4>, TestType>();
    check_mixed<DaryHeap<TestType, 8>, TestType>();
    check_mixed<DaryHeap<TestType, 16>, TestType>();
    check_mixed<CustomBinaryHeap<TestType>, TestType>();
    check_mixed<CustomDaryHeap<TestType, 4>, TestType>();
    check_mixed<CustomDaryHeap<TestType, 8>, TestType>();
    check_mixed<CustomDaryHeap<TestType, 16>, TestType>();
    check_mixed<WeakHeap<TestType>, TestType>();
}

TEMPLATE_TEST_CASE("Radix heaps order every key bit and enforce monotonicity", "[competitors]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    check_radix<TestType>();
}

template <typename Heap> void check_counts() {
    Heap heap;
    heap.push(9);
    heap.push(1);
    heap.push(7);
    heap.push(4);
    CHECK(heap.push_comparisons() > 0);
    const auto pushes = heap.push_comparisons();
    REQUIRE(heap.try_pop() == std::optional<int>(1));
    CHECK(heap.push_comparisons() == pushes);
    CHECK(heap.pop_comparisons() > 0);
    heap.reset_comparisons();
    CHECK(heap.push_comparisons() == 0);
    CHECK(heap.pop_comparisons() == 0);
    REQUIRE(heap.try_pop() == std::optional<int>(4));
}

TEST_CASE("Competitor comparison counts separate push and pop", "[competitors]") {
    check_counts<DaryHeap<int, 4, true>>();
    check_counts<CustomBinaryHeap<int, true>>();
    check_counts<WeakHeap<int, true>>();
}

struct ComparisonKey {
    int value;
    std::uint64_t *comparisons;

    friend bool operator<(const ComparisonKey &left, const ComparisonKey &right) {
        ++*left.comparisons;
        return left.value < right.value;
    }
};

template <bool Count> void check_weak_comparisons() {
    WeakHeap<ComparisonKey, Count> heap;
    std::priority_queue<int, std::vector<int>, std::greater<int>> reference;
    std::uint64_t comparisons = 0;
    std::mt19937_64 rng(42);
    for (std::size_t i = 0; i < 4000 || !reference.empty(); ++i) {
        const auto before = comparisons;
        const auto pushes = heap.push_comparisons();
        const auto pops = heap.pop_comparisons();
        if (i < 4000 && (reference.empty() || rng() % 5 < 3)) {
            const auto key = static_cast<int>(rng() % 17);
            heap.push({key, &comparisons});
            reference.push(key);
            CHECK(heap.pop_comparisons() == pops);
            CHECK(heap.push_comparisons() == pushes + (Count ? comparisons - before : 0));
        } else {
            const auto result = heap.try_pop();
            REQUIRE(result.has_value());
            REQUIRE(result->value == reference.top());
            reference.pop();
            CHECK(heap.push_comparisons() == pushes);
            CHECK(heap.pop_comparisons() == pops + (Count ? comparisons - before : 0));
        }
    }
    CHECK_FALSE(heap.try_pop().has_value());
    heap.reset_comparisons();
    CHECK(heap.push_comparisons() == 0);
    CHECK(heap.pop_comparisons() == 0);
}

TEST_CASE("Weak heap counts every key comparison", "[competitors]") {
    check_weak_comparisons<false>();
    check_weak_comparisons<true>();
}

#ifdef QUICKHEAP_BENCH_BOOST
TEMPLATE_TEST_CASE("Boost competitors preserve mixed operations and duplicates", "[competitors][boost]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    check_mixed<BoostDaryHeap<TestType, 4>, TestType>();
    check_mixed<BoostFibonacciHeap<TestType>, TestType>();
    check_mixed<BoostPairingHeap<TestType>, TestType>();
    check_mixed<BoostBinomialHeap<TestType>, TestType>();
    check_mixed<BoostSkewHeap<TestType>, TestType>();
}
TEST_CASE("Boost comparison counts survive owning heap moves", "[competitors][boost]") {
    check_counts<BoostDaryHeap<int, 4, true>>();
    check_counts<BoostFibonacciHeap<int, true>>();
    check_counts<BoostPairingHeap<int, true>>();
    check_counts<BoostBinomialHeap<int, true>>();
    check_counts<BoostSkewHeap<int, true>>();
    BoostPairingHeap<int, true> source;
    source.push(2);
    source.push(1);
    auto destination = std::move(source);
    CHECK(destination.try_pop() == std::optional<int>(1));
    CHECK(destination.try_pop() == std::optional<int>(2));
}
#endif
} // namespace
