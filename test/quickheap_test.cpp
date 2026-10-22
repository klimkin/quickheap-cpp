#include "quickheap.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <csignal>
#include <cstdlib>
#include <functional>
#include <limits>
#include <optional>
#include <queue>
#include <type_traits>
#include <utility>
#include <vector>

extern "C" int quickheap_c_api_smoke(void);

namespace {

enum class Pattern { Random, Increasing, Decreasing, MostlyMax, MostlyMin };

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
                ADD_FAILURE() << "returned payload was never inserted";
                return key;
            }
            const auto index = static_cast<std::size_t>(payload);
            EXPECT_FALSE(consumed_[index]) << "payload returned twice";
            EXPECT_EQ(key, inserted_keys_[index]) << "key/payload association changed";
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
        EXPECT_EQ(heap.size(), count);
        EXPECT_EQ(heap.empty(), count == 0);
        EXPECT_GE(heap.capacity(), heap.size());
        const auto filled_capacity = heap.capacity();

        std::sort(expected.begin(), expected.end());
        for (const auto key : expected) {
            ASSERT_EQ(heap.pop(), std::optional<T>(key));
        }
        EXPECT_EQ(heap.size(), 0U);
        EXPECT_TRUE(heap.empty());
        EXPECT_EQ(heap.pop(), std::nullopt);
        EXPECT_GE(heap.capacity(), filled_capacity) << "drain released reserved slots";
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
            ASSERT_EQ(heap.pop(), pop_reference());
            key = generator.next();
            heap.push(key);
            reference.push(key);
        }
        EXPECT_EQ(heap.size(), count);
        EXPECT_GE(heap.capacity(), heap.size());
        const auto filled_capacity = heap.capacity();

        for (std::size_t i = 0; i < count; ++i) {
            ASSERT_EQ(heap.pop(), pop_reference());
            const auto key = generator.next();
            heap.push(key);
            reference.push(key);
            ASSERT_EQ(heap.pop(), pop_reference());
        }
        EXPECT_TRUE(reference.empty());
        EXPECT_TRUE(heap.empty());
        EXPECT_EQ(heap.size(), 0U);
        EXPECT_EQ(heap.pop(), std::nullopt);
        EXPECT_GE(heap.capacity(), filled_capacity) << "drain released reserved slots";
    }
}

template <typename T> class QuickHeapTypedTest : public ::testing::Test {};
using IntegerTypes = ::testing::Types<std::int32_t, std::uint32_t,
                                      std::int64_t, std::uint64_t>;
TYPED_TEST_SUITE(QuickHeapTypedTest, IntegerTypes);

#define QUICKHEAP_WORKLOAD_TESTS(Name, PatternName)                            \
    TYPED_TEST(QuickHeapTypedTest, Sort##Name##Keys) {                         \
        sort_workload<TypeParam, false, Pattern::PatternName>();               \
    }                                                                          \
    TYPED_TEST(QuickHeapTypedTest, Sort##Name##KeyValues) {                    \
        sort_workload<TypeParam, true, Pattern::PatternName>();                \
    }                                                                          \
    TYPED_TEST(QuickHeapTypedTest, Wiggle##Name##Keys) {                       \
        wiggle_workload<TypeParam, false, Pattern::PatternName>();             \
    }                                                                          \
    TYPED_TEST(QuickHeapTypedTest, Wiggle##Name##KeyValues) {                  \
        wiggle_workload<TypeParam, true, Pattern::PatternName>();              \
    }

QUICKHEAP_WORKLOAD_TESTS(Random, Random)
QUICKHEAP_WORKLOAD_TESTS(Increasing, Increasing)
QUICKHEAP_WORKLOAD_TESTS(Decreasing, Decreasing)
QUICKHEAP_WORKLOAD_TESTS(MostlyMax, MostlyMax)
QUICKHEAP_WORKLOAD_TESTS(MostlyMin, MostlyMin)

