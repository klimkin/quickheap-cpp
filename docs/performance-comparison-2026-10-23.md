# C++ and Rust priority-queue performance report

Collected 2026-10-23. All fresh timing processes use `taskset -c 3` on a kernel-verified P core.

The report covers every current C++ queue family, every controlled Rust queue, all eight queue workloads, graph algorithms, and experimental variants. Matching timings, substitutes, and unavailable comparisons remain distinct.

Results describe the attributed binaries and configurations below. They do not establish a general performance ranking between languages.

## Measurement coverage

| Dataset                            | Coverage                                                                         | Key size and input                                               | Repetitions                                                          |
| ---------------------------------- | -------------------------------------------------------------------------------- | ---------------------------------------------------------------- | -------------------------------------------------------------------- |
| Fresh controlled comparisons       | 8 C++–Rust pairs × 8 workloads × 3 seeds = 192 matched groups                    | i64; n = 16,384; identical fixture bytes                         | 7 alternating processes per language; 3 measured trials and 1 warmup |
| Fresh C++ queue inventory          | 29 queues; monotone restrictions omit 8 incompatible queue/workload combinations | i64; n = 4,096; 3 seeds                                          | 7 processes; 3 measured trials and 1 warmup                          |
| Fresh C++ experimental inventory   | 39 configurations, including 2 baselines; all 8 workloads                        | i64; n = 4,096; 3 seeds                                          | 7 processes; 3 measured trials and 1 warmup                          |
| Fresh original-profile comparisons | Original active registrations that build on this host                            | i64; n = 16,384; matched distributions, independent random bytes | 7 alternating processes; fixed 3 measured trials and 1 warmup        |
| Fresh graph inventory              | 27 key-only C++ queues for Dijkstra; 25 for Prim                                 | 4,096 vertices; 32,768 directed arcs; deterministic seed 42      | 7 processes; 3 measured trials and 1 warmup                          |
| Fresh C++ microbenchmarks          | Scalar/SIMD partition and 3 rebalancing variants                                 | i64; n = 16,384                                                  | 7 processes; 3 measured trials and 1 warmup                          |
| Accepted earlier QuickHeap gate    | 2 clean batches; 152 matched groups across both profiles                         | i64 AVX2; sizes through 65,536                                   | See accepted run manifests                                           |

## Observed comparison summary

These ranges summarize the seed-paired geometric excess across workload cells at n = 16,384. Negative means lower C++ elapsed time.

| Family                                     | C++ paired excess across workloads | Scope                                                                      |
| ------------------------------------------ | ---------------------------------- | -------------------------------------------------------------------------- |
| Production QuickHeap, active workloads     | -14.91% to -8.32%                  | The four workloads used by the accepted production gate                    |
| Production QuickHeap, additional workloads | -15.55% to +27.75%                 | WorstCaseDescending and RandomConstantSize have overlapping process ranges |
| Standard binary heap                       | +5.53% to +69.08%                  | Distinct standard-library implementations                                  |
| Median3 scalar, linear and binary          | -2.04% to +4.69%                   | Shared controlled scalar contract; some process ranges overlap             |
| Perfect-pivot scalar, linear and binary    | -0.00% to +209.81%                 | Oracle configurations; this timing includes pivot selection                |

Eight controlled pairs comprise production AVX2 QuickHeap, standard binary heaps, and six scalar configurations. Standard-library heaps are distinct implementations. Scalar configurations follow the shared controlled contract; they do not reproduce historical Rust standard-library comparison counts.

The previously accepted performance ceiling concerns production key-only QuickHeap on four active workloads. Additional workloads and competitors in this report extend that scope.

## CPU and compiler information

| Property                                     | Value                                                                           |
| -------------------------------------------- | ------------------------------------------------------------------------------- |
| CPU                                          | Intel Core Ultra 9 285K; x86_64; family 6, model 198, stepping 2                |
| Topology                                     | 1 socket; 8 P cores and 16 E cores; 24 logical CPUs; 1 thread per core          |
| Measured CPU                                 | Logical CPU 3; core ID 20; package 0; observed affinity [3]                     |
| P-core proof                                 | `/sys/bus/event_source/devices/cpu_core/cpus` reports CPUs 0–7                  |
| SIMD                                         | AVX2 supported; AVX512 unavailable; NEON unavailable on this x86_64 host        |
| CPU3 cache, current snapshot                 | 48 KiB L1D; 64 KiB L1I; 3 MiB private L2; 36 MiB shared L3; 64-byte cache lines |
| Advertised frequency                         | 800–5700 MHz; actual frequency during each timed region was not logged          |
| Microcode and power policy, current snapshot | `0x121`; powersave governor; balance_performance preference                     |
| OS                                           | Ubuntu 26.04.1 LTS; Linux 7.0.0-38-generic; glibc 2.43                          |

| Build                                             | Compiler version                                                 | Configuration                                                                                       |
| ------------------------------------------------- | ---------------------------------------------------------------- | --------------------------------------------------------------------------------------------------- |
| Measured C++ native, Boost, and external packages | GCC 15.2.0; Ubuntu 15.2.0-16ubuntu1                              | `-m64 -O3 -DNDEBUG -std=c++20`; generic CPU; no explicit LTO; Highway AVX2 for production QuickHeap |
| Rust controlled runner                            | rustc 1.101.0-nightly; commit 7189d39d9, 2026-10-22; LLVM 23.1.4 | opt-level 3; native CPU tuning; frame pointers; standalone profile without inherited fat LTO        |
| Rust original runner                              | Same Rust nightly                                                | opt-level 3; native CPU tuning; fat LTO; codegen-units 1; frame pointers                            |
| Clang verification builds                         | Ubuntu Clang 21.1.8, package 6ubuntu1                            | Verification coverage only; these Clang C++ binaries do not supply the main timing tables           |

Rust FFI bridges use GCC 15.2.0 with their upstream `-O3 -march=native -std=c++17` flags and an explicit final `-fno-lto`. Rust code retains its original fat-LTO profile. This build-only change avoids linking GNU slim-LTO objects through rust-lld; reference source files remain unchanged.

C++ generic tuning and Rust native tuning differ. Compiler configuration, library implementation, and protocol differences remain part of each result identity.

Measured C++ packages retain historical build revision `7dc6d5389ae234bf8c73515eea17c9a56324716c`. Current C++ checkout is `11b2501dcb69a6505f8ec561e17c6314adfc7570`. All 50 checked benchmark, heap, and public-header files match those package sources byte-for-byte. Later compiler-policy and documentation changes were not rebuilt for these timings. Rust checkout is `0e0217fe32ac70ff7dff32443febe8cfc9c070e8`. Original Rust reference is `a9f2518` plus the attributed bounded-load repair `64a83c6`.

Unrelated documentation edits and staged deletions existed before this collection. Timings use previously verified packaged C++ binaries and fixed paired Rust binaries; no heap implementation source changed.

## Protocols and statistics

Controlled operations parse and validate fixtures before timing. Setup and queue destruction occur outside timing. Timed loops contain real dependent pop-to-push arithmetic and no output-buffer writes. Original compatibility matches workload distributions, reservation, repeats, cutoffs, and timed destruction; its C++ and Rust random inputs are independent.

For each seed, elapsed time is the median of seven process medians. Each process median summarizes three measured trials. Displayed central times in the controlled tables are medians of the three seed-level medians. Brackets span the minimum and maximum process medians across all three seeds. Ratio columns use the geometric mean of three paired C++/Rust ratios, which can differ from the ratio of displayed central times. Negative C++ excess means lower C++ elapsed time.

