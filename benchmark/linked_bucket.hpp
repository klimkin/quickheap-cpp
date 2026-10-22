#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <list>
#include <span>
#include <utility>
#include <vector>

namespace quickheap::bench {

template <class T, std::size_t BlockSize = 112, std::size_t BlockCapacity = 128>
class LinkedBucket {
    static_assert(BlockSize > 0 && BlockSize <= BlockCapacity);
    struct alignas(64) Block {
        std::array<T, BlockCapacity> keys;
        std::size_t size = 0;
    };
    std::list<Block> blocks_;
    std::size_t size_ = 0;

public:
    static constexpr std::size_t block_size = BlockSize;
    static constexpr std::size_t block_capacity = BlockCapacity;

    LinkedBucket() = default;
    explicit LinkedBucket(const std::vector<T>& keys) { append(keys); }

    std::size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }
    std::size_t capacity() const noexcept { return blocks_.size() * BlockCapacity; }
    void clear() noexcept { blocks_.clear(); size_ = 0; }
    void swap(LinkedBucket& other) noexcept {
        blocks_.swap(other.blocks_);
        std::swap(size_, other.size_);
    }
    void push_back(T key) {
        if (blocks_.empty() || blocks_.back().size == BlockSize) {
            blocks_.emplace_back();
        }
        auto& tail = blocks_.back();
        tail.keys[tail.size++] = key;
        ++size_;
    }
    T back() const noexcept { return blocks_.back().keys[blocks_.back().size - 1]; }
    void pop_back() noexcept {
        assert(!empty());
        if (--blocks_.back().size == 0) {
            blocks_.pop_back();
        }
        --size_;
    }
    T& operator[](std::size_t index) noexcept {
        assert(index < size_);
        auto it = blocks_.begin();
        std::advance(it, index / BlockSize);
        return it->keys[index % BlockSize];
    }
    const T& operator[](std::size_t index) const noexcept {
        assert(index < size_);
        auto it = blocks_.begin();
        std::advance(it, index / BlockSize);
        return it->keys[index % BlockSize];
    }
    template <class Function> void for_blocks(Function&& function) const {
        for (const auto& block : blocks_) {
            function(std::span<const T>(block.keys.data(), block.size));
        }
    }
    void append(std::span<const T> keys) {
        for (auto key : keys) {
            push_back(key);
        }
    }
    void concat(const LinkedBucket& other) {
        other.for_blocks([this](auto keys) { append(keys); });
    }
    std::vector<T> to_vector() const {
        std::vector<T> result;
        result.reserve(size_);
        for_blocks([&result](auto keys) { result.insert(result.end(), keys.begin(), keys.end()); });
        return result;
    }
    class ConstIterator {
        using BlockIterator = typename std::list<Block>::const_iterator;
        BlockIterator block_;
        std::size_t position_ = 0;
    public:
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;
        using iterator_category = std::forward_iterator_tag;
        ConstIterator() = default;
        explicit ConstIterator(BlockIterator block) : block_(block) {}
        reference operator*() const { return block_->keys[position_]; }
        ConstIterator& operator++() {
            if (++position_ == block_->size) {
                ++block_;
                position_ = 0;
            }
            return *this;
        }
        ConstIterator operator++(int) { auto result = *this; ++*this; return result; }
        bool operator==(const ConstIterator& other) const {
            return block_ == other.block_ && position_ == other.position_;
        }
    };
    ConstIterator begin() const { return ConstIterator(blocks_.begin()); }
    ConstIterator end() const { return ConstIterator(blocks_.end()); }
};

}
