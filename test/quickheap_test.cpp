#include "quickheap.hpp"

#include <catch2/catch_session.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <csignal>
#include <cstdlib>
#include <functional>
#include <limits>
#include <optional>
#include <queue>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <spawn.h>
#include <sys/wait.h>
extern char **environ;
#endif

extern "C" int quickheap_c_api_smoke(void);

namespace {

enum class Pattern { Random, Increasing, Decreasing, MostlyMax, MostlyMin };

TEMPLATE_TEST_CASE("QuickHeapTypedTest.SmallTerminalsRetainDuplicatesAndExtrema", "[quickheap]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    using T = TestType;
    const std::array<T, 5> keys{std::numeric_limits<T>::lowest(), std::numeric_limits<T>::max(),
                              T{0}, T{1}, T{1}};
    for (std::size_t count = 0; count <= 17; ++count) {
        quickheap::Heap<T> heap;
        quickheap::KvSimdQuickHeap<T> kv;
        std::vector<T> input;
        for (std::size_t index = 0; index < count; ++index) {
            const T key = keys[(index * 3) % keys.size()];
            input.push_back(key);
            heap.push(key);
            kv.push(key, static_cast<T>(index));
        }
        auto expected = input;
        std::sort(expected.begin(), expected.end());
        std::vector<bool> seen(count);
        for (T key : expected) {
            CHECK(heap.try_pop() == key);
            const auto row = kv.try_pop();
            REQUIRE(row);
            REQUIRE(row->first == key);
            const auto index = static_cast<std::size_t>(row->second);
            REQUIRE(index < count);
            CHECK_FALSE(seen[index]);
            CHECK(input[index] == row->first);
            seen[index] = true;
        }
        CHECK_FALSE(heap.try_pop());
        CHECK_FALSE(kv.try_pop());
    }
}

template <typename T> T from_bits(std::make_unsigned_t<T> bits) {
    if constexpr (std::is_signed_v<T>) {
        return std::bit_cast<T>(bits);
    } else {
        return bits;
    }
}

template <typename T, Pattern P> class Generator {
  public:
    T next() {
        if constexpr (P == Pattern::Increasing) {
            return from_bits<T>(static_cast<std::make_unsigned_t<T>>(
                std::numeric_limits<T>::min()) + counter_++);
        } else if constexpr (P == Pattern::Decreasing) {
            return from_bits<T>(static_cast<std::make_unsigned_t<T>>(
                std::numeric_limits<T>::max()) - counter_++);
        } else if constexpr (P == Pattern::MostlyMax) {
            return (random_bits() & 0xffffU) < 100
                       ? from_bits<T>(random_bits())
                       : std::numeric_limits<T>::max();
        } else if constexpr (P == Pattern::MostlyMin) {
            return (random_bits() & 0xffffU) < 100
                       ? from_bits<T>(random_bits())
                       : std::numeric_limits<T>::min();
        } else {
            return from_bits<T>(random_bits());
        }
    }

  private:
    std::make_unsigned_t<T> random_bits() {
        state_ += UINT64_C(0x9e3779b97f4a7c15);
        auto x = state_;
        x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
        return static_cast<std::make_unsigned_t<T>>(x ^ (x >> 31));
    }

    std::uint64_t state_ = UINT64_C(0x574956475f515549);
    std::make_unsigned_t<T> counter_ = 0;
};

constexpr std::array<std::size_t, 20> sizes = {
    0, 1, 2, 3, 4, 5, 7, 8, 9, 10, 15, 16, 17, 31, 32, 33,
    100, 1000, 10000, 100000,
};

template <typename T, bool KeyValue> class Queue {
  public:
    void push(T key) {
        if constexpr (KeyValue) {
            const auto payload = static_cast<T>(inserted_keys_.size());
            inserted_keys_.push_back(key);
            consumed_.push_back(false);
            heap_.push(key, payload);
        } else {
            heap_.push(key);
        }
    }

    std::optional<T> pop() {
        if constexpr (KeyValue) {
            const auto entry = heap_.try_pop();
            if (!entry) {
                return std::nullopt;
            }
            const auto [key, payload] = *entry;
            if (std::cmp_less(payload, 0) ||
                std::cmp_greater_equal(payload, inserted_keys_.size())) {
                FAIL_CHECK("returned payload was never inserted");
                return key;
            }
            const auto index = static_cast<std::size_t>(payload);
            INFO("payload returned twice");
            CHECK_FALSE(consumed_[index]);
            INFO("key/payload association changed");
            CHECK(key == inserted_keys_[index]);
            consumed_[index] = true;
            return key;
        } else {
            return heap_.try_pop();
        }
    }

    std::size_t size() const { return heap_.size(); }
    bool empty() const { return heap_.empty(); }
    std::size_t capacity() const { return heap_.capacity(); }

  private:
    using Heap = std::conditional_t<KeyValue, quickheap::KvSimdQuickHeap<T>,
                                    quickheap::Heap<T>>;
    Heap heap_;
    std::vector<T> inserted_keys_;
    std::vector<bool> consumed_;
};

template <typename T, bool KeyValue, Pattern P> void sort_workload() {
    Generator<T, P> generator;
    for (const auto count : sizes) {
        Queue<T, KeyValue> heap;
        std::vector<T> expected;
        expected.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            const auto key = generator.next();
            heap.push(key);
            expected.push_back(key);
        }
        CHECK(heap.size() == count);
        CHECK(heap.empty() == (count == 0));
        CHECK(heap.capacity() >= heap.size());
        const auto filled_capacity = heap.capacity();

        std::sort(expected.begin(), expected.end());
        for (const auto key : expected) {
            REQUIRE(heap.pop() == std::optional<T>(key));
        }
        CHECK(heap.size() == 0U);
        CHECK(heap.empty());
        CHECK(heap.pop() == std::nullopt);
        INFO("drain released reserved slots");
        CHECK(heap.capacity() >= filled_capacity);
    }
}