Process ranges expose variability. No statistical significance test is applied to small differences. A range overlaps when the two observed process ranges intersect; an observed median difference alone does not establish a stable advantage.

Normalized C++ inventory cells use `ns / (ops × log2(n))`. `ops` counts push/pop pairs; total timed queue calls equal `2 × ops`. Workload factor appears once.

| Workload                | Timed pair factor | Setup                                        | Retained items |
| ----------------------- | ----------------- | -------------------------------------------- | -------------- |
| HeapSort                | 1                 | None                                         | 0              |
| MonotoneConstantSize    | 10                | n+50 retained items from push/pop/push setup | n+50           |
| MonotoneWiggle          | 3                 | None                                         | 0              |
| RandomWiggle            | 3                 | None                                         | 0              |
| RandomConstantSize      | 10                | n+50 retained items from push/pop/push setup | n+50           |
| GeometricMonotoneWiggle | 3                 | None                                         | 0              |
| WorstCaseDescending     | 9                 | None                                         | 0              |
| MostlyPushDecreasing    | 10                | None                                         | 0              |

## Matched controlled results at n = 16,384

Cells show seed-aggregated C++ excess elapsed time in percent. Algorithms with different implementation identity remain labeled in the detailed tables.

| C++ queue             | HeapSort | MonotoneConstantSize | MonotoneWiggle | RandomWiggle | RandomConstantSize | GeometricMonotoneWiggle | WorstCaseDescending | MostlyPushDecreasing |
| --------------------- | -------- | -------------------- | -------------- | ------------ | ------------------ | ----------------------- | ------------------- | -------------------- |
| quickheap             | -14.9%   | -8.9%                | -14.5%         | -8.3%        | -2.1%              | -15.5%                  | +9.1%               | +27.7%               |
| std_priority_queue    | +9.3%    | +5.5%                | +17.0%         | +31.0%       | +26.9%             | +18.9%                  | +40.8%              | +69.1%               |
| scalar_median1_linear | +7.1%    | +12.3%               | +11.3%         | +9.0%        | +17.9%             | +13.1%                  | +5.7%               | +0.4%                |
| scalar_median3_linear | +0.6%    | +3.4%                | +3.9%          | -0.5%        | +0.1%              | +4.7%                   | -0.4%               | +0.1%                |
| scalar_perfect_linear | +209.5%  | +149.1%              | +133.8%        | +95.4%       | +134.8%            | +128.8%                 | +129.5%             | -0.0%                |
| scalar_median1_binary | +6.4%    | +10.7%               | +10.1%         | +8.5%        | +17.9%             | +11.8%                  | +4.7%               | +12.3%               |
| scalar_median3_binary | +0.1%    | +2.2%                | +2.3%          | -0.4%        | -0.2%              | +3.5%                   | -2.0%               | -1.4%                |
| scalar_perfect_binary | +209.8%  | +142.2%              | +127.3%        | +96.7%       | +132.1%            | +121.9%                 | +120.6%             | +29.1%               |

### Production QuickHeap AVX2

Production implementation counterparts.

Elapsed times are milliseconds. Range covers process medians across all three fixtures.

| Workload                | C++ ms [range]       | Rust ms [range]      | Paired excess, geometric mean | Excess across seeds | Ranges overlap |
| ----------------------- | -------------------- | -------------------- | ----------------------------- | ------------------- | -------------- |
| HeapSort                | 0.236 [0.230, 0.249] | 0.279 [0.269, 0.291] | -14.91%                       | -16.42% to -13.44%  | No             |
| MonotoneConstantSize    | 2.624 [2.575, 2.797] | 2.877 [2.839, 2.963] | -8.88%                        | -9.34% to -8.15%    | No             |
| MonotoneWiggle          | 0.784 [0.756, 0.803] | 0.916 [0.906, 0.937] | -14.52%                       | -14.81% to -13.95%  | No             |
| RandomWiggle            | 0.773 [0.743, 0.785] | 0.844 [0.824, 0.858] | -8.32%                        | -8.43% to -8.19%    | No             |
| RandomConstantSize      | 1.601 [1.545, 2.727] | 1.640 [1.601, 1.742] | -2.12%                        | -4.83% to +0.53%    | Yes            |
| GeometricMonotoneWiggle | 0.774 [0.763, 0.805] | 0.917 [0.908, 0.946] | -15.55%                       | -15.94% to -15.12%  | No             |
| WorstCaseDescending     | 2.600 [2.545, 3.144] | 2.542 [2.420, 2.728] | +9.10%                        | +2.18% to +21.73%   | Yes            |
| MostlyPushDecreasing    | 4.334 [4.277, 4.397] | 3.399 [3.364, 3.417] | +27.75%                       | +27.50% to +28.05%  | No             |

### Standard binary heap

Distinct standard-library implementations; declared substitute.

Elapsed times are milliseconds. Range covers process medians across all three fixtures.

| Workload                | C++ ms [range]       | Rust ms [range]      | Paired excess, geometric mean | Excess across seeds | Ranges overlap |
| ----------------------- | -------------------- | -------------------- | ----------------------------- | ------------------- | -------------- |
| HeapSort                | 0.521 [0.507, 0.537] | 0.476 [0.452, 0.483] | +9.31%                        | +8.68% to +9.75%    | No             |
| MonotoneConstantSize    | 4.884 [4.790, 5.288] | 4.628 [4.364, 4.674] | +5.53%                        | +4.74% to +6.31%    | No             |
| MonotoneWiggle          | 1.465 [1.439, 1.500] | 1.250 [1.221, 1.274] | +17.03%                       | +16.79% to +17.18%  | No             |
| RandomWiggle            | 1.728 [1.711, 1.768] | 1.321 [1.309, 1.333] | +30.97%                       | +30.45% to +31.42%  | No             |
| RandomConstantSize      | 5.674 [5.645, 5.840] | 4.472 [4.381, 4.523] | +26.85%                       | +26.67% to +26.97%  | No             |
| GeometricMonotoneWiggle | 1.590 [1.567, 1.642] | 1.343 [1.277, 1.390] | +18.90%                       | +17.53% to +20.80%  | No             |
| WorstCaseDescending     | 6.292 [5.658, 6.384] | 4.338 [4.117, 4.515] | +40.81%                       | +29.88% to +47.37%  | No             |
| MostlyPushDecreasing    | 5.460 [5.408, 5.531] | 3.229 [3.026, 3.298] | +69.08%                       | +67.63% to +70.23%  | No             |

### scalar_median1_linear

Shared controlled scalar contract; not exact historical Rust stdlib counts.

Elapsed times are milliseconds. Range covers process medians across all three fixtures.

