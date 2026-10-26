# C++ and Rust performance after fourth fix round

Collected 2026-10-26T10:13:01.719805+00:00 through 2026-10-26T15:02:11.475365+00:00. Controlled timing ran 10:13 to 10:25 UTC. Original-profile timing started at 15:02 UTC after a load-average gate below 3.

Fresh executions compare fixed C++ sources against unchanged Rust reference binaries. These configuration-specific timings replace the old ratios for the measured queues below.

Measured C++ revision `a95fe42b3e1ac174cad74a443fb15d4531b6ac77`. Rust reference checkout `0e0217fe32ac70ff7dff32443febe8cfc9c070e8`. C++ includes commits `a95fe42`.

## Coverage and build identities

| Dataset | Coverage | Method |
| --- | --- | --- |
| Controlled | 8 pairs, 8 workloads, 3 seeds; 192 groups | i64 n=16,384; same fixture bytes; 7 alternating processes; 3 trials; 1 warmup |
| Original profile | 5 available pairs, 4 workloads; 20 cells | i64 n=16,384; independent random inputs; 7 alternating processes; fixed 3 trials; 1 warmup |
| Historical context | 29 queues, 39 variants, graphs, foreign adapters, larger-size gate | Retained in previous report; those inventories were not refreshed here |

| Component | Identity |
| --- | --- |
| C++ compiler | g++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0 |
| C++ flags | `-m64 -O3 -DNDEBUG -std=c++20`; generic CPU; no explicit LTO |
| C++ binary SHA256 | fa8a56086ab3b59305f56afd46c38c97878ce23ad608c5af2c484de097f64198 |
| C++ source digest | 0333a15ef7f2b6484facae720ce8f6d2febd925c33a8423dc4e67c81fe999f53 |
| C++ dependency digest | 62fbe37bbd484aabc5de1cebfb33e8b8e35294b5985b5516da6410aa53b08a7b |
| Rust controlled | Frozen opt-level 3, native CPU, standalone paired runner without inherited fat LTO |
| Rust original | Frozen native CPU, fat LTO, codegen-units 1, avx2/ffi; attributed bridge -fno-lto |
| Rust original historical source | a9f2518d9ed452130361cfe1681c68625a68ab35+push-position-bounded-loads@64a83c655fc89e54263866c6c645e2f24bc9709a |
| Rust original binary SHA256 | 059a818f0be591f2e3985215fa9eb65aaa7988f63d36ca94ce3db4921a891dcd |
| CPU | Intel Core Ultra 9 285K; CPU 3; kernel P-core proof; affinity [3] |
| Current governor | performance |
| Current power preference | default |

C++ production archive was rebuilt from current sources. Benchmark backend archive was reused only after source/header byte comparisons. Actual source/dependency hashes must match each C++ CSV build label. Rust binaries retain archived build identities and hashes; both execute freshly.

Generic C++ and native Rust tuning differ. Original Rust additionally uses fat LTO. Results do not rank languages independently of implementation and build configuration.

## Statistics

For each fixture, central time is median of seven process medians. Each process median contains three timed trials. Displayed controlled times are medians of three seed-level medians; brackets span all observed process medians. Excess is geometric mean of paired C++/Rust ratios across three seeds. Positive means higher C++ elapsed time.

No significance test certifies small C++/Rust gaps here. Overlapping ranges remain inconclusive. Original-profile times are medians of seven process medians, with separate input and lifecycle identity.

Controlled setup, fixture parsing, validation, and destruction stay outside timed workload. Original profile includes timed destruction and fixed reservation policies. Each timing process uses `taskset -c 3`.

## Current controlled comparison

| C++ queue | HeapSort | MonotoneConstantSize | MonotoneWiggle | RandomWiggle | RandomConstantSize | GeometricMonotoneWiggle | WorstCaseDescending | MostlyPushDecreasing |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| quickheap | -26.6% | -15.1% | -19.6% | -28.8% | -30.9% | -19.0% | -22.0% | -62.8% |
| std_priority_queue | +6.4% | +2.3% | +13.8% | +28.9% | +24.4% | +17.2% | +41.1% | +67.5% |
| scalar_median1_linear | -42.3% | -41.6% | -36.2% | -20.8% | -10.1% | -36.4% | -17.4% | -0.2% |
| scalar_median3_linear | -34.6% | -31.9% | -29.4% | -17.0% | -6.2% | -29.5% | -17.4% | -0.0% |
| scalar_perfect_linear | -36.5% | -31.5% | -27.4% | -21.3% | -34.6% | -23.6% | -30.3% | -0.4% |
| scalar_median1_binary | -41.3% | -39.9% | -36.4% | -19.7% | -5.3% | -35.7% | -19.1% | +0.4% |
| scalar_median3_binary | -33.5% | -32.5% | -29.9% | -16.4% | -8.0% | -30.3% | -18.5% | -3.7% |
| scalar_perfect_binary | -36.3% | -29.3% | -27.6% | -20.6% | -25.4% | -24.3% | -31.1% | -39.5% |

