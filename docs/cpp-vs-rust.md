# C++ and Rust SimdQuickHeap performance

This report compares this C++ port with the upstream Rust
[`quickheap`](https://github.com/RagnarGrootKoerkamp/quickheap) crate. Both
run the same algorithm. Any gap therefore comes from the compiler, the
standard library, or the build, not from the heap design. For a comparison
between queue algorithms, see the [README](../README.md#performance).

**Verdict.** C++ matches or beats Rust on every measured workload and size.
The C++ advantage ranges from 2% to 26%. The largest gap, on
`MostlyPushDecreasing`, comes from a compiler choice in one binary search.
Random-key workloads show no measurable difference.

## Setup

| Item | C++ | Rust |
| --- | --- | --- |
| Source | `quickheap-cpp` `9fbee59` | `quickheap-rs` `d2b8d55`; `quickheap/` crate identical to upstream `9be8408` |
| Heap | `quickheap::detail::SimdQuickHeap<int64_t>`, AVX2 backend | `quickheap::SimdQuickHeap<i64>`, feature `quickheap-avx2` |
| Runner | `quickheap_bench` | `quickheap-dev/bench/paired` `quickheap-paired` |
| Compiler | Clang 24.0.0, libstdc++ | rustc 1.101.0-nightly (2026-10-22) |
| Flags | `-O3 -DNDEBUG -mavx2 -mbmi -mpopcnt`, no LTO | `opt-level=3`, `-C target-cpu=native`, `-C force-frame-pointers=yes`, no LTO |
| CPU | Intel Core Ultra 9 285K, P-core 3 | Same |

Collected 2026-10-27 03:19 to 03:24 UTC. Other users' jobs ran on the host
(load average 7.7 at start). Alternating order exposes both sides to the same
background noise.

**Method.** `benchmark/compare.sh` generated one fixture per workload, size,
and seed: 8 workloads, sizes $2^{10}$ to $2^{16}$ in steps of 4x, `i64` keys,
seeds 1363692610, 7, and 42. Before timing, both runners replayed every
fixture and produced byte-identical pop traces. Seven process rounds then
timed each fixture on both sides, alternating which ran first. Each process
ran 1 warmup and 3 timed trials. Timing covers queue operations only.
[benchmark/CPP-VS-RUST.md](../benchmark/CPP-VS-RUST.md) gives the steps to
reproduce.

## Results

Each cell gives C++ time relative to Rust, as the geometric mean over the
three seeds of the ratio of per-fixture medians. Negative means C++ is faster.
`~` marks a cell where the C++ and Rust process ranges overlap for at least
one seed. Treat those as no measurable difference.

| Workload | $2^{10}$ | $2^{12}$ | $2^{14}$ | $2^{16}$ |
| --- | ---: | ---: | ---: | ---: |
| HeapSort | ~-7.6% | ~-6.3% | ~-8.0% | -6.4% |
| MonotoneConstantSize | -10.0% | -10.1% | -10.6% | -10.4% |
| RandomConstantSize | ~-2.9% | ~-4.6% | ~-1.9% | ~-3.6% |
| MonotoneWiggle | -7.9% | -6.8% | -6.5% | -7.0% |
| RandomWiggle | ~-3.9% | ~-3.5% | ~-3.3% | ~-3.0% |
| GeometricMonotoneWiggle | ~-6.5% | -5.7% | ~-6.4% | -7.2% |
| WorstCaseDescending | ~-11.8% | ~-8.4% | ~-7.2% | -7.9% |
| MostlyPushDecreasing | -15.8% | -20.5% | -23.5% | -26.4% |

Absolute times at $n = 2^{16}$, as the median over seeds of nanoseconds per
workload operation. Every workload performs equal numbers of pushes and
pops, so one operation is one push plus one pop, amortized. The last
column counts seeds where C++ was faster and the process ranges did not
overlap.

| Workload | C++ ns/op | Rust ns/op | Seeds C++ faster, separated |
| --- | ---: | ---: | ---: |
| HeapSort | 15.98 | 17.08 | 3/3 |
| MonotoneConstantSize | 16.61 | 18.54 | 3/3 |
| RandomConstantSize | 9.64 | 9.97 | 1/3 |
| MonotoneWiggle | 17.85 | 19.24 | 3/3 |
| RandomWiggle | 16.99 | 17.58 | 0/3 |
| GeometricMonotoneWiggle | 17.82 | 19.25 | 3/3 |
| WorstCaseDescending | 16.48 | 18.04 | 3/3 |
| MostlyPushDecreasing | 17.85 | 24.18 | 3/3 |

`benchmark/evals/cpp_vs_rust_table.py` renders both tables from the run's
`controlled-operations/report/summary.csv`.

## Same algorithm

The headers in [`include/quickheap/detail/`](../include/quickheap/detail)
mirror the Rust crate file by file.

| C++ header | Rust source | Mirrors |
| --- | --- | --- |
| `simd_quickheap.hpp` | `lib.rs` | Bucket layout, `push`, `pop`, `partition`, terminal size 16, key/value heap |
| `simd.hpp` | `simd.rs` | AVX2 compare masks, permutation tables, `push_position` |
| `partitioning.hpp` | `partitioning.rs` | In-place SIMD partition into two buckets |
| `pivots.hpp` | `pivots.rs` | Median of 3 random `(value, position)` samples |
| `rng.hpp` | rand 0.10 `ThreadRng` | ChaCha12 stream and `random_range` |
| `sort.hpp` | `core::slice` | Small insertion sort and `partition_point` |

The fixture replay check confirms this. Every pop on every fixture returns the
same key in the same order on both sides.

The C++ port differs from Rust in a few deliberate places. None changes which
key a pop returns.

| Difference | Reason |
| --- | --- |
| `pop_into(T&)` returns `bool` instead of an optional | GCC returns `std::optional<T>` through a stack spill, which stalled store forwarding on every pop |
| One thread-local RNG object, no `Rc` clone per call | C++ has no `Rc`; Rust's `rng()` clones a reference-counted handle on every sample |
| `Vec` growth uses `operator new` instead of `realloc` | A failed allocation throws `std::bad_alloc` and leaves the vector intact |
| Key/value `push` reserves both columns before writing | Keeps keys and values paired if an allocation throws; Rust aborts instead |
| `partition` removes the new pivot if partitioning throws | Keeps the heap valid after `std::bad_alloc`; Rust aborts instead |

## Why C++ runs faster

**`MostlyPushDecreasing`, 16% to 26%.** Most pushes in this workload are
decreasing keys, so the heap accumulates more than 64 pivots. `push` then
locates the bucket with `partition_point`, a binary search over the pivots.
Rust's `partition_point` selects with `hint::select_unpredictable`, so rustc
emits a conditional move (`cmovl`). The C++ port writes the same select as a ternary, which Clang compiles to a
conditional branch (`jl`).
The branch predicts well here because consecutive keys land in nearby
buckets. The processor then speculates past each comparison, while the
conditional move waits for every load. Counters on one $2^{16}$ fixture,
whole process, 50 trials:

| Counter | C++ | Rust |
| --- | ---: | ---: |
| Cycles | 3.56 G | 4.84 G |
| Instructions | 13.42 G | 13.74 G |
| Branch misses | 20.6 M | 19.7 M |

Instruction and branch-miss counts nearly match. Rust needs 36% more cycles
for the same work. Deeper searches at larger `n` widen the gap, which matches
the trend across columns. An earlier GCC build of the same C++ ran this
workload 1% to 2% slower than Rust, so this gap depends on the compiler, not
on the C++ source.

**Monotone workloads, 6% to 10%.** A `perf record` profile of
`MonotoneConstantSize` at $2^{16}$, one run per side at 900 Hz, splits the
samples as follows:

| Function | C++ samples | Rust samples |
| --- | ---: | ---: |
| `pop` | 234 | 271 |
| `partition` | 200 | 210 |
| `push` | 162 | 149 |
| Pivot sampling (`random_range`, RNG, median select) | 34 | 60 |

Pivot sampling costs Rust 26 more samples. Each Rust `random_range` call looks
up the thread-local generator and clones its `Rc` handle. Rust also calls
`select_nth_unstable` out of line to take the median, where C++ inlines the
insertion sort. Rust's `pop` costs 37 more samples. This profile does not
isolate why. The C++ `push` costs 13 more samples. The net difference of
about 60 samples, roughly 9% of the profile, matches the measured 10%.

**Random-key workloads, no measurable difference.** `RandomConstantSize` and
`RandomWiggle` spend their time on unpredictable partitions in both
languages. Their process ranges overlap in most seeds, and cycle counts on a
`RandomWiggle` fixture match within 0.2%.

## Build differences

The comparison builds each side the way its own project builds benchmarks.
These differences remain:

- Rust compiles with `-C target-cpu=native`. C++ enables only AVX2, BMI, and
  POPCNT. The heap uses no instruction beyond these on either side.
- Rust keeps frame pointers. This adds a few instructions to every non-inlined
  call.
- C++ uses Clang 24 here. GCC changes the `MostlyPushDecreasing` result, as
  noted above.

## Earlier optimizations

Before the alignment, the C++ heap used Highway and algorithm changes that
upstream Rust lacks. [`optimizations.md`](optimizations.md) lists those
changes, their measured effects, and which ones apply to the current code.
