#include "original_profile.hpp"

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

namespace {
using namespace quickheap::bench;

struct OriginalLifecycleQueue {
    using Item = std::int32_t;
    StdKeyHeap<Item> queue;
    std::vector<std::string> &events;

    explicit OriginalLifecycleQueue(std::vector<std::string> &log)
        : queue(original_std_capacity), events(log) { events.push_back("construct/reserve"); }
    ~OriginalLifecycleQueue() { events.push_back("destroy"); }
    OriginalLifecycleQueue(const OriginalLifecycleQueue &) = delete;
    OriginalLifecycleQueue(OriginalLifecycleQueue &&) = delete;
    void push(Item key) { events.push_back("push"); queue.push(key); }
    std::optional<Item> try_pop() { events.push_back("pop"); return queue.try_pop(); }
};

TEST_CASE("Original timing includes queue destruction and excludes construction and setup",
          "[original][lifecycle]") {
    auto input = generate_fixture<std::int32_t>(Workload::MonotoneConstantSize, 1);
    std::vector<std::string> events;
    std::size_t clocks = 0;
    const auto duration = original_trial(std::move(input), [&] { return OriginalLifecycleQueue(events); }, [&] {
        events.push_back(clocks == 0 ? "start" : "stop");
        return ++clocks;
    });
    std::vector<std::string> expected{"construct/reserve"};
    for (std::size_t i = 0; i < 51; ++i) {
        expected.push_back("push");
        expected.push_back("pop");
        expected.push_back("push");
    }
    expected.push_back("start");
    for (std::size_t i = 0; i < 10; ++i) {
        expected.push_back("pop");
        expected.push_back("push");
    }
    expected.push_back("destroy");
    expected.push_back("stop");
    CHECK(duration == 1);
    CHECK(events == expected);
}

struct ConsumedInputQueue {
    using Item = std::int32_t;
    const Fixture<Item> &input;
    StdKeyHeap<Item> queue;

    void push(Item key) {
        CHECK(input.keys.capacity() != 0);
        queue.push(key);
    }
    std::optional<Item> try_pop() {
        CHECK(input.keys.empty());
        CHECK(input.keys.capacity() == 0);
        return queue.try_pop();
    }
};

TEST_CASE("Original HeapSort consumes input allocation before first pop", "[original][lifecycle]") {
    auto input = generate_fixture<std::int32_t>(Workload::HeapSort, 8);
    ConsumedInputQueue queue{input, {}};
    execute_original_workload<Workload::HeapSort>(input, queue);
    CHECK(queue.queue.size() == 0);
}

struct OriginalControlledPopQueue {
    using Item = std::int32_t;
    std::vector<Item> pushes;
    Item shift;
    std::size_t pops = 0;

    void push(Item key) { pushes.push_back(key); }
    std::optional<Item> try_pop() { return pops++ == 0 ? 10 + shift : 10; }
};

TEST_CASE("Original monotone pushes depend on tested heap pops", "[original][dependency]") {
    for (const auto workload : {Workload::MonotoneConstantSize, Workload::MonotoneWiggle}) {
        auto input = generate_fixture<std::int32_t>(workload, 1);
        std::fill(input.offsets.begin(), input.offsets.end(), 1);
        auto changed_input = input;
        OriginalControlledPopQueue original{{}, 0, 0};
        OriginalControlledPopQueue changed{{}, 7, 0};
        dispatch_workload(workload, [&]<Workload W>() {
            execute_original_workload<W>(input, original);
            execute_original_workload<W>(changed_input, changed);
        });
        const std::size_t dependent_push = workload == Workload::MonotoneConstantSize ? 0 : 1;
        CHECK(changed.pushes[dependent_push] == original.pushes[dependent_push] + 7);
    }
}

TEMPLATE_TEST_CASE("Original generated inputs match domains and validate outside timing",
                   "[original][correctness]", std::int32_t, std::int64_t) {
    std::mt19937_64 rng(1234);
    for (const auto workload : {Workload::HeapSort, Workload::MonotoneConstantSize, Workload::MonotoneWiggle,
                                Workload::RandomWiggle}) {
        const auto input = generate_original_fixture<TestType>(workload, 8, rng);
        REQUIRE_NOTHROW(validate_fixture(input, [] { return StdKeyHeap<TestType>{original_std_capacity}; }));
        REQUIRE_NOTHROW(validate_fixture(input, [] { return KeyQuickHeap<TestType>{}; }));
        for (const auto offset : input.setup_offsets) CHECK(offset < stride<TestType>());
        for (const auto offset : input.offsets) CHECK(offset < stride<TestType>());
        for (const auto key : input.keys) {
            CHECK(key != std::numeric_limits<TestType>::min());
            CHECK(key != std::numeric_limits<TestType>::max());
            if (workload == Workload::RandomWiggle) {
                CHECK(key >= 0);
                CHECK(static_cast<std::uint64_t>(key) < stride<TestType>());
            }
        }
        CHECK(input.digest == fixture_digest(encode_fixture(input)));
        const auto next = generate_original_fixture<TestType>(workload, 8, rng);
        CHECK(next.digest != input.digest);
    }
}

}