| Workload                | C++ ms [range]             | Rust ms [range]            | Paired excess, geometric mean | Excess across seeds | Ranges overlap |
| ----------------------- | -------------------------- | -------------------------- | ----------------------------- | ------------------- | -------------- |
| HeapSort                | 1.174 [1.163, 1.192]       | 1.097 [1.087, 1.112]       | +7.12%                        | +5.98% to +8.16%    | No             |
| MonotoneConstantSize    | 13.042 [12.865, 13.106]    | 11.577 [11.473, 11.667]    | +12.27%                       | +11.36% to +12.83%  | No             |
| MonotoneWiggle          | 3.667 [3.639, 3.758]       | 3.288 [3.245, 3.335]       | +11.29%                       | +10.48% to +11.86%  | No             |
| RandomWiggle            | 2.761 [2.693, 2.805]       | 2.534 [2.437, 2.594]       | +9.00%                        | +8.17% to +9.73%    | No             |
| RandomConstantSize      | 7.454 [7.326, 7.622]       | 6.334 [6.280, 6.604]       | +17.92%                       | +16.28% to +20.13%  | No             |
| GeometricMonotoneWiggle | 3.787 [3.746, 3.833]       | 3.355 [3.320, 3.516]       | +13.12%                       | +12.31% to +13.79%  | No             |
| WorstCaseDescending     | 9.364 [8.860, 9.423]       | 8.761 [8.516, 8.863]       | +5.68%                        | +1.01% to +9.19%    | Yes            |
| MostlyPushDecreasing    | 579.078 [573.473, 585.985] | 577.715 [572.104, 584.279] | +0.42%                        | +0.24% to +0.77%    | Yes            |

### scalar_median3_linear

Shared controlled scalar contract; not exact historical Rust stdlib counts.

Elapsed times are milliseconds. Range covers process medians across all three fixtures.

| Workload                | C++ ms [range]             | Rust ms [range]            | Paired excess, geometric mean | Excess across seeds | Ranges overlap |
| ----------------------- | -------------------------- | -------------------------- | ----------------------------- | ------------------- | -------------- |
| HeapSort                | 1.523 [1.513, 1.575]       | 1.513 [1.496, 1.548]       | +0.61%                        | -0.05% to +1.21%    | Yes            |
| MonotoneConstantSize    | 16.167 [15.998, 16.428]    | 15.722 [15.514, 15.865]    | +3.38%                        | +2.84% to +4.24%    | No             |
| MonotoneWiggle          | 4.701 [4.677, 4.771]       | 4.532 [4.458, 4.614]       | +3.89%                        | +3.73% to +4.19%    | No             |
| RandomWiggle            | 3.858 [3.775, 3.918]       | 3.865 [3.827, 4.062]       | -0.45%                        | -0.98% to -0.18%    | Yes            |
| RandomConstantSize      | 11.410 [11.301, 11.575]    | 11.409 [11.342, 11.515]    | +0.09%                        | -0.71% to +1.34%    | Yes            |
| GeometricMonotoneWiggle | 4.790 [4.755, 4.851]       | 4.592 [4.548, 4.641]       | +4.69%                        | +4.31% to +4.94%    | No             |
| WorstCaseDescending     | 12.007 [11.886, 12.553]    | 12.159 [11.973, 12.556]    | -0.43%                        | -3.24% to +2.77%    | Yes            |
| MostlyPushDecreasing    | 706.053 [697.441, 713.011] | 706.011 [700.006, 711.865] | +0.07%                        | -0.23% to +0.44%    | Yes            |

### scalar_perfect_linear

Shared controlled scalar contract; not exact historical Rust stdlib counts.

Elapsed times are milliseconds. Range covers process medians across all three fixtures.

| Workload                | C++ ms [range]             | Rust ms [range]            | Paired excess, geometric mean | Excess across seeds  | Ranges overlap |
| ----------------------- | -------------------------- | -------------------------- | ----------------------------- | -------------------- | -------------- |
| HeapSort                | 1.414 [1.390, 1.454]       | 0.456 [0.450, 0.468]       | +209.53%                      | +207.96% to +212.58% | No             |
| MonotoneConstantSize    | 15.364 [15.189, 15.572]    | 6.160 [6.078, 6.240]       | +149.10%                      | +148.66% to +149.92% | No             |
| MonotoneWiggle          | 4.166 [4.147, 4.208]       | 1.779 [1.758, 1.880]       | +133.83%                      | +132.25% to +135.04% | No             |
| RandomWiggle            | 2.572 [2.487, 2.637]       | 1.314 [1.288, 1.365]       | +95.42%                       | +93.08% to +99.62%   | No             |
| RandomConstantSize      | 6.556 [6.329, 6.653]       | 2.791 [2.661, 2.918]       | +134.77%                      | +134.27% to +135.15% | No             |
| GeometricMonotoneWiggle | 4.271 [4.220, 4.348]       | 1.862 [1.836, 1.921]       | +128.83%                      | +127.52% to +129.65% | No             |
| WorstCaseDescending     | 11.058 [10.998, 11.123]    | 4.785 [4.601, 5.082]       | +129.46%                      | +118.82% to +138.18% | No             |
| MostlyPushDecreasing    | 807.397 [802.651, 861.192] | 808.262 [797.864, 815.267] | -0.00%                        | -0.17% to +0.10%     | Yes            |

### scalar_median1_binary

Shared controlled scalar contract; not exact historical Rust stdlib counts.

Elapsed times are milliseconds. Range covers process medians across all three fixtures.

| Workload                | C++ ms [range]          | Rust ms [range]         | Paired excess, geometric mean | Excess across seeds | Ranges overlap |
| ----------------------- | ----------------------- | ----------------------- | ----------------------------- | ------------------- | -------------- |
| HeapSort                | 1.169 [1.157, 1.199]    | 1.101 [1.089, 1.139]    | +6.40%                        | +5.95% to +7.09%    | No             |
| MonotoneConstantSize    | 13.173 [12.976, 13.300] | 11.875 [11.738, 12.154] | +10.73%                       | +9.82% to +11.66%   | No             |
| MonotoneWiggle          | 3.718 [3.671, 3.758]    | 3.378 [3.339, 3.528]    | +10.13%                       | +9.42% to +10.52%   | No             |
| RandomWiggle            | 2.719 [2.706, 2.746]    | 2.517 [2.474, 2.602]    | +8.50%                        | +8.03% to +8.77%    | No             |
| RandomConstantSize      | 7.408 [7.288, 7.633]    | 6.297 [6.111, 6.456]    | +17.89%                       | +16.03% to +20.05%  | No             |
| GeometricMonotoneWiggle | 3.837 [3.798, 3.905]    | 3.441 [3.390, 3.506]    | +11.81%                       | +10.86% to +12.89%  | No             |
| WorstCaseDescending     | 9.462 [9.038, 9.564]    | 8.982 [8.769, 9.066]    | +4.73%                        | +0.87% to +7.30%    | Yes            |
| MostlyPushDecreasing    | 8.501 [8.404, 8.557]    | 7.587 [7.480, 7.606]    | +12.27%                       | +11.88% to +12.59%  | No             |

### scalar_median3_binary

Shared controlled scalar contract; not exact historical Rust stdlib counts.

Elapsed times are milliseconds. Range covers process medians across all three fixtures.