## Where C++ remains slower

The following cells have positive excess and nonoverlapping process ranges. This is an observed separation, not a causal diagnosis or significance test.

| Queue | Workload | C++ ms [range] | Rust ms [range] | C++ excess |
| --- | --- | --- | --- | --- |
| std_priority_queue | HeapSort | 0.514 [0.500, 0.542] | 0.482 [0.463, 0.497] | +6.43% |
| std_priority_queue | MonotoneWiggle | 1.448 [1.423, 1.540] | 1.272 [1.251, 1.307] | +13.79% |
| std_priority_queue | RandomWiggle | 1.724 [1.710, 1.763] | 1.338 [1.331, 1.345] | +28.90% |
| std_priority_queue | RandomConstantSize | 5.626 [5.603, 7.041] | 4.522 [4.438, 4.538] | +24.36% |
| std_priority_queue | GeometricMonotoneWiggle | 1.581 [1.561, 1.615] | 1.344 [1.294, 1.443] | +17.20% |
| std_priority_queue | WorstCaseDescending | 6.329 [5.616, 6.400] | 4.305 [4.129, 4.440] | +41.13% |

## Previous versus current ratios

Each column compares C++ with Rust. Collections occurred at different times. These are historical/current ratios, not isolated causal C++ speedups. Isolated and combined C++ fix measurements remain in [fix evidence](performance-fixes-2026-10-23.md).

| Queue | Workload | Previous C++ excess | Prior-fix C++ excess | Current C++ excess |
| --- | --- | --- | --- | --- |
| quickheap | HeapSort | -14.91% | -27.21% | -26.61% |
| quickheap | MonotoneConstantSize | -8.88% | -15.70% | -15.13% |
| quickheap | MonotoneWiggle | -14.52% | -19.79% | -19.60% |
| quickheap | RandomWiggle | -8.32% | -28.20% | -28.76% |
| quickheap | RandomConstantSize | -2.12% | -31.79% | -30.86% |
| quickheap | GeometricMonotoneWiggle | -15.55% | -19.87% | -19.03% |
| quickheap | WorstCaseDescending | +9.10% | -14.09% | -21.96% |
| quickheap | MostlyPushDecreasing | +27.75% | -43.60% | -62.79% |
| std_priority_queue | HeapSort | +9.31% | +6.80% | +6.43% |
| std_priority_queue | MonotoneConstantSize | +5.53% | +2.73% | +2.32% |
| std_priority_queue | MonotoneWiggle | +17.03% | +15.15% | +13.79% |
| std_priority_queue | RandomWiggle | +30.97% | +29.47% | +28.90% |
| std_priority_queue | RandomConstantSize | +26.85% | +24.23% | +24.36% |
| std_priority_queue | GeometricMonotoneWiggle | +18.90% | +16.35% | +17.20% |
| std_priority_queue | WorstCaseDescending | +40.81% | +39.09% | +41.13% |
| std_priority_queue | MostlyPushDecreasing | +69.08% | +66.74% | +67.55% |
| scalar_perfect_linear | HeapSort | +209.53% | -36.22% | -36.50% |
| scalar_perfect_linear | MonotoneConstantSize | +149.10% | -31.82% | -31.51% |
| scalar_perfect_linear | MonotoneWiggle | +133.83% | -27.48% | -27.41% |
| scalar_perfect_linear | RandomWiggle | +95.42% | -21.50% | -21.26% |
| scalar_perfect_linear | RandomConstantSize | +134.77% | -35.13% | -34.59% |
| scalar_perfect_linear | GeometricMonotoneWiggle | +128.83% | -23.95% | -23.60% |
| scalar_perfect_linear | WorstCaseDescending | +129.46% | -30.23% | -30.27% |
| scalar_perfect_linear | MostlyPushDecreasing | -0.00% | -0.51% | -0.44% |
| scalar_perfect_binary | HeapSort | +209.81% | -36.30% | -36.32% |
| scalar_perfect_binary | MonotoneConstantSize | +142.25% | -29.92% | -29.30% |
| scalar_perfect_binary | MonotoneWiggle | +127.27% | -28.22% | -27.63% |
| scalar_perfect_binary | RandomWiggle | +96.70% | -20.72% | -20.64% |
| scalar_perfect_binary | RandomConstantSize | +132.10% | -26.22% | -25.42% |
| scalar_perfect_binary | GeometricMonotoneWiggle | +121.89% | -24.54% | -24.34% |
| scalar_perfect_binary | WorstCaseDescending | +120.64% | -30.90% | -31.14% |
| scalar_perfect_binary | MostlyPushDecreasing | +29.12% | -33.22% | -39.46% |

