# C++ and Rust performance after third fix round

Collected 2026-10-24T09:36:49.698007+00:00 through 2026-10-24T09:48:26.239829+00:00.

Fresh executions compare fixed C++ sources against unchanged Rust reference binaries. These configuration-specific timings replace the old ratios for the measured queues below.

Measured C++ revision `54fc91b06c745ff2c23b2ba49352c958350130eb`. Rust reference checkout `0e0217fe32ac70ff7dff32443febe8cfc9c070e8`. C++ includes commits `837054d`, `7562bbb`, `7401284`, `bbccb0e`, `7ff00e0`, `fd3cb33`, `91d3b18`, `d3ed212`, `bb8d74f`, `5160cdd`, `54fc91b`.

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
| C++ binary SHA256 | 8a1ecb0a4e13f72935de49a1a232a077d176803d48dba42028f3db65cc23e3a0 |
| C++ source digest | 93d8965a9c5f181b9d35539006b33ff501799870878f1ce4c6691506e54920d7 |
| C++ dependency digest | c7f439aca344d723950a7c5a49d96586a8284aa202f913474af4f7939e39151b |
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
| quickheap | -27.2% | -15.7% | -19.8% | -28.2% | -31.8% | -19.9% | -14.1% | -43.6% |
| std_priority_queue | +6.8% | +2.7% | +15.2% | +29.5% | +24.2% | +16.4% | +39.1% | +66.7% |
| scalar_median1_linear | -41.9% | -40.7% | -36.1% | -20.1% | -10.2% | -36.1% | -18.3% | -0.3% |
| scalar_median3_linear | -34.4% | -32.1% | -29.6% | -17.0% | -6.4% | -29.7% | -17.1% | -0.2% |
| scalar_perfect_linear | -36.2% | -31.8% | -27.5% | -21.5% | -35.1% | -23.9% | -30.2% | -0.5% |
| scalar_median1_binary | -41.1% | -39.6% | -35.6% | -19.5% | -6.1% | -35.3% | -18.8% | +0.7% |
| scalar_median3_binary | -33.1% | -32.3% | -29.2% | -16.0% | -8.1% | -29.2% | -18.5% | -3.8% |
| scalar_perfect_binary | -36.3% | -29.9% | -28.2% | -20.7% | -26.2% | -24.5% | -30.9% | -33.2% |

## Where C++ remains slower

The following cells have positive excess and nonoverlapping process ranges. This is an observed separation, not a causal diagnosis or significance test.

| Queue | Workload | C++ ms [range] | Rust ms [range] | C++ excess |
| --- | --- | --- | --- | --- |
| std_priority_queue | HeapSort | 0.513 [0.489, 0.553] | 0.478 [0.472, 0.487] | +6.80% |
| std_priority_queue | MonotoneWiggle | 1.433 [1.407, 1.460] | 1.240 [1.226, 1.287] | +15.15% |
| std_priority_queue | RandomWiggle | 1.703 [1.691, 1.737] | 1.317 [1.301, 1.332] | +29.47% |
| std_priority_queue | RandomConstantSize | 5.522 [5.492, 5.569] | 4.446 [4.394, 4.478] | +24.23% |
| std_priority_queue | GeometricMonotoneWiggle | 1.559 [1.537, 1.606] | 1.332 [1.276, 1.362] | +16.35% |
| std_priority_queue | WorstCaseDescending | 6.113 [5.464, 6.265] | 4.278 [4.170, 4.980] | +39.09% |
| std_priority_queue | MostlyPushDecreasing | 5.362 [5.296, 5.435] | 3.231 [3.038, 3.290] | +66.74% |

## Previous versus current ratios

Each column compares C++ with Rust. Collections occurred at different times. These are historical/current ratios, not isolated causal C++ speedups. Isolated and combined C++ fix measurements remain in [fix evidence](performance-fixes-2026-10-23.md).

