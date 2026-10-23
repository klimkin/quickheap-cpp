# C++ performance fixes from regression handoff

First round uses C++ base `20efe10c54817a3a8a4c751813d4d8bd4e966d49`. Rust tracked sources remain fixed at `0e0217fe32ac70ff7dff32443febe8cfc9c070e8`. These are C++ parent/candidate comparisons. Rust timing was not rerun, so results do not establish new cross-language parity. Historical comparisons remain in `docs/performance-comparison-2026-10-23.md`.

## Measured effects

Times are milliseconds. Central time is median of three seed-level process medians. Brackets span process medians across all three seeds. Delta is geometric mean of paired seed-level candidate/parent ratios.

| Change | Workload | Parent ms [range] | Candidate ms [range] | Elapsed delta | Trials per process |
| --- | --- | --- | --- | --- | --- |
| Production pivot endpoint | MostlyPushDecreasing | 4.323 [4.281, 4.397] | 2.678 [2.652, 2.710] | -38.11% | 15 |
| Perfect binary oracle | WorstCaseDescending | 11.107 [11.045, 11.199] | 7.495 [7.410, 7.549] | -32.54% | 3 |
| Standard heap extraction | WorstCaseDescending | 6.356 [5.666, 6.449] | 6.174 [5.503, 6.289] | -2.81% | 15 |

Perfect-binary elapsed time falls 7.55% through 32.54% across all eight workloads. Perfect-linear seed 7 falls 8.64% through 32.78% on seven workloads. Perfect-linear MostlyPushDecreasing changes +0.04%, with overlapping process ranges, so no measurable gain. Median1 remains undiagnosed.

Production confirmation checks RandomWiggle and GeometricMonotoneWiggle as well. Central deltas are +0.30% and +1.96%; process ranges overlap for every seed. Initial three-trial production run contains bimodal durations. Fifteen-trial confirmation resolves target gain; both datasets remain preserved. No stable regression claim follows from overlapping ranges.

Standard heap effects are small. Exact paired sign checks use 21 process pairs per workload, then Holm correction across eight workloads. WorstCaseDescending is lower in all 21 pairs, adjusted p=0.00000763. HeapSort and MonotoneConstantSize fail adjusted p<0.05; their observed median differences remain inconclusive. Six other workloads pass that threshold. `benchmark/experiments/regression_20261023/paired_sign_test.py` reproduces results.

## Combined artifact

Combined full benchmark passes 72 exact output/count pairs, 126 Rust scalar transition/count comparisons plus two scripted retries, and Clang direct contracts with 5,033 assertions across 29 cases. Source/library/binary receipts identify actual composition. Final matched comparison uses seven processes, fifteen trials, one warmup, and three seeds on CPU 3.

| Queue | Workload | Parent ms | Combined ms | Elapsed delta |
| --- | --- | --- | --- | --- |
| quickheap | MostlyPushDecreasing | 4.333 | 2.675 | -38.11% |
| scalar_perfect_binary | WorstCaseDescending | 11.193 | 7.531 | -32.64% |
| std_priority_queue | WorstCaseDescending | 6.329 | 6.215 | -1.89% |

Production WorstCaseDescending changes 2.597 ms to 2.621 ms, +0.78% elapsed. Seventeen of 21 process pairs are slower, Holm-adjusted p=0.00720. This small combined-artifact slowdown remains an explicit tradeoff; no production WorstCaseDescending improvement is accepted.

Previously favorable production workloads retain small mixed changes in final artifact. MonotoneWiggle has +1.81% seed-aggregated elapsed time; this is another explicit residual slowdown.

| Production workload | Combined elapsed delta |
| --- | --- |
| HeapSort | +0.49% |
| MonotoneConstantSize | -0.23% |
| MonotoneWiggle | +1.81% |
| RandomWiggle | +0.51% |
| RandomConstantSize | +0.81% |
| GeometricMonotoneWiggle | +1.69% |

`integration-favorable/summary.csv` retains process ranges; paired checks remain beside it. These effects prevent a claim that every production workload improved. No new all-workload parity gate is certified.

