# C++ and Rust performance after fixes

Collected 2026-10-23T22:13:25.298887+00:00 through 2026-10-23T22:25:41.509991+00:00.

Fresh executions compare fixed C++ sources against unchanged Rust reference binaries. These configuration-specific timings replace the old ratios for the measured queues below.

Measured C++ revision `095b65a52fbe0c91acfb8a1e6bf7936277d55f7a`. Rust reference checkout `0e0217fe32ac70ff7dff32443febe8cfc9c070e8`. C++ includes commits `67ffaae`, `dfceda3`, and `c74c9fe`.

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
| C++ binary SHA256 | 6e302916e6b1848f23b3277f80d8b3d9647c57cf1b36518a613ee086ff001fc9 |
| C++ source digest | a0c43dc39cb3c7e92f3ae8ff7ec2d5b243ce051a1ce5a67fdf9e950198dc8bd3 |
| C++ dependency digest | 1421dbb73823636843fdce53a00b1a15ef71d1eaf5492fbef784231ff6493447 |
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
| quickheap | -14.4% | -9.0% | -13.2% | -10.3% | -1.3% | -13.6% | +10.2% | -20.7% |
| std_priority_queue | +7.2% | +3.0% | +15.5% | +28.8% | +24.4% | +17.0% | +38.3% | +65.4% |
| scalar_median1_linear | +6.9% | +12.2% | +11.2% | +8.1% | +18.9% | +13.0% | +5.4% | +0.2% |
| scalar_median3_linear | +0.5% | +2.9% | +3.4% | -0.6% | +0.2% | +4.5% | -0.4% | -0.1% |
| scalar_perfect_linear | +122.6% | +96.2% | +86.1% | +70.7% | +116.9% | +84.8% | +54.2% | +0.1% |
| scalar_median1_binary | +6.7% | +10.9% | +10.4% | +8.6% | +18.9% | +12.0% | +4.3% | +12.7% |
| scalar_median3_binary | +0.4% | +2.2% | +2.7% | -0.3% | -0.6% | +3.7% | -2.4% | -1.5% |
| scalar_perfect_binary | +120.9% | +91.1% | +80.4% | +70.2% | +112.7% | +78.9% | +49.3% | +10.3% |

## Where C++ remains slower

The following cells have positive excess and nonoverlapping process ranges. This is an observed separation, not a causal diagnosis or significance test.

