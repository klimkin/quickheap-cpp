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

// Pushes fresh rows under an allocation budget until one push throws; returns false if
// none does.
template <typename T>
bool push_until_failure(quickheap::KvSimdQuickHeap<T>& heap, int budget, T& next_key) {
    for (int attempt = 0; attempt < 1024; ++attempt) {
        allocations_before_failure = budget;
        try {
            heap.push(next_key, static_cast<T>(next_key * 10));
        } catch (const std::bad_alloc&) {
            allocations_before_failure = -1;
            return true;
        }
        allocations_before_failure = -1;
        ++next_key;
    }
    return false;
}

template <typename T> bool retries_preserve_rows() {
    quickheap::KvSimdQuickHeap<T> heap;
    T next_key = 5;
    if (!push_until_failure(heap, 1, next_key) || !push_until_failure(heap, 0, next_key)) {
        return false;
    }
    heap.push(next_key, static_cast<T>(next_key * 10));
    for (T key = 5; key <= next_key; ++key) {
        if (heap.pop() != std::pair<T, T>{key, static_cast<T>(key * 10)}) {
            return false;
        }
    }
    return !heap.try_pop();
}

int main() {
    if (!retries_preserve_rows<std::int32_t>() || !retries_preserve_rows<std::uint32_t>() ||
        !retries_preserve_rows<std::int64_t>() || !retries_preserve_rows<std::uint64_t>()) {
        std::cerr << "key-value allocation retry lost rows\n";
        return 1;
    }
    std::cout << "key-value allocation retries preserve rows\n";
}
