#include "quickheap/detail/simd_quickheap.hpp"

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <queue>
#include <tuple>
#include <utility>
#include <vector>

namespace {

namespace qd = quickheap::detail;

template <class T, class Backend, unsigned Opts>
struct Variant {
    using Key = T;
    using Heap = qd::SimdQuickHeap<T, Backend, Opts>;
    static constexpr unsigned opts = Opts;
};

template <class T, class Backend>
using AllOpts = std::tuple<Variant<T, Backend, 0>, Variant<T, Backend, 1>, Variant<T, Backend, 2>,
                           Variant<T, Backend, 4>, Variant<T, Backend, 8>, Variant<T, Backend, 16>,
                           Variant<T, Backend, 3>, Variant<T, Backend, 7>, Variant<T, Backend, 15>,
                           Variant<T, Backend, 31>, Variant<T, Backend, 32>,
                           Variant<T, Backend, 64>, Variant<T, Backend, 128>,
                           Variant<T, Backend, 256>, Variant<T, Backend, 512>,
                           Variant<T, Backend, 991>>;

template <class Backend>
using AllKeys = decltype(std::tuple_cat(AllOpts<std::int32_t, Backend>{}, AllOpts<std::uint32_t, Backend>{},
                                        AllOpts<std::int64_t, Backend>{}, AllOpts<std::uint64_t, Backend>{}));

using Variants = decltype(std::tuple_cat(AllKeys<qd::DefaultBackend>{}, AllKeys<qd::Scalar>{}));

class SplitMix {
public:
    std::uint64_t next() noexcept {
        auto z = state_ += 0x9e3779b97f4a7c15;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
        z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
        return z ^ (z >> 31);
    }

private:
    std::uint64_t state_ = 42;
};

template <class T>
T mixed_key(SplitMix& rng) {
    const auto r = rng.next();
    switch (r % 8) {
    case 0: return std::numeric_limits<T>::lowest();
    case 1: return std::numeric_limits<T>::max();
    case 2:
    case 3:
    case 4: return static_cast<T>(r >> 59);
    default: return static_cast<T>(r >> 8);
    }
}

template <class Heap, class T>
class Checked {
public:
    void push(T key) {
        heap_.push(key);
        reference_.push(key);
    }

    void pop() {
        T out{};
        popped_.push_back(heap_.pop_into(out) ? std::optional<T>(out) : std::nullopt);
        if (reference_.empty()) {
            expected_.push_back(std::nullopt);
        } else {
            expected_.push_back(reference_.top());
            reference_.pop();
        }
    }

    void drain() {
        while (!reference_.empty()) {
            pop();
        }
        pop();
    }

    void check() const {
        REQUIRE(heap_.size() == reference_.size());
        REQUIRE(popped_ == expected_);
    }

private:
    Heap heap_;
    std::priority_queue<T, std::vector<T>, std::greater<T>> reference_;
    std::vector<std::optional<T>> popped_;
    std::vector<std::optional<T>> expected_;
};

}

TEMPLATE_LIST_TEST_CASE("Variants pop in ascending order", "[variants]", Variants) {
    using T = typename TestType::Key;
    using Heap = typename TestType::Heap;
    INFO("Opts " << TestType::opts);
    SplitMix rng;

    SECTION("random pushes and pops with duplicates and extrema") {
        Checked<Heap, T> heap;
        for (int i = 0; i < 200'000; ++i) {
            if (rng.next() % 5 < 3) {
                heap.push(mixed_key<T>(rng));
            } else {
                heap.pop();
            }
        }
        heap.drain();
        heap.check();
    }

    SECTION("descending bursts grow past 64 pivots") {
        Checked<Heap, T> heap;
        const std::int64_t top = std::numeric_limits<std::int32_t>::max() / 2;
        for (std::int64_t round = 0; round < 400; ++round) {
            for (std::int64_t j = 0; j < 40; ++j) {
                heap.push(static_cast<T>(top - round * 48 - j / 2));
            }
            for (int j = 0; j < 3; ++j) {
                heap.push(static_cast<T>(top - static_cast<std::int64_t>(rng.next() % (400 * 48))));
            }
            heap.pop();
        }
        heap.drain();
        heap.check();
    }

    SECTION("heap sort over a narrow key range") {
        Checked<Heap, T> heap;
        for (int i = 0; i < 50'000; ++i) {
            heap.push(static_cast<T>(rng.next() % 1000));
        }
        heap.drain();
        heap.check();
    }
}

TEST_CASE("sort_unstable_small matches std::sort up to 32 keys", "[variants]") {
    SplitMix rng;
    for (std::size_t len = 0; len <= 32; ++len) {
        for (int pattern = 0; pattern < 6; ++pattern) {
            for (int round = 0; round < 200; ++round) {
                std::vector<std::int64_t> v(len);
                for (std::size_t i = 0; i < len; ++i) {
                    switch (pattern) {
                    case 0: v[i] = mixed_key<std::int64_t>(rng); break;
                    case 1: v[i] = static_cast<std::int64_t>(rng.next() % 3); break;
                    case 2: v[i] = static_cast<std::int64_t>(i); break;
                    case 3: v[i] = -static_cast<std::int64_t>(i); break;
                    case 4: v[i] = -static_cast<std::int64_t>(i / 2); break;
                    default: v[i] = static_cast<std::int64_t>(i) - (i == len / 2 ? 100 : 0); break;
                    }
                }
                INFO("len " << len << " pattern " << pattern);
                for (const bool descending : {false, true}) {
                    auto actual = v;
                    auto expected = v;
                    if (descending) {
                        qd::sort_unstable_small(actual.data(), len, std::greater<>{});
                        std::sort(expected.begin(), expected.end(), std::greater<>{});
                    } else {
                        qd::sort_unstable_small(actual.data(), len, std::less<>{});
                        std::sort(expected.begin(), expected.end());
                    }
                    REQUIRE(actual == expected);
                }
            }
        }
    }
}
