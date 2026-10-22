#ifndef QUICKHEAP_HPP
#define QUICKHEAP_HPP

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "key_value.hpp"

extern "C" {
#include "quickheap.h"
}

namespace quickheap {
namespace detail {

template <typename T> struct Ops;

#define QUICKHEAP_DEFINE_OPS(T, Handle, prefix)                                \
    template <> struct Ops<T> {                                                \
        using Handle_t = Handle;                                               \
        static Handle_t *create() { return prefix##_new(); }                   \
        static void destroy(Handle_t *h) { prefix##_free(h); }                 \
        static void push(Handle_t *h, T v) { prefix##_push(h, v); }            \
        static bool pop(Handle_t *h, T *out) { return prefix##_pop(h, out); }  \
        static std::size_t len(const Handle_t *h) { return prefix##_len(h); }  \
        static bool is_empty(const Handle_t *h) {                              \
            return prefix##_is_empty(h);                                       \
        }                                                                      \
        static std::size_t capacity(const Handle_t *h) {                       \
            return prefix##_capacity(h);                                       \
        }                                                                      \
    };

QUICKHEAP_DEFINE_OPS(std::int32_t, QuickHeapI32, quickheap_i32)
QUICKHEAP_DEFINE_OPS(std::uint32_t, QuickHeapU32, quickheap_u32)
QUICKHEAP_DEFINE_OPS(std::int64_t, QuickHeapI64, quickheap_i64)
QUICKHEAP_DEFINE_OPS(std::uint64_t, QuickHeapU64, quickheap_u64)

#undef QUICKHEAP_DEFINE_OPS

}

/// A SIMD-based min-priority queue.
///
/// `T` must be one of int32_t, uint32_t, int64_t, uint64_t.
///
/// Move-only. Copying is unsupported.
template <typename T> class Heap {
  public:
    using Ops = detail::Ops<T>;
    using Handle = typename Ops::Handle_t;

    Heap() : ptr_(Ops::create()) {}

    ~Heap() {
        if (ptr_ != nullptr) {
            Ops::destroy(ptr_);
        }
    }

    Heap(const Heap &) = delete;
    Heap &operator=(const Heap &) = delete;

    Heap(Heap &&other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }

    Heap &operator=(Heap &&other) noexcept {
        if (this != &other) {
            if (ptr_ != nullptr) {
                Ops::destroy(ptr_);
            }
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    /// Push `value` onto the heap.
    void push(T value) { Ops::push(ptr_, value); }

    /// Pop and return the smallest value, or `std::nullopt` if empty.
    std::optional<T> try_pop() {
        T out{};
        if (Ops::pop(ptr_, &out)) {
            return out;
        }
        return std::nullopt;
    }

    /// Pop and return the smallest value.
    ///
    /// Precondition: `!empty()`. Calling this on an empty heap is undefined
    /// behaviour (checked with an assertion in debug builds).
    T pop() {
        T out{};
        [[maybe_unused]] bool ok = Ops::pop(ptr_, &out);
        assert(ok && "quickheap::Heap::pop() called on an empty heap");
        return out;
    }

    /// Number of elements currently in the heap.
    std::size_t size() const { return Ops::len(ptr_); }

    /// Whether the heap has no elements.
    bool empty() const { return Ops::is_empty(ptr_); }

    /// Total capacity currently allocated over all internal buckets.
    std::size_t capacity() const { return Ops::capacity(ptr_); }

  private:
    Handle *ptr_;
};

}

#endif