| Queue | Workload | C++ ms [range] | Rust ms [range] | C++ excess |
| --- | --- | --- | --- | --- |
| std_priority_queue | HeapSort | 0.508 [0.496, 0.533] | 0.475 [0.461, 0.481] | +7.18% |
| std_priority_queue | MonotoneWiggle | 1.440 [1.417, 1.501] | 1.246 [1.229, 1.280] | +15.52% |
| std_priority_queue | RandomWiggle | 1.698 [1.684, 1.751] | 1.319 [1.300, 1.329] | +28.81% |
| std_priority_queue | RandomConstantSize | 5.532 [5.509, 5.642] | 4.448 [4.357, 4.479] | +24.38% |
| std_priority_queue | GeometricMonotoneWiggle | 1.552 [1.535, 1.618] | 1.328 [1.270, 1.373] | +17.04% |
| std_priority_queue | WorstCaseDescending | 6.144 [5.535, 6.203] | 4.290 [4.121, 4.457] | +38.28% |
| std_priority_queue | MostlyPushDecreasing | 5.320 [5.284, 5.385] | 3.211 [3.047, 3.246] | +65.39% |
| scalar_median1_linear | HeapSort | 1.165 [1.156, 1.189] | 1.091 [1.079, 1.118] | +6.95% |
| scalar_median1_linear | MonotoneConstantSize | 12.942 [12.771, 13.043] | 11.517 [11.388, 11.587] | +12.15% |
| scalar_median1_linear | MonotoneWiggle | 3.646 [3.627, 3.694] | 3.271 [3.248, 3.363] | +11.17% |
| scalar_median1_linear | RandomWiggle | 2.744 [2.687, 2.780] | 2.538 [2.450, 2.578] | +8.11% |
| scalar_median1_linear | RandomConstantSize | 7.477 [7.385, 7.653] | 6.291 [6.212, 6.569] | +18.93% |
| scalar_median1_linear | GeometricMonotoneWiggle | 3.777 [3.743, 3.863] | 3.349 [3.310, 3.376] | +12.97% |
| scalar_median3_linear | MonotoneWiggle | 4.736 [4.642, 4.795] | 4.577 [4.477, 4.606] | +3.37% |
| scalar_median3_linear | GeometricMonotoneWiggle | 4.791 [4.765, 4.899] | 4.593 [4.550, 4.654] | +4.53% |
| scalar_perfect_linear | HeapSort | 1.009 [0.996, 1.026] | 0.453 [0.447, 0.462] | +122.58% |
| scalar_perfect_linear | MonotoneConstantSize | 11.990 [11.852, 12.343] | 6.111 [6.071, 6.318] | +96.16% |
| scalar_perfect_linear | MonotoneWiggle | 3.305 [3.296, 3.341] | 1.781 [1.758, 1.809] | +86.11% |
| scalar_perfect_linear | RandomWiggle | 2.221 [2.151, 2.264] | 1.300 [1.281, 1.325] | +70.74% |
| scalar_perfect_linear | RandomConstantSize | 6.060 [5.759, 6.149] | 2.780 [2.690, 2.899] | +116.90% |
| scalar_perfect_linear | GeometricMonotoneWiggle | 3.444 [3.412, 3.467] | 1.866 [1.829, 1.925] | +84.81% |
| scalar_perfect_linear | WorstCaseDescending | 7.445 [7.373, 7.493] | 4.786 [4.573, 5.096] | +54.18% |
| scalar_median1_binary | HeapSort | 1.173 [1.164, 1.198] | 1.108 [1.090, 1.120] | +6.72% |
| scalar_median1_binary | MonotoneConstantSize | 13.184 [13.107, 13.269] | 11.904 [11.784, 12.011] | +10.94% |
| scalar_median1_binary | MonotoneWiggle | 3.722 [3.683, 3.765] | 3.370 [3.343, 3.417] | +10.43% |
| scalar_median1_binary | RandomWiggle | 2.722 [2.700, 2.755] | 2.505 [2.485, 2.574] | +8.61% |
| scalar_median1_binary | RandomConstantSize | 7.457 [7.297, 7.606] | 6.299 [6.175, 6.511] | +18.87% |
| scalar_median1_binary | GeometricMonotoneWiggle | 3.831 [3.812, 3.972] | 3.446 [3.412, 3.481] | +11.98% |
| scalar_median1_binary | MostlyPushDecreasing | 8.523 [8.439, 8.580] | 7.562 [7.509, 7.679] | +12.72% |
| scalar_median3_binary | MonotoneConstantSize | 16.475 [16.298, 16.667] | 16.161 [15.933, 16.275] | +2.22% |
| scalar_median3_binary | MonotoneWiggle | 4.740 [4.703, 4.800] | 4.620 [4.554, 4.698] | +2.72% |
| scalar_median3_binary | GeometricMonotoneWiggle | 4.857 [4.808, 4.910] | 4.674 [4.647, 4.746] | +3.74% |
| scalar_perfect_binary | HeapSort | 1.008 [0.988, 1.022] | 0.456 [0.446, 0.481] | +120.88% |
| scalar_perfect_binary | MonotoneConstantSize | 12.056 [11.903, 12.403] | 6.324 [6.250, 6.995] | +91.08% |
| scalar_perfect_binary | MonotoneWiggle | 3.321 [3.294, 3.357] | 1.842 [1.823, 1.865] | +80.38% |
| scalar_perfect_binary | RandomWiggle | 2.235 [2.203, 2.286] | 1.315 [1.296, 1.342] | +70.18% |
| scalar_perfect_binary | RandomConstantSize | 6.079 [5.799, 6.164] | 2.854 [2.739, 2.971] | +112.73% |
| scalar_perfect_binary | GeometricMonotoneWiggle | 3.433 [3.410, 3.477] | 1.923 [1.901, 1.988] | +78.90% |
| scalar_perfect_binary | WorstCaseDescending | 7.514 [7.425, 7.562] | 4.985 [4.794, 5.565] | +49.31% |
| scalar_perfect_binary | MostlyPushDecreasing | 5.674 [5.626, 5.717] | 5.135 [5.048, 5.210] | +10.29% |

