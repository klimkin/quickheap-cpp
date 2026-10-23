# C++ and Rust performance after second fix round

Collected 2026-10-23T23:40:29.633097+00:00 through 2026-10-23T23:52:09.946093+00:00.

Fresh executions compare fixed C++ sources against unchanged Rust reference binaries. These configuration-specific timings replace the old ratios for the measured queues below.

Measured C++ revision `76b86f4b365f4e9a6991a3979df561a611ad13e8`. Rust reference checkout `0e0217fe32ac70ff7dff32443febe8cfc9c070e8`. C++ includes commits `67ffaae`, `dfceda3`, `c74c9fe`, `5f04d6b`, `c90815c`, `c9e9a4b`, `10c4962`, `db55f6d`, `71b630b`.

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
| C++ binary SHA256 | 211fd14cf27e2b49d243085a1cdd94ae05070b602f68aff0bfafaa2028c89529 |
| C++ source digest | 4f831a4a6269e619d75da087bf848fe260bb8ae61e8c4023d943351e2b8eff99 |
| C++ dependency digest | 301a50ee1248367501a05d2d3bf0c48d9795d399f7702f3e4149bb5b9b727ff7 |
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
| quickheap | -15.2% | -7.8% | -12.9% | -9.4% | +0.7% | -12.2% | +11.4% | -20.4% |
| std_priority_queue | +6.9% | +1.4% | +15.9% | +29.0% | +24.5% | +18.3% | +39.2% | +67.1% |
| scalar_median1_linear | -41.4% | -41.1% | -36.1% | -19.0% | -9.5% | -35.9% | -17.9% | +0.2% |
| scalar_median3_linear | -33.1% | -31.8% | -29.7% | -16.6% | -7.2% | -29.6% | -17.2% | -0.2% |
| scalar_perfect_linear | -1.1% | -6.9% | -5.9% | -0.2% | +11.6% | -5.4% | +0.8% | -0.2% |
| scalar_median1_binary | -41.7% | -40.1% | -35.4% | -19.4% | -4.3% | -34.6% | -18.3% | +1.4% |
| scalar_median3_binary | -32.5% | -32.4% | -29.4% | -16.8% | -8.3% | -28.6% | -19.1% | -4.3% |
| scalar_perfect_binary | -1.3% | -8.8% | -6.6% | +0.0% | +15.8% | -6.3% | -1.4% | -21.3% |

## Where C++ remains slower

The following cells have positive excess and nonoverlapping process ranges. This is an observed separation, not a causal diagnosis or significance test.

| Queue | Workload | C++ ms [range] | Rust ms [range] | C++ excess |
| --- | --- | --- | --- | --- |
| std_priority_queue | HeapSort | 0.505 [0.496, 0.518] | 0.473 [0.408, 0.481] | +6.89% |
| std_priority_queue | MonotoneWiggle | 1.437 [1.412, 1.490] | 1.242 [1.198, 1.281] | +15.86% |
| std_priority_queue | RandomWiggle | 1.699 [1.682, 1.754] | 1.319 [1.301, 1.362] | +28.97% |
| std_priority_queue | RandomConstantSize | 5.554 [5.516, 5.656] | 4.474 [4.357, 4.521] | +24.48% |
| std_priority_queue | GeometricMonotoneWiggle | 1.584 [1.566, 1.622] | 1.338 [1.286, 1.378] | +18.28% |
| std_priority_queue | WorstCaseDescending | 6.183 [5.523, 6.328] | 4.309 [4.059, 4.465] | +39.23% |
| std_priority_queue | MostlyPushDecreasing | 5.341 [5.291, 5.467] | 3.213 [3.119, 3.237] | +67.14% |
| scalar_perfect_linear | RandomConstantSize | 3.096 [3.010, 3.203] | 2.792 [2.645, 2.859] | +11.64% |
| scalar_perfect_binary | RandomConstantSize | 3.256 [3.128, 3.417] | 2.838 [2.755, 2.975] | +15.80% |

## Previous versus current ratios