## Production QuickHeap AVX2

Production implementation counterparts.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 0.209 [0.203, 0.219] | 0.282 [0.275, 0.294] | -26.61% | -27.08% to -26.01% | No |
| MonotoneConstantSize | 2.482 [2.432, 2.557] | 2.929 [2.907, 2.983] | -15.13% | -15.63% to -14.32% | No |
| MonotoneWiggle | 0.759 [0.745, 0.776] | 0.945 [0.923, 1.955] | -19.60% | -19.86% to -19.30% | No |
| RandomWiggle | 0.622 [0.607, 0.636] | 0.871 [0.847, 0.910] | -28.76% | -29.89% to -27.93% | No |
| RandomConstantSize | 1.136 [1.103, 1.175] | 1.639 [1.608, 2.226] | -30.86% | -31.18% to -30.46% | No |
| GeometricMonotoneWiggle | 0.754 [0.737, 0.788] | 0.929 [0.920, 0.968] | -19.03% | -19.91% to -18.31% | No |
| WorstCaseDescending | 2.026 [2.001, 2.074] | 2.611 [2.475, 2.993] | -21.96% | -23.94% to -20.81% | No |
| MostlyPushDecreasing | 1.300 [1.272, 1.337] | 3.477 [3.441, 6.713] | -62.79% | -63.17% to -62.60% | No |

## Standard binary heap

Distinct standard-library implementations; declared substitute.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 0.514 [0.500, 0.542] | 0.482 [0.463, 0.497] | +6.43% | +4.77% to +8.24% | No |
| MonotoneConstantSize | 4.795 [4.666, 5.026] | 4.693 [4.583, 7.434] | +2.32% | +0.98% to +3.75% | Yes |
| MonotoneWiggle | 1.448 [1.423, 1.540] | 1.272 [1.251, 1.307] | +13.79% | +12.71% to +15.00% | No |
| RandomWiggle | 1.724 [1.710, 1.763] | 1.338 [1.331, 1.345] | +28.90% | +28.60% to +29.17% | No |
| RandomConstantSize | 5.626 [5.603, 7.041] | 4.522 [4.438, 4.538] | +24.36% | +24.12% to +24.55% | No |
| GeometricMonotoneWiggle | 1.581 [1.561, 1.615] | 1.344 [1.294, 1.443] | +17.20% | +16.19% to +17.83% | No |
| WorstCaseDescending | 6.329 [5.616, 6.400] | 4.305 [4.129, 4.440] | +41.13% | +28.76% to +48.47% | No |
| MostlyPushDecreasing | 5.449 [5.383, 5.500] | 3.251 [3.187, 6.283] | +67.55% | +66.15% to +68.89% | Yes |

## scalar_median1_linear

Shared controlled scalar contract; not exact historical Rust stdlib counts.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 0.643 [0.636, 0.668] | 1.117 [1.100, 1.167] | -42.25% | -42.60% to -41.77% | No |
| MonotoneConstantSize | 6.873 [6.829, 6.993] | 11.770 [11.634, 12.013] | -41.59% | -41.71% to -41.46% | No |
| MonotoneWiggle | 2.132 [2.059, 5.187] | 3.328 [3.296, 6.362] | -36.21% | -37.23% to -35.56% | Yes |
| RandomWiggle | 2.018 [2.001, 2.099] | 2.592 [2.513, 5.065] | -20.81% | -22.14% to -20.14% | No |
| RandomConstantSize | 5.710 [5.659, 6.186] | 6.444 [6.353, 6.564] | -10.12% | -11.76% to -7.13% | No |
| GeometricMonotoneWiggle | 2.148 [2.124, 2.261] | 3.402 [3.364, 3.534] | -36.37% | -37.37% to -34.91% | No |
| WorstCaseDescending | 7.826 [6.785, 7.980] | 9.179 [8.886, 9.298] | -17.40% | -25.70% to -12.21% | No |
| MostlyPushDecreasing | 586.047 [582.188, 593.442] | 587.730 [581.088, 590.137] | -0.19% | -0.32% to +0.03% | Yes |