## Previous versus current ratios

Both columns compare C++ with Rust. Collections occurred at different times. These are historical/current ratios, not isolated causal C++ speedups. Isolated and combined C++ fix measurements remain in [fix evidence](performance-fixes-2026-10-23.md).

| Queue | Workload | Previous C++ excess | Current C++ excess |
| --- | --- | --- | --- |
| quickheap | HeapSort | -14.91% | -14.44% |
| quickheap | MonotoneConstantSize | -8.88% | -8.97% |
| quickheap | MonotoneWiggle | -14.52% | -13.21% |
| quickheap | RandomWiggle | -8.32% | -10.32% |
| quickheap | RandomConstantSize | -2.12% | -1.30% |
| quickheap | GeometricMonotoneWiggle | -15.55% | -13.64% |
| quickheap | WorstCaseDescending | +9.10% | +10.24% |
| quickheap | MostlyPushDecreasing | +27.75% | -20.72% |
| std_priority_queue | HeapSort | +9.31% | +7.18% |
| std_priority_queue | MonotoneConstantSize | +5.53% | +3.01% |
| std_priority_queue | MonotoneWiggle | +17.03% | +15.52% |
| std_priority_queue | RandomWiggle | +30.97% | +28.81% |
| std_priority_queue | RandomConstantSize | +26.85% | +24.38% |
| std_priority_queue | GeometricMonotoneWiggle | +18.90% | +17.04% |
| std_priority_queue | WorstCaseDescending | +40.81% | +38.28% |
| std_priority_queue | MostlyPushDecreasing | +69.08% | +65.39% |
| scalar_perfect_linear | HeapSort | +209.53% | +122.58% |
| scalar_perfect_linear | MonotoneConstantSize | +149.10% | +96.16% |
| scalar_perfect_linear | MonotoneWiggle | +133.83% | +86.11% |
| scalar_perfect_linear | RandomWiggle | +95.42% | +70.74% |
| scalar_perfect_linear | RandomConstantSize | +134.77% | +116.90% |
| scalar_perfect_linear | GeometricMonotoneWiggle | +128.83% | +84.81% |
| scalar_perfect_linear | WorstCaseDescending | +129.46% | +54.18% |
| scalar_perfect_linear | MostlyPushDecreasing | -0.00% | +0.11% |
| scalar_perfect_binary | HeapSort | +209.81% | +120.88% |
| scalar_perfect_binary | MonotoneConstantSize | +142.25% | +91.08% |
| scalar_perfect_binary | MonotoneWiggle | +127.27% | +80.38% |
| scalar_perfect_binary | RandomWiggle | +96.70% | +70.18% |
| scalar_perfect_binary | RandomConstantSize | +132.10% | +112.73% |
| scalar_perfect_binary | GeometricMonotoneWiggle | +121.89% | +78.90% |
| scalar_perfect_binary | WorstCaseDescending | +120.64% | +49.31% |
| scalar_perfect_binary | MostlyPushDecreasing | +29.12% | +10.29% |

## Production QuickHeap AVX2

Production implementation counterparts.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 0.241 [0.234, 0.249] | 0.280 [0.275, 0.292] | -14.44% | -14.87% to -13.83% | No |
| MonotoneConstantSize | 2.638 [2.566, 2.734] | 2.895 [2.842, 2.968] | -8.97% | -9.74% to -8.29% | No |
| MonotoneWiggle | 0.802 [0.782, 0.842] | 0.924 [0.910, 0.932] | -13.21% | -14.06% to -12.56% | No |
| RandomWiggle | 0.770 [0.749, 0.788] | 0.855 [0.824, 0.904] | -10.32% | -10.91% to -9.95% | No |
| RandomConstantSize | 1.624 [1.549, 1.715] | 1.660 [1.593, 1.769] | -1.30% | -4.24% to +2.62% | Yes |
| GeometricMonotoneWiggle | 0.794 [0.783, 0.810] | 0.918 [0.895, 0.960] | -13.64% | -15.07% to -12.31% | No |
| WorstCaseDescending | 2.604 [2.574, 3.215] | 2.519 [2.420, 2.942] | +10.24% | +2.63% to +26.26% | Yes |
| MostlyPushDecreasing | 2.684 [2.642, 2.782] | 3.391 [3.374, 3.407] | -20.72% | -21.02% to -20.22% | No |

