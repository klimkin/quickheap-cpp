# Benchmark port

Native runners port workloads, competitors, and development experiments from
[upstream `quickheap-dev/bench`](https://github.com/RagnarGrootKoerkamp/quickheap/tree/a9f2518d9ed452130361cfe1681c68625a68ab35/quickheap-dev/bench).
Local Catch2 performance checks belong to the main package's `test/` directory.
This package owns algorithm and configuration comparisons. All build
inputs, tests, dependencies, and experimental kernels belong to this directory.
Native runners write CSV to standard output and diagnostics to standard error.
`--help` lists each runner's options. `quickheap_bench --list` lists enabled
heaps, workloads, and element types.

[ALGORITHMS.md](ALGORITHMS.md) reproduces the README algorithm tables.
[CPP-VS-RUST.md](CPP-VS-RUST.md) reproduces the C++ and Rust comparison in
[docs/cpp-vs-rust.md](../docs/cpp-vs-rust.md).

## Select benchmark profile

Use `original-rust` to reproduce original Rust workload lifecycle. Use `controlled-operations` to compare queue operations on shared inputs. Controlled remains default.

```sh
quickheap_bench --profile original-rust --min 10 --max 16 --i64 \
    --heap quickheap,std_priority_queue > original-cpp.csv
quickheap_bench --profile controlled-operations --fixture input.qhf \
    --heap std_priority_queue --repeat 3 > controlled-cpp.csv
```

Run Rust commands from Rust repository root.

```sh
python3 quickheap-dev/bench/profiles.py --profile original-rust \
    --min 10 --max 16 --i64 > original-rust.csv
python3 quickheap-dev/bench/profiles.py --profile controlled-operations \
    --fixture ../quickheap-cpp/input.qhf --heap binary_heap --repeat 3 > controlled-rust.csv
```

Original profile keeps one warmup, three repeats, fresh random inputs, original reservation, timed input and queue destruction, and per-workload cutoffs. Original Rust runner exports no input fixtures. Cross-language inputs match distributions. Unrepaired original Rust fails compilation on current nightly. Rust launcher can accept declared repairs under separately attributed source revisions. Failed builds never substitute another runner.

Controlled profile excludes construction, setup, validation, and destruction. Both languages consume identical fixture bytes. Compare results within one profile. Profile selection matches workload policy; [coverage catalog](COVERAGE.md) records implementation parity separately.

## Build and compare both repositories

Run [`compare.sh`](compare.sh) from the C++ checkout. Rust checkout defaults to
sibling `../quickheap-rs`. Set `RUST_REPO` for another location.

```sh
benchmark/compare.sh both
benchmark/compare.sh controlled-operations build/controlled-comparison
BENCH_CPU=2 HOST_PROFILE=default BUILD_PROFILE=default \
    benchmark/compare.sh original-rust build/original-comparison
```

Every measured subprocess uses `taskset -c` on one kernel-identified P core.
Set `BENCH_CPU=2` to select CPU 2. Default selects first allowed online CPU from
Linux `cpu_core/cpus`. Missing proof or an E-core selection fails before build.
Reports archive kernel core identity and observed child affinity. Builds retain
parallel execution; CTest and packaged timing smokes use the same pinned core.

Use installed Conan profile names or profile paths. Host profile defaults to
`default`; build profile defaults to host profile. Set `CPP_AXIS=clang-native`
for a Clang host profile. Python requires matplotlib. Run `--help` for options.

Script builds current local C++ library and benchmark packages through
`verify.py`, completes package checks, and builds Rust in a fresh target directory.
Default pair compares production Quickheap with AVX2. Set `PAIR=std` to compare
standard-library queues, which catalog labels as substitutes.

Defaults use i64 and four active workloads at exponents 10, 12, 14, and 16.
Set `WORKLOADS` to a comma-separated list of workload names to change the set.
Set `MIN`, `MAX`, `STEP`, or `ELEM` to change controlled inputs. Controlled runs
share fixture bytes, compare full correctness traces, and alternate runner order
per fixture and process. `RUNS` defaults to seven independent process runs.
`SEEDS=1363692610,7,19` selects distinct controlled fixture streams; default seed
preserves existing fixture bytes. `REPEAT` and `WARMUP` default to 3 and 1.
Original profile uses every
exponent between `MIN` and `MAX`, fixed three repeats, one warmup, and cutoffs.
Original Rust runs all enabled registrations; C++ selects chosen counterpart.

Output directory must be new. Omitted directory creates `build/compare-*`.
Each profile writes raw CSVs and full reports under `process-N/report/`, containing
summary CSV, LaTeX, paper Markdown, SVG, PDF, and provenance. Profile `report/`
aggregates process medians and their range without merging or renumbering trials,
and emits Markdown, LaTeX, SVG, and PDF for complete groups.
Its manifest attributes each input report and CPU identity. Original cutoffs can
omit larger groups from some processes; aggregate rows retain observed process
counts and manifest records incomplete groups. Controlled missing groups fail. Top-level logs retain commands,
build output, source state, original repair manifests when present, and CPU data.
Original results retain reference revision and declared Rust repairs.

`status.tsv` records completed and failed phases. Exit code 0 requires all
selected profiles to pass. With `both`, original failure still permits controlled
execution; script exits nonzero and retains failure logs. Failed profile produces
no comparison report.

## Run paired fixtures

`quickheap-cpp-v1` uses C++-owned timer boundaries and shared raw inputs.
Read [protocol](PROTOCOL.md) for lifecycle, workload, and provenance rules.
[Coverage catalog](COVERAGE.md) distinguishes ports, substitutes, and tracked gaps.

```sh
quickheap_fixture --generate MonotoneConstantSize --elem i64 --n 1024 --output input.qhf
taskset -c 2 quickheap_bench --profile controlled-operations --fixture input.qhf --heap std_priority_queue --repeat 3 > paired.csv
quickheap_bench --fixture input.qhf --heap std_priority_queue --verify-only
```

Fixture mode validates all operations with separate queue state before timing.
Monotone pushes use tested heap pops. Timed loops borrow input and write no
output buffer. Construction, setup, validation, and destruction stay outside
timer. Default synthetic timing generates same raw fixtures and uses direct loops.
Synthetic legacy comparison/ablation entrypoints retain replay until migrated.
Experiment variants, ablation, and comparisons consume direct fixtures.
Partition and rebalancing microbenchmarks retain separate historical identity.

## Run synthetic workloads

Create Quickheap in the local Conan cache, then build this directory as its own
application. Run commands from the repository root.

```sh
conan create . -pr:h default -pr:b default -s:h compiler.cppstd=20 --build=missing
conan build benchmark -pr:h default -pr:b default -s:h compiler.cppstd=20 --build=missing
benchmark/build/Release/quickheap_bench --min 10 --max 20 --step 2 \
    --repeat 3 --i64 > plot.csv
```

Select workloads and heaps with comma-separated names.

```sh
benchmark/build/Release/quickheap_bench --min 10 --max 10 \
    --heap quickheap,std_priority_queue --workload HeapSort --i32 > heapsort.csv
benchmark/build/Release/quickheap_bench --min 10 --max 20 --table > table.csv
benchmark/build/Release/quickheap_bench --min 10 --max 20 --memory > memory.csv
benchmark/build/Release/quickheap_bench --max 16 --comparisons > comparisons.csv
benchmark/build/Release/quickheap_bench --min 10 --max 16 --ablation > ablation.csv
```

`--min`, `--max`, and `--step` select powers of two. Default sizes span
`2^10` through `2^25` in exponent steps of one. Default types are `i32` and
`i64`. Add `--u32` or `--u64` for unsigned keys. Fixed seeds make input
generation repeatable. Signed monotone workloads check key overflow.

Default timing includes direct workload loops. Input generation, queue construction, setup,
output validation, and destruction run outside timing. Each measured repeat
uses fresh state after untimed warmup. `--repeat` defaults to three.
Native runners use one warmup; `quickheap_bench --warmup N` overrides its count.

Synthetic `ops` counts push/pop pairs. Push-heavy workloads include their
final drain in the timed replay, so each replay has equal push and pop totals.
Total queue calls equal `2 * ops`.

## Run graph workloads

Run Dijkstra and Prim against a DIMACS shortest-path file or directory.
Directory input selects `.gr` files in filename order.

```sh
benchmark/build/Release/quickheap_bench_graph benchmark/data/smoke.gr \
    --repeat 3 > graphs.csv
benchmark/build/Release/quickheap_bench_graph input/ --workload Dijkstra \
    --heap quickheap > dijkstra.csv
python3 benchmark/evals/graph_weights.py input/ > weights.csv
```

DIMACS input requires `p sp V E` and exactly `E` records of form `a U V W`.
Vertices use one-based indices. Weights must be nonnegative. Blank lines and
`c` comments are valid. Invalid records, endpoint indices, and counts fail
before timing.

Dijkstra follows directed arcs from vertex zero. Prim converts input to an
undirected graph, retaining minimum weight for duplicate endpoint pairs, then
runs on the component containing vertex zero. Other components remain
unreached. Dijkstra queue entries pack a distance and vertex into a 64-bit
key. Prim queue entries pack an edge weight and edge identifier.
Dijkstra rejects candidate distances at or above `UINT32_MAX`.
Radix graph queues require strictly positive Dijkstra weights because packed
vertex IDs can decrease across zero-weight edges. Default graph sweeps skip
radix for zero-weight graphs and Prim. Explicit incompatible selection fails.

Timing includes complete graph algorithms and queue operations. Loading,
CSR construction, scratch allocation, and result validation stay outside
timing. `push_count` and `pop_count` count actual queue calls. Relaxation may
insert multiple entries per vertex, so these counts differ from graph edges.

## Compare algorithm configurations

Select an experiment family with `quickheap_experiments`.

```sh
benchmark/build/Release/quickheap_experiments --mode variants \
    --min 10 --max 16 --repeat 3 > variants.csv
benchmark/build/Release/quickheap_experiments --mode partition \
    --min 10 --max 16 --repeat 3 > partition.csv
```

Modes are `variants`, `ablation`, `comparisons`, `rebalancing`, and
`partition`. `--variant` selects an exact variant name. `--list` prints
available variants. `--workload` selects a synthetic workload for the first
three modes, `best`, `worst`, or `flat` for rebalancing, and `random`,
`duplicates`, or `ascending` for partition. Key-type flags match the
synthetic runner. Experiment defaults select `i64`.

Development variants use a benchmark-private C++ engine. Variants measure
equivalent algorithm choices rather than reproducing Rust compiler output.
Production Quickheap keeps its existing API and implementation.
Variant families include scalar and SIMD partition, linear and binary pivot
search, random and sampled pivots, terminal bucket sizes, and rebalancing.
`median3_partition3` and `median3_partition4` split three or four buckets in
one pass. `blocked112_partition2`, `blocked112_partition3`, and
`blocked112_partition4` use linked blocks with 112 live slots and 128 allocated
slots per block. These variants execute block partitions directly.
SIMD availability follows the compiled target and host CPU. Runner output
identifies the variants actually measured.

Comparison runs replay separately from timing. Instrumented scalar heaps
count key comparisons. Perfect-pivot selection excludes oracle-sort comparisons
from counts, matching upstream; elapsed timing includes the sort. SIMD variants
leave exact comparison counts blank.
Blank cells mean unavailable measurements. Timing results never use
comparison instrumentation. Native timing and experiment Linux performance counters require
`--perf` and kernel access to performance events.

## Collect and plot results

Run every available family into an output directory. Supply a third argument
to include graphs.

```sh
MIN=10 MAX=14 REPEAT=3 benchmark/run-all.sh benchmark/build/Release \
    benchmark/evals/data benchmark/data
python3 benchmark/evals/plot.py benchmark/evals/data/plot.csv --table > summary.csv
python3 benchmark/evals/plot.py benchmark/evals/data/plot.csv --output plot.svg
python3 benchmark/evals/plot.py benchmark/evals/data/graphs.csv \
    --normalize pushes --output graphs.pdf
python3 benchmark/evals/plot.py benchmark/evals/data/comparisons.csv \
    --metric comparisons --output comparisons.png
```

`run-all.sh` accepts binary, output, and graph-input directories. `MIN`,
`MAX`, `STEP`, and `REPEAT` control size and repeats. `ELEM`, `HEAP`, and
`WORKLOAD` filter synthetic timing. `MEMORY_HEAP` filters memory runs.
`VARIANT` filters the `variants` experiment family.
`GRAPH_HEAP` and `GRAPH_WORKLOAD` filter graph runs. `PERF=1` enables
performance counters for experiments. Missing optional experiment
executables or graph input produce explicit skip messages. Failed runners
leave prior CSV files intact.

Tables use Python's standard library. Plots require `matplotlib` and accept
`.svg`, `.png`, or `.pdf` output. Plotting uses a noninteractive backend.
Heap names come directly from CSV. Filters accept exact `--heap`,
`--workload`, and `--elem` values. Repeat a filter to include multiple values.

Summaries group synthetic rows by heap, workload, element type, and `n`.
Graph summaries group by heap, workload, graph path, and vertex count.
Each group reports median, minimum, maximum, and repeat count after
normalizing each repeat. Plot error bars span minimum through maximum.
Non-timing CSVs skip with a diagnostic. Malformed columns, invalid numbers,
zero operation counts, and zero normalization denominators fail.

Default synthetic units are `ns/push-pop pair`. Default graph units are
`ns/queue call`, dividing by `push_count + pop_count`. `--normalize pushes`
uses graph push count and labels `ns/push`. `--normalize opslogn` divides synthetic metrics by `ops * log2(n)` and
applies workload factor once. `--normalize nlogn` divides by
`n * log2(n)` and labels `ns/(n log2 n)`. `--normalize none` retains raw
values. `--metric comparisons` sums push and pop comparisons. Other metric
names select numeric CSV columns, including hardware counters.
Partition CSVs use `variant` and `backend` names. Default partition units
are `ns/element`, dividing one partition's elapsed time by input size `n`.

## Support reference

| Upstream feature | C++ port |
| --- | --- |
| Eight synthetic workloads | Native comparison replay |
| Signed and unsigned 32-bit and 64-bit keys | All four types |
| Key/value Quickheap | Native synthetic comparisons |
| Binary, d-ary, weak, radix competitors | Native benchmark adapters |
| Boost heap competitors | Optional build dependency |
| SequenceHeap, S3Q, original Quickheap | Optional pinned external sources |
| Dijkstra, Prim, DIMACS input | Native graph runner |
| SIMD, pivot, terminal bucket, rebalancing experiments | Benchmark-private C++ variants |
| Key comparisons | Separate instrumented replay |
| Hardware counters | Optional Linux performance events |
| CSV summaries and plots | Python standard library, optional matplotlib |

External competitors require `QUICKHEAP_BENCH_EXTERNAL=ON`, network access,
and compatible workload keys. SequenceHeap and S3Q reserve both extreme key
values. Original Quickheap requires fixed capacity. These adapters support
key-only workloads. Source revisions and license terms appear in
[external competitor reference](external/README.md).
Default sweeps skip SequenceHeap and S3Q cases containing reserved keys.
Explicit selection of incompatible cases fails before timing.

Benchmark Conan options `&:boost=True` and `&:external=True` enable optional
competitors. Set `boost/*:header_only=True` in the host profile or command line
for header-only Boost. Set `quickheap/*:scalar=True` to select scalar kernels
in both the packaged library and benchmark. Direct CMake
configuration uses `QUICKHEAP_BENCH_BOOST` and `QUICKHEAP_BENCH_EXTERNAL`.

## CSV reference

Synthetic timing and comparison CSVs retain upstream columns.

```text
elem,heap,n,workload,ops,repeat,nanos,push_comparisons,pop_comparisons,branch_misses,l1_cache_misses,hw_cache_misses,hw_cache_references,l3_cache_misses
```

Graph CSVs retain upstream columns.

```text
heap,graph,workload,vertices,edges,push_count,pop_count,repeat,nanos
```

Memory CSVs report allocated element slots after replay.

```text
elem,heap,n,workload,size,capacity,capacity_ratio
```

Memory `capacity_ratio` divides capacity by workload `n`. Key/value capacity
sums both columns. Capacity excludes container bookkeeping and allocator
overhead. Comparison and hardware-counter fields remain blank when the
runner cannot measure them.

Experiment replay CSVs add `partitions`, `elements_partitioned`, `rebalances`,
`backend`, `multi_partitions`, and `blocks_partitioned` to synthetic columns.
Partition microbenchmarks use their own
schema.

```text
elem,variant,backend,n,workload,repeat,nanos,upper,lower,branch_misses,hw_cache_misses,hw_cache_references
```

Partition `upper` and `lower` count output elements and sum to `n`.

Source mapping follows [upstream bench runners](https://github.com/RagnarGrootKoerkamp/quickheap/tree/a9f2518d9ed452130361cfe1681c68625a68ab35/quickheap-dev/bench/examples)
and [evaluation tools](https://github.com/RagnarGrootKoerkamp/quickheap/tree/a9f2518d9ed452130361cfe1681c68625a68ab35/quickheap-dev/bench/evals).
Plots correct upstream graph normalization labels and avoid Rust-specific
heap-name rewriting or hidden workload filters.

| Upstream example | Native entrypoint |
| --- | --- |
| `bench.rs` | `quickheap_bench`, timing, table, memory, comparisons, ablation |
| `bench_graph.rs` | `quickheap_bench_graph`, directed Dijkstra and undirected Prim |
| `run.rs` | `quickheap_experiments --mode variants`, pivot/search/terminal choices |
| `simd.rs` | `--mode variants`, flat and multipartition choices; `--mode partition` |
| `neon.rs` | Linked-block variants use host-selected Highway backend |
| `measure_rebal_speedup.rs` | `--mode rebalancing`, best, worst, flat layouts |

C++ linked blocks use RAII ownership rather than Rust's shared block arena.
Highway selects the host backend instead of exposing Rust's backend types.
Partition diagnostics count blocks and elements; Rust's spill/copy-instrumentation
fields have no direct counterpart. Scalar perfect pivots exclude oracle-sort
comparisons. Numeric results require separate measurement on each build.

## Verify benchmark port

```sh
ctest --test-dir benchmark/build/Release --output-on-failure
MIN=8 MAX=8 REPEAT=1 benchmark/run-all.sh benchmark/build/Release \
    build/benchmark-results benchmark/data
python3 benchmark/evals/plot.py build/benchmark-results/variants.csv --table
```

Contract tests compare queues with sorted or standard-queue oracles. Cases cover
all four key types, duplicate extrema, graph parsing, source components, overflow,
external sentinels, multiway partitions, and linked blocks. CTest exercises all
five experiment modes. Size256 run-all checks trigger multipartition above128
and blocked processing beyond one112-element block.

`conan create benchmark` packages executables under `bin` and tooling and
fixtures under `share/quickheap-bench`. Its `test_package` runs the installed
CLIs and graph analysis against packaged fixtures. Benchmark CMake resolves
`quickheap::quickheap` from Conan; it never reads library source directories.

Independent SIMD executables build with `-DQUICKHEAP_BENCH_FIXED_TARGETS=ON`
or Conan `-o '&:fixed_targets=True'`. `--backend avx2` selects AVX2 in
generic runner. Unsupported CPU targets fail before queue execution.

`--diagnostics --fixture FILE` collects queue capacity, slot bytes, peak
occupancy, partitions, retries, samples, and available layer shape outside
timing. Unavailable measurements remain blank.

`evals/report.py CSV --output DIR --fixtures FIXTURE_DIR --plot` writes
attributed CSV, LaTeX, paper Markdown, plots, and manifest.
[Reporting reference](evals/README.md) defines grouping and historical archive
validation. Historical files retain unknown measurement-build provenance.
