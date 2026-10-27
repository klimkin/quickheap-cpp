# Reproduce the C++ versus Rust comparison

This guide regenerates the measurements in
[docs/cpp-vs-rust.md](../docs/cpp-vs-rust.md). For the comparison between
queue algorithms, see [ALGORITHMS.md](ALGORITHMS.md).

## Inputs

| Side | Source |
| --- | --- |
| C++ | This checkout, `quickheap` heap, AVX2 backend |
| Rust | Sibling checkout `../quickheap-rs`, crate `quickheap/` at upstream algorithm, `quickheap-dev/bench/paired` runner with feature `quickheap-avx2` |

Set `RUST_REPO` when the Rust checkout lives elsewhere. The paired Rust runner
reads the same fixture format and prints the same CSV as `quickheap_bench`.

## Run

```sh
BENCH_CPU=3 HOST_PROFILE=clang-native CPP_AXIS=clang-native \
RUSTFLAGS="-C target-cpu=native -C force-frame-pointers=yes" \
SEEDS=1363692610,7,42 \
WORKLOADS=HeapSort,MonotoneConstantSize,MonotoneWiggle,RandomWiggle,RandomConstantSize,GeometricMonotoneWiggle,WorstCaseDescending,MostlyPushDecreasing \
    benchmark/compare.sh controlled-operations build/cpp-vs-rust
```

`RUSTFLAGS` replaces any Cargo `build.rustflags`, so it pins the Rust flags
the report used regardless of local Cargo configuration. `HOST_PROFILE` selects the Conan host profile and `CPP_AXIS` the compiler
configuration. On x86-64 the CMake build adds
`-mavx2 -mbmi -mpopcnt` itself, so the profile needs no `-march`.
[`compare.sh`](compare.sh) performs these steps:

1. Builds and tests the C++ library and benchmark packages through
   [`verify.py`](verify.py).
2. Runs `cargo test`, then builds the Rust paired runner in release mode.
3. Generates one fixture per workload, size, and seed with
   `quickheap_fixture`. Defaults cover sizes $2^{10}$, $2^{12}$, $2^{14}$,
   and $2^{16}$ with `i64` keys.
4. Replays each fixture untimed through both runners with `--verify-only` and
   requires byte-identical pop traces (`cmp`).
5. Runs `RUNS` (default 7) process rounds. Each round times every fixture on
   both sides, alternating which side runs first. Each process runs 1 warmup
   and 3 timed trials.
6. Writes per-round reports and an aggregate under
   `controlled-operations/report/`.

Every measured process runs under `taskset -c $BENCH_CPU`. The script rejects
a CPU that the kernel does not report as a performance core.

## Read the results

| Path | Content |
| --- | --- |
| `status.tsv` | Pass or fail per phase |
| `controlled-operations/report/` | Aggregate CSV, Markdown, LaTeX, SVG, PDF, manifest |
| `controlled-operations/process-N/` | Raw C++ and Rust CSVs per round |
| `logs/` | Every command, its exit status, stdout, and stderr |
| `cpp.revision`, `rust.revision`, `*.diff` | Source state of both checkouts |
| `cpu.json`, `cpu.txt` | Core identity and `lscpu` |

The aggregate reports the median of the per-process medians, with the range
of process medians. [PROTOCOL.md](PROTOCOL.md) defines the fixture format,
timer boundaries, and provenance fields. [evals/README.md](evals/README.md)
defines report grouping.

The timed region excludes fixture parsing, queue construction, setup,
validation, and destruction on both sides. Both runners consume identical
fixture bytes, so input distribution and order cannot explain a gap.
