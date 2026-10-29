# Heap optimizations before the upstream alignment

From 2026-10-22 to 2026-10-25 the C++ heap carried optimizations that upstream
Rust lacks. Commit `f9b3c84` then replaced the heap with a step-for-step mirror
of upstream Rust `9be8408`, so later algorithm work lands in both repositories
against one baseline. The current code contains none of these changes. This
file lists them so they can be ported to both sides.

Each commit message records the full measurement and test results. Run
`git show <commit>` to see the code. That code predates the alignment: it
lives in `src/bucket_heap.hpp` and `src/simd.cpp` and uses Highway, not the
headers in [`include/quickheap/detail/`](../include/quickheap/detail).

## Measured effects

Each delta compares a commit with its parent on the old heap, not with the
current code. Treat them as a guide to what is worth trying, not as a
prediction.

Unless a row says otherwise, runs use GCC 15.2, i64 keys, $n = 16384$, seeds
1363692610, 7, and 42, seven alternating processes per side, and one pinned
P-core of an Intel Core Ultra 9 285K. From `ab2d043` through `c1f9de6`, every
change left all 24 workload and seed cells without a separated regression,
and its output traces matched Rust.

| Commit | Change | Effect |
| --- | --- | --- |
| `92ff19a` | No `std::optional` copy in `pop`, no value-initialized bucket storage, `std::sort` for key-only terminals | Monotone time 37.13 to 25.37 ns per pair (−32%) |
| `9c4263a` | 16-key bitonic network for key-only terminals | Repeatable monotone gains, no workload worse by more than 3% |
| `d2ab8c5` | Capacity check inline in `push`, reservation out of line | About −2% |
| `c2f1463` | Equal-lane parity mask built once per partition | Partition-heavy workloads faster, no number recorded |
| `67ffaae` | With more than 64 pivots, a key below the last pivot goes straight to the terminal bucket | MostlyPushDecreasing 4.323 to 2.678 ms (−38%) |
| `ab2d043` | Insertion sort instead of the network for terminals of up to 8 keys | WorstCaseDescending −3.8% to −5.4% |
| `b596f3c` | Terminal shortcut for any pivot count, checked before SIMD dispatch | RandomConstantSize −13.4% to −15.4% |
| `c1fb120` | One mask and one count per partition vector | WorstCaseDescending −0.9% to −1.5% |
| `38230cd` | Terminal sort and split moved out of the `pop` fast path | RandomConstantSize −17.9% to −18.8%, WorstCaseDescending −4.8% to −5.4% |
| `c1f9de6` | Pivot sample index by multiply-high instead of `%` | WorstCaseDescending, seed 1363692610: 2.799 to 2.131 ms (−24%) |
| `08f48a0` | `is_sorted` check before sorting a terminal | MostlyPushDecreasing −32% to −33%, WorstCaseDescending −7% to −8% |

`ce4c097` repeats `92ff19a` on a merged branch.

## What applies to the current heap

**Terminal shortcut (`67ffaae`, `b596f3c`).** Applies. `push_position` in
`simd.hpp` always scans or searches the pivots. A key below `pivots_.back()`,
or any key when there are no pivots, belongs in the terminal bucket, and
`push` can skip the search. This was the largest single gain.

**Out-of-line terminal maintenance (`38230cd`).** Partly present.
`partition()` is already `[[gnu::noinline]]`, but `pop_into` still inlines the
split loop and `sort_descending`. Moving the `> N` branch into one noinline
function keeps the common pop small.

**One mask and count per vector (`c1fb120`).** Applies. `partition_fast`
computes both `popcount(large)` and `popcount(small)`. One equals
`L - popcount(other)`.

**Terminal sorting (`92ff19a`, `9c4263a`, `ab2d043`, `08f48a0`).** Open
question. The current heap sorts every terminal by insertion, like Rust's
`sort_unstable` at these sizes. Insertion sort needs only $n - 1$ comparisons
on sorted input, so the `is_sorted` gain, which skipped a network, should
mostly not carry over. A sorting network for 9 to 16 keys may still beat
insertion on random workloads, but no one has measured it against this code.

**Division-free sampling (`c1f9de6`).** Already present. `random_range` in
`rng.hpp` uses a widening multiply, like rand 0.10.

**Inline capacity check (`d2ab8c5`).** Already present. `Vec::push` checks
`len_ == cap_` inline and grows out of line.

**No optional copy, no value initialization (`92ff19a`).** Already present.
`pop_into` returns through a reference, and `Vec::set_len` leaves new slots
uninitialized.

**Parity mask hoist (`c2f1463`).** Does not apply. The current partition
picks equal lanes from the `equal_masks[pivot_pos % 16]` table.

## Benchmark heaps

Commits named `perf(bench)` speed up the competitor heaps in the benchmark,
not the quickheap heap. That code is still in [`benchmark/`](../benchmark).
`git log --grep '^perf(bench)'` lists them.