template <typename T, bool KeyValue, Pattern P> void wiggle_workload() {
    Generator<T, P> generator;
    for (const auto count : sizes) {
        Queue<T, KeyValue> heap;
        std::priority_queue<T, std::vector<T>, std::greater<T>> reference;
        const auto pop_reference = [&]() -> std::optional<T> {
            if (reference.empty()) {
                return std::nullopt;
            }
            const auto key = reference.top();
            reference.pop();
            return key;
        };

        for (std::size_t i = 0; i < count; ++i) {
            auto key = generator.next();
            heap.push(key);
            reference.push(key);
            REQUIRE(heap.pop() == pop_reference());
            key = generator.next();
            heap.push(key);
            reference.push(key);
        }
        CHECK(heap.size() == count);
        CHECK(heap.capacity() >= heap.size());
        const auto filled_capacity = heap.capacity();

        for (std::size_t i = 0; i < count; ++i) {
            REQUIRE(heap.pop() == pop_reference());
            const auto key = generator.next();
            heap.push(key);
            reference.push(key);
            REQUIRE(heap.pop() == pop_reference());
        }
        CHECK(reference.empty());
        CHECK(heap.empty());
        CHECK(heap.size() == 0U);
        CHECK(heap.pop() == std::nullopt);
        INFO("drain released reserved slots");
        CHECK(heap.capacity() >= filled_capacity);
    }
}

TEMPLATE_TEST_CASE("QuickHeapTypedTest.SortRandomKeys", "[quickheap]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    sort_workload<TestType, false, Pattern::Random>();
}

TEMPLATE_TEST_CASE("QuickHeapTypedTest.SortRandomKeyValues", "[quickheap]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    sort_workload<TestType, true, Pattern::Random>();
}

TEMPLATE_TEST_CASE("QuickHeapTypedTest.WiggleRandomKeys", "[quickheap]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    wiggle_workload<TestType, false, Pattern::Random>();
}

TEMPLATE_TEST_CASE("QuickHeapTypedTest.WiggleRandomKeyValues", "[quickheap]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    wiggle_workload<TestType, true, Pattern::Random>();
}

TEMPLATE_TEST_CASE("QuickHeapTypedTest.SortIncreasingKeys", "[quickheap]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    sort_workload<TestType, false, Pattern::Increasing>();
}

TEMPLATE_TEST_CASE("QuickHeapTypedTest.SortIncreasingKeyValues", "[quickheap]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    sort_workload<TestType, true, Pattern::Increasing>();
}

TEMPLATE_TEST_CASE("QuickHeapTypedTest.WiggleIncreasingKeys", "[quickheap]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    wiggle_workload<TestType, false, Pattern::Increasing>();
}

