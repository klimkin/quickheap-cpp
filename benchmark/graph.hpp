#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace quickheap::bench {

struct Vertex {
    std::uint32_t value;
    friend bool operator==(Vertex, Vertex) = default;
};

struct Edge {
    Vertex from;
    Vertex to;
    std::uint32_t weight;
    friend bool operator==(const Edge &, const Edge &) = default;
};

class Graph {
  public:
    static Graph from_dimacs(const std::filesystem::path &path);
    static Graph from_edges(std::size_t vertices, std::vector<Edge> edges);
    Graph undirected() const;

    std::span<const Edge> outgoing(Vertex vertex) const;
    std::size_t first_edge(Vertex vertex) const;
    const Edge &edge(std::uint32_t id) const;
    std::size_t vertex_count() const { return offsets_.size() - 1; }
    std::size_t edge_count() const { return edges_.size(); }

  private:
    Graph(std::vector<std::size_t> offsets, std::vector<Edge> edges);
    std::vector<std::size_t> offsets_;
    std::vector<Edge> edges_;
};

} // namespace quickheap::bench