## Standard binary heap

Distinct standard-library implementations; declared substitute.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 0.508 [0.496, 0.533] | 0.475 [0.461, 0.481] | +7.18% | +5.55% to +9.40% | No |
| MonotoneConstantSize | 4.778 [4.637, 4.986] | 4.613 [4.528, 4.652] | +3.01% | +1.44% to +4.05% | Yes |
| MonotoneWiggle | 1.440 [1.417, 1.501] | 1.246 [1.229, 1.280] | +15.52% | +15.03% to +15.98% | No |
| RandomWiggle | 1.698 [1.684, 1.751] | 1.319 [1.300, 1.329] | +28.81% | +28.23% to +29.46% | No |
| RandomConstantSize | 5.532 [5.509, 5.642] | 4.448 [4.357, 4.479] | +24.38% | +24.32% to +24.42% | No |
| GeometricMonotoneWiggle | 1.552 [1.535, 1.618] | 1.328 [1.270, 1.373] | +17.04% | +16.48% to +17.62% | No |
| WorstCaseDescending | 6.144 [5.535, 6.203] | 4.290 [4.121, 4.457] | +38.28% | +29.37% to +43.37% | No |
| MostlyPushDecreasing | 5.320 [5.284, 5.385] | 3.211 [3.047, 3.246] | +65.39% | +64.55% to +65.91% | No |

## scalar_median1_linear

Shared controlled scalar contract; not exact historical Rust stdlib counts.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 1.165 [1.156, 1.189] | 1.091 [1.079, 1.118] | +6.95% | +6.76% to +7.13% | No |
| MonotoneConstantSize | 12.942 [12.771, 13.043] | 11.517 [11.388, 11.587] | +12.15% | +11.17% to +12.92% | No |
| MonotoneWiggle | 3.646 [3.627, 3.694] | 3.271 [3.248, 3.363] | +11.17% | +10.95% to +11.49% | No |
| RandomWiggle | 2.744 [2.687, 2.780] | 2.538 [2.450, 2.578] | +8.11% | +7.38% to +8.97% | No |
| RandomConstantSize | 7.477 [7.385, 7.653] | 6.291 [6.212, 6.569] | +18.93% | +18.08% to +20.48% | No |
| GeometricMonotoneWiggle | 3.777 [3.743, 3.863] | 3.349 [3.310, 3.376] | +12.97% | +12.63% to +13.44% | No |
| WorstCaseDescending | 9.301 [8.824, 9.352] | 8.706 [8.489, 8.878] | +5.43% | +0.78% to +8.86% | Yes |
| MostlyPushDecreasing | 577.399 [569.527, 588.340] | 575.737 [572.117, 583.162] | +0.17% | -0.12% to +0.41% | Yes |

## scalar_median3_linear

Shared controlled scalar contract; not exact historical Rust stdlib counts.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 1.519 [1.504, 1.547] | 1.512 [1.490, 1.537] | +0.49% | +0.24% to +0.76% | Yes |
| MonotoneConstantSize | 16.144 [15.947, 16.474] | 15.744 [15.444, 16.294] | +2.93% | +2.54% to +3.40% | Yes |
| MonotoneWiggle | 4.736 [4.642, 4.795] | 4.577 [4.477, 4.606] | +3.37% | +3.04% to +3.70% | No |
| RandomWiggle | 3.841 [3.758, 3.919] | 3.839 [3.777, 3.896] | -0.59% | -1.58% to +0.18% | Yes |
| RandomConstantSize | 11.399 [11.279, 11.659] | 11.439 [11.278, 11.738] | +0.21% | -0.35% to +1.04% | Yes |
| GeometricMonotoneWiggle | 4.791 [4.765, 4.899] | 4.593 [4.550, 4.654] | +4.53% | +4.23% to +5.01% | No |
| WorstCaseDescending | 11.943 [11.871, 12.550] | 12.160 [11.941, 12.400] | -0.39% | -3.09% to +2.94% | Yes |
| MostlyPushDecreasing | 705.784 [694.844, 713.996] | 706.566 [699.057, 713.465] | -0.09% | -0.24% to +0.03% | Yes |

