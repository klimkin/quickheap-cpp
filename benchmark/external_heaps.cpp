#include "external_heaps.hpp"

#include <algorithm>
#include <ctime>
#include <iostream>
#include <limits>
#include <stdexcept>

#include <s3q/s3q.hpp>

#include <spq/knheap.h>
#include <spq/knheap.C>

#include <MinRandQH2.hpp>

namespace quickheap::bench {

namespace {

struct NoValue {};

template <class T> struct S3QConfig : s3q::DefaultCfg {
    using Item = T;
};

int original_capacity(std::size_t capacity) {
    // The original uses signed int positions, 2*capacity, and power-of-two rounding.
    if (capacity > (1U << 29)) {
        throw std::length_error("original Quickheap capacity exceeds its signed index range");
    }
    return static_cast<int>(std::max<std::size_t>(capacity, 8));
}

} // namespace

template <class T> struct SequenceHeap<T>::Impl {
    KNHeap<T, NoValue> heap{std::numeric_limits<T>::max(), std::numeric_limits<T>::lowest()};
};

template <class T> SequenceHeap<T>::SequenceHeap(std::size_t) : impl_(std::make_unique<Impl>()) {}
template <class T> SequenceHeap<T>::~SequenceHeap() = default;
template <class T> SequenceHeap<T>::SequenceHeap(SequenceHeap&&) noexcept = default;
template <class T> SequenceHeap<T>& SequenceHeap<T>::operator=(SequenceHeap&&) noexcept = default;

template <class T> void SequenceHeap<T>::push(T key) {
    if (key == std::numeric_limits<T>::max() || key == std::numeric_limits<T>::lowest()) {
        throw std::domain_error("SequenceHeap reserves minimum and maximum keys as sentinels");
    }
    if (size() == static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::length_error("SequenceHeap exceeds its signed index range");
    }
    impl_->heap.insert(key, NoValue{});
}

template <class T> std::optional<T> SequenceHeap<T>::try_pop() {
    if (size() == 0) return std::nullopt;
    T key;
    NoValue value;
    impl_->heap.deleteMin(&key, &value);
    return key;
}

template <class T> std::size_t SequenceHeap<T>::size() const {
    return static_cast<std::size_t>(impl_->heap.getSize());
}

template <class T> struct S3QHeap<T>::Impl {
    s3q::PriorityQueue<S3QConfig<T>> heap;
};

template <class T> S3QHeap<T>::S3QHeap(std::size_t) : impl_(std::make_unique<Impl>()) {}
template <class T> S3QHeap<T>::~S3QHeap() = default;
template <class T> S3QHeap<T>::S3QHeap(S3QHeap&&) noexcept = default;
template <class T> S3QHeap<T>& S3QHeap<T>::operator=(S3QHeap&&) noexcept = default;
template <class T> void S3QHeap<T>::push(T key) {
    if (key == std::numeric_limits<T>::max() || key == std::numeric_limits<T>::lowest()) {
        throw std::domain_error("S3Q reserves minimum and maximum keys as sentinels");
    }
    impl_->heap.push(key);
}
template <class T> std::optional<T> S3QHeap<T>::try_pop() {
    if (impl_->heap.empty()) return std::nullopt;
    return impl_->heap.pop();
}
template <class T> std::size_t S3QHeap<T>::size() const { return impl_->heap.size(); }

template <class T> struct OriginalQuickHeap<T>::Impl {
    explicit Impl(std::size_t capacity) : heap(original_capacity(capacity), false) {}
    MinRandQH2<T> heap;
};

template <class T>
OriginalQuickHeap<T>::OriginalQuickHeap(std::size_t capacity) : impl_(std::make_unique<Impl>(capacity)) {}
template <class T> OriginalQuickHeap<T>::~OriginalQuickHeap() = default;
template <class T> OriginalQuickHeap<T>::OriginalQuickHeap(OriginalQuickHeap&&) noexcept = default;
template <class T> OriginalQuickHeap<T>& OriginalQuickHeap<T>::operator=(OriginalQuickHeap&&) noexcept = default;
template <class T> void OriginalQuickHeap<T>::push(T key) {
    if (!impl_->heap.insert(key)) {
        throw std::length_error("original Quickheap reached its fixed capacity; increase benchmark capacity");
    }
}
template <class T> std::optional<T> OriginalQuickHeap<T>::try_pop() {
    if (impl_->heap.isEmpty()) return std::nullopt;
    return impl_->heap.extractMin();
}
template <class T> std::size_t OriginalQuickHeap<T>::size() const {
    return static_cast<std::size_t>(impl_->heap.size());
}

template class SequenceHeap<std::int32_t>;
template class SequenceHeap<std::int64_t>;
template class SequenceHeap<std::uint32_t>;
template class SequenceHeap<std::uint64_t>;
template class S3QHeap<std::int32_t>;
template class S3QHeap<std::int64_t>;
template class S3QHeap<std::uint32_t>;
template class S3QHeap<std::uint64_t>;
template class OriginalQuickHeap<std::int32_t>;
template class OriginalQuickHeap<std::int64_t>;
template class OriginalQuickHeap<std::uint32_t>;
template class OriginalQuickHeap<std::uint64_t>;

} // namespace quickheap::bench
