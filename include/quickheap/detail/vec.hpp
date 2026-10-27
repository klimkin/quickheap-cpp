#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

namespace quickheap::detail {

// Mirrors Rust `Vec<T>` for `Copy` element types: amortized growth, `set_len` over spare
// capacity, and memmove-based `insert`. Storage comes from `operator new` instead of
// `realloc` so a failed allocation throws `std::bad_alloc` and leaves the vector intact.
template <class T>
class Vec {
    static_assert(std::is_trivially_copyable_v<T>);

    T* ptr_ = nullptr;
    std::size_t len_ = 0;
    std::size_t cap_ = 0;

    static constexpr std::size_t min_non_zero_cap = sizeof(T) == 1 ? 8 : sizeof(T) <= 1024 ? 4 : 1;

    void grow_to(std::size_t cap) {
        auto* ptr = static_cast<T*>(::operator new(cap * sizeof(T)));
        if (len_ != 0) {
            std::memcpy(ptr, ptr_, len_ * sizeof(T));
        }
        ::operator delete(ptr_);
        ptr_ = ptr;
        cap_ = cap;
    }

    void grow_amortized(std::size_t additional) {
        if (additional > static_cast<std::size_t>(-1) / sizeof(T) - len_) {
            throw std::bad_alloc();
        }
        grow_to(std::max({cap_ * 2, len_ + additional, min_non_zero_cap}));
    }

public:
    Vec() noexcept = default;

    static Vec with_capacity(std::size_t cap) {
        Vec result;
        if (cap != 0) {
            result.grow_to(cap);
        }
        return result;
    }

    Vec(Vec&& other) noexcept
        : ptr_(std::exchange(other.ptr_, nullptr)),
          len_(std::exchange(other.len_, 0)),
          cap_(std::exchange(other.cap_, 0)) {}

    Vec& operator=(Vec&& other) noexcept {
        swap(other);
        return *this;
    }

    Vec(const Vec&) = delete;
    Vec& operator=(const Vec&) = delete;

    ~Vec() { ::operator delete(ptr_); }

    void swap(Vec& other) noexcept {
        std::swap(ptr_, other.ptr_);
        std::swap(len_, other.len_);
        std::swap(cap_, other.cap_);
    }

    std::size_t size() const noexcept { return len_; }
    std::size_t capacity() const noexcept { return cap_; }
    bool empty() const noexcept { return len_ == 0; }
    T* data() noexcept { return ptr_; }
    const T* data() const noexcept { return ptr_; }
    T& operator[](std::size_t index) noexcept { return ptr_[index]; }
    const T& operator[](std::size_t index) const noexcept { return ptr_[index]; }

    void reserve(std::size_t additional) {
        if (cap_ - len_ < additional) {
            grow_amortized(additional);
        }
    }

    void push(T value) {
        if (len_ == cap_) {
            grow_amortized(1);
        }
        ptr_[len_++] = value;
    }

    T pop() noexcept {
        assert(len_ != 0);
        return ptr_[--len_];
    }

    void insert(std::size_t index, T value) {
        assert(index <= len_);
        if (len_ == cap_) {
            grow_amortized(1);
        }
        if (index < len_) {
            std::memmove(ptr_ + index + 1, ptr_ + index, (len_ - index) * sizeof(T));
        }
        ptr_[index] = value;
        ++len_;
    }

    void clear() noexcept { len_ = 0; }

    void set_len(std::size_t len) noexcept {
        assert(len <= cap_);
        len_ = len;
    }
};

}