## scalar_perfect_linear

Shared controlled scalar contract; not exact historical Rust stdlib counts.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 1.009 [0.996, 1.026] | 0.453 [0.447, 0.462] | +122.58% | +122.40% to +122.79% | No |
| MonotoneConstantSize | 11.990 [11.852, 12.343] | 6.111 [6.071, 6.318] | +96.16% | +95.82% to +96.45% | No |
| MonotoneWiggle | 3.305 [3.296, 3.341] | 1.781 [1.758, 1.809] | +86.11% | +85.50% to +86.46% | No |
| RandomWiggle | 2.221 [2.151, 2.264] | 1.300 [1.281, 1.325] | +70.74% | +69.59% to +72.79% | No |
| RandomConstantSize | 6.060 [5.759, 6.149] | 2.780 [2.690, 2.899] | +116.90% | +114.87% to +119.31% | No |
| GeometricMonotoneWiggle | 3.444 [3.412, 3.467] | 1.866 [1.829, 1.925] | +84.81% | +84.06% to +85.90% | No |
| WorstCaseDescending | 7.445 [7.373, 7.493] | 4.786 [4.573, 5.096] | +54.18% | +47.83% to +58.86% | No |
| MostlyPushDecreasing | 811.486 [807.949, 814.532] | 811.030 [803.426, 816.232] | +0.11% | +0.04% to +0.22% | Yes |

## scalar_median1_binary

Shared controlled scalar contract; not exact historical Rust stdlib counts.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 1.173 [1.164, 1.198] | 1.108 [1.090, 1.120] | +6.72% | +5.83% to +7.36% | No |
| MonotoneConstantSize | 13.184 [13.107, 13.269] | 11.904 [11.784, 12.011] | +10.94% | +10.17% to +11.51% | No |
| MonotoneWiggle | 3.722 [3.683, 3.765] | 3.370 [3.343, 3.417] | +10.43% | +10.02% to +10.83% | No |
| RandomWiggle | 2.722 [2.700, 2.755] | 2.505 [2.485, 2.574] | +8.61% | +8.08% to +9.25% | No |
| RandomConstantSize | 7.457 [7.297, 7.606] | 6.299 [6.175, 6.511] | +18.87% | +18.39% to +19.44% | No |
| GeometricMonotoneWiggle | 3.831 [3.812, 3.972] | 3.446 [3.412, 3.481] | +11.98% | +10.60% to +13.48% | No |
| WorstCaseDescending | 9.464 [9.030, 9.645] | 9.019 [8.780, 9.421] | +4.35% | +0.20% to +6.93% | Yes |
| MostlyPushDecreasing | 8.523 [8.439, 8.580] | 7.562 [7.509, 7.679] | +12.72% | +12.46% to +12.90% | No |

## scalar_median3_binary

Shared controlled scalar contract; not exact historical Rust stdlib counts.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 1.534 [1.517, 1.562] | 1.526 [1.504, 1.549] | +0.35% | -0.16% to +0.75% | Yes |
| MonotoneConstantSize | 16.475 [16.298, 16.667] | 16.161 [15.933, 16.275] | +2.22% | +1.94% to +2.64% | No |
| MonotoneWiggle | 4.740 [4.703, 4.800] | 4.620 [4.554, 4.698] | +2.72% | +1.83% to +3.19% | No |
| RandomWiggle | 3.785 [3.751, 3.850] | 3.805 [3.747, 3.900] | -0.26% | -0.90% to +0.45% | Yes |
| RandomConstantSize | 11.141 [11.095, 11.379] | 11.274 [11.169, 11.738] | -0.62% | -1.20% to +0.42% | Yes |
| GeometricMonotoneWiggle | 4.857 [4.808, 4.910] | 4.674 [4.647, 4.746] | +3.74% | +2.89% to +4.44% | No |
| WorstCaseDescending | 12.022 [11.994, 12.623] | 12.494 [12.284, 12.792] | -2.40% | -5.54% to +0.75% | Yes |
| MostlyPushDecreasing | 11.988 [11.921, 12.064] | 12.174 [12.094, 12.291] | -1.53% | -1.62% to -1.36% | No |