## scalar_median3_linear

Shared controlled scalar contract; not exact historical Rust stdlib counts.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 1.006 [0.995, 1.035] | 1.536 [1.518, 1.575] | -34.61% | -34.93% to -34.36% | No |
| MonotoneConstantSize | 10.900 [10.686, 11.046] | 15.932 [15.832, 16.111] | -31.93% | -32.32% to -31.53% | No |
| MonotoneWiggle | 3.244 [3.205, 3.294] | 4.585 [4.550, 7.642] | -29.37% | -29.61% to -29.08% | No |
| RandomWiggle | 3.242 [3.175, 6.259] | 3.919 [3.826, 4.091] | -16.99% | -17.36% to -16.50% | Yes |
| RandomConstantSize | 10.828 [10.709, 11.008] | 11.544 [11.488, 11.845] | -6.21% | -7.48% to -4.94% | No |
| GeometricMonotoneWiggle | 3.281 [3.220, 3.345] | 4.644 [4.586, 4.694] | -29.47% | -29.67% to -29.27% | No |
| WorstCaseDescending | 9.921 [9.755, 11.064] | 12.395 [12.139, 12.612] | -17.36% | -21.25% to -10.99% | No |
| MostlyPushDecreasing | 716.372 [712.251, 721.207] | 717.172 [710.824, 723.772] | -0.01% | -0.28% to +0.13% | Yes |

## scalar_perfect_linear

Shared controlled scalar contract; not exact historical Rust stdlib counts.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 0.295 [0.289, 0.311] | 0.465 [0.452, 0.472] | -36.50% | -36.74% to -36.12% | No |
| MonotoneConstantSize | 4.266 [4.177, 4.338] | 6.218 [6.184, 6.355] | -31.51% | -31.81% to -31.02% | No |
| MonotoneWiggle | 1.307 [1.287, 1.353] | 1.802 [1.785, 1.843] | -27.41% | -27.78% to -27.04% | No |
| RandomWiggle | 1.037 [1.017, 1.061] | 1.315 [1.297, 1.353] | -21.26% | -21.87% to -20.60% | No |
| RandomConstantSize | 1.853 [1.744, 1.881] | 2.810 [2.720, 2.897] | -34.59% | -35.82% to -33.48% | No |
| GeometricMonotoneWiggle | 1.438 [1.406, 1.457] | 1.888 [1.858, 1.916] | -23.60% | -23.92% to -22.97% | No |
| WorstCaseDescending | 3.409 [3.355, 3.507] | 4.888 [4.672, 5.219] | -30.27% | -33.25% to -27.90% | No |
| MostlyPushDecreasing | 817.614 [806.427, 822.167] | 821.572 [815.392, 824.782] | -0.44% | -0.50% to -0.32% | Yes |

## scalar_median1_binary

Shared controlled scalar contract; not exact historical Rust stdlib counts.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 0.652 [0.641, 0.672] | 1.112 [1.100, 1.126] | -41.28% | -41.43% to -41.05% | No |
| MonotoneConstantSize | 7.221 [7.145, 7.292] | 11.987 [11.919, 12.124] | -39.86% | -40.37% to -39.45% | No |
| MonotoneWiggle | 2.173 [2.118, 2.262] | 3.411 [3.375, 3.455] | -36.39% | -36.93% to -35.57% | No |
| RandomWiggle | 2.056 [1.972, 2.079] | 2.538 [2.525, 2.627] | -19.70% | -21.34% to -18.51% | No |
| RandomConstantSize | 5.954 [5.870, 6.199] | 6.331 [6.256, 6.581] | -5.26% | -6.63% to -3.00% | No |
| GeometricMonotoneWiggle | 2.220 [2.163, 5.246] | 3.486 [3.436, 3.544] | -35.71% | -36.61% to -34.10% | Yes |
| WorstCaseDescending | 7.657 [6.662, 7.929] | 9.205 [8.959, 9.306] | -19.11% | -26.95% to -14.63% | No |
| MostlyPushDecreasing | 7.854 [7.697, 8.079] | 7.852 [7.746, 7.922] | +0.36% | -0.38% to +0.95% | Yes |