| Workload                | C++ ms [range]          | Rust ms [range]         | Paired excess, geometric mean | Excess across seeds | Ranges overlap |
| ----------------------- | ----------------------- | ----------------------- | ----------------------------- | ------------------- | -------------- |
| HeapSort                | 1.530 [1.514, 1.561]    | 1.530 [1.507, 1.566]    | +0.13%                        | -1.00% to +1.37%    | Yes            |
| MonotoneConstantSize    | 16.430 [16.233, 16.735] | 16.109 [16.008, 16.272] | +2.18%                        | +1.56% to +3.14%    | Yes            |
| MonotoneWiggle          | 4.744 [4.724, 4.798]    | 4.639 [4.596, 4.685]    | +2.31%                        | +2.27% to +2.36%    | No             |
| RandomWiggle            | 3.812 [3.757, 3.843]    | 3.824 [3.782, 3.946]    | -0.37%                        | -0.66% to +0.10%    | Yes            |
| RandomConstantSize      | 11.158 [11.065, 11.487] | 11.245 [11.135, 11.360] | -0.19%                        | -0.94% to +0.91%    | Yes            |
| GeometricMonotoneWiggle | 4.828 [4.792, 4.896]    | 4.664 [4.627, 4.719]    | +3.55%                        | +3.20% to +3.93%    | No             |
| WorstCaseDescending     | 12.062 [11.954, 12.605] | 12.423 [12.242, 12.765] | -2.04%                        | -4.74% to +1.11%    | Yes            |
| MostlyPushDecreasing    | 11.992 [11.886, 12.103] | 12.150 [12.070, 12.227] | -1.40%                        | -1.81% to -1.18%    | Yes            |

### scalar_perfect_binary

Shared controlled scalar contract; not exact historical Rust stdlib counts.

Elapsed times are milliseconds. Range covers process medians across all three fixtures.

| Workload                | C++ ms [range]          | Rust ms [range]      | Paired excess, geometric mean | Excess across seeds  | Ranges overlap |
| ----------------------- | ----------------------- | -------------------- | ----------------------------- | -------------------- | -------------- |
| HeapSort                | 1.412 [1.390, 1.445]    | 0.457 [0.450, 0.493] | +209.81%                      | +207.90% to +212.74% | No             |
| MonotoneConstantSize    | 15.385 [15.054, 15.571] | 6.328 [6.213, 6.471] | +142.25%                      | +141.01% to +143.14% | No             |
| MonotoneWiggle          | 4.178 [4.165, 4.242]    | 1.843 [1.829, 1.862] | +127.27%                      | +126.47% to +128.84% | No             |
| RandomWiggle            | 2.576 [2.529, 2.643]    | 1.314 [1.280, 1.371] | +96.70%                       | +92.06% to +102.18%  | No             |
| RandomConstantSize      | 6.627 [6.368, 6.687]    | 2.829 [2.732, 2.921] | +132.10%                      | +130.52% to +134.40% | No             |
| GeometricMonotoneWiggle | 4.265 [4.232, 4.315]    | 1.929 [1.904, 1.967] | +121.89%                      | +120.92% to +123.73% | No             |
| WorstCaseDescending     | 11.145 [11.038, 11.209] | 4.988 [4.887, 5.429] | +120.64%                      | +111.97% to +127.22% | No             |
| MostlyPushDecreasing    | 6.630 [6.521, 7.617]    | 5.114 [5.034, 5.276] | +29.12%                       | +27.57% to +30.17%   | No             |

## Original-profile competitor results at n = 16,384

Original uses fresh independent random inputs and timed lifecycle. These results must remain separate from controlled-operation tables. Times are medians of seven process medians, in milliseconds.

This cohort enables Rust `avx2,ffi` with the explicitly attributed bridge `-fno-lto` build setting. The first default FFI build failure and the independent non-FFI cohort remain archived.

| C++ counterpart       | Workload             | C++ ms [range]          | Rust ms [range]         | C++ excess |
| --------------------- | -------------------- | ----------------------- | ----------------------- | ---------- |
| quickheap             | HeapSort             | 0.257 [0.238, 0.269]    | 0.295 [0.272, 0.342]    | -12.76%    |
| quickheap             | MonotoneConstantSize | 2.641 [2.581, 2.723]    | 3.002 [2.911, 3.335]    | -12.03%    |
| quickheap             | MonotoneWiggle       | 0.848 [0.815, 0.893]    | 0.918 [0.905, 1.018]    | -7.64%     |
| quickheap             | RandomWiggle         | 0.785 [0.754, 0.823]    | 0.880 [0.863, 1.045]    | -10.85%    |
| std_priority_queue    | HeapSort             | 0.519 [0.509, 0.538]    | 0.468 [0.373, 0.481]    | +10.86%    |
| std_priority_queue    | MonotoneConstantSize | 4.895 [4.837, 5.106]    | 4.611 [4.395, 4.654]    | +6.17%     |
| std_priority_queue    | MonotoneWiggle       | 1.465 [1.451, 1.529]    | 1.218 [1.196, 1.239]    | +20.29%    |
| std_priority_queue    | RandomWiggle         | 1.720 [1.704, 1.743]    | 1.326 [1.317, 1.335]    | +29.69%    |
| dary8                 | HeapSort             | 0.949 [0.943, 0.954]    | 0.333 [0.330, 0.341]    | +185.04%   |
| dary8                 | MonotoneConstantSize | 10.433 [10.350, 10.533] | 3.316 [3.286, 3.371]    | +214.58%   |
| dary8                 | MonotoneWiggle       | 2.794 [2.768, 2.827]    | 0.960 [0.950, 1.001]    | +191.20%   |
| dary8                 | RandomWiggle         | 2.444 [2.426, 2.490]    | 1.344 [1.333, 1.382]    | +81.82%    |
| weak_heap             | HeapSort             | 1.278 [1.266, 1.303]    | 0.959 [0.941, 1.015]    | +33.34%    |
| weak_heap             | MonotoneConstantSize | 13.037 [12.896, 13.280] | 10.051 [9.822, 10.429]  | +29.71%    |
| weak_heap             | MonotoneWiggle       | 3.937 [3.911, 4.026]    | 3.129 [3.077, 3.229]    | +25.81%    |
| weak_heap             | RandomWiggle         | 4.420 [4.374, 4.454]    | 2.981 [2.946, 3.030]    | +48.24%    |
| scalar_median3_linear | HeapSort             | 1.540 [1.514, 1.590]    | 2.057 [1.996, 2.142]    | -25.12%    |
| scalar_median3_linear | MonotoneConstantSize | 16.241 [16.031, 16.500] | 20.773 [20.070, 22.054] | -21.81%    |
| scalar_median3_linear | MonotoneWiggle       | 4.682 [4.626, 4.756]    | 6.008 [5.826, 6.420]    | -22.07%    |
| scalar_median3_linear | RandomWiggle         | 3.799 [3.725, 3.874]    | 4.508 [4.303, 4.937]    | -15.72%    |
| sequence_heap         | HeapSort             | 0.598 [0.590, 0.649]    | 0.581 [0.543, 0.658]    | +2.85%     |
| sequence_heap         | MonotoneConstantSize | 6.473 [6.261, 6.728]    | 6.443 [6.270, 6.737]    | +0.46%     |
| sequence_heap         | MonotoneWiggle       | 1.706 [1.660, 1.835]    | 1.711 [1.637, 2.076]    | -0.28%     |
| sequence_heap         | RandomWiggle         | 1.340 [1.319, 1.478]    | 1.324 [1.293, 1.509]    | +1.19%     |
| s3q                   | HeapSort             | 0.414 [0.394, 0.430]    | 0.400 [0.373, 0.434]    | +3.64%     |
| s3q                   | MonotoneConstantSize | 3.479 [3.427, 3.559]    | 3.317 [3.277, 3.376]    | +4.89%     |
| s3q                   | MonotoneWiggle       | 1.123 [1.096, 1.187]    | 1.093 [1.056, 1.133]    | +2.73%     |
| s3q                   | RandomWiggle         | 1.192 [1.170, 1.234]    | 1.159 [1.126, 1.208]    | +2.91%     |
| original_quickheap    | HeapSort             | 0.708 [0.700, 0.727]    | 0.717 [0.701, 0.774]    | -1.20%     |
| original_quickheap    | MonotoneConstantSize | 8.172 [8.033, 8.436]    | 8.117 [8.002, 8.340]    | +0.68%     |
| original_quickheap    | MonotoneWiggle       | 2.341 [2.248, 2.428]    | 2.323 [2.278, 2.360]    | +0.81%     |
| original_quickheap    | RandomWiggle         | 2.384 [2.330, 2.506]    | 2.365 [2.319, 2.483]    | +0.81%     |
| external_radix_heap   | HeapSort             | 0.544 [0.508, 0.573]    | 0.525 [0.516, 0.539]    | +3.63%     |
| external_radix_heap   | MonotoneConstantSize | 4.861 [4.711, 5.201]    | 4.567 [4.377, 4.670]    | +6.43%     |
| external_radix_heap   | MonotoneWiggle       | 1.428 [1.403, 1.442]    | 1.403 [1.383, 1.423]    | +1.78%     |