## scalar_perfect_binary

Shared controlled scalar contract; not exact historical Rust stdlib counts.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 1.008 [0.988, 1.022] | 0.456 [0.446, 0.481] | +120.88% | +119.31% to +122.15% | No |
| MonotoneConstantSize | 12.056 [11.903, 12.403] | 6.324 [6.250, 6.995] | +91.08% | +90.61% to +92.02% | No |
| MonotoneWiggle | 3.321 [3.294, 3.357] | 1.842 [1.823, 1.865] | +80.38% | +80.04% to +80.75% | No |
| RandomWiggle | 2.235 [2.203, 2.286] | 1.315 [1.296, 1.342] | +70.18% | +68.08% to +72.39% | No |
| RandomConstantSize | 6.079 [5.799, 6.164] | 2.854 [2.739, 2.971] | +112.73% | +111.50% to +113.69% | No |
| GeometricMonotoneWiggle | 3.433 [3.410, 3.477] | 1.923 [1.901, 1.988] | +78.90% | +78.48% to +79.63% | No |
| WorstCaseDescending | 7.514 [7.425, 7.562] | 4.985 [4.794, 5.565] | +49.31% | +43.12% to +54.53% | No |
| MostlyPushDecreasing | 5.674 [5.626, 5.717] | 5.135 [5.048, 5.210] | +10.29% | +9.65% to +11.06% | No |

## Current original-profile comparison

Original profile matches distributions and timed lifecycle, with independent random bytes. It is separate from controlled fixture timing. Dary8 remains Orx substitute; WeakHeap historical internal-work parity is not established. Scalar uses the paired C++ model; historical Rust comparison-count parity is not certified. Original reference retains legacy validation limitations; full paired output traces certify controlled runs only.

This retimes the frozen historical Rust binary built from `a9f2518d9ed452130361cfe1681c68625a68ab35+push-position-bounded-loads@64a83c655fc89e54263866c6c645e2f24bc9709a`. The current checkout record does not mean this original binary was rebuilt from it. Archived launcher metadata labels its implicit production default scalar; compiled reference uses AVX2/native tuning.

| C++ counterpart | Workload | C++ ms [range] | Rust ms [range] | C++ excess | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| quickheap | HeapSort | 0.249 [0.243, 0.264] | 0.299 [0.274, 0.324] | -16.67% | No |
| quickheap | MonotoneConstantSize | 2.673 [2.620, 2.770] | 3.004 [2.955, 3.142] | -11.01% | No |
| quickheap | MonotoneWiggle | 0.853 [0.833, 0.900] | 0.925 [0.913, 0.947] | -7.79% | No |
| quickheap | RandomWiggle | 0.784 [0.770, 0.807] | 0.886 [0.877, 0.932] | -11.52% | No |
| std_priority_queue | HeapSort | 0.505 [0.496, 0.527] | 0.472 [0.424, 0.478] | +7.03% | No |
| std_priority_queue | MonotoneConstantSize | 4.821 [4.698, 4.916] | 4.633 [4.580, 4.644] | +4.06% | No |
| std_priority_queue | MonotoneWiggle | 1.433 [1.422, 1.526] | 1.225 [1.218, 1.233] | +16.99% | No |
| std_priority_queue | RandomWiggle | 1.701 [1.693, 1.755] | 1.325 [1.321, 1.336] | +28.32% | No |
| scalar_median3_linear | HeapSort | 1.528 [1.515, 1.567] | 2.040 [2.005, 2.064] | -25.12% | No |
| scalar_median3_linear | MonotoneConstantSize | 16.258 [16.218, 16.404] | 20.671 [20.347, 20.863] | -21.35% | No |
| scalar_median3_linear | MonotoneWiggle | 4.675 [4.637, 4.692] | 5.989 [5.895, 6.127] | -21.95% | No |
| scalar_median3_linear | RandomWiggle | 3.799 [3.758, 3.855] | 4.525 [4.447, 4.731] | -16.05% | No |
| dary8 | HeapSort | 0.935 [0.930, 0.938] | 0.333 [0.330, 0.335] | +180.78% | No |
| dary8 | MonotoneConstantSize | 10.304 [10.234, 10.374] | 3.304 [3.269, 3.332] | +211.86% | No |
| dary8 | MonotoneWiggle | 2.748 [2.734, 2.760] | 0.961 [0.953, 0.976] | +185.99% | No |
| dary8 | RandomWiggle | 2.387 [2.375, 2.389] | 1.340 [1.333, 1.357] | +78.20% | No |
| weak_heap | HeapSort | 1.280 [1.266, 1.297] | 0.965 [0.946, 1.018] | +32.60% | No |
| weak_heap | MonotoneConstantSize | 13.057 [12.982, 13.199] | 10.046 [9.862, 10.262] | +29.97% | No |
| weak_heap | MonotoneWiggle | 3.973 [3.965, 4.046] | 3.159 [3.101, 3.197] | +25.78% | No |
| weak_heap | RandomWiggle | 4.409 [4.388, 4.479] | 2.998 [2.936, 3.014] | +47.05% | No |

