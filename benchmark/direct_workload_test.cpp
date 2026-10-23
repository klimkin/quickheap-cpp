#include "direct_workloads.hpp"

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <sstream>

namespace {
using namespace quickheap::bench;

TEMPLATE_TEST_CASE("Checked offsets preserve the complete integer domain", "[direct][arithmetic]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    using T = TestType;
    const auto low = std::numeric_limits<T>::lowest();
    const auto high = std::numeric_limits<T>::max();
    const std::array<T, 5> bases{low, static_cast<T>(low + 1), T{0}, static_cast<T>(high - 1), high};
    const std::array<std::uint64_t, 7> offsets{0, 1, 2, UINT32_MAX, UINT64_C(1) << 63,
                                              (UINT64_C(1) << 63) + 1, UINT64_MAX};
    const auto check = [&](T base, std::uint64_t offset) {
        CAPTURE(base, offset);
#if defined(__SIZEOF_INT128__)
        const __int128 exact = static_cast<__int128>(base) + static_cast<__int128>(offset);
        if (exact > static_cast<__int128>(high)) {
            CHECK_THROWS_AS(offset_key<true>(base, offset), std::runtime_error);
        } else {
            CHECK(static_cast<__int128>(offset_key<true>(base, offset)) == exact);
        }
#else
        const auto expected = add_positive(base, offset);
        if (expected) CHECK(offset_key<true>(base, offset) == *expected);
        else CHECK_THROWS_AS(offset_key<true>(base, offset), std::runtime_error);
#endif
    };
    for (T base : bases) for (auto offset : offsets) check(base, offset);
    if constexpr (std::is_signed_v<T>) {
        check(T{-1}, 1);
        check(T{-1}, UINT64_C(1) << (sizeof(T) * 8 - 1));
        check(T{-1}, (UINT64_C(1) << (sizeof(T) * 8 - 1)) + 1);
    }
    std::mt19937_64 rng(42);
    for (std::size_t i = 0; i < 1000; ++i) {
        const T base = std::bit_cast<T>(static_cast<std::make_unsigned_t<T>>(rng()));
        const auto offset = rng();
        check(base, offset);
    }
}

struct ControlledPopQueue {
    using Item = std::int32_t;
    std::vector<Item> pushes;
    Item shift;
    std::size_t pops = 0;

