#pragma once

#include <algorithm>
#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "branchless_sort.hpp"

namespace quickheap::bench {

struct ScalarCounters {
    std::uint64_t push_comparisons = 0;
    std::uint64_t pop_comparisons = 0;
    std::uint64_t partitions = 0;
    std::uint64_t elements_partitioned = 0;
    std::uint64_t retries = 0;
    std::uint64_t samples = 0;
    bool comparisons_exact = true;

    std::uint64_t comparisons() const noexcept {
        return push_comparisons + pop_comparisons;
    }
};

template <class T, std::size_t M = 1, bool Perfect = false,
          bool Linear = false, bool Count = false>
class ScalarHeap {
    static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>);
    static_assert(M == 1 || M == 3);

public:
    using Item = T;

    struct PartitionEvent {
        T pivot;
        std::size_t pivot_index;
        bool retry;
        std::span<const T> upper;
        std::span<const T> lower;
        std::uint64_t push_comparisons;
        std::uint64_t pop_comparisons;
    };

    explicit ScalarHeap(std::span<const std::uint64_t> scripted_indices = {},
                        std::uint64_t seed = UINT64_C(0x243f6a8885a308d3))
        : buckets_(1), random_state_(seed),
          scripted_indices_(scripted_indices.begin(), scripted_indices.end()) {
        pivots_.reserve(128);
        buckets_.reserve(128);
        counters_.comparisons_exact = Count;
    }

    void reserve(std::size_t count) { buckets_.front().reserve(count); }

    void push(T key) {
        const auto index = find_bucket(key);
        buckets_[index].push_back(key);
        ++size_;
    }

    T pop() {
        if (size_ == 0) throw std::runtime_error("scalar heap popped while empty");
        return pop_nonempty(NoObserver{});
    }

    std::optional<T> try_pop() {
        if (size_ == 0) return std::nullopt;
        return pop_nonempty(NoObserver{});
    }

    template <class Observer>
    std::optional<T> try_pop(Observer&& observer) {
        if (size_ == 0) return std::nullopt;
        return pop_nonempty(observer);
    }

    std::size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }
    T last_payload() const noexcept { return T{}; }

    std::size_t capacity() const noexcept {
        std::size_t count = 0;
        for (const auto& bucket : buckets_) count += bucket.capacity();
        return count;
    }

    const ScalarCounters& counters() const noexcept { return counters_; }
    std::uint64_t push_comparisons() const noexcept {
        return counters_.push_comparisons;
    }
    std::uint64_t pop_comparisons() const noexcept {
        return counters_.pop_comparisons;
    }

    void reset_comparisons() noexcept {
        counters_ = {};
        counters_.comparisons_exact = Count;
    }

    void reset_counters() noexcept { reset_comparisons(); }

    std::span<const T> pivots() const noexcept { return pivots_; }
    std::span<const std::vector<T>> buckets() const noexcept {
        return {buckets_.data(), pivots_.size() + 1};
    }

    bool valid() const noexcept {
        std::size_t count = 0;
        for (std::size_t i = 0; i <= pivots_.size(); ++i) {
            const auto& bucket = buckets_[i];
            if (bucket.empty() && (i != 0 || size_ != 0)) return false;
            count += bucket.size();
            for (const auto key : bucket) {
                if (i != 0 && key > pivots_[i - 1]) return false;
                if (i != pivots_.size() && key < pivots_[i]) return false;
            }
        }
        return count == size_ &&
            std::is_sorted(pivots_.begin(), pivots_.end(), std::greater<T>{});
    }

