#pragma once

#include "simd.hpp"
#include "experiment_simd.hpp"
#include "linked_bucket.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace quickheap::bench {

enum class Pivot { Random, Median3, Median5, Perfect, Log2, Cbrt, Table };
enum class Search { Linear, Binary, Simd };
enum class Partition { Scalar, Simd };
enum class Rebalance { None, PivotForgetting, NaiveLog };

struct ExperimentalConfig {
    Pivot pivot = Pivot::Median3;
    Search search = Search::Binary;
    Partition partition = Partition::Scalar;
    std::size_t terminal_size = 16;
    Rebalance rebalance = Rebalance::None;
    std::size_t rebalance_factor = 2;
    std::size_t rebalance_interval = 2048;
    std::size_t log_threshold = 10;
    std::size_t sample_scale = 1;
    std::size_t sample_offset = 0;
    std::size_t partition_count = 2;
};

struct ExperimentalCounters {
    std::uint64_t push_comparisons = 0;
    std::uint64_t pop_comparisons = 0;
    std::uint64_t partitions = 0;
    std::uint64_t elements_partitioned = 0;
    std::uint64_t rebalances = 0;
    std::uint64_t multi_partitions = 0;
    std::uint64_t blocks_partitioned = 0;
    bool comparisons_exact = true;

    std::uint64_t comparisons() const noexcept {
        return push_comparisons + pop_comparisons;
    }
};

template <class T, bool CountComparisons = false, class Bucket = std::vector<T>>
class ExperimentalHeap {
    static_assert(std::is_same_v<T, std::int32_t> ||
                  std::is_same_v<T, std::uint32_t> ||
                  std::is_same_v<T, std::int64_t> ||
                  std::is_same_v<T, std::uint64_t>);

public:
    using Item = T;
    using Config = ExperimentalConfig;

    explicit ExperimentalHeap(Config config = {},
                              std::uint64_t seed = UINT64_C(0x243f6a8885a308d3))
        : config_(config), buckets_(1), random_state_(seed) {
        if (config_.terminal_size == 0 || config_.sample_scale == 0 ||
            config_.rebalance_factor == 0 || config_.log_threshold == 0 ||
            config_.partition_count < 2 || config_.partition_count > 4) {
            throw std::invalid_argument("experimental heap parameters must be positive");
        }
        pivots_.reserve(128);
        buckets_.reserve(128);
        counters_.comparisons_exact = CountComparisons &&
            config_.search != Search::Simd && config_.partition != Partition::Simd;
    }

    static ExperimentalHeap from_layers(std::vector<std::vector<T>> layers,
                                       Config config = {},
                                       std::uint64_t seed = UINT64_C(0x243f6a8885a308d3)) {
        ExperimentalHeap result(config, seed);
        if (layers.empty()) {
            return result;
        }
        for (std::size_t i = 0; i < layers.size(); ++i) {
            if (layers[i].empty()) {
                throw std::invalid_argument("initial heap layers must be nonempty");
            }
            result.size_ += layers[i].size();
            if (i + 1 < layers.size()) {
                const auto pivot = *std::min_element(layers[i].begin(), layers[i].end());
                if (std::any_of(layers[i + 1].begin(), layers[i + 1].end(),
                                [pivot](T key) { return key > pivot; })) {
                    throw std::invalid_argument("initial heap layers must decrease by key");
                }
                result.pivots_.push_back(pivot);
            }
        }
        result.buckets_.clear();
        for (auto& layer : layers) {
            if constexpr (std::is_same_v<Bucket, std::vector<T>>) {
                result.buckets_.push_back(std::move(layer));
            } else {
                result.buckets_.emplace_back(layer);
            }
        }
        result.prepare_terminal();
        result.reset_counters();
        return result;
    }