`std_priority_queue` is the C++ standard-library substitute. `dary8` is a declared substitute for Orx DaryHeap<8>. WeakHeap is an algorithm counterpart with unproved historical count parity. Scalar uses the paired C++ implementation. Source adapters retain their own compiler and reservation policies.

### Rust development AVX2 without an exact C++ counterpart

| Workload             | Rust ms [range]      | C++ comparison                                                  |
| -------------------- | -------------------- | --------------------------------------------------------------- |
| HeapSort             | 0.277 [0.270, 0.289] | Unavailable; C++ experimental variants are declared substitutes |
| MonotoneConstantSize | 3.021 [2.935, 3.406] | Unavailable; C++ experimental variants are declared substitutes |
| MonotoneWiggle       | 0.926 [0.907, 0.943] | Unavailable; C++ experimental variants are declared substitutes |
| RandomWiggle         | 0.893 [0.863, 0.962] | Unavailable; C++ experimental variants are declared substitutes |

## Accepted production QuickHeap comparison at n = 65,536

These tables collect the second previously accepted CPU3 batch. They use seven alternating processes. Original uses three measured trials; controlled uses fifteen, each after one warmup. They provide larger-size production context without mixing the new competitor sweep into the old gate.

### Original profile

| Workload             | C++ ms [range]          | Rust ms [range]         | C++ excess |
| -------------------- | ----------------------- | ----------------------- | ---------- |
| HeapSort             | 1.053 [1.040, 1.084]    | 1.107 [1.101, 1.126]    | -4.83%     |
| MonotoneConstantSize | 11.242 [11.171, 11.328] | 12.413 [11.963, 12.583] | -9.44%     |
| MonotoneWiggle       | 3.466 [3.353, 3.598]    | 3.757 [3.744, 3.820]    | -7.76%     |
| RandomWiggle         | 3.214 [3.166, 3.257]    | 3.545 [3.519, 3.568]    | -9.34%     |

### Controlled profile

| Workload             | C++ ms [range]          | Rust ms [range]         | C++ excess across seeds |
| -------------------- | ----------------------- | ----------------------- | ----------------------- |
| HeapSort             | 1.000 [0.979, 1.046]    | 1.148 [1.135, 1.161]    | -13.11%                 |
| MonotoneConstantSize | 11.024 [10.931, 11.264] | 12.163 [12.106, 12.380] | -9.36%                  |
| MonotoneWiggle       | 3.331 [3.264, 3.399]    | 3.859 [3.811, 3.987]    | -13.50%                 |
| RandomWiggle         | 3.148 [3.090, 3.371]    | 3.431 [3.351, 3.615]    | -8.16%                  |

The accepted gate passed all 152 groups across two clean batches and both profiles. Its worst production C++ excess was −3.86% for original and −7.04% for controlled. These are maxima across that specific prior inventory, not limits on newly added workloads.

## Every C++ queue at n = 4,096

These are C++ standalone controlled timings. No Rust ratio is inferred for queues without a corresponding runner. Each cell is `ns/(ops × log2(n))`, summarized across three seeds and seven process medians per seed. `N/A` marks an incompatible monotone workload.

| Queue                 | HeapSort | MonotoneConstantSize | MonotoneWiggle | RandomWiggle | RandomConstantSize | GeometricMonotoneWiggle | WorstCaseDescending | MostlyPushDecreasing |
| --------------------- | -------- | -------------------- | -------------- | ------------ | ------------------ | ----------------------- | ------------------- | -------------------- |
| quickheap             | 1.175    | 1.253                | 1.268          | 1.268        | 0.823              | 1.289                   | 1.390               | 1.924                |
| std_priority_queue    | 2.162    | 2.017                | 2.101          | 2.495        | 2.446              | 2.297                   | 3.088               | 2.339                |
| kv_quickheap          | 1.696    | 1.829                | 1.815          | 1.512        | 1.002              | 1.811                   | 1.901               | 1.670                |
| kv_std_priority_queue | 2.398    | 2.245                | 2.250          | 2.612        | 2.747              | 2.473                   | 3.176               | 2.615                |
| custom_binary_heap    | 3.624    | 3.645                | 3.588          | 3.286        | 1.697              | 3.733                   | 3.963               | 2.720                |
| dary4                 | 3.897    | 4.239                | 3.858          | 3.479        | 1.914              | 3.927                   | 4.269               | 2.417                |
| dary8                 | 4.024    | 4.558                | 4.006          | 3.780        | 2.255              | 4.090                   | 4.683               | 2.341                |
| dary16                | 4.396    | 5.094                | 4.428          | 4.127        | 2.198              | 4.470                   | 5.212               | 2.389                |
| custom_dary4          | 3.826    | 4.208                | 3.768          | 3.439        | 1.821              | 3.824                   | 4.173               | 2.286                |
| custom_dary8          | 3.926    | 4.493                | 3.885          | 3.662        | 2.162              | 3.951                   | 4.542               | 2.207                |
| weak_heap             | 5.513    | 5.853                | 5.885          | 6.734        | 3.986              | 6.097                   | 6.161               | 2.755                |
| radix_heap            | 2.923    | 2.508                | 2.440          | N/A          | N/A                | 2.446                   | N/A                 | N/A                  |
| scalar_quickheap      | 2.992    | 3.296                | 2.977          | 1.887        | 1.065              | 3.087                   | 2.292               | 1.527                |
| blocked_quickheap     | 5.963    | 6.484                | 5.786          | 3.172        | 1.747              | 5.946                   | 4.900               | 4.062                |
| scalar_median1_linear | 5.563    | 6.070                | 5.775          | 4.437        | 3.649              | 5.998                   | 4.761               | 67.367               |
| scalar_median3_linear | 7.307    | 7.710                | 7.433          | 6.312        | 5.662              | 7.704                   | 6.503               | 81.399               |
| scalar_perfect_linear | 6.171    | 6.851                | 6.251          | 4.040        | 3.087              | 6.380                   | 5.572               | 89.203               |
| scalar_median1_binary | 5.567    | 6.157                | 5.826          | 4.429        | 3.636              | 6.043                   | 4.850               | 4.156                |
| scalar_median3_binary | 7.244    | 7.815                | 7.461          | 6.257        | 5.576              | 7.777                   | 6.598               | 5.882                |
| scalar_perfect_binary | 6.164    | 6.838                | 6.280          | 4.052        | 3.118              | 6.425                   | 5.634               | 3.249                |
| boost_dary4           | 1.501    | 1.435                | 1.538          | 1.837        | 1.540              | 1.625                   | 2.493               | 1.601                |
| boost_fibonacci       | 9.315    | 6.787                | 9.251          | 6.128        | 2.560              | 9.206                   | 9.891               | 2.920                |
| boost_pairing         | 13.914   | 17.281               | 15.088         | 7.135        | 3.860              | 14.959                  | 11.733              | 1.249                |
| boost_binomial        | 21.704   | 21.616               | 21.498         | 12.643       | 5.334              | 21.546                  | 21.606              | 3.711                |
| boost_skew            | 6.037    | 5.847                | 5.766          | 3.111        | 1.594              | 5.678                   | 6.423               | 1.145                |
| sequence_heap         | 2.313    | 2.698                | 2.216          | 2.067        | 1.615              | 2.710                   | 2.495               | 1.475                |
| s3q                   | 2.246    | 1.753                | 1.903          | 2.066        | 1.566              | 2.036                   | 2.065               | 1.854                |
| original_quickheap    | 3.186    | 3.713                | 3.461          | 3.750        | 2.621              | 3.687                   | 3.943               | 6.039                |
| external_radix_heap   | 2.704    | 2.318                | 2.333          | N/A          | N/A                | 2.321                   | N/A                 | N/A                  |

