#include "quickheap.hpp"

#include <cstdint>
#include <utility>

int main() {
    quickheap::Heap<std::uint64_t> heap;
    heap.push(9);
    heap.push(2);
    if (heap.pop() != 2 || heap.pop() != 9 || !heap.empty()) {
        return 1;
    }

    quickheap::KvSimdQuickHeap<std::int32_t> pairs;
    pairs.push(9, 42);
    pairs.push(2, 7);
    const auto first = pairs.try_pop();
    const auto second = pairs.try_pop();
    if (!first || !second || *first != std::make_pair(2, 7) ||
        *second != std::make_pair(9, 42) || pairs.try_pop()) {
        return 2;
    }
    return 0;
}