    void push(T key) {
        if constexpr (CountComparisons) {
            in_push_ = true;
        }
        const auto target = find_bucket(key);
        auto& bucket = buckets_[target];
        if (target == pivots_.size() && bucket.size() < config_.terminal_size) {
            bucket.push_back(key);
            auto pos = bucket.size() - 1;
            while (pos > 0 && less(bucket[pos - 1], key)) {
                bucket[pos] = bucket[pos - 1];
                --pos;
            }
            bucket[pos] = key;
        } else {
            bucket.push_back(key);
        }
        ++size_;
    }

    std::optional<T> try_pop() {
        if (size_ == 0) {
            return std::nullopt;
        }
        if constexpr (CountComparisons) {
            in_push_ = false;
        }
        if (terminal().size() > config_.terminal_size) {
            do {
                split_terminal();
            } while (terminal().size() > config_.terminal_size);
            sort_terminal();
        }
        auto& bucket = terminal();
        const auto result = bucket.back();
        bucket.pop_back();
        --size_;
        if (bucket.empty() && !pivots_.empty()) {
            pivots_.pop_back();
            prepare_terminal();
        }
        if (config_.rebalance != Rebalance::None &&
            ++pops_since_rebalance_ > config_.rebalance_interval) {
            pops_since_rebalance_ = 0;
            rebalance();
        }
        return result;
    }

    std::size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }
    T last_payload() const noexcept { return T{}; }
    const Config& config() const noexcept { return config_; }
    const ExperimentalCounters& counters() const noexcept { return counters_; }
    std::size_t layers() const noexcept { return pivots_.size() + 1; }

    void reset_counters() noexcept {
        const auto exact = counters_.comparisons_exact;
        counters_ = {};
        counters_.comparisons_exact = exact;
    }

    std::size_t capacity() const noexcept {
        std::size_t result = 0;
        for (const auto& bucket : buckets_) {
            result += bucket.capacity();
        }
        return result;
    }

    bool valid() const noexcept {
        std::size_t total = 0;
        for (std::size_t i = 0; i <= pivots_.size(); ++i) {
            const auto& bucket = buckets_[i];
            if (bucket.empty() && (size_ != 0 || i != 0)) {
                return false;
            }
            total += bucket.size();
            for (auto key : bucket) {
                if ((i != 0 && key > pivots_[i - 1]) ||
                    (i != pivots_.size() && key < pivots_[i])) {
                    return false;
                }
            }
        }
        const auto& last = buckets_[pivots_.size()];
        return total == size_ && (last.size() > config_.terminal_size ||
            std::is_sorted(last.begin(), last.end(), std::greater<T>{}));
    }

