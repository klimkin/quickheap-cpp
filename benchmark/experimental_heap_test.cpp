#include "experimental_heap.hpp"

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <queue>
#include <random>
#include <vector>

namespace quickheap::bench {
namespace {

template <class T> T key_bits(std::uint64_t value) {
    using U = std::make_unsigned_t<T>;
    return std::bit_cast<T>(static_cast<U>(value));
}

TEMPLATE_TEST_CASE("benchmark kernels preserve paired keys and values", "[bench-port]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    using T = TestType;
    for (const auto count : {2U, 3U, 16U, 65U}) {
        for (const auto salt : {0U, 1U}) {
            for (const bool duplicates : {false, true}) {
                std::vector<T> keys(count);
                std::vector<T> values(count);
                for (std::size_t i = 0; i < count; ++i) {
                    keys[i] = duplicates ? T{} : i % 2 == 0
                        ? std::numeric_limits<T>::min() : std::numeric_limits<T>::max();
                    values[i] = static_cast<T>(i);
                }
                const auto original = keys;
                std::vector<T> lower_keys(count);
                std::vector<T> lower_values(count);
                for (const auto pivot : {keys.front(), keys.back()}) {
                    keys = original;
                    for (std::size_t i = 0; i < count; ++i) values[i] = static_cast<T>(i);
                    const auto split = partition(keys.data(), values.data(), lower_keys.data(),
                                                 lower_values.data(), count, pivot, salt);
                    REQUIRE(split.upper + split.lower == count);
                    REQUIRE(split.upper > 0);
                    REQUIRE(split.lower > 0);
                    std::vector<bool> seen(count);
                    for (std::size_t i = 0; i < split.upper; ++i) {
                        REQUIRE(keys[i] >= pivot);
                        const auto index = static_cast<std::size_t>(values[i]);
                        REQUIRE(index < count);
                        REQUIRE_FALSE(seen[index]);
                        REQUIRE(keys[i] == original[index]);
                        seen[index] = true;
                    }
                    for (std::size_t i = 0; i < split.lower; ++i) {
                        REQUIRE(lower_keys[i] <= pivot);
                        const auto index = static_cast<std::size_t>(lower_values[i]);
                        REQUIRE(index < count);
                        REQUIRE_FALSE(seen[index]);
                        REQUIRE(lower_keys[i] == original[index]);
                        seen[index] = true;
                    }
                }
                auto pivots = original;
                std::sort(pivots.begin(), pivots.end(), std::greater<T>{});
                for (const auto key : {std::numeric_limits<T>::min(), T{},
                                        std::numeric_limits<T>::max()}) {
                    REQUIRE(push_position(pivots.data(), pivots.size(), key) ==
                            static_cast<std::size_t>(std::count_if(
                                pivots.begin(), pivots.end(), [key](T pivot) { return pivot > key; })));
                }
            }
        }
    }
}

TEMPLATE_TEST_CASE("experimental strategies preserve heap order", "[bench-port]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    using T = TestType;
    constexpr std::array pivots = {Pivot::Random, Pivot::Median3, Pivot::Median5,
                                  Pivot::Perfect, Pivot::Log2, Pivot::Cbrt, Pivot::Table};
    constexpr std::array searches = {Search::Linear, Search::Binary, Search::Simd};
    constexpr std::array partitions = {Partition::Scalar, Partition::Simd};
    constexpr std::array<std::size_t, 3> terminals = {1, 8, 16};
    constexpr std::array rebalances = {Rebalance::None, Rebalance::PivotForgetting,
                                      Rebalance::NaiveLog};

    for (auto pivot : pivots) {
        for (auto search : searches) {
            for (auto partition : partitions) {
                for (auto terminal : terminals) {
                    for (auto rebalance : rebalances) {
                        INFO("pivot " << static_cast<int>(pivot) << " search "
                             << static_cast<int>(search) << " partition "
                             << static_cast<int>(partition) << " terminal " << terminal
                             << " rebalance " << static_cast<int>(rebalance));
                        ExperimentalConfig config;
                        config.pivot = pivot;
                        config.search = search;
                        config.partition = partition;
                        config.terminal_size = terminal;
                        config.rebalance = rebalance;
                        config.rebalance_interval = 3;
                        config.log_threshold = 1;
                        ExperimentalHeap<T, true> heap(config);
                        std::priority_queue<T, std::vector<T>, std::greater<T>> reference;
                        std::mt19937_64 random(937);
                        CHECK_FALSE(heap.try_pop());
                        for (std::size_t i = 0; i < 240; ++i) {
                            const auto key = i % 7 == 0 ? std::numeric_limits<T>::min()
                                : i % 7 == 1 ? std::numeric_limits<T>::max()
                                : i % 7 == 2 ? T{} : key_bits<T>(random());
                            heap.push(key);
                            reference.push(key);
                        }
                        for (std::size_t i = 0; i < 400; ++i) {
                            if (!reference.empty() && random() % 2 == 0) {
                                REQUIRE(heap.try_pop() == reference.top());
                                reference.pop();
                            } else {
                                const auto key = i % 3 == 0 ? T{} : key_bits<T>(random());
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
                        CHECK(heap.empty());
                        CHECK(heap.valid());
                        CHECK_FALSE(heap.try_pop());
                        CHECK(heap.counters().partitions > 0);
                        CHECK(heap.counters().elements_partitioned >= heap.counters().partitions * 2);
                        CHECK(heap.counters().comparisons_exact ==
                              (search != Search::Simd && partition != Partition::Simd));
                    }
                }
            }
        }
    }
}

TEMPLATE_TEST_CASE("experimental heaps terminate on duplicate extremes", "[bench-port]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    for (const auto key : {std::numeric_limits<TestType>::min(),
                           std::numeric_limits<TestType>::max(), TestType{}}) {
        for (const auto partition : {Partition::Scalar, Partition::Simd}) {
            ExperimentalConfig config;
            config.pivot = Pivot::Random;
            config.partition = partition;
            config.terminal_size = 1;
            ExperimentalHeap<TestType> heap(config);
            for (std::size_t i = 0; i < 1000; ++i) {
                heap.push(key);
            }
            for (std::size_t i = 0; i < 1000; ++i) {
                REQUIRE(heap.try_pop() == key);
            }
            CHECK(heap.empty());
            CHECK(heap.valid());
        }
    }
}

TEST_CASE("experimental rebalancing merges bad initial layers", "[bench-port]") {
    using Heap = ExperimentalHeap<std::int64_t, true>;
    std::vector<std::vector<std::int64_t>> layers;
    for (std::int64_t key = 127; key >= 0; --key) {
        layers.push_back({key});
    }
    for (auto rebalance : {Rebalance::None, Rebalance::PivotForgetting, Rebalance::NaiveLog}) {
        ExperimentalConfig config;
        config.rebalance = rebalance;
        config.rebalance_interval = 0;
        config.log_threshold = 1;
        auto heap = Heap::from_layers(layers, config);
        REQUIRE(heap.valid());
        REQUIRE(heap.layers() == layers.size());
        REQUIRE(heap.try_pop() == 0);
        if (rebalance != Rebalance::None) {
            REQUIRE(heap.counters().rebalances > 0);
            REQUIRE(heap.layers() < layers.size() - 1);
        } else {
            CHECK(heap.counters().rebalances == 0);
        }
        for (std::int64_t key = 1; key < 128; ++key) {
            REQUIRE(heap.try_pop() == key);
            REQUIRE(heap.valid());
        }
    }
    CHECK_THROWS_AS(Heap::from_layers({{1}, {2}}), std::invalid_argument);
    CHECK_THROWS_AS(Heap::from_layers({{1}, {}}), std::invalid_argument);
}

TEST_CASE("experimental scalar counters distinguish push and pop", "[bench-port]") {
    ExperimentalConfig config;
    config.terminal_size = 8;
    ExperimentalHeap<std::int64_t, true> heap(config);
    heap.push(1);
    heap.push(2);
    CHECK(heap.counters().push_comparisons == 1);
    CHECK(heap.counters().pop_comparisons == 0);
    heap.reset_counters();
    CHECK(heap.counters().comparisons() == 0);
    for (std::int64_t i = 10; i < 100; ++i) {
        heap.push(i);
    }
    REQUIRE(heap.try_pop() == 1);
    CHECK(heap.counters().pop_comparisons > 0);
    CHECK(heap.counters().comparisons_exact);
}

template <class T, class Heap> void check_partition_layouts() {
    for (auto partitions : {2U, 3U, 4U}) {
        for (auto partition : {Partition::Scalar, Partition::Simd}) {
            for (auto search : {Search::Linear, Search::Binary, Search::Simd}) {
                for (auto pivot : {Pivot::Random, Pivot::Median3, Pivot::Median5, Pivot::Perfect,
                                    Pivot::Log2, Pivot::Cbrt, Pivot::Table}) {
                    for (auto rebalance : {Rebalance::None, Rebalance::PivotForgetting,
                                          Rebalance::NaiveLog}) {
                        ExperimentalConfig config;
                        config.partition_count = partitions;
                        config.partition = partition;
                        config.search = search;
                        config.pivot = pivot;
                        config.rebalance = rebalance;
                        config.rebalance_interval = 3;
                        config.log_threshold = 1;
                        Heap heap(config);
                        std::priority_queue<T, std::vector<T>, std::greater<T>> reference;
                        std::mt19937_64 random(501);
                        for (std::size_t i = 0; i < 1025; ++i) {
                            const auto key = i % 7 == 0 ? std::numeric_limits<T>::min()
                                : i % 7 == 1 ? std::numeric_limits<T>::max()
                                : key_bits<T>(random());
                            heap.push(key);
                            reference.push(key);
                        }
                        for (std::size_t i = 0; i < 257; ++i) {
                            REQUIRE(heap.try_pop() == reference.top());
                            reference.pop();
                            const auto key = key_bits<T>(random());
                            heap.push(key);
                            reference.push(key);
                            REQUIRE(heap.valid());
                        }
                        while (!reference.empty()) {
                            REQUIRE(heap.try_pop() == reference.top());
                            reference.pop();
                        }
                        CHECK(heap.empty());
                        CHECK(heap.valid());
                        CHECK(heap.counters().partitions > 0);
                        if (partitions > 2) {
                            CHECK(heap.counters().multi_partitions > 0);
                        }
                    }
                }
            }
        }
    }
}

TEMPLATE_TEST_CASE("multipartition heaps preserve order", "[bench-port]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    check_partition_layouts<TestType, ExperimentalHeap<TestType, true>>();
}

TEMPLATE_TEST_CASE("linked block heaps preserve order", "[bench-port]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    check_partition_layouts<TestType, BlockQuickHeap<TestType, true>>();
    BlockQuickHeap<TestType, true> heap;
    for (std::size_t i = 0; i < 1000; ++i) {
        heap.push(TestType{});
    }
    CHECK(heap.capacity() == ((1000 + 111) / 112) * 128);
    REQUIRE(heap.try_pop() == TestType{});
    CHECK(heap.counters().blocks_partitioned > 0);
    while (!heap.empty()) {
        REQUIRE(heap.try_pop() == TestType{});
    }
    CHECK(heap.valid());
    for (auto partition : {Partition::Scalar, Partition::Simd}) {
        for (auto partitions : {3U, 4U}) {
            ExperimentalConfig config;
            config.partition_count = partitions;
            config.partition = partition;
            for (auto key : {std::numeric_limits<TestType>::min(),
                             std::numeric_limits<TestType>::max()}) {
                BlockQuickHeap<TestType, true> duplicate_heap(config);
                for (std::size_t i = 0; i < 1000; ++i) duplicate_heap.push(key);
                for (std::size_t i = 0; i < 1000; ++i) REQUIRE(duplicate_heap.try_pop() == key);
                CHECK(duplicate_heap.valid());
            }
        }
    }
}

}
}