#undef QUICKHEAP_WORKLOAD_TESTS

TYPED_TEST(QuickHeapTypedTest, KeyWrapperMovesAndRetainsCapacity) {
    using T = TypeParam;
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
    EXPECT_EQ(moved.capacity(), capacity);
    EXPECT_EQ(moved.size(), 3U);

    Heap destination;
    destination.push(static_cast<T>(99));
    destination = std::move(moved);
    auto &alias = destination;
    destination = std::move(alias);
    EXPECT_EQ(destination.capacity(), capacity);
    EXPECT_EQ(destination.pop(), static_cast<T>(1));
    EXPECT_EQ(destination.pop(), static_cast<T>(2));
    EXPECT_EQ(destination.pop(), static_cast<T>(3));
    EXPECT_TRUE(destination.empty());
    EXPECT_EQ(destination.try_pop(), std::nullopt);
    EXPECT_GE(destination.capacity(), capacity);
}

TYPED_TEST(QuickHeapTypedTest, KeyValueWrapperMovesAndRetainsCapacity) {
    using T = TypeParam;
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
    EXPECT_EQ(moved.capacity(), capacity);
    EXPECT_EQ(moved.size(), 3U);

    Heap destination;
    destination.push(static_cast<T>(99), static_cast<T>(990));
    destination = std::move(moved);
    auto &alias = destination;
    destination = std::move(alias);
    EXPECT_EQ(destination.capacity(), capacity);
    EXPECT_EQ(destination.pop(), (std::pair<T, T>{1, 10}));
    EXPECT_EQ(destination.pop(), (std::pair<T, T>{2, 20}));
    EXPECT_EQ(destination.pop(), (std::pair<T, T>{3, 30}));
    EXPECT_TRUE(destination.empty());
    EXPECT_EQ(destination.try_pop(), std::nullopt);
    EXPECT_GE(destination.capacity(), capacity);
}

template <typename T> struct CApi;
#define QUICKHEAP_C_API(T, HandleType, Prefix)                                  \
    template <> struct CApi<T> {                                               \
        using Handle = HandleType;                                             \
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

TYPED_TEST(QuickHeapTypedTest, CApiEmptyPopLeavesOutputUntouched) {
    using Api = CApi<TypeParam>;
    auto *heap = Api::create();
    ASSERT_NE(heap, nullptr);
    TypeParam out = static_cast<TypeParam>(71);
    EXPECT_FALSE(Api::pop(heap, &out));
    EXPECT_EQ(out, static_cast<TypeParam>(71));
    Api::push(heap, static_cast<TypeParam>(4));
    EXPECT_TRUE(Api::pop(heap, &out));
    EXPECT_EQ(out, static_cast<TypeParam>(4));
    out = static_cast<TypeParam>(71);
    EXPECT_FALSE(Api::pop(heap, &out));
    EXPECT_EQ(out, static_cast<TypeParam>(71));
    Api::destroy(heap);
}

TYPED_TEST(QuickHeapTypedTest, CApiRejectsEveryNullPointer) {
    using Api = CApi<TypeParam>;
    const auto expect_abort = [](auto operation) {
        EXPECT_EXIT({
            std::signal(SIGABRT, [](int) { std::_Exit(86); });
            operation();
        }, ::testing::ExitedWithCode(86), "");
    };
    expect_abort([] { Api::destroy(nullptr); });
    expect_abort([] { Api::push(nullptr, TypeParam{}); });
    expect_abort([] { TypeParam out{}; Api::pop(nullptr, &out); });
    expect_abort([] { Api::size(nullptr); });
    expect_abort([] { Api::empty(nullptr); });
    expect_abort([] { Api::capacity(nullptr); });
    expect_abort([] { auto* heap = Api::create(); Api::pop(heap, nullptr); });
}

TEST(CApiFromC, AllFourIntegerTypes) { EXPECT_EQ(quickheap_c_api_smoke(), 0); }

}
