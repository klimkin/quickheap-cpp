#include "graph_workloads.hpp"
#include "heap_catalog.hpp"
#include "backend.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace quickheap::bench;

struct Options {
    std::filesystem::path path = "input/";
    std::size_t repeats = 3;
    std::string heap;
    std::string workload;
    std::string backend = "native";
    bool help = false;
    bool verify_only = false;
};

Options parse_options(int argc, char **argv) {
    Options options;
    bool has_path = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        if (argument == "--help" || argument == "-h") {
            options.help = true;
        } else if (argument == "--verify-only") {
            options.verify_only = true;
        } else if (argument == "--repeat" || argument == "--heap" || argument == "--workload" || argument == "--backend") {
            if (++i == argc) throw std::invalid_argument("missing value for " + std::string(argument));
            const std::string_view value = argv[i];
            if (argument == "--repeat") {
                const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), options.repeats);
                if (error != std::errc{} || end != value.data() + value.size() || options.repeats == 0) {
                    throw std::invalid_argument("--repeat requires positive integer");
                }
            } else if (argument == "--backend") {
                options.backend = value;
            } else if (argument == "--heap") {
                const auto heap = std::find_if(queue_catalog.begin(), queue_catalog.end(),
                    [value](const auto &item) { return item.name == value; });
                if (heap == queue_catalog.end() || !key_only_queue(heap->kind)) {
                    throw std::invalid_argument("--heap expects a key-only queue from quickheap_bench --list");
                }
                options.heap = value;
            } else {
                if (value != "Dijkstra" && value != "Prim") {
                    throw std::invalid_argument("--workload expects Dijkstra or Prim");
                }
                options.workload = value;
            }
        } else if (argument.starts_with('-')) {
            throw std::invalid_argument("unknown option " + std::string(argument));
        } else {
            if (has_path) throw std::invalid_argument("expected one graph path");
            options.path = argument;
            has_path = true;
        }
    }
    return options;
}

std::vector<std::filesystem::path> graph_paths(const std::filesystem::path &path) {
    if (std::filesystem::is_regular_file(path)) return {path};
    if (!std::filesystem::is_directory(path)) {
        throw std::runtime_error(path.string() + ": expected graph file or directory");
    }
    std::vector<std::filesystem::path> paths;
    for (const auto &entry : std::filesystem::directory_iterator(path)) {
        if (entry.is_regular_file() && entry.path().extension() == ".gr") paths.push_back(entry.path());
    }
    std::sort(paths.begin(), paths.end());
    if (paths.empty()) throw std::runtime_error(path.string() + ": no .gr files");
    return paths;
}

std::string csv_field(std::string_view value) {
    if (value.find_first_of(",\"\r\n") == std::string_view::npos) return std::string(value);
    std::string result = "\"";
    for (char character : value) {
        if (character == '"') result += '"';
        result += character;
    }
    return result + '"';
}

template <class Queue, class Workload>
void run_workload(const Workload &workload, const Graph &graph,
                  const std::filesystem::path &path, std::string_view heap,
                  std::string_view name, std::size_t repeats, bool verify_only = false) {
    if (verify_only) {
        auto trial = workload.template prepare<Queue>(Queue(graph.edge_count() + 1));
        trial.execute();
        workload.verify(trial);
        const auto counts = trial.counts();
        std::cout << "counts," << name << ',' << counts.pushes << ',' << counts.pops << '\n';
        if constexpr (requires { trial.distances(); }) {
            std::cout << "distances";
            for (const auto distance : trial.distances()) std::cout << ',' << distance;
            std::cout << '\n';
        } else {
            std::cout << "parents";
            for (const auto parent : trial.parents()) std::cout << ',' << parent;
            std::cout << "\nweight," << trial.weight() << "\ntree_edges";
            for (const auto edge : trial.tree_edges()) std::cout << ',' << edge;
            std::cout << '\n';
        }
        return;
    }
    {
        auto warmup = workload.template prepare<Queue>(Queue(graph.edge_count() + 1));
        warmup.execute();
        workload.verify(warmup);
    }
    for (std::size_t repeat = 0; repeat < repeats; ++repeat) {
        auto trial = workload.template prepare<Queue>(Queue(graph.edge_count() + 1));
        const auto start = std::chrono::steady_clock::now();
        trial.execute();
        const auto stop = std::chrono::steady_clock::now();
        workload.verify(trial);
        const auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count();
        const auto counts = trial.counts();
        std::cout << heap << ',' << csv_field(path.string()) << ',' << name << ','
                  << graph.vertex_count() << ',' << graph.edge_count() << ','
                  << counts.pushes << ',' << counts.pops << ',' << repeat << ',' << nanos << '\n';
        std::cout.flush();
        if (!std::cout) throw std::runtime_error("cannot write benchmark CSV");
    }
}