private:
    Config config_;
    std::vector<T> pivots_;
    std::vector<Bucket> buckets_;
    std::vector<T> samples_;
    std::size_t size_ = 0;
    std::size_t pops_since_rebalance_ = 0;
    std::uint64_t random_state_;
    ExperimentalCounters counters_;
    bool in_push_ = false;

    Bucket& terminal() noexcept { return buckets_[pivots_.size()]; }

    bool less(T left, T right) noexcept {
        if constexpr (CountComparisons) {
            ++(in_push_ ? counters_.push_comparisons : counters_.pop_comparisons);
        }
        return left < right;
    }

    bool equal(T left, T right) noexcept {
        if constexpr (CountComparisons) {
            ++(in_push_ ? counters_.push_comparisons : counters_.pop_comparisons);
        }
        return left == right;
    }

    std::uint64_t random() noexcept {
        auto value = (random_state_ += UINT64_C(0x9e3779b97f4a7c15));
        value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
        return value ^ (value >> 31);
    }

    std::size_t find_bucket(T key) {
        if (config_.search == Search::Simd) {
            return push_position(pivots_.data(), pivots_.size(), key);
        }
        if (config_.search == Search::Linear) {
            std::size_t position = 0;
            while (position < pivots_.size() && less(key, pivots_[position])) {
                ++position;
            }
            return position;
        }
        std::size_t first = 0;
        auto count = pivots_.size();
        while (count != 0) {
            const auto step = count / 2;
            const auto middle = first + step;
            if (less(key, pivots_[middle])) {
                first = middle + 1;
                count -= step + 1;
            } else {
                count = step;
            }
        }
        return first;
    }

    void sort_terminal() {
        auto& bucket = terminal();
        for (std::size_t i = 1; i < bucket.size(); ++i) {
            const auto key = bucket[i];
            auto pos = i;
            while (pos != 0 && less(bucket[pos - 1], key)) {
                bucket[pos] = bucket[pos - 1];
                --pos;
            }
            bucket[pos] = key;
        }
    }

    void prepare_terminal() {
        if (terminal().size() <= config_.terminal_size) {
            sort_terminal();
        }
    }

    std::size_t sample_count(std::size_t n) const {
        constexpr std::array<std::size_t, 32> table = {
            1, 1, 1, 3, 3, 3, 5, 5, 5, 7, 7, 9, 9, 11, 13, 15,
            15, 17, 17, 21, 21, 23, 23, 23, 27, 31, 37, 37, 41, 41, 45, 45};
        constexpr std::array<std::size_t, 32> cbrt = {
            1, 1, 1, 2, 2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 25, 32,
            40, 50, 64, 80, 101, 128, 161, 203, 256, 322, 406, 512,
            645, 812, 1024, 1290};
        const auto bits = static_cast<std::size_t>(std::bit_width(n));
        switch (config_.pivot) {
        case Pivot::Random: return 1;
        case Pivot::Median3: return 3;
        case Pivot::Median5: return 5;
        case Pivot::Log2:
            return (config_.sample_scale * bits + config_.sample_offset) | 1;
        case Pivot::Cbrt:
            return ((bits < cbrt.size() ? cbrt[bits]
                : static_cast<std::size_t>(std::exp2(static_cast<double>(bits) / 3))) /
                config_.sample_scale + config_.sample_offset) | 1;
        case Pivot::Table: return bits < table.size() ? table[bits] : 47;
        case Pivot::Perfect: return 0;
        }
        throw std::logic_error("invalid pivot strategy");
    }

    T sample_pivot(Bucket& bucket) {
        if (config_.pivot == Pivot::Perfect) {
            // Upstream treats oracle selection as free when counting comparisons.
            if constexpr (std::is_same_v<Bucket, std::vector<T>>) {
                std::sort(bucket.begin(), bucket.end());
                return bucket[(bucket.size() - 1) / 2];
            } else {
                auto keys = bucket.to_vector();
                std::sort(keys.begin(), keys.end());
                return keys[(keys.size() - 1) / 2];
            }
        }
        samples_.resize(sample_count(bucket.size()));
        for (auto& sample : samples_) {
            sample = bucket[static_cast<std::size_t>(random() % bucket.size())];
        }
        const auto median = samples_.begin() + samples_.size() / 2;
        std::nth_element(samples_.begin(), median, samples_.end(),
                         [this](T left, T right) { return less(left, right); });
        return *median;
    }

    SplitResult scalar_partition(std::vector<T>& upper,
                                 std::vector<T>& lower, T pivot,
                                 std::size_t salt) {
        std::size_t high_count = 0;
        std::size_t low_count = 0;
        for (std::size_t i = 0; i < upper.size(); ++i) {
            const auto key = upper[i];
            if (less(key, pivot) || (equal(key, pivot) && ((i ^ salt) & 1) == 0)) {
                lower[low_count++] = key;
            } else {
                upper[high_count++] = key;
            }
        }
        if (low_count == 0) {
            std::size_t position = 0;
            while (!equal(upper[position], pivot)) {
                ++position;
            }
            lower[low_count++] = upper[position];
            upper[position] = upper[--high_count];
        } else if (high_count == 0) {
            std::size_t position = 0;
            while (!equal(lower[position], pivot)) {
                ++position;
            }
            upper[high_count++] = lower[position];
            lower[position] = lower[--low_count];
        }
        return {high_count, low_count};
    }

    void split_terminal() {
        if (config_.partition_count > 2 && terminal().size() > 128) {
            multi_split_terminal();
            return;
        }
        if constexpr (!std::is_same_v<Bucket, std::vector<T>>) {
            blocked_split_terminal();
        } else {
        const auto index = pivots_.size();
        if (index + 1 == buckets_.size()) {
            buckets_.emplace_back();
        }
        auto& upper = buckets_[index];
        auto& lower = buckets_[index + 1];
        const auto count = upper.size();
        const auto pivot = sample_pivot(upper);
        lower.resize(count);
        const auto salt = static_cast<std::size_t>(random());
        const auto split = config_.partition == Partition::Simd
            ? partition(upper.data(), static_cast<T*>(nullptr), lower.data(),
                                static_cast<T*>(nullptr), count, pivot, salt)
            : scalar_partition(upper, lower, pivot, salt);
        assert(split.upper != 0 && split.lower != 0);
        upper.resize(split.upper);
        lower.resize(split.lower);
        pivots_.push_back(pivot);
        if constexpr (CountComparisons) {
            ++counters_.partitions;
            counters_.elements_partitioned += count;
        }
        }
    }

    template <class Outputs>
    void partition_keys(std::span<const T> keys, std::span<const T> boundaries,
                        Outputs& outputs, std::size_t salt) {
        const auto partitions = boundaries.size() + 1;
        if (config_.partition == Partition::Simd) {
            auto dispatch = [&](auto& scratch) {
                std::array<T*, 4> pointers{};
                for (std::size_t i = 0; i < partitions; ++i) {
                    pointers[i] = scratch[i].data();
                }
                const auto counts = partition_chunk(keys.data(), keys.size(), boundaries.data(),
                                                     partitions, pointers.data(), salt);
                for (std::size_t i = 0; i < partitions; ++i) {
                    for (std::size_t key = 0; key < counts[i]; ++key) {
                        outputs[i].push_back(scratch[i][key]);
                    }
                }
            };
            if constexpr (std::is_same_v<Bucket, std::vector<T>>) {
                std::array<std::vector<T>, 4> scratch;
                for (std::size_t i = 0; i < partitions; ++i) {
                    scratch[i].resize(keys.size());
                }
                dispatch(scratch);
            } else {
                std::array<std::array<T, Bucket::block_capacity>, 4> scratch;
                dispatch(scratch);
            }
        } else {
            std::size_t position = 0;
            for (auto key : keys) {
                std::size_t target = 0;
                while (target + 1 < partitions &&
                       (less(key, boundaries[target]) ||
                        (equal(key, boundaries[target]) && ((position ^ salt) & 1) == 0))) {
                    ++target;
                }
                outputs[target].push_back(key);
                ++position;
            }
        }
    }

    template <class Outputs>
    void partition_bucket(const Bucket& input, std::span<const T> boundaries,
                          Outputs& outputs, std::size_t salt) {
        if constexpr (std::is_same_v<Bucket, std::vector<T>>) {
            partition_keys(std::span<const T>(input), boundaries, outputs, salt);
        } else {
            std::size_t offset = 0;
            input.for_blocks([&](auto keys) {
                partition_keys(keys, boundaries, outputs, salt ^ offset);
                offset += keys.size();
                if constexpr (CountComparisons) {
                    ++counters_.blocks_partitioned;
                }
            });
        }
    }

    void blocked_split_terminal() {
        const auto pivot = sample_pivot(terminal());
        const auto count = terminal().size();
        std::array<Bucket, 2> outputs;
        const std::array boundaries = {pivot};
        partition_bucket(terminal(), boundaries, outputs, static_cast<std::size_t>(random()));
        if (outputs[0].empty() || outputs[1].empty()) {
            const auto source = outputs[0].empty() ? 1U : 0U;
            auto& bucket = outputs[source];
            std::size_t pos = 0;
            while (!equal(bucket[pos], pivot)) {
                ++pos;
            }
            outputs[1 - source].push_back(bucket[pos]);
            bucket[pos] = bucket.back();
            bucket.pop_back();
        }
        const auto index = pivots_.size();
        buckets_[index] = std::move(outputs[0]);
        if (index + 1 == buckets_.size()) {
            buckets_.emplace_back();
        }
        buckets_[index + 1] = std::move(outputs[1]);
        pivots_.push_back(pivot);
        if constexpr (CountComparisons) {
            ++counters_.partitions;
            counters_.elements_partitioned += count;
        }
    }

    void multi_split_terminal() {
        const auto partitions = config_.partition_count;
        const auto count = terminal().size();
        const auto sample_size = (std::size_t{1} << (partitions - 1)) - 1;
        samples_.resize(sample_size);
        for (auto& sample : samples_) {
            sample = terminal()[static_cast<std::size_t>(random() % count)];
        }
        std::sort(samples_.begin(), samples_.end(),
                  [this](T left, T right) { return less(left, right); });
        std::array<T, 3> boundaries{};
        for (std::size_t i = 0; i + 1 < partitions; ++i) {
            boundaries[i] = samples_[(sample_size + 1) / (std::size_t{1} << (i + 1)) - 1];
        }
        std::array<Bucket, 4> outputs;
        partition_bucket(terminal(), std::span<const T>(boundaries.data(), partitions - 1),
                         outputs, static_cast<std::size_t>(random()));
        const auto populated = std::count_if(outputs.begin(), outputs.begin() + partitions,
                                            [](const auto& bucket) { return !bucket.empty(); });
        if constexpr (CountComparisons) {
            ++counters_.partitions;
            ++counters_.multi_partitions;
            counters_.elements_partitioned += count;
        }
        if (populated < 2) {
            const auto previous = config_.partition_count;
            config_.partition_count = 2;
            split_terminal();
            config_.partition_count = previous;
            return;
        }
        auto index = pivots_.size();
        bool first = true;
        for (std::size_t i = 0; i < partitions; ++i) {
            if (outputs[i].empty()) {
                continue;
            }
            if (!first) {
                pivots_.push_back(boundaries[i - 1]);
                ++index;
                if (index == buckets_.size()) {
                    buckets_.emplace_back();
                }
            }
            buckets_[index] = std::move(outputs[i]);
            first = false;
        }
    }

    void merge_layers(std::size_t upper_index) {
        auto& upper = buckets_[upper_index];
        auto& lower = buckets_[upper_index + 1];
        if (upper.size() < lower.size()) {
            upper.swap(lower);
        }
        if constexpr (std::is_same_v<Bucket, std::vector<T>>) {
            upper.insert(upper.end(), lower.begin(), lower.end());
        } else {
            upper.concat(lower);
        }
        buckets_.erase(buckets_.begin() + upper_index + 1);
        pivots_.erase(pivots_.begin() + upper_index);
        if constexpr (CountComparisons) {
            ++counters_.rebalances;
        }
    }

    void rebalance() {
        if (size_ == 0 || pivots_.empty()) {
            return;
        }
        if (config_.rebalance == Rebalance::NaiveLog) {
            const auto max_layers = config_.log_threshold * (std::bit_width(size_) - 1);
            if (pivots_.size() > max_layers) {
                while (!pivots_.empty()) {
                    merge_layers(pivots_.size() - 1);
                }
            }
        } else {
            std::size_t total = 0;
            auto layer = pivots_.size();
            while (layer != 0) {
                const auto pair_size = buckets_[layer].size() + buckets_[layer - 1].size();
                const auto factor = config_.rebalance_factor;
                const auto threshold = total > std::numeric_limits<std::size_t>::max() / factor
                    ? std::numeric_limits<std::size_t>::max() : factor * total;
                if (pair_size < threshold) {
                    merge_layers(layer - 1);
                } else {
                    total += buckets_[layer].size();
                }
                --layer;
            }
        }
        prepare_terminal();
    }
};

template <class T, bool CountComparisons = false, std::size_t BlockSize = 112,
          std::size_t BlockCapacity = 128>
using BlockQuickHeap = ExperimentalHeap<T, CountComparisons,
                                       LinkedBucket<T, BlockSize, BlockCapacity>>;

}