| Queue | Workload | Previous C++ excess | Prior-fix C++ excess | Current C++ excess |
| --- | --- | --- | --- | --- |
| quickheap | HeapSort | -14.91% | -15.15% | -27.21% |
| quickheap | MonotoneConstantSize | -8.88% | -7.80% | -15.70% |
| quickheap | MonotoneWiggle | -14.52% | -12.86% | -19.79% |
| quickheap | RandomWiggle | -8.32% | -9.44% | -28.20% |
| quickheap | RandomConstantSize | -2.12% | +0.68% | -31.79% |
| quickheap | GeometricMonotoneWiggle | -15.55% | -12.22% | -19.87% |
| quickheap | WorstCaseDescending | +9.10% | +11.42% | -14.09% |
| quickheap | MostlyPushDecreasing | +27.75% | -20.40% | -43.60% |
| std_priority_queue | HeapSort | +9.31% | +6.89% | +6.80% |
| std_priority_queue | MonotoneConstantSize | +5.53% | +1.39% | +2.73% |
| std_priority_queue | MonotoneWiggle | +17.03% | +15.86% | +15.15% |
| std_priority_queue | RandomWiggle | +30.97% | +28.97% | +29.47% |
| std_priority_queue | RandomConstantSize | +26.85% | +24.48% | +24.23% |
| std_priority_queue | GeometricMonotoneWiggle | +18.90% | +18.28% | +16.35% |
| std_priority_queue | WorstCaseDescending | +40.81% | +39.23% | +39.09% |
| std_priority_queue | MostlyPushDecreasing | +69.08% | +67.14% | +66.74% |
| scalar_perfect_linear | HeapSort | +209.53% | -1.10% | -36.22% |
| scalar_perfect_linear | MonotoneConstantSize | +149.10% | -6.92% | -31.82% |
| scalar_perfect_linear | MonotoneWiggle | +133.83% | -5.92% | -27.48% |
| scalar_perfect_linear | RandomWiggle | +95.42% | -0.20% | -21.50% |
| scalar_perfect_linear | RandomConstantSize | +134.77% | +11.64% | -35.13% |
| scalar_perfect_linear | GeometricMonotoneWiggle | +128.83% | -5.37% | -23.95% |
| scalar_perfect_linear | WorstCaseDescending | +129.46% | +0.79% | -30.23% |
| scalar_perfect_linear | MostlyPushDecreasing | -0.00% | -0.22% | -0.51% |
| scalar_perfect_binary | HeapSort | +209.81% | -1.29% | -36.30% |
| scalar_perfect_binary | MonotoneConstantSize | +142.25% | -8.83% | -29.92% |
| scalar_perfect_binary | MonotoneWiggle | +127.27% | -6.62% | -28.22% |
| scalar_perfect_binary | RandomWiggle | +96.70% | +0.00% | -20.72% |
| scalar_perfect_binary | RandomConstantSize | +132.10% | +15.80% | -26.22% |
| scalar_perfect_binary | GeometricMonotoneWiggle | +121.89% | -6.27% | -24.54% |
| scalar_perfect_binary | WorstCaseDescending | +120.64% | -1.38% | -30.90% |
| scalar_perfect_binary | MostlyPushDecreasing | +29.12% | -21.33% | -33.22% |

## Production QuickHeap AVX2

Production implementation counterparts.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 0.204 [0.195, 0.211] | 0.280 [0.273, 0.296] | -27.21% | -27.67% to -26.47% | No |
| MonotoneConstantSize | 2.431 [2.383, 2.506] | 2.882 [2.839, 2.924] | -15.70% | -16.53% to -14.92% | No |
| MonotoneWiggle | 0.736 [0.725, 0.756] | 0.920 [0.903, 0.943] | -19.79% | -20.13% to -19.43% | No |
| RandomWiggle | 0.609 [0.596, 0.646] | 0.848 [0.825, 0.975] | -28.20% | -28.44% to -27.95% | No |
| RandomConstantSize | 1.116 [1.084, 1.178] | 1.629 [1.570, 1.741] | -31.79% | -33.17% to -30.71% | No |
| GeometricMonotoneWiggle | 0.731 [0.713, 0.758] | 0.910 [0.891, 0.975] | -19.87% | -20.70% to -18.86% | No |
| WorstCaseDescending | 2.183 [2.125, 2.269] | 2.550 [2.446, 2.680] | -14.09% | -16.20% to -12.78% | No |
| MostlyPushDecreasing | 1.907 [1.855, 1.957] | 3.384 [3.361, 3.435] | -43.60% | -43.70% to -43.43% | No |