Each column compares C++ with Rust. Collections occurred at different times. These are historical/current ratios, not isolated causal C++ speedups. Isolated and combined C++ fix measurements remain in [fix evidence](performance-fixes-2026-10-23.md).

| Queue | Workload | Previous C++ excess | Prior-fix C++ excess | Current C++ excess |
| --- | --- | --- | --- | --- |
| quickheap | HeapSort | -14.91% | -14.44% | -15.15% |
| quickheap | MonotoneConstantSize | -8.88% | -8.97% | -7.80% |
| quickheap | MonotoneWiggle | -14.52% | -13.21% | -12.86% |
| quickheap | RandomWiggle | -8.32% | -10.32% | -9.44% |
| quickheap | RandomConstantSize | -2.12% | -1.30% | +0.68% |
| quickheap | GeometricMonotoneWiggle | -15.55% | -13.64% | -12.22% |
| quickheap | WorstCaseDescending | +9.10% | +10.24% | +11.42% |
| quickheap | MostlyPushDecreasing | +27.75% | -20.72% | -20.40% |
| std_priority_queue | HeapSort | +9.31% | +7.18% | +6.89% |
| std_priority_queue | MonotoneConstantSize | +5.53% | +3.01% | +1.39% |
| std_priority_queue | MonotoneWiggle | +17.03% | +15.52% | +15.86% |
| std_priority_queue | RandomWiggle | +30.97% | +28.81% | +28.97% |
| std_priority_queue | RandomConstantSize | +26.85% | +24.38% | +24.48% |
| std_priority_queue | GeometricMonotoneWiggle | +18.90% | +17.04% | +18.28% |
| std_priority_queue | WorstCaseDescending | +40.81% | +38.28% | +39.23% |
| std_priority_queue | MostlyPushDecreasing | +69.08% | +65.39% | +67.14% |
| scalar_perfect_linear | HeapSort | +209.53% | +122.58% | -1.10% |
| scalar_perfect_linear | MonotoneConstantSize | +149.10% | +96.16% | -6.92% |
| scalar_perfect_linear | MonotoneWiggle | +133.83% | +86.11% | -5.92% |
| scalar_perfect_linear | RandomWiggle | +95.42% | +70.74% | -0.20% |
| scalar_perfect_linear | RandomConstantSize | +134.77% | +116.90% | +11.64% |
| scalar_perfect_linear | GeometricMonotoneWiggle | +128.83% | +84.81% | -5.37% |
| scalar_perfect_linear | WorstCaseDescending | +129.46% | +54.18% | +0.79% |
| scalar_perfect_linear | MostlyPushDecreasing | -0.00% | +0.11% | -0.22% |
| scalar_perfect_binary | HeapSort | +209.81% | +120.88% | -1.29% |
| scalar_perfect_binary | MonotoneConstantSize | +142.25% | +91.08% | -8.83% |
| scalar_perfect_binary | MonotoneWiggle | +127.27% | +80.38% | -6.62% |
| scalar_perfect_binary | RandomWiggle | +96.70% | +70.18% | +0.00% |
| scalar_perfect_binary | RandomConstantSize | +132.10% | +112.73% | +15.80% |
| scalar_perfect_binary | GeometricMonotoneWiggle | +121.89% | +78.90% | -6.27% |
| scalar_perfect_binary | WorstCaseDescending | +120.64% | +49.31% | -1.38% |
| scalar_perfect_binary | MostlyPushDecreasing | +29.12% | +10.29% | -21.33% |

## Production QuickHeap AVX2