    void push(Item key) { pushes.push_back(key); }
    std::optional<Item> try_pop() { return pops++ == 0 ? 10 + shift : 10; }
};

TEST_CASE("Monotone pushes depend on tested heap pops", "[direct][dependency]") {
    for (const auto workload : {Workload::MonotoneConstantSize, Workload::MonotoneWiggle,
                                Workload::GeometricMonotoneWiggle}) {
        CAPTURE(fixture_workload_name(workload));
        auto fixture = generate_fixture<std::int32_t>(workload, 1);
        std::fill(fixture.offsets.begin(), fixture.offsets.end(), 1);
        ControlledPopQueue original{{}, 0, 0};
        ControlledPopQueue changed{{}, 7, 0};
        dispatch_workload(workload, [&]<Workload W>() {
            execute_workload<W>(fixture, original);
            execute_workload<W>(fixture, changed);
        });
        const std::size_t dependent_push = workload == Workload::MonotoneConstantSize ? 0 : 1;
        CHECK(changed.pushes[dependent_push] == original.pushes[dependent_push] + 7);
        if (dependent_push != 0) CHECK(changed.pushes[0] == original.pushes[0]);
    }
}

TEST_CASE("Random pushes remain independent of tested heap pops", "[direct][dependency]") {
    for (const auto workload : {Workload::RandomConstantSize, Workload::RandomWiggle}) {
        CAPTURE(fixture_workload_name(workload));
        const auto fixture = generate_fixture<std::int32_t>(workload, 1);
        ControlledPopQueue original{{}, 0, 0};
        ControlledPopQueue changed{{}, 7, 0};
        dispatch_workload(workload, [&]<Workload W>() {
            execute_workload<W>(fixture, original);
            execute_workload<W>(fixture, changed);
        });
        CHECK(changed.pushes == original.pushes);
    }
}

TEST_CASE("Monotone setup preserves heap dependency", "[direct][dependency]") {
    auto fixture = generate_fixture<std::int32_t>(Workload::MonotoneConstantSize, 1);
    std::fill(fixture.setup_offsets.begin(), fixture.setup_offsets.end(), 1);
    ControlledPopQueue original{{}, 0, 0};
    ControlledPopQueue changed{{}, 7, 0};
    setup_workload(fixture, original);
    setup_workload(fixture, changed);
    CHECK(changed.pushes[0] == original.pushes[0]);
    CHECK(changed.pushes[1] == original.pushes[1] + 7);
    CHECK(changed.pushes[2] == original.pushes[2] + 7);
}

struct ValueControlledPopQueue : ControlledPopQueue {
    explicit ValueControlledPopQueue(Item shift) : ControlledPopQueue{{}, shift, 0} {}
    Item pop() { return *ControlledPopQueue::try_pop(); }
    std::optional<Item> try_pop() { return -1; }
};

TEST_CASE("Monotone pushes depend on value-returning pops", "[direct][dependency]") {
    auto fixture = generate_fixture<std::int32_t>(Workload::MonotoneWiggle, 1);
    std::fill(fixture.offsets.begin(), fixture.offsets.end(), 1);
    ValueControlledPopQueue original(0);
    ValueControlledPopQueue changed(7);
    execute_workload<Workload::MonotoneWiggle>(fixture, original);
    execute_workload<Workload::MonotoneWiggle>(fixture, changed);
    CHECK(original.pushes[0] == changed.pushes[0]);
    CHECK(changed.pushes[1] == original.pushes[1] + 7);
}

TEMPLATE_TEST_CASE("Direct workloads validate every key and retained state", "[direct][correctness]",
                   std::int32_t, std::int64_t, std::uint32_t, std::uint64_t) {
    for (const auto &entry : workloads) {
        CAPTURE(entry.name);
        for (const auto n : {std::size_t{1}, std::size_t{8}, std::size_t{32}}) {
            CAPTURE(n);
            const auto fixture = generate_fixture<TestType>(entry.workload, n);
            REQUIRE_NOTHROW(validate_fixture(fixture, [] { return KeyQuickHeap<TestType>{}; }));
            REQUIRE_NOTHROW(validate_fixture(fixture, [] { return KvQuickHeap<TestType>{}; }));
            REQUIRE_NOTHROW(validate_fixture(fixture, [] { return StdKeyHeap<TestType>{}; }));
            REQUIRE_NOTHROW(validate_fixture(fixture, [] { return StdKvHeap<TestType>{}; }));
            auto key_queue = KeyQuickHeap<TestType>{};
            ValidationQueue<TestType, decltype(key_queue)> key_validation(key_queue, nullptr);
            setup_workload(fixture, key_validation);
            REQUIRE_NOTHROW(dispatch_workload(entry.workload, [&]<Workload W>() {
                execute_workload<W>(fixture, key_validation);
            }));
            const bool constant_size = entry.workload == Workload::MonotoneConstantSize ||
                                       entry.workload == Workload::RandomConstantSize;
            REQUIRE_NOTHROW(key_validation.finish(constant_size ? n + 50 : 0));
            auto pair_queue = KvQuickHeap<TestType>{};
            ValidationQueue<TestType, decltype(pair_queue)> pair_validation(pair_queue, nullptr);
            setup_workload(fixture, pair_validation);
            REQUIRE_NOTHROW(dispatch_workload(entry.workload, [&]<Workload W>() {
                execute_workload<W>(fixture, pair_validation);
            }));
            REQUIRE_NOTHROW(pair_validation.finish(constant_size ? n + 50 : 0));
        }
    }
}

struct CorruptPopQueue {
    using Item = std::pair<std::int32_t, std::int32_t>;
    StdKvHeap<std::int32_t> queue;
    bool corrupt_payload;