## scalar_median3_binary

Shared controlled scalar contract; not exact historical Rust stdlib counts.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 1.028 [1.018, 1.044] | 1.544 [1.521, 1.808] | -33.52% | -33.67% to -33.27% | No |
| MonotoneConstantSize | 11.048 [10.921, 11.121] | 16.342 [16.181, 16.495] | -32.55% | -32.72% to -32.31% | No |
| MonotoneWiggle | 3.283 [3.261, 3.313] | 4.688 [4.624, 7.651] | -29.85% | -30.00% to -29.77% | No |
| RandomWiggle | 3.239 [3.218, 3.278] | 3.870 [3.821, 3.906] | -16.40% | -16.83% to -16.05% | No |
| RandomConstantSize | 10.423 [10.374, 10.834] | 11.389 [11.287, 11.501] | -7.96% | -8.71% to -6.62% | No |
| GeometricMonotoneWiggle | 3.311 [3.269, 3.368] | 4.751 [4.686, 7.739] | -30.29% | -31.01% to -29.88% | No |
| WorstCaseDescending | 10.044 [9.930, 11.137] | 12.679 [12.465, 12.985] | -18.45% | -22.28% to -12.29% | No |
| MostlyPushDecreasing | 12.096 [11.930, 12.340] | 12.629 [12.481, 12.811] | -3.75% | -4.22% to -3.14% | No |

## scalar_perfect_binary

Shared controlled scalar contract; not exact historical Rust stdlib counts.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 0.296 [0.290, 0.310] | 0.463 [0.453, 0.483] | -36.32% | -36.97% to -35.73% | No |
| MonotoneConstantSize | 4.527 [4.473, 4.593] | 6.403 [6.301, 6.586] | -29.30% | -29.43% to -29.14% | No |
| MonotoneWiggle | 1.351 [1.339, 1.400] | 1.870 [1.852, 1.901] | -27.63% | -28.09% to -27.29% | No |
| RandomWiggle | 1.064 [1.029, 1.094] | 1.329 [1.307, 1.355] | -20.64% | -21.65% to -19.96% | No |
| RandomConstantSize | 2.154 [2.053, 2.166] | 2.875 [2.787, 2.930] | -25.42% | -26.11% to -25.04% | No |
| GeometricMonotoneWiggle | 1.469 [1.458, 1.497] | 1.944 [1.917, 1.969] | -24.34% | -24.58% to -24.08% | No |
| WorstCaseDescending | 3.539 [3.516, 3.590] | 5.081 [4.939, 5.424] | -31.14% | -34.18% to -29.22% | No |
| MostlyPushDecreasing | 3.473 [3.427, 3.714] | 5.740 [5.439, 5.959] | -39.46% | -39.86% to -39.02% | No |

## Current original-profile comparison

Original profile matches distributions and timed lifecycle, with independent random bytes. It is separate from controlled fixture timing. Dary8 remains Orx substitute; WeakHeap historical internal-work parity is not established. Scalar uses the paired C++ model; historical Rust comparison-count parity is not certified. Original reference retains legacy validation limitations; full paired output traces certify controlled runs only.

This retimes the frozen historical Rust binary built from `a9f2518d9ed452130361cfe1681c68625a68ab35+push-position-bounded-loads@64a83c655fc89e54263866c6c645e2f24bc9709a`. The current checkout record does not mean this original binary was rebuilt from it. Archived launcher metadata labels its implicit production default scalar; compiled reference uses AVX2/native tuning.