Production implementation counterparts.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 0.239 [0.233, 0.248] | 0.282 [0.272, 0.300] | -15.15% | -15.77% to -14.20% | No |
| MonotoneConstantSize | 2.644 [2.563, 2.713] | 2.882 [2.842, 2.908] | -7.80% | -8.54% to -6.60% | No |
| MonotoneWiggle | 0.799 [0.784, 0.831] | 0.919 [0.910, 0.943] | -12.86% | -13.78% to -11.75% | No |
| RandomWiggle | 0.771 [0.751, 0.808] | 0.854 [0.832, 0.879] | -9.44% | -11.49% to -7.92% | No |
| RandomConstantSize | 1.657 [1.521, 1.712] | 1.653 [1.585, 1.854] | +0.68% | -0.59% to +3.09% | Yes |
| GeometricMonotoneWiggle | 0.802 [0.773, 0.853] | 0.915 [0.901, 0.938] | -12.22% | -12.95% to -11.50% | No |
| WorstCaseDescending | 2.626 [2.558, 3.232] | 2.499 [2.415, 2.932] | +11.42% | +1.68% to +29.46% | Yes |
| MostlyPushDecreasing | 2.694 [2.653, 2.894] | 3.386 [3.364, 3.457] | -20.40% | -20.66% to -20.14% | No |

## Standard binary heap

Distinct standard-library implementations; declared substitute.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 0.505 [0.496, 0.518] | 0.473 [0.408, 0.481] | +6.89% | +6.29% to +7.19% | No |
| MonotoneConstantSize | 4.668 [4.570, 4.921] | 4.608 [4.505, 4.687] | +1.39% | +0.84% to +2.03% | Yes |
| MonotoneWiggle | 1.437 [1.412, 1.490] | 1.242 [1.198, 1.281] | +15.86% | +15.27% to +16.20% | No |
| RandomWiggle | 1.699 [1.682, 1.754] | 1.319 [1.301, 1.362] | +28.97% | +28.42% to +29.95% | No |
| RandomConstantSize | 5.554 [5.516, 5.656] | 4.474 [4.357, 4.521] | +24.48% | +24.15% to +24.72% | No |
| GeometricMonotoneWiggle | 1.584 [1.566, 1.622] | 1.338 [1.286, 1.378] | +18.28% | +16.88% to +19.15% | No |
| WorstCaseDescending | 6.183 [5.523, 6.328] | 4.309 [4.059, 4.465] | +39.23% | +30.90% to +44.56% | No |
| MostlyPushDecreasing | 5.341 [5.291, 5.467] | 3.213 [3.119, 3.237] | +67.14% | +66.17% to +68.60% | No |

## scalar_median1_linear

Shared controlled scalar contract; not exact historical Rust stdlib counts.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 0.641 [0.627, 0.674] | 1.091 [1.075, 1.134] | -41.41% | -41.93% to -41.14% | No |
| MonotoneConstantSize | 6.793 [6.732, 7.171] | 11.537 [11.461, 11.633] | -41.13% | -41.28% to -40.96% | No |
| MonotoneWiggle | 2.106 [2.029, 2.226] | 3.285 [3.241, 3.450] | -36.07% | -36.74% to -35.55% | No |
| RandomWiggle | 2.026 [1.968, 2.094] | 2.518 [2.470, 2.597] | -19.02% | -20.42% to -18.14% | No |
| RandomConstantSize | 5.626 [5.582, 6.125] | 6.287 [6.241, 6.428] | -9.47% | -11.12% to -6.64% | No |
| GeometricMonotoneWiggle | 2.122 [2.093, 2.251] | 3.340 [3.302, 3.404] | -35.91% | -36.51% to -34.74% | No |
| WorstCaseDescending | 7.417 [6.501, 7.574] | 8.791 [8.525, 8.903] | -17.91% | -25.54% to -13.38% | No |
| MostlyPushDecreasing | 577.783 [572.782, 588.872] | 576.947 [571.827, 585.801] | +0.17% | +0.14% to +0.20% | Yes |

## scalar_median3_linear