private:
    struct NoObserver {};
    struct Sample { T key; std::size_t index; };

    std::vector<T> pivots_;
    std::vector<T> merge_scratch_;
    std::vector<std::vector<T>> buckets_;
    std::size_t size_ = 0;
    std::uint64_t random_state_;
    std::vector<std::uint64_t> scripted_indices_;
    std::size_t script_position_ = 0;
    ScalarCounters counters_;

    bool push_less(T left, T right) noexcept {
        if constexpr (Count) ++counters_.push_comparisons;
        return left < right;
    }

    std::size_t find_bucket(T key) {
        if constexpr (Linear) {
            std::size_t index = 0;
            while (index < pivots_.size() && !push_less(pivots_[index], key)) {
                ++index;
            }
            return index;
        } else {
            std::size_t first = 0;
            auto count = pivots_.size();
            while (count != 0) {
                const auto step = count / 2;
                const auto middle = first + step;
                if (push_less(pivots_[middle], key)) {
                    count = step;
                } else {
                    first = middle + 1;
                    count -= step + 1;
                }
            }
            return first;
        }
    }

    std::uint64_t random() noexcept {
        auto value = (random_state_ += UINT64_C(0x9e3779b97f4a7c15));
        value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
        return value ^ (value >> 31);
    }

    std::size_t sample_index(std::size_t count) {
        if (script_position_ != scripted_indices_.size()) {
            const auto index = scripted_indices_[script_position_++];
            if (index >= count) {
                throw std::invalid_argument("scalar pivot sample exceeds bucket size");
            }
            return static_cast<std::size_t>(index);
        }
        const auto bound = static_cast<std::uint64_t>(count);
        const auto threshold = (std::uint64_t{0} - bound) % bound;
        auto value = random();
        while (value < threshold) value = random();
        return static_cast<std::size_t>(value % bound);
    }

    bool sample_less(const Sample& left, const Sample& right) noexcept {
        if constexpr (Count) ++counters_.pop_comparisons;
        const auto order = left.key <=> right.key;
        return order < 0 || (order == 0 && left.index < right.index);
    }

    // A terminal bucket is a sorted run plus freshly pushed keys, so sort only the
    // unsorted tail and merge it into the run.
    void sort_bucket(std::vector<T>& bucket) {
        const auto first = bucket.begin();
        const auto run_end = std::is_sorted_until(first, bucket.end());
        if (run_end == bucket.end()) return;
        branchless_sort(&*run_end, static_cast<std::size_t>(bucket.end() - run_end));
        const auto merge_begin = std::upper_bound(first, run_end, *run_end);
        merge_scratch_.assign(merge_begin, run_end);
        std::merge(merge_scratch_.begin(), merge_scratch_.end(), run_end, bucket.end(),
                   merge_begin);
    }

    std::size_t select_pivot(std::vector<T>& bucket) {
        if constexpr (Perfect) {
            sort_bucket(bucket);
            return (bucket.size() - 1) / 2;
        } else {
            std::array<Sample, M> samples{};
            for (auto& sample : samples) {
                sample.index = sample_index(bucket.size());
                sample.key = bucket[sample.index];
            }
            if constexpr (Count) counters_.samples += M;
            for (std::size_t i = 1; i < M; ++i) {
                const auto sample = samples[i];
                auto position = i;
                while (position != 0 && sample_less(sample, samples[position - 1])) {
                    samples[position] = samples[position - 1];
                    --position;
                }
                samples[position] = sample;
            }
            return samples[M / 2].index;
        }
    }

    template <class Observer>
    void split_terminal(Observer& observer) {
        const auto index = pivots_.size();
        if (index + 1 == buckets_.size()) buckets_.emplace_back();
        auto& upper = buckets_[index];
        auto& lower = buckets_[index + 1];
        const auto count = upper.size();
        const auto pivot_index = select_pivot(upper);
        const auto pivot = upper[pivot_index];
        if constexpr (Perfect && !Count) {
            // Counting builds retain the literal split for the Rust comparison oracle.
            const auto middle = upper.begin() + pivot_index + 1;
            lower.assign(upper.begin(), middle);
            upper.erase(upper.begin(), middle);
        } else if constexpr (Perfect) {
            // Sorted keys make the split branch predictable.
            lower.clear();
            lower.reserve(count);
            std::size_t upper_count = 0;
            for (std::size_t i = 0; i < count; ++i) {
                const auto key = upper[i];
                if constexpr (Count) ++counters_.pop_comparisons;
                const auto goes_lower = i <= pivot_index ? key <= pivot : key < pivot;
                if (goes_lower) lower.push_back(key);
                else upper[upper_count++] = key;
            }
            upper.resize(upper_count);
        } else {
            // Write each key to both sides and advance one cursor, so the split has no branch.
            lower.resize(count);
            T* const lower_data = lower.data();
            T* const upper_data = upper.data();
            std::size_t lower_count = 0;
            std::size_t upper_count = 0;
            for (std::size_t i = 0; i < count; ++i) {
                const auto key = upper_data[i];
                if constexpr (Count) ++counters_.pop_comparisons;
                const bool goes_lower = i <= pivot_index ? key <= pivot : key < pivot;
                lower_data[lower_count] = key;
                upper_data[upper_count] = key;
                lower_count += goes_lower;
                upper_count += !goes_lower;
            }
            lower.resize(lower_count);
            upper.resize(upper_count);
        }
        const auto retry = upper.empty();
        if constexpr (Count) {
            ++counters_.partitions;
            counters_.elements_partitioned += count;
            if (retry) ++counters_.retries;
        }
        if constexpr (!std::is_same_v<std::remove_cvref_t<Observer>, NoObserver>) {
            observer(PartitionEvent{pivot, pivot_index, retry, upper, lower,
                counters_.push_comparisons, counters_.pop_comparisons});
        }
        if (retry) upper.swap(lower);
        else pivots_.push_back(pivot);
    }

    template <class Observer>
    T pop_nonempty(Observer&& observer) {
        constexpr bool skip_pair_split = Perfect && !Count &&
            std::is_same_v<std::remove_cvref_t<Observer>, NoObserver>;
        while (true) {
            auto& bucket = buckets_[pivots_.size()];
            if constexpr (skip_pair_split) {
                // Splitting two keys and popping the lower half leaves only the larger key.
                if (bucket.size() == 2) {
                    const auto low = std::min(bucket[0], bucket[1]);
                    bucket[0] = std::max(bucket[0], bucket[1]);
                    bucket.pop_back();
                    --size_;
                    return low;
                }
            }
            if (bucket.size() <= 1) break;
            split_terminal(observer);
        }
        auto& bucket = buckets_[pivots_.size()];
        const auto key = bucket.back();
        bucket.pop_back();
        --size_;
        if (!pivots_.empty()) pivots_.pop_back();
        return key;
    }
};

}
