#include "radix_external.hpp"
#include "direct_workloads.hpp"

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <functional>
#include <limits>
#include <queue>
#include <random>
#include <string>
#include <vector>

namespace {
using namespace quickheap::bench;

TEMPLATE_TEST_CASE("Pinned radix queue preserves signed order and duplicate extrema",
                   "[benchmark][external][radix]", std::int32_t, std::int64_t,
                   std::uint32_t, std::uint64_t) {
    using T = TestType;
    ExternalRadixHeap<T> queue;
    REQUIRE_FALSE(queue.try_pop());
    std::vector<T> keys{std::numeric_limits<T>::max(), T{0},
                        std::numeric_limits<T>::lowest(), T{1},
                        std::numeric_limits<T>::max(), std::numeric_limits<T>::lowest()};
    if constexpr (std::is_signed_v<T>) keys.push_back(T{-1});
    for (T key : keys) queue.push(key);
    REQUIRE(queue.size() == keys.size());
    std::sort(keys.begin(), keys.end());
    for (std::size_t i = 0; i < keys.size(); ++i) {
        REQUIRE(queue.try_pop() == keys[i]);
        REQUIRE(queue.size() == keys.size() - i - 1);
    }
    REQUIRE(queue.empty());
    REQUIRE_FALSE(queue.try_pop());
    queue.push(std::numeric_limits<T>::max());
    REQUIRE(queue.try_pop() == std::numeric_limits<T>::max());
}

TEMPLATE_TEST_CASE("Pinned radix queue rejects descending insertion and supports clear",
                   "[benchmark][external][radix]", std::int32_t, std::int64_t,
                   std::uint32_t, std::uint64_t) {
    using T = TestType;
    ExternalRadixHeap<T> queue;
    queue.push(T{10});
    queue.push(T{20});
    REQUIRE(queue.try_pop() == T{10});
    REQUIRE_THROWS_AS(queue.push(T{9}), std::invalid_argument);
    REQUIRE(queue.size() == 1);
    queue.push(T{10});
    REQUIRE(queue.try_pop() == T{10});
    REQUIRE(queue.try_pop() == T{20});
    REQUIRE_THROWS_AS(queue.push(T{19}), std::invalid_argument);
    REQUIRE(queue.empty());
    queue.clear();
    queue.push(std::numeric_limits<T>::lowest());
    REQUIRE(queue.try_pop() == std::numeric_limits<T>::lowest());
    queue.push(std::numeric_limits<T>::max());
    queue.clear();
    REQUIRE(queue.empty());
    queue.push(std::numeric_limits<T>::lowest());
    REQUIRE(queue.try_pop() == std::numeric_limits<T>::lowest());
}

TEMPLATE_TEST_CASE("Pinned radix queue matches minimum queue under monotone schedules",
                   "[benchmark][external][radix]", std::int32_t, std::int64_t,
                   std::uint32_t, std::uint64_t) {
    using T = TestType;
    ExternalRadixHeap<T> queue;
    std::priority_queue<T, std::vector<T>, std::greater<T>> expected;
    std::mt19937_64 random(42);
    T last = std::is_signed_v<T> ? static_cast<T>(-100000) : T{0};
    for (unsigned i = 0; i < 20000; ++i) {
        if (expected.empty() || random() % 3 != 0) {
            const T key = static_cast<T>(last + static_cast<T>(random() % 32));
            queue.push(key);
            expected.push(key);
        } else {
            last = expected.top();
            REQUIRE(queue.try_pop() == last);
            expected.pop();
        }
        REQUIRE(queue.size() == expected.size());
    }
    while (!expected.empty()) {
        REQUIRE(queue.try_pop() == expected.top());
        expected.pop();
    }
    REQUIRE(queue.empty());
}

TEMPLATE_TEST_CASE("Pinned radix queue consumes shared active monotone fixtures",
                   "[benchmark][external][radix][fixtures]", std::int32_t, std::int64_t,
                   std::uint32_t, std::uint64_t) {
    const auto fixtures = std::filesystem::path(__FILE__).parent_path() / "fixtures";
    for (std::string_view workload : {"MonotoneConstantSize", "MonotoneWiggle"}) {
        const auto path = fixtures / (std::string(workload) + "-" +
                                     std::string(key_name<TestType>()) + "-n8.qhf");
        CAPTURE(path.string());
        const auto fixture = load_fixture<TestType>(path);
        REQUIRE_NOTHROW(validate_fixture(fixture, [] { return ExternalRadixHeap<TestType>{}; }));
    }
    const auto geometric = generate_fixture<TestType>(Workload::GeometricMonotoneWiggle, 32);
    REQUIRE_NOTHROW(validate_fixture(geometric, [] { return ExternalRadixHeap<TestType>{}; }));
}

} // namespace