Shared controlled scalar contract; not exact historical Rust stdlib counts.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 1.014 [1.004, 1.045] | 1.515 [1.493, 1.572] | -33.07% | -33.65% to -32.52% | No |
| MonotoneConstantSize | 10.725 [10.528, 10.863] | 15.703 [15.532, 15.921] | -31.84% | -32.14% to -31.51% | No |
| MonotoneWiggle | 3.194 [3.153, 3.249] | 4.545 [4.493, 4.713] | -29.71% | -30.28% to -29.30% | No |
| RandomWiggle | 3.204 [3.160, 3.288] | 3.846 [3.805, 3.923] | -16.57% | -16.79% to -16.34% | No |
| RandomConstantSize | 10.615 [10.483, 10.938] | 11.410 [11.318, 11.792] | -7.17% | -8.06% to -5.68% | No |
| GeometricMonotoneWiggle | 3.222 [3.179, 3.275] | 4.587 [4.535, 4.798] | -29.62% | -30.02% to -29.41% | No |
| WorstCaseDescending | 9.778 [9.656, 10.880] | 12.101 [11.935, 12.400] | -17.24% | -20.97% to -10.88% | No |
| MostlyPushDecreasing | 706.769 [697.057, 719.535] | 709.207 [703.586, 720.889] | -0.21% | -0.46% to +0.26% | Yes |

## scalar_perfect_linear

Shared controlled scalar contract; not exact historical Rust stdlib counts.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 0.454 [0.447, 0.475] | 0.459 [0.447, 0.484] | -1.10% | -1.36% to -0.90% | Yes |
| MonotoneConstantSize | 5.700 [5.652, 5.867] | 6.121 [6.063, 6.222] | -6.92% | -7.37% to -6.51% | No |
| MonotoneWiggle | 1.684 [1.659, 1.738] | 1.776 [1.751, 1.838] | -5.92% | -7.78% to -4.97% | No |
| RandomWiggle | 1.302 [1.282, 1.343] | 1.303 [1.283, 1.333] | -0.20% | -0.48% to -0.02% | Yes |
| RandomConstantSize | 3.096 [3.010, 3.203] | 2.792 [2.645, 2.859] | +11.64% | +10.87% to +12.50% | No |
| GeometricMonotoneWiggle | 1.762 [1.735, 1.802] | 1.866 [1.841, 1.924] | -5.37% | -5.57% to -5.07% | No |
| WorstCaseDescending | 4.860 [4.763, 5.036] | 4.820 [4.578, 5.091] | +0.79% | -3.65% to +4.22% | Yes |
| MostlyPushDecreasing | 809.158 [804.648, 820.885] | 810.813 [805.039, 819.439] | -0.22% | -0.45% to +0.01% | Yes |

## scalar_median1_binary

Shared controlled scalar contract; not exact historical Rust stdlib counts.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 0.641 [0.625, 0.689] | 1.100 [1.085, 1.138] | -41.71% | -42.26% to -41.11% | No |
| MonotoneConstantSize | 7.063 [7.006, 7.423] | 11.808 [11.728, 11.996] | -40.14% | -40.72% to -39.50% | No |
| MonotoneWiggle | 2.168 [2.101, 2.279] | 3.356 [3.315, 3.539] | -35.44% | -36.17% to -34.71% | No |
| RandomWiggle | 2.015 [1.958, 2.212] | 2.487 [2.462, 2.554] | -19.43% | -20.13% to -18.45% | No |
| RandomConstantSize | 5.929 [5.808, 6.259] | 6.227 [6.108, 6.489] | -4.32% | -6.14% to -2.01% | Yes |
| GeometricMonotoneWiggle | 2.242 [2.141, 2.319] | 3.427 [3.395, 3.484] | -34.56% | -34.92% to -34.17% | No |
| WorstCaseDescending | 7.489 [6.647, 7.829] | 8.952 [8.763, 9.047] | -18.34% | -25.08% to -14.19% | No |
| MostlyPushDecreasing | 7.630 [7.455, 7.683] | 7.529 [7.458, 7.871] | +1.39% | +0.97% to +1.96% | Yes |

## scalar_median3_binary

