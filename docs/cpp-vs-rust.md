# C++ and Rust SimdQuickHeap performance

This report compares this C++ port with the upstream Rust
[`quickheap`](https://github.com/RagnarGrootKoerkamp/quickheap) crate. Both
run the same algorithm. Any gap therefore comes from the compiler, the
standard library, or the build, not from the heap design. For a comparison
between queue algorithms, see the [README](../README.md#performance).

**Verdict.** C++ beats Rust on every measured workload and size, by 1.6% to
28%. The largest gap, on `MostlyPushDecreasing`, comes from a compiler choice
in one binary search. Random-key workloads show the smallest gap, 1.6% to
2.2% on `RandomWiggle`.

## Setup

| Item | C++ | Rust |
| --- | --- | --- |
| Source | `quickheap-cpp` `c1b1079`; heap headers unchanged since `663db43` | `quickheap-rs` `d2b8d55`; `quickheap/` crate identical to upstream `9be8408` |
| Heap | `quickheap::detail::SimdQuickHeap<int64_t>`, AVX2 backend | `quickheap::SimdQuickHeap<i64>`, feature `quickheap-avx2` |
| Runner | `quickheap_bench` | `quickheap-dev/bench/paired` `quickheap-paired` |
| Compiler | Clang 24.0.0, libstdc++ | rustc 1.101.0-nightly (2026-10-27) |
| Flags | `-O3 -DNDEBUG -mavx2 -mbmi -mpopcnt`, no LTO | `opt-level=3`, `-C target-cpu=native`, `-C force-frame-pointers=yes`, no LTO |
| CPU | Intel Core Ultra 9 285K, P-core 3 | Same |

Collected 2026-10-29 19:39 to 19:57 UTC in two runs that split the cells
between them. Other users' jobs ran on the host (load average 2.1 to 3.1).
Alternating order exposes both sides to the same background noise.

**Method.** `benchmark/compare.sh` generated one fixture per workload, size,
and seed: 8 workloads, sizes $2^{10}$ to $2^{16}$ in steps of 4x, `i64` keys,
seeds 1363692610, 7, and 42. Before timing, both runners replayed every
fixture and produced byte-identical pop traces. A pilot process per side then
timed each fixture 15 times and set its trial count so that the bootstrap
standard error of each side's median falls to 0.5%. The counts ranged from 5
trials of 11 ms to 776 trials of 0.1 ms, with at least 10 ms of warmup. Fifteen
process rounds then timed each fixture on both sides with that plan,
alternating which ran first. Timing covers queue operations only.
[benchmark/CPP-VS-RUST.md](../benchmark/CPP-VS-RUST.md) gives the steps to
reproduce.

## Results

Each cell gives C++ time relative to Rust. Negative means C++ is faster.
Within a round both sides time the same fixture back to back, so their ratio
cancels drift the two share. Each round contributes the mean over seeds of
log(C++ / Rust). The cell gives the Hodges-Lehmann estimate over the 15
rounds. Every 90% interval excludes zero.

| Workload | $2^{10}$ | $2^{12}$ | $2^{14}$ | $2^{16}$ |
| --- | ---: | ---: | ---: | ---: |
| HeapSort | -9.9% | -7.7% | -6.5% | -5.5% |
| MonotoneConstantSize | -10.7% | -10.1% | -10.2% | -10.2% |
| RandomConstantSize | -7.5% | -6.7% | -4.9% | -2.2% |
| MonotoneWiggle | -8.0% | -6.5% | -6.3% | -6.2% |
| RandomWiggle | -1.8% | -1.9% | -2.2% | -1.6% |
| GeometricMonotoneWiggle | -7.1% | -6.3% | -5.6% | -5.6% |
| WorstCaseDescending | -10.0% | -8.1% | -7.3% | -7.8% |
| MostlyPushDecreasing | -16.2% | -20.0% | -23.0% | -28.2% |

90% Wilcoxon signed-rank intervals over rounds:

| Workload | $2^{10}$ | $2^{12}$ | $2^{14}$ | $2^{16}$ |
| --- | ---: | ---: | ---: | ---: |
| HeapSort | -10.2% to -9.7% | -8.0% to -7.5% | -6.9% to -6.2% | -5.8% to -5.3% |
| MonotoneConstantSize | -11.1% to -10.4% | -10.4% to -9.9% | -10.7% to -10.1% | -10.5% to -9.9% |
| RandomConstantSize | -7.8% to -7.2% | -7.1% to -6.2% | -5.4% to -4.4% | -2.8% to -1.7% |
| MonotoneWiggle | -8.3% to -7.6% | -6.8% to -6.3% | -6.5% to -6.1% | -6.4% to -6.0% |
| RandomWiggle | -2.1% to -1.5% | -2.3% to -1.2% | -2.9% to -1.7% | -2.4% to -1.1% |
| GeometricMonotoneWiggle | -7.4% to -6.9% | -6.8% to -5.9% | -6.0% to -5.4% | -6.3% to -5.3% |
| WorstCaseDescending | -10.5% to -9.6% | -8.3% to -7.9% | -7.6% to -7.0% | -8.3% to -7.6% |
| MostlyPushDecreasing | -16.4% to -16.0% | -20.1% to -19.9% | -23.1% to -22.8% | -28.5% to -27.8% |

Absolute times at $n = 2^{16}$, as the median over seeds of nanoseconds per
workload operation. Every workload performs equal numbers of pushes and
pops, so one operation is one push plus one pop, amortized.

| Workload | C++ ns/op | Rust ns/op | 90% interval | Paired rounds |
| --- | ---: | ---: | ---: | ---: |
| HeapSort | 16.08 | 17.04 | -5.8% to -5.3% | 15 |
| MonotoneConstantSize | 16.78 | 18.64 | -10.5% to -9.9% | 15 |
| RandomConstantSize | 9.68 | 9.80 | -2.8% to -1.7% | 15 |
| MonotoneWiggle | 18.16 | 19.38 | -6.4% to -6.0% | 15 |
| RandomWiggle | 17.17 | 17.42 | -2.4% to -1.1% | 15 |
| GeometricMonotoneWiggle | 18.15 | 19.19 | -6.3% to -5.3% | 15 |
| WorstCaseDescending | 16.15 | 17.60 | -8.3% to -7.6% | 15 |
| MostlyPushDecreasing | 15.99 | 22.23 | -28.5% to -27.8% | 15 |

`benchmark/evals/cpp_vs_rust_table.py` renders these tables from the raw
`controlled-operations` CSVs of both runs.

Short processes shrink some gaps. With 1 warmup and 3 trials per process,
as in an earlier run, `RandomConstantSize` at $2^{10}$ measured -2.6% (90%
interval -4.5% to -0.8%) on the same host and hour, against -7.5% here. This
report gives the steady-state gap.

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

**`MostlyPushDecreasing`, 16% to 28%.** Most pushes in this workload are
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

**Monotone workloads, 6% to 11%.** A `perf record` profile of
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

**Random-key workloads, 2% to 8%.** `RandomConstantSize` and `RandomWiggle`
spend their time on unpredictable partitions in both languages. C++ leads
`RandomWiggle` by about 2% at every size. Its lead on `RandomConstantSize`
falls from 7.5% at $2^{10}$ to 2.2% at $2^{16}$. Cycle counts on one
`RandomWiggle` fixture matched within 0.2% in an earlier whole-process
measurement, so this report does not isolate the cause.

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
