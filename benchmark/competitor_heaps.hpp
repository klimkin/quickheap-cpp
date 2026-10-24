#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#ifdef QUICKHEAP_BENCH_BOOST
#include <boost/heap/binomial_heap.hpp>
#include <boost/heap/d_ary_heap.hpp>
#include <boost/heap/fibonacci_heap.hpp>
#include <boost/heap/pairing_heap.hpp>
#include <boost/heap/skew_heap.hpp>
#endif

namespace quickheap::bench {

struct ComparisonStats {
    std::uint64_t pushes = 0;
    std::uint64_t pops = 0;
};

template <typename T, std::size_t Arity, bool CountComparisons = false,
          bool SwapSift = false>
class DaryHeap {
    static_assert(Arity >= 2);

  public:
    using Item = T;
    static constexpr bool monotone = false;
    static constexpr std::size_t columns = 1;
    static constexpr bool comparison_counted = CountComparisons;

    void push(T key) {
        elements_.push_back(std::move(key));
        auto pos = elements_.size() - 1;
        if constexpr (SwapSift) {
            while (pos != 0) {
                const auto parent = (pos - 1) / Arity;
                if (!less(elements_[pos], elements_[parent], stats_.pushes)) break;
                std::swap(elements_[pos], elements_[parent]);
                pos = parent;
            }
        } else {
            T value = std::move(elements_[pos]);
            while (pos != 0) {
                const auto parent = (pos - 1) / Arity;
                if (!less(value, elements_[parent], stats_.pushes)) break;
                elements_[pos] = std::move(elements_[parent]);
                pos = parent;
            }
            elements_[pos] = std::move(value);
        }
    }

    std::optional<T> try_pop() {
        if (elements_.empty()) return std::nullopt;
        T result = std::move(elements_.front());
        if (elements_.size() == 1) {
            elements_.pop_back();
            return result;
        }
        elements_.front() = std::move(elements_.back());
        elements_.pop_back();
        auto pos = std::size_t{0};
        if constexpr (SwapSift) {
            while (has_child(pos)) {
                const auto child = smallest_child(pos);
                if (!less(elements_[child], elements_[pos], stats_.pops)) break;
                std::swap(elements_[pos], elements_[child]);
                pos = child;
            }
        } else {
            T value = std::move(elements_.front());
            const auto len = elements_.size();
            for (;;) {
                const auto first = pos * Arity + 1;
                if (first >= len) break;
                // Keep the best key in a register so the scan lowers to cmovs.
                auto best = first;
                auto best_key = elements_[first];
                if (len - first >= Arity) {
                    for (std::size_t i = 1; i < Arity; ++i) {
                        const auto child = first + i;
                        const auto child_key = elements_[child];
                        const bool smaller = less(child_key, best_key, stats_.pops);
                        best = smaller ? child : best;
                        best_key = smaller ? child_key : best_key;
                    }
                } else {
                    for (auto child = first + 1; child < len; ++child) {
                        if (less(elements_[child], best_key, stats_.pops)) {
                            best = child;
                            best_key = elements_[child];
                        }
                    }
                }
                if (!less(best_key, value, stats_.pops)) break;
                elements_[pos] = std::move(best_key);
                pos = best;
            }
            elements_[pos] = std::move(value);
        }
        return result;
    }

    bool empty() const { return elements_.empty(); }
    std::size_t size() const { return elements_.size(); }
    std::size_t capacity() const { return elements_.capacity(); }
    bool valid() const {
        for (std::size_t i = 1; i < elements_.size(); ++i)
            if (std::less<T>{}(elements_[i], elements_[(i - 1) / Arity])) return false;
        return true;
    }
    std::uint64_t push_comparisons() const { return stats_.pushes; }
    std::uint64_t pop_comparisons() const { return stats_.pops; }
    void reset_comparisons() { stats_ = {}; }

  private:
    bool less(const T &a, const T &b, std::uint64_t &counter) {
        if constexpr (CountComparisons) ++counter;
        return std::less<T>{}(a, b);
    }
    bool has_child(std::size_t pos) const {
        return elements_.size() > 1 && pos <= (elements_.size() - 2) / Arity;
    }
    std::size_t smallest_child(std::size_t pos) {
        const auto first = pos * Arity + 1;
        const auto end = first + std::min(Arity, elements_.size() - first);
        auto best = first;
        for (auto child = first + 1; child < end; ++child)
            if (less(elements_[child], elements_[best], stats_.pops)) best = child;
        return best;
    }

