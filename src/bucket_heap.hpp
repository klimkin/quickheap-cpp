#pragma once

#include "simd.hpp"

#include "hwy/base.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace quickheap::detail {

inline constexpr std::size_t terminal_limit = 16;

template <class T>
class DefaultInitAllocator {
public:
    using value_type = T;
    using is_always_equal = std::true_type;

    DefaultInitAllocator() noexcept = default;

    template <class U>
    DefaultInitAllocator(const DefaultInitAllocator<U>&) noexcept {}

    T* allocate(std::size_t count) {
        return std::allocator<T>{}.allocate(count);
    }

    void deallocate(T* storage, std::size_t count) noexcept {
        std::allocator<T>{}.deallocate(storage, count);
    }

    template <class U>
    void construct(U* object) noexcept(std::is_nothrow_default_constructible_v<U>) {
        ::new (static_cast<void*>(object)) U;
    }

    template <class U, class... Args>
        requires(sizeof...(Args) != 0)
    void construct(U* object, Args&&... args) {
        std::construct_at(object, std::forward<Args>(args)...);
    }

    template <class U>
    bool operator==(const DefaultInitAllocator<U>&) const noexcept {
        return true;
    }
};

template <class T, class Allocator>
void reserve_geometrically(std::vector<T, Allocator>& values, std::size_t required) {
    if (required <= values.capacity()) {
        return;
    }
    const auto capacity = values.capacity();
    const auto grown = capacity > values.max_size() - capacity
                           ? values.max_size()
                           : capacity * 2;
    values.reserve(std::max(required, grown));
}

template <class T, std::size_t Columns>
class BucketHeap;

template <class T, std::size_t Columns>
class Bucket {
    static_assert(std::is_integral_v<T> && (sizeof(T) == 4 || sizeof(T) == 8));
    static_assert(Columns == 1 || Columns == 2);
    friend class BucketHeap<T, Columns>;
    using Row = std::array<T, Columns>;

    std::array<std::vector<T, DefaultInitAllocator<T>>, Columns> columns_;

    std::size_t size() const noexcept { return columns_[0].size(); }

    std::size_t capacity() const noexcept {
        std::size_t result = 0;
        for (const auto& column : columns_) {
            result += column.capacity();
        }
        return result;
    }

    void reserve_rows(std::size_t count) {
        for (auto& column : columns_) {
            reserve_geometrically(column, count);
        }
    }

    void resize_prepared(std::size_t count) noexcept {
        for (auto& column : columns_) {
            assert(count <= column.capacity());
            column.resize(count);
        }
    }

    Row row(std::size_t index) const noexcept {
        Row result;
        for (std::size_t column = 0; column < Columns; ++column) {
            result[column] = columns_[column][index];
        }
        return result;
    }

    void set_row(std::size_t index, Row value) noexcept {
        for (std::size_t column = 0; column < Columns; ++column) {
            columns_[column][index] = value[column];
        }
    }

    void append_prepared(Row value) noexcept {
        for (std::size_t column = 0; column < Columns; ++column) {
            assert(columns_[column].size() < columns_[column].capacity());
            columns_[column].push_back(value[column]);
        }
    }

    void insert_descending_prepared(Row value) noexcept {
        auto position = size();
        append_prepared(value);
        while (position != 0 && columns_[0][position - 1] < value[0]) {
            set_row(position, row(position - 1));
            --position;
        }
        set_row(position, value);
    }

    HWY_NOINLINE void sort_descending() noexcept {
        if (Columns == 1 && size() > 8) {
            std::array<T, terminal_limit> sorted;
            sorted.fill(std::numeric_limits<T>::lowest());
            std::copy(columns_[0].begin(), columns_[0].end(), sorted.begin());
            for (std::size_t width = 2; width <= terminal_limit; width *= 2) {
                for (std::size_t distance = width / 2; distance != 0; distance /= 2) {
                    for (std::size_t index = 0; index < terminal_limit; ++index) {
                        const auto partner = index ^ distance;
                        if (partner <= index) continue;
                        const auto high = std::max(sorted[index], sorted[partner]);
                        const auto low = std::min(sorted[index], sorted[partner]);
                        sorted[index] = (index & width) == 0 ? high : low;
                        sorted[partner] = (index & width) == 0 ? low : high;
                    }
                }
            }
            std::copy_n(sorted.begin(), size(), columns_[0].begin());
        } else {
            for (std::size_t index = 1; index < size(); ++index) {
                const auto value = row(index);
                auto position = index;
                while (position != 0 && columns_[0][position - 1] < value[0]) {
                    set_row(position, row(position - 1));
                    --position;
                }
                set_row(position, value);
            }
        }
    }