## Standard binary heap

Distinct standard-library implementations; declared substitute.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 0.513 [0.489, 0.553] | 0.478 [0.472, 0.487] | +6.80% | +5.10% to +7.74% | No |
| MonotoneConstantSize | 4.729 [4.585, 4.988] | 4.608 [4.486, 4.659] | +2.73% | +2.21% to +3.55% | Yes |
| MonotoneWiggle | 1.433 [1.407, 1.460] | 1.240 [1.226, 1.287] | +15.15% | +14.46% to +15.76% | No |
| RandomWiggle | 1.703 [1.691, 1.737] | 1.317 [1.301, 1.332] | +29.47% | +29.24% to +29.61% | No |
| RandomConstantSize | 5.522 [5.492, 5.569] | 4.446 [4.394, 4.478] | +24.23% | +24.10% to +24.38% | No |
| GeometricMonotoneWiggle | 1.559 [1.537, 1.606] | 1.332 [1.276, 1.362] | +16.35% | +14.76% to +17.20% | No |
| WorstCaseDescending | 6.113 [5.464, 6.265] | 4.278 [4.170, 4.980] | +39.09% | +31.04% to +44.20% | No |
| MostlyPushDecreasing | 5.362 [5.296, 5.435] | 3.231 [3.038, 3.290] | +66.74% | +65.97% to +68.24% | No |

## scalar_median1_linear

Shared controlled scalar contract; not exact historical Rust stdlib counts.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 0.643 [0.621, 0.654] | 1.091 [1.077, 1.126] | -41.86% | -42.51% to -41.04% | No |
| MonotoneConstantSize | 6.802 [6.685, 7.129] | 11.539 [11.390, 11.901] | -40.72% | -42.16% to -38.91% | No |
| MonotoneWiggle | 2.087 [2.039, 2.177] | 3.266 [3.245, 3.312] | -36.06% | -36.87% to -34.98% | No |
| RandomWiggle | 2.019 [1.958, 2.087] | 2.540 [2.465, 2.593] | -20.13% | -20.51% to -19.73% | No |
| RandomConstantSize | 5.619 [5.548, 5.958] | 6.321 [6.217, 6.588] | -10.20% | -11.77% to -7.64% | No |
| GeometricMonotoneWiggle | 2.122 [2.073, 2.210] | 3.348 [3.313, 3.404] | -36.09% | -36.93% to -34.92% | No |
| WorstCaseDescending | 7.414 [6.472, 7.547] | 8.785 [8.578, 8.910] | -18.29% | -25.39% to -13.68% | No |
| MostlyPushDecreasing | 577.349 [571.649, 593.772] | 579.223 [574.707, 586.115] | -0.35% | -0.54% to +0.04% | Yes |

## scalar_median3_linear

Shared controlled scalar contract; not exact historical Rust stdlib counts.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 0.994 [0.979, 1.013] | 1.516 [1.492, 1.565] | -34.37% | -34.72% to -34.14% | No |
| MonotoneConstantSize | 10.761 [10.598, 10.857] | 15.818 [15.565, 16.018] | -32.06% | -32.35% to -31.80% | No |
| MonotoneWiggle | 3.197 [3.170, 3.265] | 4.536 [4.491, 4.740] | -29.55% | -29.58% to -29.52% | No |
| RandomWiggle | 3.195 [3.160, 3.317] | 3.839 [3.783, 3.929] | -17.03% | -17.54% to -16.11% | No |
| RandomConstantSize | 10.670 [10.559, 10.926] | 11.446 [11.321, 11.679] | -6.40% | -7.32% to -5.08% | No |
| GeometricMonotoneWiggle | 3.234 [3.189, 3.290] | 4.607 [4.538, 4.674] | -29.74% | -30.19% to -29.22% | No |
| WorstCaseDescending | 9.803 [9.663, 10.740] | 12.096 [11.929, 12.649] | -17.12% | -20.76% to -11.54% | No |
| MostlyPushDecreasing | 704.951 [695.421, 711.035] | 707.008 [701.539, 717.966] | -0.23% | -0.29% to -0.15% | Yes |