    std::vector<T> elements_;
    ComparisonStats stats_;
};

template <typename T, std::size_t Arity, bool CountComparisons = false>
using CustomDaryHeap = DaryHeap<T, Arity, CountComparisons, true>;

template <typename T, bool CountComparisons = false>
using CustomBinaryHeap = CustomDaryHeap<T, 2, CountComparisons>;

template <typename T, bool CountComparisons = false> class WeakHeap {
  public:
    using Item = T;
    static constexpr bool monotone = false;
    static constexpr std::size_t columns = 1;
    static constexpr bool comparison_counted = CountComparisons;

    void push(T key) {
        const auto old_size = elements_.size();
        elements_.push_back(std::move(key));
        reverse_.push_back(0);
        if (old_size == 0) return;
        if (old_size % 2 == 0) reverse_[old_size / 2] = 0;
        // Hole-based sift-up as in weakheap 0.1.0; a childless leaf needs no bit flip.
        T value = std::move(elements_[old_size]);
        auto hole = old_size;
        auto cur = old_size;
        while (cur != 0) {
            auto ancestor = cur / 2;
            while (ancestor != 0 && cur % 2 == static_cast<std::size_t>(reverse_[ancestor])) {
                cur /= 2;
                ancestor /= 2;
            }
            if constexpr (CountComparisons) ++stats_.pushes;
            if (!std::less<T>{}(value, elements_[ancestor])) break;
            elements_[hole] = std::move(elements_[ancestor]);
            hole = ancestor;
            cur = ancestor;
        }
        elements_[hole] = std::move(value);
    }

    std::optional<T> try_pop() {
        if (elements_.empty()) return std::nullopt;
        T result = std::move(elements_.front());
        if (elements_.size() == 1) {
            elements_.pop_back();
            reverse_.pop_back();
            return result;
        }
        T root = std::move(elements_.back());
        elements_.pop_back();
        reverse_.pop_back();
        auto *const data = elements_.data();
        auto *const reverse = reverse_.data();
        const auto size = elements_.size();
        if (size > 1) {
            auto pos = std::size_t{1};
            while (pos <= (size - 1) / 2) {
                const auto child = 2 * pos + static_cast<std::size_t>(reverse[pos]);
                if (child >= size) break;
                pos = child;
            }
            while (pos != 0) {
                if constexpr (CountComparisons) ++stats_.pops;
                const bool exchange = std::less<T>{}(data[pos], root);
                if constexpr (std::is_integral_v<T>) {
                    const T child = data[pos];
                    data[pos] = exchange ? root : child;
                    root = exchange ? child : root;
                    reverse[pos] ^= static_cast<std::uint8_t>(exchange);
                } else if (exchange) {
                    std::swap(root, data[pos]);
                    reverse[pos] ^= 1;
                }
                pos /= 2;
            }
        }
        data[0] = std::move(root);
        return result;
    }

    bool empty() const { return elements_.empty(); }
    std::size_t size() const { return elements_.size(); }
    std::size_t capacity() const { return elements_.capacity(); }
    bool valid() const {
        if (reverse_.size() != elements_.size()) return false;
        for (std::size_t i = 1; i < elements_.size(); ++i)
            if (std::less<T>{}(elements_[i], elements_[distinguished_ancestor(i)]))
                return false;
        return true;
    }
    std::uint64_t push_comparisons() const { return stats_.pushes; }
    std::uint64_t pop_comparisons() const { return stats_.pops; }
    void reset_comparisons() { stats_ = {}; }

  private:
    std::size_t distinguished_ancestor(std::size_t pos) const {
        while (pos > 1 && pos % 2 == static_cast<std::size_t>(reverse_[pos / 2]))
            pos /= 2;
        return pos / 2;
    }
    std::vector<T> elements_;
    std::vector<std::uint8_t> reverse_;
    ComparisonStats stats_;
};

template <std::integral T> class RadixHeap {
    static_assert(!std::same_as<T, bool>);
    using Unsigned = std::make_unsigned_t<T>;
    static constexpr auto bits = std::numeric_limits<Unsigned>::digits;

  public:
    using Item = T;
    static constexpr bool monotone = true;
    static constexpr std::size_t columns = 1;
    static constexpr bool comparison_counted = false;

    void push(T key) {
        const auto encoded = encode(key);
        if (encoded < last_) throw std::invalid_argument("radix heap insertion precedes last pop");
        buckets_[bucket(encoded)].push_back(encoded);
        ++size_;
    }
    std::optional<T> try_pop() {
        if (size_ == 0) return std::nullopt;
        if (buckets_[0].empty()) {
            auto index = std::size_t{1};
            while (buckets_[index].empty()) ++index;
            auto &source = buckets_[index];
            last_ = *std::min_element(source.begin(), source.end());
            for (const auto key : source) buckets_[bucket(key)].push_back(key);
            source.clear();
        }
        const auto key = buckets_[0].back();
        buckets_[0].pop_back();
        --size_;
        return decode(key);
    }
    bool empty() const { return size_ == 0; }
    std::size_t size() const { return size_; }
    std::size_t capacity() const {
        std::size_t result = 0;
        for (const auto &bucket : buckets_) result += bucket.capacity();
        return result;
    }
    bool valid() const {
        std::size_t count = 0;
        for (std::size_t i = 0; i < buckets_.size(); ++i) {
            count += buckets_[i].size();
            for (const auto key : buckets_[i])
                if (key < last_ || bucket(key) != i) return false;
        }
        return count == size_;
    }

  private:
    static Unsigned encode(T key) {
        if constexpr (std::is_signed_v<T>)
            return std::bit_cast<Unsigned>(key) ^ (Unsigned{1} << (bits - 1));
        else
            return key;
    }
    static T decode(Unsigned key) {
        if constexpr (std::is_signed_v<T>)
            return std::bit_cast<T>(static_cast<Unsigned>(key ^ (Unsigned{1} << (bits - 1))));
        else
            return key;
    }
    std::size_t bucket(Unsigned key) const { return std::bit_width(static_cast<Unsigned>(key ^ last_)); }

    std::array<std::vector<Unsigned>, bits + 1> buckets_;
    Unsigned last_ = 0;
    std::size_t size_ = 0;
};

#ifdef QUICKHEAP_BENCH_BOOST

enum class BoostHeapKind { Dary, Fibonacci, Pairing, Binomial, Skew };

template <typename T, bool CountComparisons> struct BoostGreater {
    ComparisonStats *stats = nullptr;
    bool *popping = nullptr;
    bool operator()(const T &a, const T &b) const {
        if constexpr (CountComparisons) {
            if (*popping) ++stats->pops;
            else ++stats->pushes;
        }
        return std::greater<T>{}(a, b);
    }
};

template <typename T, BoostHeapKind Kind, std::size_t Arity, bool CountComparisons>
struct BoostHeapType;

template <typename T, std::size_t Arity, bool CountComparisons>
struct BoostHeapType<T, BoostHeapKind::Dary, Arity, CountComparisons> {
    using type = boost::heap::d_ary_heap<T, boost::heap::arity<Arity>,
                                       boost::heap::compare<BoostGreater<T, CountComparisons>>>;
};
#define QUICKHEAP_BOOST_HEAP_TYPE(Kind, Name)                                  \
    template <typename T, std::size_t Arity, bool CountComparisons>             \
    struct BoostHeapType<T, BoostHeapKind::Kind, Arity, CountComparisons> {     \
        using type = boost::heap::Name<T,                                     \
            boost::heap::compare<BoostGreater<T, CountComparisons>>>;          \
    };
QUICKHEAP_BOOST_HEAP_TYPE(Fibonacci, fibonacci_heap)
QUICKHEAP_BOOST_HEAP_TYPE(Pairing, pairing_heap)
QUICKHEAP_BOOST_HEAP_TYPE(Binomial, binomial_heap)
QUICKHEAP_BOOST_HEAP_TYPE(Skew, skew_heap)
#undef QUICKHEAP_BOOST_HEAP_TYPE

template <typename T, BoostHeapKind Kind, std::size_t Arity = 4,
          bool CountComparisons = false>
class BoostHeap {
    struct State {
        ComparisonStats stats;
        bool popping = false;
    };
    using Heap = typename BoostHeapType<T, Kind, Arity, CountComparisons>::type;

