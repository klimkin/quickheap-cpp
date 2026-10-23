#include "key_value.hpp"

#include <cstdlib>
#include <iostream>
#include <new>

namespace {
int allocations_before_failure = -1;
}

void* operator new(std::size_t count) {
    if (allocations_before_failure == 0) throw std::bad_alloc();
    if (allocations_before_failure > 0) --allocations_before_failure;
    if (auto* storage = std::malloc(count == 0 ? 1 : count)) return storage;
    throw std::bad_alloc();
}

void operator delete(void* storage) noexcept { std::free(storage); }
void operator delete(void* storage, std::size_t) noexcept { std::free(storage); }

template <typename T> bool retries_preserve_rows() {
    quickheap::KvSimdQuickHeap<T> heap;
    heap.push(5, 50);
    allocations_before_failure = 1;
    try {
        heap.push(6, 60);
        allocations_before_failure = -1;
        return false;
    } catch (const std::bad_alloc&) {}
    allocations_before_failure = 0;
    try {
        heap.push(6, 60);
        allocations_before_failure = -1;
        return false;
    } catch (const std::bad_alloc&) {}
    allocations_before_failure = -1;
    heap.push(6, 60);
    return heap.pop() == std::pair<T, T>{5, 50} &&
           heap.pop() == std::pair<T, T>{6, 60} && !heap.try_pop();
}

int main() {
    if (!retries_preserve_rows<std::int32_t>() || !retries_preserve_rows<std::uint32_t>() ||
        !retries_preserve_rows<std::int64_t>() || !retries_preserve_rows<std::uint64_t>()) {
        std::cerr << "key-value allocation retry lost rows\n";
        return 1;
    }
    std::cout << "key-value allocation retries preserve rows\n";
}