TEMPLATE_TEST_CASE("QuickHeapTypedTest.WiggleIncreasingKeyValues", "[quickheap]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    wiggle_workload<TestType, true, Pattern::Increasing>();
}

TEMPLATE_TEST_CASE("QuickHeapTypedTest.SortDecreasingKeys", "[quickheap]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    sort_workload<TestType, false, Pattern::Decreasing>();
}

TEMPLATE_TEST_CASE("QuickHeapTypedTest.SortDecreasingKeyValues", "[quickheap]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    sort_workload<TestType, true, Pattern::Decreasing>();
}

TEMPLATE_TEST_CASE("QuickHeapTypedTest.WiggleDecreasingKeys", "[quickheap]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    wiggle_workload<TestType, false, Pattern::Decreasing>();
}

TEMPLATE_TEST_CASE("QuickHeapTypedTest.WiggleDecreasingKeyValues", "[quickheap]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    wiggle_workload<TestType, true, Pattern::Decreasing>();
}

TEMPLATE_TEST_CASE("QuickHeapTypedTest.SortMostlyMaxKeys", "[quickheap]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    sort_workload<TestType, false, Pattern::MostlyMax>();
}

TEMPLATE_TEST_CASE("QuickHeapTypedTest.SortMostlyMaxKeyValues", "[quickheap]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    sort_workload<TestType, true, Pattern::MostlyMax>();
}

TEMPLATE_TEST_CASE("QuickHeapTypedTest.WiggleMostlyMaxKeys", "[quickheap]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    wiggle_workload<TestType, false, Pattern::MostlyMax>();
}

TEMPLATE_TEST_CASE("QuickHeapTypedTest.WiggleMostlyMaxKeyValues", "[quickheap]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    wiggle_workload<TestType, true, Pattern::MostlyMax>();
}

TEMPLATE_TEST_CASE("QuickHeapTypedTest.SortMostlyMinKeys", "[quickheap]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    sort_workload<TestType, false, Pattern::MostlyMin>();
}

TEMPLATE_TEST_CASE("QuickHeapTypedTest.SortMostlyMinKeyValues", "[quickheap]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    sort_workload<TestType, true, Pattern::MostlyMin>();
}

TEMPLATE_TEST_CASE("QuickHeapTypedTest.WiggleMostlyMinKeys", "[quickheap]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    wiggle_workload<TestType, false, Pattern::MostlyMin>();
}

TEMPLATE_TEST_CASE("QuickHeapTypedTest.WiggleMostlyMinKeyValues", "[quickheap]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    wiggle_workload<TestType, true, Pattern::MostlyMin>();
}

TEMPLATE_TEST_CASE("QuickHeapTypedTest.KeyValueReusePreservesAssociations", "[quickheap]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    using T = TestType;
    Queue<T, true> heap;
    Generator<T, Pattern::Random> generator;
    std::size_t retained_capacity = 0;
    for (const std::size_t count : {17U, 257U, 33U, 4097U, 8193U, 17U,
                                   16385U, 1025U, 16385U}) {
        std::vector<T> expected;
        expected.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            const auto key = index % 4 == 0 ? std::numeric_limits<T>::min()
                           : index % 4 == 1 ? std::numeric_limits<T>::max()
                                            : generator.next();
            heap.push(key);
            expected.push_back(key);
        }
        CHECK(heap.size() == count);
        CHECK(heap.capacity() >= retained_capacity);
        retained_capacity = heap.capacity();
        std::sort(expected.begin(), expected.end());
        for (const auto key : expected) {
            REQUIRE(heap.pop() == std::optional<T>(key));
        }
        CHECK(heap.empty());
        CHECK(heap.size() == 0U);
        CHECK(heap.pop() == std::nullopt);
        CHECK(heap.capacity() >= retained_capacity);
        retained_capacity = heap.capacity();
    }
}