Key-value queues carry payloads and have no matched Rust payload runner in this report. Their timings include that additional data movement. Native radix and external radix accept only monotone-compatible workloads. Optional Boost and external queues use separate attributed package binaries.

## Experimental C++ configurations at n = 4,096

Every experimental registration is included. Except the two baseline rows, implementation names retain the `substitute_` prefix. These C++ configurations do not establish exact Rust development-SIMD parity. Units and aggregation match the C++ queue inventory above.

| Configuration                     | HeapSort | MonotoneConstantSize | MonotoneWiggle | RandomWiggle | RandomConstantSize | GeometricMonotoneWiggle | WorstCaseDescending | MostlyPushDecreasing |
| --------------------------------- | -------- | -------------------- | -------------- | ------------ | ------------------ | ----------------------- | ------------------- | -------------------- |
| quickheap                         | 1.156    | 1.265                | 1.262          | 1.265        | 0.806              | 1.280                   | 1.353               | 1.923                |
| std_priority_queue                | 2.041    | 1.941                | 2.058          | 2.415        | 2.414              | 2.234                   | 3.021               | 2.207                |
| substitute_median1_binary         | 2.994    | 3.252                | 2.981          | 1.902        | 1.119              | 3.080                   | 2.233               | 1.454                |
| substitute_median1_linear         | 2.951    | 3.158                | 2.909          | 1.800        | 1.069              | 2.993                   | 2.169               | 20.357               |
| substitute_median1_simd           | 1.495    | 1.664                | 1.615          | 1.364        | 0.924              | 1.625                   | 1.502               | 1.590                |
| substitute_median3_binary         | 3.079    | 3.370                | 3.047          | 1.932        | 1.131              | 3.159                   | 2.321               | 1.543                |
| substitute_median3_linear         | 3.007    | 3.277                | 3.014          | 1.840        | 1.068              | 3.121                   | 2.267               | 20.089               |
| substitute_median3_simd           | 1.600    | 1.710                | 1.667          | 1.393        | 0.929              | 1.682                   | 1.515               | 1.683                |
| substitute_median5_binary         | 3.268    | 3.521                | 3.234          | 2.026        | 1.133              | 3.354                   | 2.517               | 1.676                |
| substitute_median5_linear         | 3.205    | 3.446                | 3.169          | 1.920        | 1.085              | 3.266                   | 2.461               | 19.756               |
| substitute_median5_simd           | 1.744    | 1.846                | 1.808          | 1.437        | 0.940              | 1.779                   | 1.655               | 1.819                |
| substitute_perfect_binary         | 4.171    | 7.026                | 5.941          | 3.099        | 2.029              | 5.977                   | 4.385               | 1.441                |
| substitute_perfect_linear         | 4.199    | 7.009                | 5.965          | 3.045        | 1.979              | 5.952                   | 4.348               | 19.699               |
| substitute_perfect_simd           | 4.328    | 7.102                | 6.020          | 3.115        | 2.117              | 5.940                   | 4.166               | 1.609                |
| substitute_log2_binary            | 3.471    | 3.721                | 3.402          | 2.062        | 1.210              | 3.495                   | 2.711               | 1.686                |
| substitute_log2_linear            | 3.396    | 3.644                | 3.334          | 2.016        | 1.143              | 3.440                   | 2.625               | 19.729               |
| substitute_log2_simd              | 1.889    | 1.980                | 1.925          | 1.489        | 0.968              | 1.920                   | 1.776               | 1.822                |
| substitute_cbrt_binary            | 3.304    | 3.599                | 3.272          | 2.030        | 1.156              | 3.375                   | 2.502               | 1.550                |
| substitute_cbrt_linear            | 3.278    | 3.518                | 3.193          | 1.912        | 1.101              | 3.316                   | 2.449               | 20.103               |
| substitute_cbrt_simd              | 1.714    | 1.866                | 1.821          | 1.451        | 0.949              | 1.806                   | 1.637               | 1.686                |
| substitute_table_binary           | 3.272    | 3.544                | 3.217          | 2.014        | 1.133              | 3.301                   | 2.431               | 1.544                |
| substitute_table_linear           | 3.265    | 3.436                | 3.151          | 1.903        | 1.085              | 3.229                   | 2.382               | 20.087               |
| substitute_table_simd             | 1.661    | 1.814                | 1.774          | 1.416        | 0.916              | 1.757                   | 1.601               | 1.684                |
| substitute_median1_simd_search    | 3.082    | 3.178                | 2.891          | 1.892        | 1.213              | 2.898                   | 2.216               | 1.599                |
| substitute_median1_simd_partition | 1.373    | 1.679                | 1.692          | 1.425        | 0.825              | 1.746                   | 1.512               | 1.430                |
| substitute_median3_simd_search    | 3.182    | 3.315                | 3.008          | 1.912        | 1.202              | 3.064                   | 2.304               | 1.702                |
| substitute_median3_simd_partition | 1.466    | 1.741                | 1.714          | 1.386        | 0.848              | 1.763                   | 1.547               | 1.529                |
| substitute_median5_simd_search    | 3.360    | 3.500                | 3.175          | 2.009        | 1.230              | 3.183                   | 2.309               | 1.845                |
| substitute_median5_simd_partition | 1.603    | 1.850                | 1.823          | 1.411        | 0.827              | 1.901                   | 1.868               | 1.662                |
| substitute_median3_partition3     | 2.227    | 2.375                | 2.229          | 1.572        | 1.022              | 2.168                   | 2.675               | 1.690                |
| substitute_median3_partition4     | 2.414    | 2.351                | 2.197          | 1.580        | 0.998              | 2.177                   | 2.777               | 1.683                |
| substitute_blocked112_partition2  | 3.984    | 4.280                | 3.902          | 2.432        | 1.448              | 3.933                   | 3.839               | 4.050                |
| substitute_blocked112_partition3  | 3.588    | 3.884                | 3.680          | 2.328        | 1.366              | 3.663                   | 3.333               | 4.020                |
| substitute_blocked112_partition4  | 3.458    | 3.847                | 3.629          | 2.319        | 1.367              | 3.612                   | 3.202               | 4.018                |
| substitute_median3_terminal1      | 6.116    | 6.569                | 6.284          | 4.971        | 3.822              | 6.410                   | 5.439               | 4.734                |
| substitute_median3_terminal8      | 3.633    | 3.990                | 3.635          | 2.270        | 1.264              | 3.747                   | 2.790               | 2.022                |
| substitute_median3_terminal16     | 3.090    | 3.373                | 3.058          | 1.958        | 1.123              | 3.150                   | 2.332               | 1.555                |
| substitute_pivot_forgetting       | 1.607    | 1.737                | 1.698          | 1.388        | 0.943              | 1.679                   | 1.563               | 2.462                |
| substitute_naive_log              | 1.530    | 1.736                | 1.673          | 1.366        | 0.940              | 1.682                   | 1.547               | 2.545                |

