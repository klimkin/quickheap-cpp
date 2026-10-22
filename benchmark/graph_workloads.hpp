#pragma once

#include "graph.hpp"
#include "quickheap.hpp"

#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace quickheap::bench {

struct GraphCounts {
    std::uint64_t pushes = 0;
    std::uint64_t pops = 0;
    friend bool operator==(GraphCounts, GraphCounts) = default;
};

class GraphQuickQueue {
  public:
    void push(std::uint64_t value) { queue_.push(value); }
    std::optional<std::uint64_t> try_pop() { return queue_.try_pop(); }
    bool empty() const { return queue_.empty(); }

  private:
    quickheap::Heap<std::uint64_t> queue_;
};

class GraphStdQueue {
  public:
    void push(std::uint64_t value) { queue_.push(value); }
    std::optional<std::uint64_t> try_pop() {
        if (queue_.empty()) return std::nullopt;
        const auto value = queue_.top();
        queue_.pop();
        return value;
    }
    bool empty() const { return queue_.empty(); }

  private:
    std::priority_queue<std::uint64_t, std::vector<std::uint64_t>,
                        std::greater<std::uint64_t>> queue_;
};

inline constexpr auto unreachable = std::numeric_limits<std::uint32_t>::max();

inline std::uint64_t pack_graph_entry(std::uint32_t key, std::uint32_t id) {
    return (std::uint64_t{key} << 32) | id;
}

template <class Queue> class DijkstraTrial {
  public:
    DijkstraTrial(const Graph &graph, Vertex source, Queue queue = {})
        : graph_(graph), source_(source), queue_(std::move(queue)),
          distances_(graph.vertex_count(), unreachable) {}

    void execute() {
        distances_[source_.value] = 0;
        queue_.push(pack_graph_entry(0, source_.value));
        ++counts_.pushes;
        while (const auto entry = queue_.try_pop()) {
            ++counts_.pops;
            const auto vertex = Vertex{static_cast<std::uint32_t>(*entry)};
            const auto distance = static_cast<std::uint32_t>(*entry >> 32);
            if (distance != distances_[vertex.value]) continue;
            for (const auto &edge : graph_.outgoing(vertex)) {
                const auto candidate = std::uint64_t{distance} + edge.weight;
                if (candidate >= unreachable) {
                    throw std::overflow_error("Dijkstra candidate distance reaches reserved UINT32_MAX");
                }
                if (candidate < distances_[edge.to.value]) {
                    distances_[edge.to.value] = static_cast<std::uint32_t>(candidate);
                    queue_.push(pack_graph_entry(static_cast<std::uint32_t>(candidate), edge.to.value));
                    ++counts_.pushes;
                }
            }
        }
    }

    const std::vector<std::uint32_t> &distances() const { return distances_; }
    GraphCounts counts() const { return counts_; }
    bool empty() const { return queue_.empty(); }

  private:
    const Graph &graph_;
    Vertex source_;
    Queue queue_;
    std::vector<std::uint32_t> distances_;
    GraphCounts counts_;
};

class DijkstraWorkload {
  public:
    explicit DijkstraWorkload(const Graph &graph, Vertex source = Vertex{0})
        : graph_(graph), source_(source) {
        if (source.value >= graph.vertex_count()) throw std::invalid_argument("Dijkstra source outside graph");
        auto reference = prepare<GraphStdQueue>();
        reference.execute();
        expected_ = reference.distances();
        counts_ = reference.counts();
    }

    template <class Queue> DijkstraTrial<Queue> prepare(Queue queue = {}) const {
        return {graph_, source_, std::move(queue)};
    }
    template <class Queue> void verify(const DijkstraTrial<Queue> &trial) const {
        if (trial.distances() != expected_ || trial.counts() != counts_ || !trial.empty()) {
            throw std::runtime_error("Dijkstra result disagrees with standard priority queue");
        }
    }

  private:
    const Graph &graph_;
    Vertex source_;
    std::vector<std::uint32_t> expected_;
    GraphCounts counts_;
};

template <class Queue> class PrimTrial {
  public:
    PrimTrial(const Graph &graph, Vertex source, Queue queue = {})
        : graph_(graph), source_(source), queue_(std::move(queue)),
          visited_(graph.vertex_count(), 0),
          parents_(graph.vertex_count(), unreachable) {
        tree_edges_.reserve(graph.vertex_count() - 1);
    }

    void execute() {
        relax(source_);
        visited_[source_.value] = 1;
        parents_[source_.value] = source_.value;
        while (const auto entry = queue_.try_pop()) {
            ++counts_.pops;
            const auto id = static_cast<std::uint32_t>(*entry);
            const auto &edge = graph_.edge(id);
            if (visited_[edge.to.value]) continue;
            tree_edges_.push_back(id);
            weight_ += edge.weight;
            parents_[edge.to.value] = edge.from.value;
            visited_[edge.to.value] = 1;
            relax(edge.to);
        }
    }

    const std::vector<std::uint32_t> &tree_edges() const { return tree_edges_; }
    const std::vector<std::uint32_t> &parents() const { return parents_; }
    std::uint64_t weight() const { return weight_; }
    GraphCounts counts() const { return counts_; }
    bool empty() const { return queue_.empty(); }

  private:
    void relax(Vertex vertex) {
        auto id = graph_.first_edge(vertex);
        for (const auto &edge : graph_.outgoing(vertex)) {
            if (!visited_[edge.to.value]) {
                queue_.push(pack_graph_entry(edge.weight, static_cast<std::uint32_t>(id)));
                ++counts_.pushes;
            }
            ++id;
        }
    }

    const Graph &graph_;
    Vertex source_;
    Queue queue_;
    std::vector<std::uint8_t> visited_;
    std::vector<std::uint32_t> parents_;
    std::vector<std::uint32_t> tree_edges_;
    std::uint64_t weight_ = 0;
    GraphCounts counts_;
};

class PrimWorkload {
  public:
    explicit PrimWorkload(const Graph &undirected_graph, Vertex source = Vertex{0})
        : graph_(undirected_graph), source_(source) {
        if (source.value >= graph_.vertex_count()) throw std::invalid_argument("Prim source outside graph");
        auto reference = prepare<GraphStdQueue>();
        reference.execute();
        expected_ = reference.tree_edges();
        parents_ = reference.parents();
        weight_ = reference.weight();
        counts_ = reference.counts();
    }

    template <class Queue> PrimTrial<Queue> prepare(Queue queue = {}) const {
        return {graph_, source_, std::move(queue)};
    }
    template <class Queue> void verify(const PrimTrial<Queue> &trial) const {
        if (trial.tree_edges() != expected_ || trial.parents() != parents_ ||
            trial.weight() != weight_ || trial.counts() != counts_ || !trial.empty()) {
            throw std::runtime_error("Prim result disagrees with standard priority queue");
        }
    }

  private:
    const Graph &graph_;
    Vertex source_;
    std::vector<std::uint32_t> expected_;
    std::vector<std::uint32_t> parents_;
    std::uint64_t weight_ = 0;
    GraphCounts counts_;
};

} // namespace quickheap::bench