TEMPLATE_TEST_CASE("QuickHeapTypedTest.KeyWrapperMovesAndRetainsCapacity", "[quickheap]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    using T = TestType;
    using Heap = quickheap::Heap<T>;
    static_assert(!std::is_copy_constructible_v<Heap>);
    static_assert(!std::is_copy_assignable_v<Heap>);
    static_assert(std::is_nothrow_move_constructible_v<Heap>);
    static_assert(std::is_nothrow_move_assignable_v<Heap>);

    Heap source;
    source.push(static_cast<T>(3));
    source.push(static_cast<T>(1));
    source.push(static_cast<T>(2));
    const auto capacity = source.capacity();
    Heap moved(std::move(source));
    CHECK(moved.capacity() == capacity);
    CHECK(moved.size() == 3U);

    Heap destination;
    destination.push(static_cast<T>(99));
    destination = std::move(moved);
    auto &alias = destination;
    destination = std::move(alias);
    CHECK(destination.capacity() == capacity);
    CHECK(destination.pop() == static_cast<T>(1));
    CHECK(destination.pop() == static_cast<T>(2));
    CHECK(destination.pop() == static_cast<T>(3));
    CHECK(destination.empty());
    CHECK(destination.try_pop() == std::nullopt);
    CHECK(destination.capacity() >= capacity);
}

TEMPLATE_TEST_CASE("QuickHeapTypedTest.KeyValueWrapperMovesAndRetainsCapacity", "[quickheap]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    using T = TestType;
    using Heap = quickheap::KvSimdQuickHeap<T>;
    static_assert(!std::is_copy_constructible_v<Heap>);
    static_assert(!std::is_copy_assignable_v<Heap>);
    static_assert(std::is_move_constructible_v<Heap>);
    static_assert(std::is_move_assignable_v<Heap>);

    Heap source;
    source.push(static_cast<T>(3), static_cast<T>(30));
    source.push(static_cast<T>(1), static_cast<T>(10));
    source.push(static_cast<T>(2), static_cast<T>(20));
    const auto capacity = source.capacity();
    Heap moved(std::move(source));
    CHECK(moved.capacity() == capacity);
    CHECK(moved.size() == 3U);

    Heap destination;
    destination.push(static_cast<T>(99), static_cast<T>(990));
    destination = std::move(moved);
    auto &alias = destination;
    destination = std::move(alias);
    CHECK(destination.capacity() == capacity);
    CHECK(destination.pop() == (std::pair<T, T>{1, 10}));
    CHECK(destination.pop() == (std::pair<T, T>{2, 20}));
    CHECK(destination.pop() == (std::pair<T, T>{3, 30}));
    CHECK(destination.empty());
    CHECK(destination.try_pop() == std::nullopt);
    CHECK(destination.capacity() >= capacity);
}