## Dijkstra and Prim graph results

C++ graph timings use a deterministic connected bidirectional graph with 4,096 vertices and 32,768 directed arcs. All weights are positive. Prim uses the minimum-edge undirected conversion. Setup and destruction occur outside timing; correctness verification follows each trial outside timing.

Complete distances, parents, tree edges, total weight, and queue counts match the independent Rust paired checker for the standard queue. Every measured C++ graph trial also validates against its workload reference. The paired Rust checker has no timer. Original Rust graph timing has a separate lifecycle and Prim semantics, so this report supplies no matched graph speed ratio.

| Queue                 | Dijkstra ms [range]  | Pushes/pops | Prim ms [range]      | Pushes/pops | Rust timing      |
| --------------------- | -------------------- | ----------- | -------------------- | ----------- | ---------------- |
| quickheap             | 0.246 [0.240, 0.258] | 7368/7368   | 0.500 [0.491, 0.513] | 16384/16384 | No matched timer |
| std_priority_queue    | 0.307 [0.302, 0.320] | 7368/7368   | 0.654 [0.634, 0.667] | 16384/16384 | No matched timer |
| custom_binary_heap    | 0.440 [0.431, 0.457] | 7368/7368   | 0.954 [0.946, 0.978] | 16384/16384 | No matched timer |
| dary4                 | 0.457 [0.450, 0.467] | 7368/7368   | 1.034 [1.029, 1.049] | 16384/16384 | No matched timer |
| dary8                 | 0.463 [0.457, 0.474] | 7368/7368   | 1.036 [1.030, 1.044] | 16384/16384 | No matched timer |
| dary16                | 0.511 [0.502, 0.526] | 7368/7368   | 1.130 [1.122, 1.147] | 16384/16384 | No matched timer |
| custom_dary4          | 0.453 [0.448, 0.459] | 7368/7368   | 0.999 [0.986, 1.009] | 16384/16384 | No matched timer |
| custom_dary8          | 0.463 [0.455, 0.478] | 7368/7368   | 1.029 [1.020, 1.042] | 16384/16384 | No matched timer |
| weak_heap             | 0.627 [0.617, 0.647] | 7368/7368   | 1.476 [1.452, 1.507] | 16384/16384 | No matched timer |
| radix_heap            | 0.317 [0.308, 0.334] | 7368/7368   | N/A                  | N/A         | No matched timer |
| scalar_quickheap      | 0.385 [0.379, 0.412] | 7368/7368   | 0.785 [0.769, 0.810] | 16384/16384 | No matched timer |
| blocked_quickheap     | 0.623 [0.618, 0.648] | 7368/7368   | 1.215 [1.200, 1.245] | 16384/16384 | No matched timer |
| scalar_median1_linear | 0.569 [0.558, 0.579] | 7368/7368   | 1.168 [1.161, 1.193] | 16384/16384 | No matched timer |
| scalar_median3_linear | 0.732 [0.724, 0.742] | 7368/7368   | 1.499 [1.486, 1.538] | 16384/16384 | No matched timer |
| scalar_perfect_linear | 0.608 [0.591, 0.653] | 7368/7368   | 1.225 [1.198, 1.263] | 16384/16384 | No matched timer |
| scalar_median1_binary | 0.578 [0.571, 0.591] | 7368/7368   | 1.161 [1.148, 1.173] | 16384/16384 | No matched timer |
| scalar_median3_binary | 0.728 [0.724, 0.745] | 7368/7368   | 1.553 [1.538, 1.586] | 16384/16384 | No matched timer |
| scalar_perfect_binary | 0.617 [0.600, 0.677] | 7368/7368   | 1.228 [1.218, 1.255] | 16384/16384 | No matched timer |
| boost_dary4           | 0.234 [0.230, 0.462] | 7368/7368   | 0.523 [0.516, 0.540] | 16384/16384 | No matched timer |
| boost_fibonacci       | 0.984 [0.956, 1.013] | 7368/7368   | 2.154 [2.113, 2.184] | 16384/16384 | No matched timer |
| boost_pairing         | 1.461 [1.435, 1.494] | 7368/7368   | 3.123 [3.088, 3.154] | 16384/16384 | No matched timer |
| boost_binomial        | 2.186 [2.111, 2.431] | 7368/7368   | 4.799 [4.662, 5.382] | 16384/16384 | No matched timer |
| boost_skew            | 0.617 [0.610, 0.657] | 7368/7368   | 1.335 [1.316, 1.367] | 16384/16384 | No matched timer |
| sequence_heap         | 0.380 [0.374, 0.392] | 7368/7368   | 0.840 [0.832, 0.852] | 16384/16384 | No matched timer |
| s3q                   | 0.313 [0.309, 0.327] | 7368/7368   | 0.526 [0.514, 0.559] | 16384/16384 | No matched timer |
| original_quickheap    | 0.446 [0.426, 0.486] | 7368/7368   | 0.989 [0.973, 1.025] | 16384/16384 | No matched timer |
| external_radix_heap   | 0.290 [0.278, 0.301] | 7368/7368   | N/A                  | N/A         | No matched timer |

Radix queues omit Prim because packed edge-ID keys do not preserve monotonicity. Key-value queues are excluded from the key-only graph runner.

## C++ partition and rebalancing microbenchmarks

These independent microbenchmarks use n = 16,384, seven processes, three measured trials, and one warmup. Times are milliseconds. They measure different work from complete queue workloads and have no matching Rust measurement in this collection.

| Mode        | Variant          | Input layout | C++ ms [range]       | Rust timing |
| ----------- | ---------------- | ------------ | -------------------- | ----------- |
| partition   | scalar           | ascending    | 0.003 [0.003, 0.004] | Unavailable |
| partition   | scalar           | duplicates   | 0.005 [0.005, 0.006] | Unavailable |
| partition   | scalar           | random       | 0.043 [0.042, 0.046] | Unavailable |
| partition   | simd             | ascending    | 0.005 [0.004, 0.006] | Unavailable |
| partition   | simd             | duplicates   | 0.005 [0.004, 0.005] | Unavailable |
| partition   | simd             | random       | 0.004 [0.004, 0.005] | Unavailable |
| rebalancing | naive_log        | best         | 0.046 [0.044, 0.050] | Unavailable |
| rebalancing | naive_log        | flat         | 0.089 [0.086, 0.092] | Unavailable |
| rebalancing | naive_log        | worst        | 0.120 [0.115, 0.122] | Unavailable |
| rebalancing | none             | best         | 0.046 [0.044, 0.049] | Unavailable |
| rebalancing | none             | flat         | 0.089 [0.087, 0.095] | Unavailable |
| rebalancing | none             | worst        | 0.119 [0.116, 0.130] | Unavailable |
| rebalancing | pivot_forgetting | best         | 0.046 [0.044, 0.049] | Unavailable |
| rebalancing | pivot_forgetting | flat         | 0.090 [0.087, 0.092] | Unavailable |
| rebalancing | pivot_forgetting | worst        | 0.122 [0.117, 0.124] | Unavailable |