| C++ counterpart | Workload | C++ ms [range] | Rust ms [range] | C++ excess | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| quickheap | HeapSort | 0.213 [0.208, 0.233] | 0.308 [0.278, 0.325] | -30.85% | No |
| quickheap | MonotoneConstantSize | 2.516 [2.438, 2.586] | 3.007 [2.967, 3.014] | -16.32% | No |
| quickheap | MonotoneWiggle | 0.807 [0.783, 0.818] | 0.926 [0.915, 0.934] | -12.78% | No |
| quickheap | RandomWiggle | 0.623 [0.614, 0.654] | 0.882 [0.869, 0.893] | -29.39% | No |
| std_priority_queue | HeapSort | 0.502 [0.496, 0.516] | 0.472 [0.459, 0.491] | +6.40% | No |
| std_priority_queue | MonotoneConstantSize | 4.812 [4.750, 5.009] | 4.608 [4.464, 4.632] | +4.42% | No |
| std_priority_queue | MonotoneWiggle | 1.443 [1.412, 1.476] | 1.243 [1.214, 1.256] | +16.11% | No |
| std_priority_queue | RandomWiggle | 1.701 [1.684, 1.716] | 1.322 [1.320, 1.333] | +28.66% | No |
| scalar_median3_linear | HeapSort | 1.019 [1.001, 1.055] | 2.065 [2.016, 2.088] | -50.65% | No |
| scalar_median3_linear | MonotoneConstantSize | 11.045 [10.853, 11.186] | 20.633 [20.373, 20.973] | -46.47% | No |
| scalar_median3_linear | MonotoneWiggle | 3.159 [3.137, 3.273] | 5.981 [5.931, 6.135] | -47.19% | No |
| scalar_median3_linear | RandomWiggle | 3.219 [3.189, 3.238] | 4.535 [4.363, 4.613] | -29.02% | No |
| dary8 | HeapSort | 0.321 [0.318, 0.329] | 0.334 [0.331, 0.339] | -3.77% | No |
| dary8 | MonotoneConstantSize | 3.204 [3.153, 3.251] | 3.351 [3.287, 3.439] | -4.37% | No |
| dary8 | MonotoneWiggle | 0.924 [0.900, 0.930] | 0.980 [0.952, 1.003] | -5.65% | No |
| dary8 | RandomWiggle | 1.113 [1.099, 1.148] | 1.374 [1.351, 1.403] | -18.95% | No |
| weak_heap | HeapSort | 0.490 [0.486, 0.495] | 0.959 [0.948, 0.977] | -48.93% | No |
| weak_heap | MonotoneConstantSize | 4.058 [4.053, 4.119] | 9.954 [9.865, 10.369] | -59.23% | No |
| weak_heap | MonotoneWiggle | 1.440 [1.431, 1.449] | 3.147 [3.110, 3.170] | -54.23% | No |
| weak_heap | RandomWiggle | 1.836 [1.830, 1.850] | 2.984 [2.960, 3.031] | -38.47% | No |

## Scope and remaining gaps

- Full outputs and push/pop/drain counts match before controlled timing. Scalar transition/count checks retain fixed Rust reference.
- Standard heaps are different standard-library implementations. Orx is a declared substitute. WeakHeap and Dary8 count mismatches remain documented in prior investigation.
- Foreign source identity does not establish matching constructor RNG, capacity, or key translation. Their old timings stay historical.
- Graph checker has no matched Rust timer. Graphs, key/value queues, Boost families, and 39 experimental configurations were not recollected.
- Native/LTO C++ performance, other key types and sizes, and full Conan install/sanitizer matrices are outside this refresh.
- Prior C++-only fix runs found small production slowdowns on MonotoneWiggle, GeometricMonotoneWiggle, and WorstCaseDescending. Current Rust comparison does not erase those parent/candidate findings.

## Reproduction and records

[Historical full comparison](performance-comparison-2026-10-23.md) remains unchanged.

Raw evidence lives in ignored local `build/performance-report-after-fixes-4-20261025/`. A shared copy of this report requires that evidence directory for its raw-data links and regeneration command.

- [Fresh controlled seed summaries](../build/performance-report-after-fixes-4-20261025/controlled/summary.csv)
- [Fresh controlled process medians](../build/performance-report-after-fixes-4-20261025/controlled/process-medians.json)
- [Controlled command receipts](../build/performance-report-after-fixes-4-20261025/controlled/commands.jsonl)
- [Current C++ build receipt](../build/performance-report-after-fixes-4-20261025/controlled/cpp-build-receipt.json)
- [Fresh original-profile summaries](../build/performance-report-after-fixes-4-20261025/original/summary.csv)
- [Original-profile identities and completion](../build/performance-report-after-fixes-4-20261025/original/run.json)
- [Current CPU and compiler observations](../build/performance-report-after-fixes-4-20261025/environment-current.json)

- [Exact collector source used for this run](../build/performance-report-after-fixes-4-20261025/collect_after_fixes-executed.py)

Regenerate Markdown from saved data:

```sh
python3 benchmark/experiments/regression_20261023/generate_after_fixes_report.py \
  --data build/performance-report-after-fixes-4-20261025 \
  --report docs/performance-comparison-2026-10-25-after-fixes-4.md
```