## Scope and remaining gaps

- Full outputs and push/pop/drain counts match before controlled timing. Scalar transition/count checks retain fixed Rust reference.
- Standard heaps are different standard-library implementations. Orx is a declared substitute. WeakHeap and Dary8 count mismatches remain documented in prior investigation.
- Foreign source identity does not establish matching constructor RNG, capacity, or key translation. Their old timings stay historical.
- Graph checker has no matched Rust timer. Graphs, key/value queues, Boost families, and 39 experimental configurations were not recollected.
- Native/LTO C++ performance, other key types and sizes, and full Conan install/sanitizer matrices are outside this refresh.
- Prior C++-only fix runs found small production slowdowns on MonotoneWiggle, GeometricMonotoneWiggle, and WorstCaseDescending. Current Rust comparison does not erase those parent/candidate findings.

Separate profiles of the fresh C++ binary identify terminal pop work in production MostlyPushDecreasing, merge sorting in perfect-pivot MonotoneConstantSize, and standard-library sift work in standard-heap MonotoneConstantSize. Profiles include untimed CLI work and do not supply reported elapsed times. They identify residual C++ cost centers, not isolated causes of every language gap.

## Reproduction and records

[Historical full comparison](performance-comparison-2026-10-23.md) remains unchanged.

Raw evidence lives in ignored local `build/performance-report-after-fixes-20261023/`. A shared copy of this report requires that evidence directory for its raw-data links and regeneration command.

- [Fresh controlled seed summaries](../build/performance-report-after-fixes-20261023/controlled/summary.csv)
- [Fresh controlled process medians](../build/performance-report-after-fixes-20261023/controlled/process-medians.json)
- [Controlled command receipts](../build/performance-report-after-fixes-20261023/controlled/commands.jsonl)
- [Current C++ build receipt](../build/performance-report-after-fixes-20261023/controlled/cpp-build-receipt.json)
- [Fresh original-profile summaries](../build/performance-report-after-fixes-20261023/original/summary.csv)
- [Original-profile identities and completion](../build/performance-report-after-fixes-20261023/original/run.json)
- [Current CPU and compiler observations](../build/performance-report-after-fixes-20261023/environment-current.json)

- [Separate profile receipts](../build/performance-report-after-fixes-20261023/profiles/run.json)
- [Exact collector source used for this run](../build/performance-report-after-fixes-20261023/collect_after_fixes-executed.py)

Regenerate Markdown from saved data:

```sh
python3 benchmark/experiments/regression_20261023/generate_after_fixes_report.py \
  --data build/performance-report-after-fixes-20261023 \
  --report docs/performance-comparison-2026-10-23-after-fixes.md
```