## Counterpart coverage and remaining gaps

| C++ queue             | Rust counterpart                                  | Timing status                                                                   |
| --------------------- | ------------------------------------------------- | ------------------------------------------------------------------------------- |
| quickheap             | quickheap                                         | Matched controlled timing for all eight workloads                               |
| std_priority_queue    | binary_heap                                       | Matched controlled timing for all eight workloads                               |
| kv_quickheap          | No matching active Rust runner in this collection | C++ standalone timing; payload, extra competitor, or historical activation gap  |
| kv_std_priority_queue | No matching active Rust runner in this collection | C++ standalone timing; payload, extra competitor, or historical activation gap  |
| custom_binary_heap    | No matching active Rust runner in this collection | C++ standalone timing; payload, extra competitor, or historical activation gap  |
| dary4                 | No matching active Rust runner in this collection | C++ standalone timing; payload, extra competitor, or historical activation gap  |
| dary8                 | Orx DaryHeap<8>                                   | Original-profile timing; declared substitute or unproved exact algorithm parity |
| dary16                | No matching active Rust runner in this collection | C++ standalone timing; payload, extra competitor, or historical activation gap  |
| custom_dary4          | No matching active Rust runner in this collection | C++ standalone timing; payload, extra competitor, or historical activation gap  |
| custom_dary8          | No matching active Rust runner in this collection | C++ standalone timing; payload, extra competitor, or historical activation gap  |
| weak_heap             | Rust WeakHeap                                     | Original-profile timing; declared substitute or unproved exact algorithm parity |
| radix_heap            | No matching active Rust runner in this collection | C++ standalone timing; payload, extra competitor, or historical activation gap  |
| scalar_quickheap      | No matching active Rust runner in this collection | C++ standalone timing; payload, extra competitor, or historical activation gap  |
| blocked_quickheap     | No matching active Rust runner in this collection | C++ standalone timing; payload, extra competitor, or historical activation gap  |
| scalar_median1_linear | scalar_median1_linear                             | Matched controlled timing for all eight workloads                               |
| scalar_median3_linear | scalar_median3_linear                             | Matched controlled timing for all eight workloads                               |
| scalar_perfect_linear | scalar_perfect_linear                             | Matched controlled timing for all eight workloads                               |
| scalar_median1_binary | scalar_median1_binary                             | Matched controlled timing for all eight workloads                               |
| scalar_median3_binary | scalar_median3_binary                             | Matched controlled timing for all eight workloads                               |
| scalar_perfect_binary | scalar_perfect_binary                             | Matched controlled timing for all eight workloads                               |
| boost_dary4           | No matching active Rust runner in this collection | C++ standalone timing; payload, extra competitor, or historical activation gap  |
| boost_fibonacci       | No matching active Rust runner in this collection | C++ standalone timing; payload, extra competitor, or historical activation gap  |
| boost_pairing         | No matching active Rust runner in this collection | C++ standalone timing; payload, extra competitor, or historical activation gap  |
| boost_binomial        | No matching active Rust runner in this collection | C++ standalone timing; payload, extra competitor, or historical activation gap  |
| boost_skew            | No matching active Rust runner in this collection | C++ standalone timing; payload, extra competitor, or historical activation gap  |
| sequence_heap         | Pinned Rust FFI source adapter                    | Original-profile timing with attributed bridge flags                            |
| s3q                   | Pinned Rust FFI source adapter                    | Original-profile timing with attributed bridge flags                            |
| original_quickheap    | Pinned Rust FFI source adapter                    | Original-profile timing with attributed bridge flags                            |
| external_radix_heap   | Pinned Rust FFI source adapter                    | Original-profile timing with attributed bridge flags                            |

| Configuration family                                                  | Status                                                                                                            |
| --------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------- |
| Production AVX512                                                     | Registered but host lacks AVX512; no execution                                                                    |
| Development AVX2                                                      | Rust original timing present; exact C++ flat128 implementation remains a declared-substitute gap                  |
| Development AVX512 and flat256 multi-partition variants               | Host lacks AVX512; C++ substitutes do not establish exact historical parity                                       |
| NEON flat/list configurations                                         | Requires an Arm host; no native execution here                                                                    |
| Rust dormant Boost, pairing, Fibonacci, and extra d-ary registrations | Not activated in original source; C++ counterparts measured where implemented                                     |
| Key-value counterparts                                                | C++ timings present; no matching Rust controlled payload runner                                                   |
| Graph timing parity                                                   | Paired correctness available; matched Rust timer unavailable                                                      |
| Historical data                                                       | Archived CSVs lack original compiler/build provenance or use obsolete opcode replay; excluded from current ratios |

## Validation and reproducibility

All fresh common-pair fixtures pass independent full validation and byte-identical push/pop/drain traces before timing. Controlled runners and C++ graph trials perform full correctness validation outside their reported timers. Fixture and binary hashes remain fixed. Negative controls reject wrong fixture digests, wrong operation totals, and wrong heap identity.

Original reference execution retains its original validation limitations and fresh random inputs. Its source manifest and repair identity are attributed explicitly. Successful original execution is not a claim that original and controlled profiles share input bytes.

The C++ standalone inventories contain 672 queue/seed/workload groups and 936 experimental groups. All 1,608 groups contain seven complete processes. Graph inventory contains 52 queue/algorithm groups. Microbenchmark inventory contains 15 groups.

Full seed rows, process ranges, build strings, command receipts, input hashes, and compressed traces remain in the evidence directory. Larger accepted results remain in their original immutable directories.

Regenerate this Markdown from saved inputs with:

```sh
python3 build/performance-report/generate_report.py
```

Evidence and tools:

- [Fresh matched summary CSV](../build/performance-report/fresh-controlled/summary.csv).
- [All C++ queue and variant seed rows](../build/performance-report/cpp-extensions/summary.csv).
- [Original, graph, and microbenchmark receipts](../build/performance-report/original-and-graphs/run.json).
- [Compiler and CPU provenance](../build/performance-report/environment.json).
- [Historical package and current source comparison](../build/performance-report/current-source-verification.json).
- [Machine-readable algorithm catalog](../build/performance-report/inventory.json).
- [Accepted larger-size gate](../build/cpp-rust-parity/final-gate.json).
- [Common-pair collection script](../build/performance-report/collect.py).
- [C++ inventory collection script](../build/performance-report/collect_extensions.py).
- [Original, graph, and microbenchmark collection script](../build/performance-report/collect_original_graphs.py).
- [Attributed FFI comparison cohort](../build/performance-report/original-ffi/run.json).
- [FFI bridge build configuration](../build/performance-report/original-ffi/build-command.json).

Detailed source and protocol definitions are in [PROTOCOL.md](../benchmark/PROTOCOL.md), [COVERAGE.md](../benchmark/COVERAGE.md), and [verification.md](../benchmark/verification.md).