    void push(std::int32_t key) { queue.push(key); }
    std::optional<Item> try_pop() {
        auto item = queue.try_pop();
        if (item) {
            if (corrupt_payload) ++item->second;
            else ++item->first;
        }
        return item;
    }
    std::size_t size() const { return queue.size(); }
};

TEST_CASE("Separate validation rejects corrupted keys and payloads", "[direct][correctness]") {
    const auto fixture = generate_fixture<std::int32_t>(Workload::HeapSort, 8);
    REQUIRE_THROWS(validate_fixture(fixture, [] { return CorruptPopQueue{{}, false}; }));
    REQUIRE_THROWS(validate_fixture(fixture, [] { return CorruptPopQueue{{}, true}; }));
}

struct CorruptRetainedQueue {
    using Item = std::int32_t;
    StdKeyHeap<Item> queue;
    std::size_t pops = 0;
    static constexpr std::size_t workload_pops = 51 + 10;

    void push(Item key) { queue.push(key); }
    std::optional<Item> try_pop() {
        auto item = queue.try_pop();
        if (item && pops++ >= workload_pops) ++*item;
        return item;
    }
    std::size_t size() const { return queue.size(); }
};

TEST_CASE("Separate validation rejects corruption in retained keys", "[direct][correctness]") {
    const auto fixture = generate_fixture<std::int32_t>(Workload::RandomConstantSize, 1);
    REQUIRE_THROWS(validate_fixture(fixture, [] { return CorruptRetainedQueue{}; }));
}

enum class PopFault { Key, Payload, OptionalKey };

struct DualPopQueue {
    using Item = std::pair<std::int32_t, std::int32_t>;
    StdKvHeap<std::int32_t> queue;
    PopFault fault;
    std::size_t corrupt_after = 0;
    std::size_t pops = 0;

    void push(std::int32_t key) { queue.push(key); }
    Item pop() {
        auto item = *queue.try_pop();
        if (pops++ >= corrupt_after) {
            if (fault == PopFault::Key) ++item.first;
            else if (fault == PopFault::Payload) ++item.second;
        }
        return item;
    }
    std::optional<Item> try_pop() {
        auto item = queue.try_pop();
        if (item && fault == PopFault::OptionalKey) ++item->first;
        return item;
    }
    std::size_t size() const { return queue.size(); }
};

TEST_CASE("Separate validation checks value-returning keys and payloads", "[direct][correctness]") {
    auto fixture = generate_fixture<std::int32_t>(Workload::HeapSort, 8);
    std::fill(fixture.keys.begin(), fixture.keys.end(), 7);
    REQUIRE_THROWS_AS(validate_fixture(fixture, [] { return DualPopQueue{{}, PopFault::Key}; }),
                      std::runtime_error);
    REQUIRE_THROWS_AS(validate_fixture(fixture, [] { return DualPopQueue{{}, PopFault::Payload}; }),
                      std::runtime_error);
    REQUIRE_NOTHROW(validate_fixture(fixture, [] { return DualPopQueue{{}, PopFault::OptionalKey}; }));
}

TEST_CASE("Retained drain checks value-returning pops", "[direct][correctness]") {
    auto fixture = generate_fixture<std::int32_t>(Workload::RandomConstantSize, 1);
    std::fill(fixture.setup_keys.begin(), fixture.setup_keys.end(), 7);
    std::fill(fixture.keys.begin(), fixture.keys.end(), 7);
    REQUIRE_THROWS_AS(validate_fixture(fixture, [] { return DualPopQueue{{}, PopFault::Key, 51 + 10}; }),
                      std::runtime_error);
    REQUIRE_NOTHROW(validate_fixture(fixture, [] { return DualPopQueue{{}, PopFault::OptionalKey}; }));
}

struct EmptyValueQueue {
    using Item = std::int32_t;
    void push(Item) {}
    Item pop() { throw std::logic_error("nonempty pop reached an empty queue"); }
    std::optional<Item> try_pop() { return std::nullopt; }
    std::size_t size() const { return 0; }
};

TEST_CASE("Separate validation rejects empty state before a nonempty pop", "[direct][correctness]") {
    const auto fixture = generate_fixture<std::int32_t>(Workload::HeapSort, 1);
    REQUIRE_THROWS_AS(validate_fixture(fixture, [] { return EmptyValueQueue{}; }), std::runtime_error);
}

struct VoidPopQueue {
    using Item = std::int32_t;
    StdKeyHeap<Item> queue;
    void push(Item key) { queue.push(key); }
    void pop() { throw std::logic_error("unrelated pop selected"); }
    std::optional<Item> try_pop() { return queue.try_pop(); }
};

TEST_CASE("Direct extraction preserves checked fallback for void pops", "[direct][correctness]") {
    VoidPopQueue queue;
    queue.push(7);
    CHECK(pop_key(queue) == 7);
    CHECK_THROWS_AS(pop_key(queue), std::runtime_error);
}

TEMPLATE_TEST_CASE("Separate validation rejects monotone overflow before timing", "[direct][correctness]",
                   std::int32_t, std::int64_t, std::uint32_t, std::uint64_t) {
    auto fixture = generate_fixture<TestType>(Workload::MonotoneWiggle, 1);
    fixture.offsets = {static_cast<std::uint64_t>(std::numeric_limits<TestType>::max()), 1, 0};
    REQUIRE_THROWS(validate_fixture(fixture, [] { return StdKeyHeap<TestType>{}; }));
    fixture.workload = Workload::GeometricMonotoneWiggle;
    REQUIRE_THROWS(validate_fixture(fixture, [] { return StdKeyHeap<TestType>{}; }));
    fixture = generate_fixture<TestType>(Workload::MonotoneConstantSize, 1);
    std::fill(fixture.setup_offsets.begin(), fixture.setup_offsets.end(), 0);
    fixture.setup_offsets[0] = static_cast<std::uint64_t>(std::numeric_limits<TestType>::max());
    fixture.setup_offsets[1] = 1;
    REQUIRE_THROWS(validate_fixture(fixture, [] { return StdKeyHeap<TestType>{}; }));
}

TEST_CASE("Fixture validation records operations and retained drain", "[direct][trace]") {
    auto fixture = generate_fixture<std::int32_t>(Workload::RandomConstantSize, 1);
    std::fill(fixture.setup_keys.begin(), fixture.setup_keys.end(), -1);
    std::fill(fixture.keys.begin(), fixture.keys.end(), -1);
    std::ostringstream trace;
    validate_fixture(fixture, [] { return StdKvHeap<std::int32_t>{}; }, &trace);
    const std::string bits = "4294967295";
    std::string expected;
    for (std::size_t i = 0; i < 51; ++i)
        expected += "push," + bits + "\npop," + bits + "\npush," + bits + "\n";
    for (std::size_t i = 0; i < 10; ++i)
        expected += "pop," + bits + "\npush," + bits + "\n";
    expected += "final,51\n";
    for (std::size_t i = 0; i < 51; ++i) expected += "drain," + bits + "\n";
    CHECK(trace.str() == expected);
}

struct LifecycleQueue {
    using Item = std::int32_t;
    StdKeyHeap<Item> queue;
    std::vector<std::string> &events;