Final numbers describe composition; isolated table above attributes each change. Final standard-heap gain is smaller than isolated gain. `integration-timing/paired-sign-test.json` retains paired checks. `integration-artifacts/verification.json` records complete correctness gate.

Accepted code commits are `67ffaae`, `dfceda3`, `c74c9fe`.

## Causes and changes

- `src/simd.cpp` returns terminal bucket index when more than 64 decreasing pivots exist and inserted key is strictly below last pivot. Equality and smaller arrays retain existing paths. Untimed MostlyPushDecreasing counters show zero insertion shifts. CPU profile identifies pivot search; candidate profile leaves terminal pop work as main cost. WorstCaseDescending has no accepted production-specific fix.
- `benchmark/scalar_heap.hpp` checks ascending order before perfect-pivot `stable_sort`. Already sorted buckets need no sort or scratch allocation. Unsorted buckets retain existing sort, median, transitions, and exported count contract. Merge sorting remains main residual cost.
- `benchmark/workloads.hpp` gives `StdKeyHeap` checked value `pop()` while preserving optional `try_pop()`. Existing direct dispatcher and validator use value extraction. Full-binary assembly confirms optional transport disappears from that path. `std::priority_queue` identity, reservation, and sift implementation remain; sift dominates candidate profile.

## Method and evidence

All timing processes run through `taskset -c 3`. `cpu.json` records kernel P-core proof and observed affinity. Parent/candidate use identical GCC 15.2.0 generic `-m64 -O3 -DNDEBUG -std=c++20` release flags, fixed dependencies, no native tuning, and no LTO. Fixtures use i64 n=16,384 and seeds 1363692610, 7, and 42. Seven fresh processes per side alternate AB/BA. Each process uses one warmup. Setup, parsing, validation, and destruction stay outside measured workload.

Root rerunnable tools and data live under `build/regression-subagents-20261023/`. `benchmark/experiments/regression_20261023/compare_candidates.py` preserves frozen i64 n=16,384 protocol and rejects wrong work, fixture, heap, backend, and duration fields. It requires preserved fixtures under `build/performance-report/fresh-controlled/fixtures/` and separately built parent/candidate binaries. Full output traces match before timing. JSON command receipts, CSV trials, process medians, binary/source hashes, compressed traces, profiles, and counter receipts remain there. Profiles run separately and include untimed CLI work; sampled percentages describe diagnosis, not isolated timed-region shares.

Production comparison reuses one benchmark object and links separately hashed production libraries. Embedded production build labels remain historical. External source/library/binary receipts bind candidate identity. Collector checks source-label presence, not source origin. Future reruns require their own external source/build receipt checks. Scalar and stdlib comparisons rebuild complete benchmark translation units. Original-profile lifecycle timing remains outside this round.

Independent reviews reside in `candidate-review.md` and `stdlib-review.md` under evidence root. Compiler checks build actual Clang 21 all-attainable, Clang scalar-only, and GCC scalar-only libraries. Each passes 36,186,029 assertions across 105 cases. Explicit position checks cover four integer types, duplicate/equality/extrema boundaries, AVX2, and actual scalar artifacts. Six C11/C++17 direct consumers pass. Scalar contracts pass 278,809 assertions across 12 cases; stdlib contracts pass 6,233 assertions across 55 cases. Full Conan install/metadata and sanitizer matrices were not rerun.

## Competitor findings and remaining scope

| Pair | Shared operations | Output or size mismatches | Per-operation count mismatches |
| --- | --- | --- | --- |
| C++ WeakHeap / Rust weakheap 0.1.0 | 14,391 | 0 | 5,974 |
| C++ Dary8 / Rust Orx 1.9.0 | 14,391 | 0 | 10,693 |

Matching outputs do not establish matching internal work. Orx remains declared substitute. Foreign primary headers match pinned sources except declared SequenceHeap safety patches. Original Quickheap also differs in constructor RNG reseeding. Competitor lane proposes no unprofiled optimization. Graphs retain C++ timing and matched Rust correctness only.

Broader all-competitor target remains unspecified. First round accepts focused C++ gains and preserves unresolved production WorstCaseDescending, scalar median1, and competitor identity/profiling work. Cleanup backups and prior performance records remain intact.