## scalar_perfect_linear

Shared controlled scalar contract; not exact historical Rust stdlib counts.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 0.291 [0.286, 0.303] | 0.456 [0.443, 0.467] | -36.22% | -36.90% to -35.38% | No |
| MonotoneConstantSize | 4.207 [4.166, 4.602] | 6.167 [6.069, 8.311] | -31.82% | -32.35% to -31.31% | No |
| MonotoneWiggle | 1.291 [1.275, 1.320] | 1.778 [1.757, 1.897] | -27.48% | -28.11% to -26.92% | No |
| RandomWiggle | 1.019 [1.003, 1.056] | 1.296 [1.272, 1.357] | -21.50% | -21.95% to -20.98% | No |
| RandomConstantSize | 1.826 [1.711, 1.866] | 2.769 [2.659, 2.864] | -35.13% | -36.12% to -33.76% | No |
| GeometricMonotoneWiggle | 1.410 [1.383, 1.427] | 1.858 [1.830, 1.884] | -23.95% | -24.27% to -23.41% | No |
| WorstCaseDescending | 3.343 [3.288, 3.472] | 4.788 [4.576, 5.087] | -30.23% | -33.34% to -28.15% | No |
| MostlyPushDecreasing | 804.775 [798.548, 811.279] | 808.031 [803.463, 817.916] | -0.51% | -0.79% to -0.27% | Yes |

## scalar_median1_binary

Shared controlled scalar contract; not exact historical Rust stdlib counts.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 0.647 [0.628, 0.667] | 1.093 [1.084, 1.109] | -41.10% | -41.80% to -40.68% | No |
| MonotoneConstantSize | 7.134 [7.022, 7.376] | 11.796 [11.712, 11.875] | -39.63% | -39.77% to -39.52% | No |
| MonotoneWiggle | 2.166 [2.097, 2.264] | 3.348 [3.302, 3.394] | -35.63% | -36.15% to -35.32% | No |
| RandomWiggle | 2.014 [1.952, 2.764] | 2.494 [2.448, 2.842] | -19.50% | -21.15% to -17.73% | Yes |
| RandomConstantSize | 5.895 [5.789, 6.128] | 6.292 [6.179, 6.733] | -6.07% | -7.52% to -4.53% | No |
| GeometricMonotoneWiggle | 2.191 [2.150, 2.374] | 3.427 [3.388, 3.501] | -35.28% | -36.29% to -33.71% | No |
| WorstCaseDescending | 7.531 [6.623, 7.782] | 9.001 [8.763, 9.187] | -18.81% | -25.96% to -14.69% | No |
| MostlyPushDecreasing | 7.639 [7.548, 7.727] | 7.585 [7.429, 7.824] | +0.67% | +0.37% to +0.85% | Yes |

## scalar_median3_binary

Shared controlled scalar contract; not exact historical Rust stdlib counts.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 1.023 [1.005, 1.033] | 1.531 [1.507, 1.556] | -33.14% | -33.63% to -32.58% | No |
| MonotoneConstantSize | 10.892 [10.731, 11.171] | 16.096 [15.858, 16.220] | -32.29% | -32.54% to -31.86% | No |
| MonotoneWiggle | 3.252 [3.210, 3.375] | 4.628 [4.558, 4.713] | -29.24% | -29.82% to -28.27% | No |
| RandomWiggle | 3.203 [3.168, 3.275] | 3.813 [3.748, 3.869] | -16.04% | -16.57% to -15.58% | No |
| RandomConstantSize | 10.249 [10.202, 10.610] | 11.226 [11.124, 11.947] | -8.15% | -9.06% to -6.66% | No |
| GeometricMonotoneWiggle | 3.307 [3.227, 3.438] | 4.652 [4.607, 4.749] | -29.19% | -29.90% to -28.76% | No |
| WorstCaseDescending | 9.861 [9.706, 11.004] | 12.456 [12.249, 12.776] | -18.53% | -22.01% to -12.75% | No |
| MostlyPushDecreasing | 11.712 [11.584, 12.251] | 12.176 [12.019, 12.464] | -3.75% | -3.97% to -3.53% | Yes |