    Row pop_back() noexcept {
        const auto result = row(size() - 1);
        for (auto& column : columns_) {
            column.pop_back();
        }
        return result;
    }

    T* values() noexcept {
        if constexpr (Columns == 2) {
            return columns_[1].data();
        } else {
            return nullptr;
        }
    }

    void split_into(Bucket& lower, T pivot, std::size_t salt) {
        const auto count = size();
        assert(lower.size() == 0);
        lower.reserve_rows(count);
        lower.resize_prepared(count);

        const auto split = partition(columns_[0].data(), values(),
                                     lower.columns_[0].data(), lower.values(),
                                     count, pivot, salt);
        assert(split.upper != 0 && split.lower != 0);
        assert(split.upper + split.lower == count);
        resize_prepared(split.upper);
        lower.resize_prepared(split.lower);
    }
};

template <class T, std::size_t Columns>
class BucketHeap {
public:
    using Row = std::array<T, Columns>;

    BucketHeap() : buckets_(128) { pivots_.reserve(128); }

    void push(Row value) {
        const auto index = pivots_.empty() || pivots_.back() > value[0]
                               ? pivots_.size()
                               : push_position(pivots_.data(), pivots_.size(), value[0]);
        auto& bucket = buckets_[index];
        for (const auto& column : bucket.columns_) {
            if (bucket.size() == column.capacity()) {
                bucket.reserve_rows(bucket.size() + 1);
                break;
            }
        }
        if (index == pivots_.size() && bucket.size() < terminal_limit) {
            bucket.insert_descending_prepared(value);
        } else {
            bucket.append_prepared(value);
        }
        ++size_;
    }

    bool pop_into(Row& output) {
        if (size_ == 0) {
            return false;
        }
        if (buckets_[pivots_.size()].size() > terminal_limit) {
            do {
                split_terminal();
            } while (buckets_[pivots_.size()].size() > terminal_limit);
            buckets_[pivots_.size()].sort_descending();
        }

        auto& bucket = buckets_[pivots_.size()];
        assert(bucket.size() != 0);
        const auto result = bucket.pop_back();
        if (bucket.size() == 0 && !pivots_.empty()) {
            pivots_.pop_back();
            auto& terminal = buckets_[pivots_.size()];
            if (terminal.size() <= terminal_limit) {
                terminal.sort_descending();
            }
        }
        --size_;
        output = result;
        return true;
    }

    std::size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }

    std::size_t capacity() const noexcept {
        std::size_t result = 0;
        for (const auto& bucket : buckets_) {
            result += bucket.capacity();
        }
        return result;
    }

private:
    std::vector<T> pivots_;
    std::vector<Bucket<T, Columns>> buckets_;
    std::size_t size_ = 0;
    std::uint64_t random_state_ = UINT64_C(0x243f6a8885a308d3);

    std::uint64_t random() noexcept {
        auto value = (random_state_ += UINT64_C(0x9e3779b97f4a7c15));
        value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
        return value ^ (value >> 31);
    }

    std::size_t random_index(std::size_t count) noexcept {
        std::uint64_t index;
        hwy::Mul128(random(), static_cast<std::uint64_t>(count), &index);
        return static_cast<std::size_t>(index);
    }

    T sample_pivot(const Bucket<T, Columns>& bucket) noexcept {
        const auto& keys = bucket.columns_[0];
        auto first = keys[random_index(keys.size())];
        auto second = keys[random_index(keys.size())];
        const auto third = keys[random_index(keys.size())];
        if (first > second) {
            std::swap(first, second);
        }
        return std::max(first, std::min(second, third));
    }

    HWY_NOINLINE void split_terminal() {
        const auto index = pivots_.size();
        reserve_geometrically(pivots_, index + 1);
        if (index + 1 == buckets_.size()) {
            buckets_.emplace_back();
        }

        auto& current = buckets_[index];
        auto& lower = buckets_[index + 1];
        assert(current.size() > terminal_limit);
        const auto pivot = sample_pivot(current);
        current.split_into(lower, pivot, static_cast<std::size_t>(random()));
        pivots_.push_back(pivot);
    }
};

}