  public:
    using Item = T;
    static constexpr bool monotone = false;
    static constexpr std::size_t columns = 1;
    static constexpr bool comparison_counted = CountComparisons;
    BoostHeap() : state_(std::make_unique<State>()),
        heap_(BoostGreater<T, CountComparisons>{&state_->stats, &state_->popping}) {}
    BoostHeap(const BoostHeap &) = delete;
    BoostHeap &operator=(const BoostHeap &) = delete;
    BoostHeap(BoostHeap &&) = default;
    BoostHeap &operator=(BoostHeap &&) = default;

    void push(T key) {
        state_->popping = false;
        heap_.push(std::move(key));
    }
    std::optional<T> try_pop() {
        if (heap_.empty()) return std::nullopt;
        T key = heap_.top();
        state_->popping = true;
        heap_.pop();
        return key;
    }
    bool empty() const { return heap_.empty(); }
    std::size_t size() const { return heap_.size(); }
    bool valid() const { return true; }
    std::uint64_t push_comparisons() const { return state_->stats.pushes; }
    std::uint64_t pop_comparisons() const { return state_->stats.pops; }
    void reset_comparisons() { state_->stats = {}; }

  private:
    std::unique_ptr<State> state_;
    Heap heap_;
};

template <typename T, std::size_t Arity = 4, bool CountComparisons = false>
using BoostDaryHeap = BoostHeap<T, BoostHeapKind::Dary, Arity, CountComparisons>;
template <typename T, bool CountComparisons = false>
using BoostFibonacciHeap = BoostHeap<T, BoostHeapKind::Fibonacci, 4, CountComparisons>;
template <typename T, bool CountComparisons = false>
using BoostPairingHeap = BoostHeap<T, BoostHeapKind::Pairing, 4, CountComparisons>;
template <typename T, bool CountComparisons = false>
using BoostBinomialHeap = BoostHeap<T, BoostHeapKind::Binomial, 4, CountComparisons>;
template <typename T, bool CountComparisons = false>
using BoostSkewHeap = BoostHeap<T, BoostHeapKind::Skew, 4, CountComparisons>;
#endif

} // namespace quickheap::bench
