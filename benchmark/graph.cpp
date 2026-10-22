#include "graph.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace quickheap::bench {
namespace {

constexpr auto max_packed = std::numeric_limits<std::uint32_t>::max();

[[noreturn]] void parse_error(const std::filesystem::path &path,
                             std::size_t line, std::string_view message) {
    throw std::runtime_error(path.string() + ":" + std::to_string(line) +
                             ": " + std::string(message));
}

std::uint32_t number(const std::string &text,
                     const std::filesystem::path &path, std::size_t line) {
    std::uint32_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) {
        parse_error(path, line, "expected unsigned 32-bit integer, got '" + text + "'");
    }
    return value;
}

auto edge_order(const Edge &edge) {
    return std::tuple(edge.from.value, edge.to.value, edge.weight);
}

} // namespace

Graph::Graph(std::vector<std::size_t> offsets, std::vector<Edge> edges)
    : offsets_(std::move(offsets)), edges_(std::move(edges)) {}

Graph Graph::from_edges(std::size_t vertices, std::vector<Edge> edges) {
    if (vertices == 0 || vertices > max_packed) {
        throw std::invalid_argument("graph vertex count must fit positive u32");
    }
    if (edges.size() > max_packed) {
        throw std::invalid_argument("graph edge count exceeds packed u32 IDs");
    }
    for (const auto &edge : edges) {
        if (edge.from.value >= vertices || edge.to.value >= vertices) {
            throw std::invalid_argument("graph endpoint outside vertex range");
        }
    }
    std::sort(edges.begin(), edges.end(), [](const Edge &a, const Edge &b) {
        return edge_order(a) < edge_order(b);
    });
    std::vector<std::size_t> offsets(vertices + 1, 0);
    for (const auto &edge : edges) ++offsets[edge.from.value + 1];
    for (std::size_t i = 1; i < offsets.size(); ++i) offsets[i] += offsets[i - 1];
    return Graph(std::move(offsets), std::move(edges));
}

Graph Graph::from_dimacs(const std::filesystem::path &path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error(path.string() + ": cannot open graph");
    bool header = false;
    std::uint32_t vertices = 0;
    std::uint32_t declared_edges = 0;
    std::vector<Edge> edges;
    std::size_t line_number = 0;
    std::string line;
    while (std::getline(input, line)) {
        ++line_number;
        std::istringstream tokens(line);
        std::string kind;
        if (!(tokens >> kind) || kind == "c") continue;
        std::string first, second, third, extra;
        if (!(tokens >> first >> second >> third) || tokens >> extra) {
            parse_error(path, line_number, "expected four fields");
        }
        if (kind == "p") {
            if (header) parse_error(path, line_number, "duplicate problem header");
            if (first != "sp") parse_error(path, line_number, "expected 'p sp' header");
            vertices = number(second, path, line_number);
            declared_edges = number(third, path, line_number);
            if (vertices == 0) parse_error(path, line_number, "graph needs at least one vertex");
            header = true;
        } else if (kind == "a") {
            if (!header) parse_error(path, line_number, "arc before problem header");
            const auto from = number(first, path, line_number);
            const auto to = number(second, path, line_number);
            const auto weight = number(third, path, line_number);
            if (from == 0 || from > vertices || to == 0 || to > vertices) {
                parse_error(path, line_number, "arc endpoint outside 1..vertices");
            }
            if (edges.size() >= declared_edges) {
                parse_error(path, line_number, "more arcs than declared");
            }
            edges.push_back({Vertex{from - 1}, Vertex{to - 1}, weight});
        } else {
            parse_error(path, line_number, "unknown record '" + kind + "'");
        }
    }
    if (input.bad()) parse_error(path, line_number, "graph read failed");
    if (!header) parse_error(path, line_number + 1, "missing problem header");
    if (edges.size() != declared_edges) {
        parse_error(path, line_number + 1, "arc count differs from problem header");
    }
    return from_edges(vertices, std::move(edges));
}

Graph Graph::undirected() const {
    std::vector<Edge> edges;
    if (edges_.size() <= edges.max_size() / 2) edges.reserve(edges_.size() * 2);
    for (const auto &edge : edges_) {
        edges.push_back(edge);
        edges.push_back({edge.to, edge.from, edge.weight});
    }
    std::sort(edges.begin(), edges.end(), [](const Edge &a, const Edge &b) {
        return edge_order(a) < edge_order(b);
    });
    edges.erase(std::unique(edges.begin(), edges.end(), [](const Edge &a, const Edge &b) {
        return a.from == b.from && a.to == b.to;
    }), edges.end());
    return from_edges(vertex_count(), std::move(edges));
}

std::span<const Edge> Graph::outgoing(Vertex vertex) const {
    return std::span<const Edge>(edges_).subspan(offsets_[vertex.value],
        offsets_[vertex.value + 1] - offsets_[vertex.value]);
}

std::size_t Graph::first_edge(Vertex vertex) const { return offsets_[vertex.value]; }
const Edge &Graph::edge(std::uint32_t id) const { return edges_[id]; }

} // namespace quickheap::bench