## scalar_perfect_binary

Shared controlled scalar contract; not exact historical Rust stdlib counts.

| Workload | C++ ms [range] | Rust ms [range] | C++ excess | Excess across seeds | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| HeapSort | 0.292 [0.286, 0.299] | 0.456 [0.449, 0.482] | -36.30% | -36.94% to -35.62% | No |
| MonotoneConstantSize | 4.449 [4.374, 4.600] | 6.379 [6.238, 6.497] | -29.92% | -30.33% to -29.18% | No |
| MonotoneWiggle | 1.338 [1.323, 1.355] | 1.863 [1.835, 1.900] | -28.22% | -28.37% to -28.04% | No |
| RandomWiggle | 1.040 [1.019, 1.066] | 1.310 [1.284, 1.358] | -20.72% | -20.86% to -20.61% | No |
| RandomConstantSize | 2.127 [2.012, 2.163] | 2.841 [2.763, 2.988] | -26.22% | -28.23% to -25.04% | No |
| GeometricMonotoneWiggle | 1.449 [1.434, 1.528] | 1.916 [1.893, 1.976] | -24.54% | -25.02% to -24.18% | No |
| WorstCaseDescending | 3.496 [3.450, 3.596] | 4.980 [4.854, 5.396] | -30.90% | -34.33% to -28.44% | No |
| MostlyPushDecreasing | 3.425 [3.353, 3.548] | 5.136 [5.032, 5.423] | -33.22% | -34.02% to -32.61% | No |

## Current original-profile comparison

Original profile matches distributions and timed lifecycle, with independent random bytes. It is separate from controlled fixture timing. Dary8 remains Orx substitute; WeakHeap historical internal-work parity is not established. Scalar uses the paired C++ model; historical Rust comparison-count parity is not certified. Original reference retains legacy validation limitations; full paired output traces certify controlled runs only.

This retimes the frozen historical Rust binary built from `a9f2518d9ed452130361cfe1681c68625a68ab35+push-position-bounded-loads@64a83c655fc89e54263866c6c645e2f24bc9709a`. The current checkout record does not mean this original binary was rebuilt from it. Archived launcher metadata labels its implicit production default scalar; compiled reference uses AVX2/native tuning.

