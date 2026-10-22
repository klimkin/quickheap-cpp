#pragma once

#include "simd.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace quickheap::detail {

template <class T>
void reserve_geometrically(std::vector<T>& values, std::size_t required) {
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
    static_assert(Columns == 1 || Columns == 2);
    friend class BucketHeap<T, Columns>;
    using Row = std::array<T, Columns>;

    std::array<std::vector<T>, Columns> columns_;

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

    void sort_descending() noexcept {
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
};

template <class T, std::size_t Columns>
class BucketHeap {
public:
    using Row = std::array<T, Columns>;

    BucketHeap() : buckets_(128) { pivots_.reserve(128); }

    void push(Row value) {
        const auto index = push_position(pivots_.data(), pivots_.size(), value[0]);
        auto& bucket = buckets_[index];
        bucket.reserve_rows(bucket.size() + 1);
        if (index == pivots_.size() && bucket.size() < terminal_limit) {
            bucket.insert_descending_prepared(value);
        } else {
            bucket.append_prepared(value);
        }
        ++size_;
    }

    std::optional<Row> pop() {
        if (size_ == 0) {
            return std::nullopt;
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
        return result;
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
    static constexpr std::size_t terminal_limit = 16;
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

    T sample_pivot(const Bucket<T, Columns>& bucket) noexcept {
        const auto& keys = bucket.columns_[0];
        auto first = keys[static_cast<std::size_t>(random() % keys.size())];
        auto second = keys[static_cast<std::size_t>(random() % keys.size())];
        const auto third = keys[static_cast<std::size_t>(random() % keys.size())];
        if (first > second) {
            std::swap(first, second);
        }
        return std::max(first, std::min(second, third));
    }

    void split_terminal() {
        const auto index = pivots_.size();
        reserve_geometrically(pivots_, index + 1);
        if (index + 1 == buckets_.size()) {
            buckets_.emplace_back();
        }

        auto& current = buckets_[index];
        auto& lower = buckets_[index + 1];
        const auto count = current.size();
        assert(count > terminal_limit);
        assert(lower.size() == 0);
        lower.reserve_rows(count);
        lower.resize_prepared(count);

        const auto pivot = sample_pivot(current);
        const auto split = partition(current.columns_[0].data(), current.values(),
                                     lower.columns_[0].data(), lower.values(), count,
                                     pivot, static_cast<std::size_t>(random()));
        assert(split.upper != 0 && split.lower != 0);
        assert(split.upper + split.lower == count);
        current.resize_prepared(split.upper);
        lower.resize_prepared(split.lower);
        pivots_.push_back(pivot);
    }
};

}
