# Reproduce the algorithm comparison

This guide regenerates the [README performance tables](../README.md#performance)
and plot. They compare priority-queue algorithms implemented in C++. For the
C++ versus Rust comparison, see [CPP-VS-RUST.md](CPP-VS-RUST.md).

## Workload

`MonotoneConstantSize` first fills the queue to `n + 50` keys outside the
timer. The timed loop then runs `10 n` steps. Each step pops the minimum and
pushes it plus a random offset, so the queue size stays constant and keys
never decrease. Input generation, setup, validation, and destruction run
outside the timer.

| Metric | Definition |
| --- | --- |
| ns/pair | Median nanoseconds per push and pop pair |
| LLC misses | Median `PERF_COUNT_HW_CACHE_MISSES` per pair at $n = 2^{25}$; Intel maps the event to last-level cache misses |

The Rust README labels the same counter as "L2 misses". Its numbers come from
an AMD CPU, where the event counts different misses, so the counts are not
comparable across the two READMEs.

## Build

Build the library package, then the benchmark with Boost and the pinned
external competitors. External sources need network access during
configuration. See [external/README.md](external/README.md) for revisions and
licenses.

```sh
conan create . -pr:a clang-native --build=missing
conan build benchmark -pr:a clang-native --build=missing \
    -o '&:boost=True' -o '&:external=True' -o '&:fixed_targets=True' \
    --output-folder build/algorithms-bench
```

Use your own Conan profile in place of `clang-native`. On x86-64 the CMake
build adds `-mavx2 -mbmi -mpopcnt` itself, so the profile needs no `-march`.
`quickheap_bench --list` prints `backend,avx2,compiled=1,supported=1` when it
does.

## Run

```sh
BENCH_CPU=5 benchmark/evals/algorithms.sh \
    build/algorithms-bench/build/Release/quickheap_bench build/algorithms
```

[`evals/algorithms.sh`](evals/algorithms.sh) pins every run to `BENCH_CPU`
(default 3) and writes one CSV per step:

| File | Heaps | Sizes | Trials |
| --- | --- | --- | --- |
| `plot.csv` | Plotted heaps | $2^{10}$ to $2^{25}$, every power | 3, after 1 warmup |
| `table.csv` | Remaining heaps | $2^{10}$, $2^{15}$, $2^{20}$ | 3, after 1 warmup |
| `table-25.csv` | Remaining fast heaps | $2^{25}$ | 3, no warmup |
| `slow-25.csv` | Boost Fibonacci, pairing, binomial, skew | $2^{25}$ | 1, no warmup |
| `types.csv` | `quickheap`, `kv_quickheap`, `u32` and `u64` | $2^{20}$ | 5, after 1 warmup |
| `machine.txt` | Kernel, `lscpu`, runner `--list` | | |

A slow Boost heap needs 10 to 20 minutes per trial at $2^{25}$, so those cells
come from one trial. A full run takes about three hours on an Intel Core
Ultra 9 285K. Pick a performance core: on hybrid Intel CPUs,
`/sys/devices/cpu_core/cpus` lists them.

## Render

```sh
python3 benchmark/evals/algorithms_table.py build/algorithms
python3 benchmark/evals/plot.py build/algorithms/plot.csv \
    --normalize opslogn --output docs/img/algorithms-u64.svg
```

`algorithms_table.py` prints both README tables as Markdown. A `*` marks a
single-trial cell. The plot divides time by `log2 n`, so a comparison sort's
$n \log_2 n$ bound draws a horizontal line. Error bars span the minimum and
maximum trial.

For other workloads, key types, or heaps, call `quickheap_bench` directly. See
[README.md](README.md#run-synthetic-workloads) for its options.