    explicit LifecycleQueue(std::vector<std::string> &log) : events(log) { events.push_back("construct"); }
    ~LifecycleQueue() { events.push_back("destroy"); }
    LifecycleQueue(const LifecycleQueue &) = delete;
    LifecycleQueue(LifecycleQueue &&) = delete;
    void push(Item key) { events.push_back("push"); queue.push(key); }
    std::optional<Item> try_pop() { events.push_back("pop"); return queue.try_pop(); }
    std::size_t size() const { return queue.size(); }
};

TEST_CASE("Direct timing excludes RAII construction setup and destruction", "[direct][lifecycle]") {
    const auto fixture = generate_fixture<std::int32_t>(Workload::RandomConstantSize, 1);
    std::vector<std::string> events;
    std::size_t clocks = 0;
    const auto duration = direct_trial(fixture, [&] { return LifecycleQueue(events); }, [&] {
        events.push_back(clocks == 0 ? "start" : "stop");
        return ++clocks;
    });
    std::vector<std::string> expected{"construct"};
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
    expected.push_back("stop");
    expected.push_back("destroy");
    CHECK(duration == 1);
    CHECK(events == expected);
}

TEST_CASE("Direct execution still rejects an empty heap", "[direct][correctness]") {
    const auto fixture = generate_fixture<std::uint32_t>(Workload::MonotoneConstantSize, 1);
    StdKeyHeap<std::uint32_t> queue;
    REQUIRE_THROWS(execute_workload<Workload::MonotoneConstantSize>(fixture, queue));
}

} // namespace