| C++ counterpart | Workload | C++ ms [range] | Rust ms [range] | C++ excess | Ranges overlap |
| --- | --- | --- | --- | --- | --- |
| quickheap | HeapSort | 0.210 [0.203, 0.226] | 0.313 [0.275, 0.321] | -32.80% | No |
| quickheap | MonotoneConstantSize | 2.470 [2.407, 2.594] | 3.015 [2.938, 3.059] | -18.09% | No |
| quickheap | MonotoneWiggle | 0.792 [0.777, 0.817] | 0.924 [0.920, 0.955] | -14.31% | No |
| quickheap | RandomWiggle | 0.614 [0.606, 0.631] | 0.876 [0.864, 0.904] | -29.91% | No |
| std_priority_queue | HeapSort | 0.503 [0.495, 0.529] | 0.477 [0.466, 0.485] | +5.38% | No |
| std_priority_queue | MonotoneConstantSize | 4.741 [4.710, 4.955] | 4.620 [4.601, 4.639] | +2.64% | No |
| std_priority_queue | MonotoneWiggle | 1.435 [1.419, 1.482] | 1.240 [1.223, 1.245] | +15.75% | No |
| std_priority_queue | RandomWiggle | 1.694 [1.685, 1.710] | 1.328 [1.321, 1.366] | +27.57% | No |
| scalar_median3_linear | HeapSort | 1.012 [1.005, 1.043] | 2.045 [1.972, 2.124] | -50.51% | No |
| scalar_median3_linear | MonotoneConstantSize | 10.944 [10.894, 11.128] | 20.529 [19.894, 23.150] | -46.69% | No |
| scalar_median3_linear | MonotoneWiggle | 3.179 [3.138, 3.252] | 5.954 [5.814, 6.837] | -46.61% | No |
| scalar_median3_linear | RandomWiggle | 3.213 [3.203, 3.238] | 4.432 [4.377, 5.165] | -27.49% | No |
| dary8 | HeapSort | 0.323 [0.319, 0.325] | 0.334 [0.332, 0.336] | -3.18% | No |
| dary8 | MonotoneConstantSize | 3.184 [3.155, 3.234] | 3.301 [3.276, 3.336] | -3.54% | No |
| dary8 | MonotoneWiggle | 0.905 [0.897, 0.909] | 0.986 [0.958, 0.999] | -8.16% | No |
| dary8 | RandomWiggle | 1.100 [1.094, 1.115] | 1.353 [1.342, 1.378] | -18.72% | No |
| weak_heap | HeapSort | 0.490 [0.486, 0.494] | 0.961 [0.950, 0.968] | -49.01% | No |
| weak_heap | MonotoneConstantSize | 4.049 [4.043, 4.290] | 9.962 [9.838, 10.321] | -59.35% | No |
| weak_heap | MonotoneWiggle | 1.438 [1.433, 1.464] | 3.110 [3.087, 3.149] | -53.77% | No |
| weak_heap | RandomWiggle | 1.841 [1.828, 1.856] | 2.984 [2.934, 3.002] | -38.31% | No |

## Scope and remaining gaps

- Full outputs and push/pop/drain counts match before controlled timing. Scalar transition/count checks retain fixed Rust reference.
- Standard heaps are different standard-library implementations. Orx is a declared substitute. WeakHeap and Dary8 count mismatches remain documented in prior investigation.
- Foreign source identity does not establish matching constructor RNG, capacity, or key translation. Their old timings stay historical.
- Graph checker has no matched Rust timer. Graphs, key/value queues, Boost families, and 39 experimental configurations were not recollected.
- Native/LTO C++ performance, other key types and sizes, and full Conan install/sanitizer matrices are outside this refresh.
- Prior C++-only fix runs found small production slowdowns on MonotoneWiggle, GeometricMonotoneWiggle, and WorstCaseDescending. Current Rust comparison does not erase those parent/candidate findings.

## Reproduction and records

[Historical full comparison](performance-comparison-2026-10-23.md) remains unchanged.

Raw evidence lives in ignored local `build/performance-report-after-fixes-3-20261023/`. A shared copy of this report requires that evidence directory for its raw-data links and regeneration command.

- [Fresh controlled seed summaries](../build/performance-report-after-fixes-3-20261023/controlled/summary.csv)
- [Fresh controlled process medians](../build/performance-report-after-fixes-3-20261023/controlled/process-medians.json)
- [Controlled command receipts](../build/performance-report-after-fixes-3-20261023/controlled/commands.jsonl)
- [Current C++ build receipt](../build/performance-report-after-fixes-3-20261023/controlled/cpp-build-receipt.json)
- [Fresh original-profile summaries](../build/performance-report-after-fixes-3-20261023/original/summary.csv)
- [Original-profile identities and completion](../build/performance-report-after-fixes-3-20261023/original/run.json)
- [Current CPU and compiler observations](../build/performance-report-after-fixes-3-20261023/environment-current.json)

- [Exact collector source used for this run](../build/performance-report-after-fixes-3-20261023/collect_after_fixes-executed.py)

Regenerate Markdown from saved data:

```sh
python3 benchmark/experiments/regression_20261023/generate_after_fixes_report.py \
  --data build/performance-report-after-fixes-3-20261023 \
  --report docs/performance-comparison-2026-10-24-after-fixes-3.md
```
