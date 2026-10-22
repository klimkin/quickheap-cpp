#include "quickheap.h"
#include "key_value.hpp"

#include "bucket_heap.hpp"

#include <cassert>
#include <cstdlib>
#include <memory>
#include <optional>
#include <utility>

namespace {

template <class Function>
auto c_boundary(Function&& function) noexcept -> decltype(function()) {
    try {
        return function();
    } catch (...) {
        std::abort();
    }
}

template <class T>
T& require_pointer(T* pointer) noexcept {
    if (pointer == nullptr) {
        std::abort();
    }
    return *pointer;
}

}

#define QUICKHEAP_DEFINE_C(T, Handle, prefix)                                 \
    struct Handle {                                                         \
        quickheap::detail::BucketHeap<T, 1> heap;                            \
    };                                                                      \
    extern "C" {                                                            \
    Handle* prefix##_new(void) {                                             \
        return c_boundary([] { return new Handle; });                       \
    }                                                                       \
    void prefix##_free(Handle* pointer) {                                    \
        c_boundary([&] { delete &require_pointer(pointer); });              \
    }                                                                       \
    void prefix##_push(Handle* pointer, T value) {                            \
        c_boundary([&] { require_pointer(pointer).heap.push({value}); });     \
    }                                                                       \
    bool prefix##_pop(Handle* pointer, T* output) {                           \
        return c_boundary([&] {                                             \
            auto& heap = require_pointer(pointer).heap;                     \
            auto& destination = require_pointer(output);                   \
            quickheap::detail::BucketHeap<T, 1>::Row result;                \
            if (!heap.pop_into(result)) {                                   \
                return false;                                              \
            }                                                              \
            destination = result[0];                                       \
            return true;                                                   \
        });                                                                 \
    }                                                                       \
    uintptr_t prefix##_len(const Handle* pointer) {                          \
        return c_boundary([&] { return require_pointer(pointer).heap.size(); }); \
    }                                                                       \
    bool prefix##_is_empty(const Handle* pointer) {                          \
        return c_boundary([&] { return require_pointer(pointer).heap.empty(); }); \
    }                                                                       \
    uintptr_t prefix##_capacity(const Handle* pointer) {                     \
        return c_boundary([&] { return require_pointer(pointer).heap.capacity(); }); \
    }                                                                       \
    }

QUICKHEAP_DEFINE_C(std::int32_t, QuickHeapI32, quickheap_i32)
QUICKHEAP_DEFINE_C(std::uint32_t, QuickHeapU32, quickheap_u32)
QUICKHEAP_DEFINE_C(std::int64_t, QuickHeapI64, quickheap_i64)
QUICKHEAP_DEFINE_C(std::uint64_t, QuickHeapU64, quickheap_u64)

#undef QUICKHEAP_DEFINE_C

namespace quickheap {

template <class T>
class KvSimdQuickHeap<T>::Impl {
public:
    detail::BucketHeap<T, 2> heap;
};

template <class T>
KvSimdQuickHeap<T>::KvSimdQuickHeap() : impl_(std::make_unique<Impl>()) {}

template <class T>
KvSimdQuickHeap<T>::~KvSimdQuickHeap() = default;

template <class T>
KvSimdQuickHeap<T>::KvSimdQuickHeap(KvSimdQuickHeap&&) noexcept = default;

template <class T>
KvSimdQuickHeap<T>& KvSimdQuickHeap<T>::operator=(KvSimdQuickHeap&&) noexcept = default;

template <class T>
void KvSimdQuickHeap<T>::push(T key, T value) {
    impl_->heap.push({key, value});
}

template <class T>
std::optional<std::pair<T, T>> KvSimdQuickHeap<T>::try_pop() {
    typename detail::BucketHeap<T, 2>::Row result;
    if (!impl_->heap.pop_into(result)) {
        return std::nullopt;
    }
    return std::pair<T, T>{result[0], result[1]};
}

template <class T>
std::pair<T, T> KvSimdQuickHeap<T>::pop() {
    const auto result = try_pop();
    assert(result && "quickheap::KvSimdQuickHeap::pop() called on an empty heap");
    return *result;
}

template <class T>
std::size_t KvSimdQuickHeap<T>::size() const {
    return impl_->heap.size();
}

template <class T>
bool KvSimdQuickHeap<T>::empty() const {
    return impl_->heap.empty();
}

template <class T>
std::size_t KvSimdQuickHeap<T>::capacity() const {
    return impl_->heap.capacity();
}

template class KvSimdQuickHeap<std::int32_t>;
template class KvSimdQuickHeap<std::uint32_t>;
template class KvSimdQuickHeap<std::int64_t>;
template class KvSimdQuickHeap<std::uint64_t>;

}