template <typename T> struct CApi;
#define QUICKHEAP_C_API(T, HandleType, Prefix)                                  \
    template <> struct CApi<T> {                                               \
        using Handle = HandleType;                                             \
        static constexpr const char *name = #HandleType;                       \
        static Handle *create() { return Prefix##_new(); }                     \
        static void destroy(Handle *h) { Prefix##_free(h); }                   \
        static void push(Handle *h, T x) { Prefix##_push(h, x); }               \
        static bool pop(Handle *h, T *out) { return Prefix##_pop(h, out); }     \
        static std::size_t size(const Handle *h) { return Prefix##_len(h); }   \
        static bool empty(const Handle *h) { return Prefix##_is_empty(h); }    \
        static std::size_t capacity(const Handle *h) {                         \
            return Prefix##_capacity(h);                                       \
        }                                                                      \
    };

QUICKHEAP_C_API(std::int32_t, QuickHeapI32, quickheap_i32)
QUICKHEAP_C_API(std::uint32_t, QuickHeapU32, quickheap_u32)
QUICKHEAP_C_API(std::int64_t, QuickHeapI64, quickheap_i64)
QUICKHEAP_C_API(std::uint64_t, QuickHeapU64, quickheap_u64)
#undef QUICKHEAP_C_API

enum class NullOperation { Destroy, Push, Pop, Size, Empty, Capacity, Output };

struct NamedNullOperation {
    const char *name;
    NullOperation operation;
};

constexpr std::array null_operations = {
    NamedNullOperation{"destroy", NullOperation::Destroy},
    NamedNullOperation{"push", NullOperation::Push},
    NamedNullOperation{"pop", NullOperation::Pop},
    NamedNullOperation{"size", NullOperation::Size},
    NamedNullOperation{"empty", NullOperation::Empty},
    NamedNullOperation{"capacity", NullOperation::Capacity},
    NamedNullOperation{"output", NullOperation::Output},
};

template <typename T> void run_null_operation(NullOperation operation) {
    using Api = CApi<T>;
    switch (operation) {
    case NullOperation::Destroy:
        Api::destroy(nullptr);
        break;
    case NullOperation::Push:
        Api::push(nullptr, T{});
        break;
    case NullOperation::Pop: {
        T out{};
        Api::pop(nullptr, &out);
        break;
    }
    case NullOperation::Size:
        Api::size(nullptr);
        break;
    case NullOperation::Empty:
        Api::empty(nullptr);
        break;
    case NullOperation::Capacity:
        Api::capacity(nullptr);
        break;
    case NullOperation::Output: {
        auto *heap = Api::create();
        Api::pop(heap, nullptr);
        Api::destroy(heap);
        break;
    }
    }
}

struct NullTestType {
    const char *name;
    void (*run)(NullOperation);
};

constexpr std::array null_test_types = {
    NullTestType{CApi<std::int32_t>::name, run_null_operation<std::int32_t>},
    NullTestType{CApi<std::uint32_t>::name, run_null_operation<std::uint32_t>},
    NullTestType{CApi<std::int64_t>::name, run_null_operation<std::int64_t>},
    NullTestType{CApi<std::uint64_t>::name, run_null_operation<std::uint64_t>},
};

constexpr const char *null_test_argument = "--quickheap-null-test";
constexpr int abort_exit_code = 86;
const char *test_executable = nullptr;

int run_null_test_child(std::string_view type_name,
                        std::string_view operation_name) {
    const auto type = std::find_if(null_test_types.begin(), null_test_types.end(),
                                  [type_name](const auto &entry) {
                                      return type_name == entry.name;
                                  });
    const auto operation = std::find_if(
        null_operations.begin(), null_operations.end(),
        [operation_name](const auto &entry) { return operation_name == entry.name; });
    if (type == null_test_types.end() || operation == null_operations.end()) {
        return 2;
    }
    if (std::signal(SIGABRT, [](int) { std::_Exit(abort_exit_code); }) == SIG_ERR) {
        return 2;
    }
    type->run(operation->operation);
    return 0;
}

int null_operation_exit_code(const char *type_name, const char *operation_name) {
#ifdef _WIN32
    const char *arguments[] = {"quickheap_tests", null_test_argument, type_name,
                              operation_name, nullptr};
    return static_cast<int>(_spawnvp(_P_WAIT, test_executable, arguments));
#else
    char *arguments[] = {const_cast<char *>(test_executable),
                         const_cast<char *>(null_test_argument),
                         const_cast<char *>(type_name),
                         const_cast<char *>(operation_name), nullptr};
    pid_t child;
    if (posix_spawnp(&child, test_executable, nullptr, nullptr, arguments,
                     environ) != 0) {
        return -1;
    }
    int status;
    pid_t waited;
    do {
        waited = waitpid(child, &status, 0);
    } while (waited == -1 && errno == EINTR);
    return waited == child && WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

TEMPLATE_TEST_CASE("QuickHeapTypedTest.CApiEmptyPopLeavesOutputUntouched", "[quickheap]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    using Api = CApi<TestType>;
    auto *heap = Api::create();
    REQUIRE(heap != nullptr);
    TestType out = static_cast<TestType>(71);
    CHECK_FALSE(Api::pop(heap, &out));
    CHECK(out == static_cast<TestType>(71));
    Api::push(heap, static_cast<TestType>(4));
    CHECK(Api::pop(heap, &out));
    CHECK(out == static_cast<TestType>(4));
    out = static_cast<TestType>(71);
    CHECK_FALSE(Api::pop(heap, &out));
    CHECK(out == static_cast<TestType>(71));
    Api::destroy(heap);
}

TEMPLATE_TEST_CASE("QuickHeapTypedTest.CApiRejectsEveryNullPointer", "[quickheap]",
                   std::int32_t, std::uint32_t, std::int64_t, std::uint64_t) {
    for (const auto &operation : null_operations) {
        CAPTURE(operation.name);
        CHECK(null_operation_exit_code(CApi<TestType>::name, operation.name) ==
              abort_exit_code);
    }
}

TEST_CASE("CApiFromC.AllFourIntegerTypes", "[quickheap][c-api]") {
    CHECK(quickheap_c_api_smoke() == 0);
}

}

int main(int argc, char *argv[]) {
    test_executable = argv[0];
    if (argc > 1 && std::string_view(argv[1]) == null_test_argument) {
        if (argc != 4) {
            return 2;
        }
        return run_null_test_child(argv[2], argv[3]);
    }
    return Catch::Session().run(argc, argv);
}