Shared controlled scalar contract; not exact historical Rust stdlib counts.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 1.027 [1.006, 1.043] | 1.518 [1.494, 1.542] | -32.49% | -33.16% to -31.76% | No |
| MonotoneConstantSize | 10.873 [10.733, 11.168] | 16.042 [15.932, 16.402] | -32.44% | -32.79% to -32.22% | No |
| MonotoneWiggle | 3.249 [3.203, 3.375] | 4.601 [4.546, 4.646] | -29.38% | -29.86% to -28.87% | No |
| RandomWiggle | 3.171 [3.136, 3.366] | 3.805 [3.767, 3.847] | -16.82% | -17.34% to -16.45% | No |
| RandomConstantSize | 10.231 [10.173, 10.490] | 11.220 [11.146, 11.553] | -8.26% | -9.03% to -6.93% | No |
| GeometricMonotoneWiggle | 3.343 [3.233, 4.637] | 4.659 [4.607, 5.206] | -28.60% | -29.89% to -27.70% | Yes |
| WorstCaseDescending | 9.841 [9.674, 10.863] | 12.503 [12.225, 12.729] | -19.08% | -22.13% to -13.63% | No |
| MostlyPushDecreasing | 11.561 [11.477, 11.668] | 12.086 [12.017, 12.287] | -4.29% | -4.43% to -4.09% | No |

## scalar_perfect_binary

Shared controlled scalar contract; not exact historical Rust stdlib counts.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 0.451 [0.445, 0.463] | 0.456 [0.447, 0.490] | -1.29% | -2.07% to -0.17% | Yes |
| MonotoneConstantSize | 5.856 [5.703, 6.113] | 6.389 [6.233, 6.588] | -8.83% | -10.55% to -7.60% | No |
| MonotoneWiggle | 1.727 [1.701, 1.821] | 1.842 [1.821, 1.904] | -6.62% | -6.99% to -6.16% | No |
| RandomWiggle | 1.323 [1.276, 1.363] | 1.324 [1.283, 1.363] | +0.00% | -0.58% to +0.73% | Yes |
| RandomConstantSize | 3.256 [3.128, 3.417] | 2.838 [2.755, 2.975] | +15.80% | +14.73% to +16.41% | No |
| GeometricMonotoneWiggle | 1.810 [1.800, 1.868] | 1.936 [1.893, 1.985] | -6.27% | -6.77% to -5.45% | No |
| WorstCaseDescending | 5.000 [4.908, 5.037] | 5.036 [4.863, 5.454] | -1.38% | -5.64% to +1.82% | Yes |
| MostlyPushDecreasing | 4.044 [3.941, 4.097] | 5.139 [4.974, 5.302] | -21.33% | -21.44% to -21.23% | No |

## Current original-profile comparison

Original profile matches distributions and timed lifecycle, with independent random bytes. It is separate from controlled fixture timing. Dary8 remains Orx substitute; WeakHeap historical internal-work parity is not established. Scalar uses the paired C++ model; historical Rust comparison-count parity is not certified. Original reference retains legacy validation limitations; full paired output traces certify controlled runs only.

This retimes the frozen historical Rust binary built from `a9f2518d9ed452130361cfe1681c68625a68ab35+push-position-bounded-loads@64a83c655fc89e54263866c6c645e2f24bc9709a`. The current checkout record does not mean this original binary was rebuilt from it. Archived launcher metadata labels its implicit production default scalar; compiled reference uses AVX2/native tuning.