template <class Queue> class GraphQueue {
  public:
    explicit GraphQueue(std::size_t capacity) : queue_(make_queue<Queue>(capacity)) {}
    void push(std::uint64_t key) {
        if constexpr (reserved_endpoints) {
            if (key >= std::numeric_limits<std::uint64_t>::max() - 1)
                throw std::overflow_error("graph queue key reaches external sentinel");
            ++key;
        }
        queue_.push(key);
    }
    std::optional<std::uint64_t> try_pop() {
        auto key = queue_.try_pop();
        if constexpr (reserved_endpoints) {
            if (key) --*key;
        }
        return key;
    }
    bool empty() const { return queue_.size() == 0; }

  private:
#ifdef QUICKHEAP_BENCH_EXTERNAL
    static constexpr bool reserved_endpoints = std::is_same_v<Queue, SequenceHeap<std::uint64_t>> ||
        std::is_same_v<Queue, S3QHeap<std::uint64_t>>;
#else
    static constexpr bool reserved_endpoints = false;
#endif
    Queue queue_;
};

template <class Workload>
void run_heaps(const Workload &workload, const Graph &graph,
               const std::filesystem::path &path, std::string_view name,
               const Options &options) {
    for (const auto &heap : queue_catalog) {
        if (!key_only_queue(heap.kind) || (!options.heap.empty() && options.heap != heap.name)) continue;
        const bool zero_weight = [&] {
            for (std::size_t id = 0; id < graph.edge_count(); ++id)
                if (graph.edge(static_cast<std::uint32_t>(id)).weight == 0) return true;
            return false;
        }();
        if (heap.monotone && (name == "Prim" || zero_weight)) {
            if (!options.heap.empty())
                throw std::invalid_argument("radix_heap requires Dijkstra with strictly positive weights for packed ID ordering");
            std::cerr << "skip radix_heap/" << name << ": packed keys require strictly positive Dijkstra weights\n";
            continue;
        }
        visit_queue<std::uint64_t, false>(heap.kind, [&]<class Queue>() {
            if constexpr (std::is_same_v<typename Queue::Item, std::uint64_t>)
                run_workload<GraphQueue<Queue>>(workload, graph, path, heap.name, name, options.repeats, options.verify_only);
        });
    }
}

} // namespace

int main(int argc, char **argv) {
    try {
        const auto options = parse_options(argc, argv);
        if (options.help) {
            std::cout << "Usage: quickheap_bench_graph [file.gr|directory] [options]\n"
                         "  --repeat N                 Positive trial count, default 3\n"
                         "  --heap HEAP                Key-only queue, default all available\n"
                         "  --workload WORKLOAD        Dijkstra or Prim, default both\n"
                         "  --help                     Show help\n"
                         "  --verify-only              Untimed results and queue counts\n"
                         "Default input directory: input/\n"
                         "Source vertex: 0. Prim uses minimum-edge undirected conversion.\n"
                         "Dijkstra rejects candidate distances >= UINT32_MAX.\n";
            return 0;
        }
        select_backend(options.backend);
        const auto paths = graph_paths(options.path);
        if (!options.verify_only) std::cout << "heap,graph,workload,vertices,edges,push_count,pop_count,repeat,nanos\n";
        for (const auto &path : paths) {
            const auto graph = Graph::from_dimacs(path);
            if (options.workload.empty() || options.workload == "Dijkstra") {
                const DijkstraWorkload workload(graph);
                run_heaps(workload, graph, path, "Dijkstra", options);
            }
            if (options.workload.empty() || options.workload == "Prim") {
                // Upstream runner omits conversion although its Prim test requires it.
                const auto undirected = graph.undirected();
                const PrimWorkload workload(undirected);
                run_heaps(workload, undirected, path, "Prim", options);
            }
        }
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "quickheap_bench_graph: " << error.what() << '\n';
        return 1;
    }
}
