#pragma once

#include "rng.hpp"
#include "sort.hpp"

#include <array>
#include <cstddef>
#include <functional>
#include <utility>

namespace quickheap::detail {

// Mirrors `pivots::MedianOfM`: M uniform samples, median by (value, position), where
// `select_nth_unstable` on at most 16 elements is an insertion sort.
template <std::size_t M>
struct MedianOfM {
    static_assert(M >= 3 && M <= 16 && M % 2 == 1);

    template <class T>
    static std::pair<T, std::size_t> pick(const T* layer, std::size_t n) noexcept {
        std::array<std::pair<T, std::size_t>, M> pivots;
        for (auto& pivot : pivots) {
            const auto pos = random_range(n);
            pivot = {layer[pos], pos};
        }
        insertion_sort_shift_left(pivots.data(), M, 1, std::less<>{});
        return pivots[M / 2];
    }
};

}