| C++ counterpart | Workload | C++ ms [range] | Rust ms [range] | C++ excess | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| quickheap | HeapSort | 0.249 [0.242, 0.280] | 0.293 [0.271, 0.308] | -14.91% | Yes |
| quickheap | MonotoneConstantSize | 2.663 [2.643, 2.769] | 2.998 [2.958, 3.063] | -11.20% | No |
| quickheap | MonotoneWiggle | 0.865 [0.845, 0.874] | 0.920 [0.915, 0.927] | -5.94% | No |
| quickheap | RandomWiggle | 0.784 [0.769, 0.804] | 0.877 [0.870, 0.887] | -10.62% | No |
| std_priority_queue | HeapSort | 0.504 [0.502, 0.521] | 0.479 [0.427, 0.480] | +5.18% | No |
| std_priority_queue | MonotoneConstantSize | 4.741 [4.677, 5.041] | 4.618 [4.544, 4.687] | +2.66% | Yes |
| std_priority_queue | MonotoneWiggle | 1.442 [1.418, 1.473] | 1.234 [1.214, 1.249] | +16.84% | No |
| std_priority_queue | RandomWiggle | 1.698 [1.688, 1.719] | 1.325 [1.314, 1.356] | +28.11% | No |
| scalar_median3_linear | HeapSort | 1.012 [1.003, 1.025] | 2.063 [2.038, 2.108] | -50.97% | No |
| scalar_median3_linear | MonotoneConstantSize | 10.918 [10.788, 11.027] | 20.958 [20.558, 21.653] | -47.90% | No |
| scalar_median3_linear | MonotoneWiggle | 3.161 [3.125, 3.195] | 6.087 [5.835, 6.386] | -48.07% | No |
| scalar_median3_linear | RandomWiggle | 3.187 [3.148, 3.310] | 4.491 [4.440, 4.886] | -29.05% | No |
| dary8 | HeapSort | 0.348 [0.346, 0.379] | 0.333 [0.330, 0.340] | +4.36% | No |
| dary8 | MonotoneConstantSize | 3.431 [3.412, 3.509] | 3.311 [3.291, 3.330] | +3.64% | No |
| dary8 | MonotoneWiggle | 0.984 [0.970, 0.995] | 0.958 [0.953, 0.985] | +2.67% | Yes |
| dary8 | RandomWiggle | 1.184 [1.179, 1.201] | 1.348 [1.333, 1.378] | -12.12% | No |
| weak_heap | HeapSort | 1.064 [1.054, 1.085] | 0.959 [0.940, 0.976] | +10.91% | No |
| weak_heap | MonotoneConstantSize | 10.736 [10.687, 10.781] | 9.968 [9.857, 10.316] | +7.70% | No |
| weak_heap | MonotoneWiggle | 3.339 [3.320, 3.457] | 3.129 [3.105, 3.169] | +6.72% | No |
| weak_heap | RandomWiggle | 3.124 [3.096, 3.139] | 2.945 [2.923, 2.989] | +6.10% | No |

## Scope and remaining gaps

- Full outputs and push/pop/drain counts match before controlled timing. Scalar transition/count checks retain fixed Rust reference.
- Standard heaps are different standard-library implementations. Orx is a declared substitute. WeakHeap and Dary8 count mismatches remain documented in prior investigation.
- Foreign source identity does not establish matching constructor RNG, capacity, or key translation. Their old timings stay historical.
- Graph checker has no matched Rust timer. Graphs, key/value queues, Boost families, and 39 experimental configurations were not recollected.
- Native/LTO C++ performance, other key types and sizes, and full Conan install/sanitizer matrices are outside this refresh.
- Prior C++-only fix runs found small production slowdowns on MonotoneWiggle, GeometricMonotoneWiggle, and WorstCaseDescending. Current Rust comparison does not erase those parent/candidate findings.

## Reproduction and records

[Historical full comparison](performance-comparison-2026-10-23.md) remains unchanged.

Raw evidence lives in ignored local `build/performance-report-after-fixes-2-20261023/`. A shared copy of this report requires that evidence directory for its raw-data links and regeneration command.

- [Fresh controlled seed summaries](../build/performance-report-after-fixes-2-20261023/controlled/summary.csv)
- [Fresh controlled process medians](../build/performance-report-after-fixes-2-20261023/controlled/process-medians.json)
- [Controlled command receipts](../build/performance-report-after-fixes-2-20261023/controlled/commands.jsonl)
- [Current C++ build receipt](../build/performance-report-after-fixes-2-20261023/controlled/cpp-build-receipt.json)
- [Fresh original-profile summaries](../build/performance-report-after-fixes-2-20261023/original/summary.csv)
- [Original-profile identities and completion](../build/performance-report-after-fixes-2-20261023/original/run.json)
- [Current CPU and compiler observations](../build/performance-report-after-fixes-2-20261023/environment-current.json)

- [Exact collector source used for this run](../build/performance-report-after-fixes-2-20261023/collect_after_fixes-executed.py)

Regenerate Markdown from saved data:

```sh
python3 benchmark/experiments/regression_20261023/generate_after_fixes_report.py \
  --data build/performance-report-after-fixes-2-20261023 \
  --report docs/performance-comparison-2026-10-23-after-fixes-2.md
```
