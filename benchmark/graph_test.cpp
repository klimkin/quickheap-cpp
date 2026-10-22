#include "graph_workloads.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace {

using namespace quickheap::bench;

Graph upstream_graph() {
    return Graph::from_edges(6, {
        {{0}, {1}, 2}, {{0}, {3}, 3}, {{1}, {0}, 3}, {{2}, {5}, 6},
        {{3}, {1}, 6}, {{3}, {2}, 9}, {{3}, {4}, 10}, {{4}, {2}, 1},
        {{4}, {3}, 11}, {{4}, {5}, 6}, {{5}, {0}, 12}, {{5}, {2}, 3},
        {{5}, {4}, 2}});
}

class DimacsFile {
  public:
    explicit DimacsFile(const std::string &text) {
        static std::atomic<unsigned> next{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
            ("quickheap-graph-" + std::to_string(stamp) + "-" + std::to_string(next++) + ".gr");
        std::ofstream output(path_);
        output << text;
        if (!output) throw std::runtime_error("cannot write DIMACS test fixture");
    }
    ~DimacsFile() {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }
    const std::filesystem::path &path() const { return path_; }

  private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("Graph Dijkstra matches upstream six-vertex distances", "[graph]") {
    const auto graph = upstream_graph();
    const DijkstraWorkload workload(graph, Vertex{2});
    auto trial = workload.prepare<GraphQuickQueue>();
    trial.execute();
    workload.verify(trial);
    REQUIRE(trial.distances() == std::vector<std::uint32_t>{18, 20, 0, 19, 8, 6});
    REQUIRE(trial.counts().pushes == trial.counts().pops);
    REQUIRE(trial.empty());
    auto from_zero = DijkstraWorkload(graph).prepare<GraphStdQueue>();
    from_zero.execute();
    REQUIRE(from_zero.distances() == std::vector<std::uint32_t>{0, 2, 12, 3, 13, 18});
}

TEST_CASE("Graph Prim matches upstream parents and minimum weight", "[graph]") {
    const auto graph = upstream_graph().undirected();
    const PrimWorkload workload(graph, Vertex{4});
    auto trial = workload.prepare<GraphQuickQueue>();
    trial.execute();
    workload.verify(trial);
    REQUIRE(trial.parents() == std::vector<std::uint32_t>{3, 0, 4, 2, 4, 4});
    REQUIRE(trial.weight() == 17);
    REQUIRE(trial.tree_edges().size() == 5);
    REQUIRE(trial.empty());
}

TEST_CASE("Graph algorithms drain stale entries and respect disconnected components", "[graph]") {
    const auto graph = Graph::from_edges(6, {
        {{0}, {1}, 7}, {{0}, {1}, 7}, {{0}, {2}, 1}, {{0}, {3}, 1},
        {{2}, {1}, 0}, {{3}, {1}, 0}, {{4}, {5}, 0}});
    const DijkstraWorkload workload(graph);
    auto trial = workload.prepare<GraphQuickQueue>();
    trial.execute();
    workload.verify(trial);
    REQUIRE(trial.distances() == std::vector<std::uint32_t>{0, 1, 1, 1, unreachable, unreachable});
    REQUIRE(trial.counts() == GraphCounts{5, 5});
    const auto undirected = graph.undirected();
    REQUIRE(undirected.edge_count() == 12);
    const PrimWorkload prim(undirected);
    auto tree = prim.prepare<GraphQuickQueue>();
    tree.execute();
    prim.verify(tree);
    REQUIRE(tree.tree_edges().size() == 3);
    REQUIRE(tree.weight() == 1);
    REQUIRE(tree.parents()[4] == unreachable);
    REQUIRE(tree.parents()[5] == unreachable);
}

TEST_CASE("Graph undirected conversion keeps minimum endpoint weight and stable IDs", "[graph]") {
    const auto graph = Graph::from_edges(3, {
        {{1}, {0}, 8}, {{0}, {1}, 9}, {{0}, {1}, 2}, {{2}, {2}, 0}}).undirected();
    REQUIRE(graph.edge_count() == 3);
    REQUIRE(graph.edge(0) == Edge{{0}, {1}, 2});
    REQUIRE(graph.edge(1) == Edge{{1}, {0}, 2});
    REQUIRE(graph.edge(2) == Edge{{2}, {2}, 0});
    REQUIRE(graph.first_edge(Vertex{1}) == 1);
    REQUIRE(graph.outgoing(Vertex{1}).front() == graph.edge(1));
}

TEST_CASE("Graph empty component and zero weights remain valid", "[graph]") {
    const auto singleton = Graph::from_edges(1, {});
    auto dijkstra = DijkstraWorkload(singleton).prepare<GraphQuickQueue>();
    dijkstra.execute();
    REQUIRE(dijkstra.distances() == std::vector<std::uint32_t>{0});
    REQUIRE(dijkstra.counts() == GraphCounts{1, 1});
    auto prim = PrimWorkload(singleton).prepare<GraphQuickQueue>();
    prim.execute();
    REQUIRE(prim.weight() == 0);
    REQUIRE(prim.tree_edges().empty());
    REQUIRE(prim.counts() == GraphCounts{0, 0});
    const auto zero = Graph::from_edges(2, {{{0}, {1}, 0}, {{1}, {0}, 0}});
    auto zero_trial = DijkstraWorkload(zero).prepare<GraphQuickQueue>();
    zero_trial.execute();
    REQUIRE(zero_trial.distances() == std::vector<std::uint32_t>{0, 0});
}

TEST_CASE("Graph Dijkstra rejects finite sentinel and overflowing candidates", "[graph]") {
    const auto sentinel = Graph::from_edges(2, {{{0}, {1}, unreachable}});
    REQUIRE_THROWS_AS(DijkstraWorkload(sentinel), std::overflow_error);
    auto sentinel_trial = DijkstraTrial<GraphQuickQueue>(sentinel, Vertex{0});
    REQUIRE_THROWS_AS(sentinel_trial.execute(), std::overflow_error);
    const auto overflow = Graph::from_edges(3, {
        {{0}, {1}, unreachable - 1}, {{1}, {2}, 2}});
    REQUIRE_THROWS_AS(DijkstraWorkload(overflow), std::overflow_error);
    const auto boundary = Graph::from_edges(2, {{{0}, {1}, unreachable - 1}});
    auto trial = DijkstraWorkload(boundary).prepare<GraphQuickQueue>();
    trial.execute();
    REQUIRE(trial.distances()[1] == unreachable - 1);
    const auto unreachable_overflow = Graph::from_edges(3, {{{1}, {2}, unreachable}});
    auto disconnected = DijkstraWorkload(unreachable_overflow).prepare<GraphQuickQueue>();
    disconnected.execute();
    REQUIRE(disconnected.distances()[2] == unreachable);
}

TEST_CASE("Graph Prim sums maximum weights in u64", "[graph]") {
    const auto graph = Graph::from_edges(3, {
        {{0}, {1}, unreachable}, {{1}, {2}, unreachable}}).undirected();
    const PrimWorkload workload(graph);
    auto trial = workload.prepare<GraphQuickQueue>();
    trial.execute();
    workload.verify(trial);
    REQUIRE(trial.weight() == std::uint64_t{unreachable} * 2);
    REQUIRE(trial.parents() == std::vector<std::uint32_t>{0, 0, 1});
}

TEST_CASE("DIMACS parser accepts whitespace and preserves directed duplicates", "[graph]") {
    const DimacsFile file("\n  c fixture\r\n\tp\tsp 3 3\n a 2 3 0\n a 1 2 7\n a 1 2 3\n\n");
    const auto graph = Graph::from_dimacs(file.path());
    REQUIRE(graph.vertex_count() == 3);
    REQUIRE(graph.edge_count() == 3);
    REQUIRE(graph.edge(0) == Edge{{0}, {1}, 3});
    REQUIRE(graph.edge(1) == Edge{{0}, {1}, 7});
    REQUIRE(graph.outgoing(Vertex{2}).empty());
}

TEST_CASE("DIMACS parser rejects malformed graphs with path and line", "[graph]") {
    const std::vector<std::string> malformed = {
        "", "p sp 0 0\n", "p sp 2 1\n", "a 1 2 3\n", "p sp 2 0\np sp 2 0\n",
        "p xx 2 0\n", "p sp 2 1\na 0 2 3\n", "p sp 2 1\na 1 3 3\n",
        "p sp 2 1\na 1 2 -1\n", "p sp 2 1\na 1 2 4294967296\n",
        "p sp 4294967296 0\n", "p sp 2 4294967296\n", "p sp 2 0\na 1 2 0\n",
        "p sp 2 1\na 1 2\n", "p sp 2 1\na 1 2 0 extra\n", "p sp 2 0\nx 1 2 3\n"};
    for (const auto &text : malformed) {
        CAPTURE(text);
        const DimacsFile file(text);
        try {
            const auto graph = Graph::from_dimacs(file.path());
            FAIL("malformed DIMACS unexpectedly accepted");
        } catch (const std::runtime_error &error) {
            const std::string message = error.what();
            REQUIRE(message.starts_with(file.path().string() + ':'));
        }
    }
    REQUIRE_THROWS_AS(Graph::from_edges(0, {}), std::invalid_argument);
    REQUIRE_THROWS_AS(Graph::from_edges(1, {{{0}, {1}, 0}}), std::invalid_argument);
    if constexpr (sizeof(std::size_t) > sizeof(std::uint32_t)) {
        REQUIRE_THROWS_AS(Graph::from_edges(std::size_t{unreachable} + 1, {}), std::invalid_argument);
    }
    const auto singleton = Graph::from_edges(1, {});
    REQUIRE_THROWS_AS(DijkstraWorkload(singleton, Vertex{1}), std::invalid_argument);
    REQUIRE_THROWS_AS(PrimWorkload(singleton, Vertex{1}), std::invalid_argument);
}
