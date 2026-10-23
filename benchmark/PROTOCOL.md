# Benchmark protocol

Two profiles answer different performance questions. `--profile controlled-operations` remains default.

| Profile | Protocol | Comparison |
| --- | --- | --- |
| `original-rust` | `quickheap-original-rust-a9f2518-v1` | Original Rust workload distributions, reservation, timed lifecycle, schedule, and cutoffs. |
| `controlled-operations` | `quickheap-cpp-v1` | Queue operations on identical shared fixture bytes, with preparation and destruction outside timing. |

Each result retains profile and protocol. Reports reject mixed profiles. Neither profile establishes algorithm equivalence for catalog entries marked substitutes or gaps.

## Original Rust compatibility

Reference revision is `a9f2518d9ed452130361cfe1681c68625a68ab35`. Rust profile launcher invokes unchanged original `bench` example. C++ uses workload-specific loops with matching ownership and schedule policies. C++ validates a separate fresh input before each size/workload warmup; measured trials do not replay validation on their inputs. Checked monotone conversions remain inside original timed loops. Original Rust heaps, workloads, and timer remain authoritative.

| Region | Work |
| --- | --- |
| Before timer | Generate fresh inputs, construct queue, reserve initial capacity, execute constant-size setup. |
| Inside timer | Execute workload, release consumed input storage, observe final queue, destroy queue. HeapSort releases consumed key storage before its pop loop. |
| After timer | Serialize result and compute median for cutoff. |

Standard binary heap reserves `2^20` elements. Foreign original Quickheap reserves `2^28`. Other queues use backend defaults. Every size uses one warmup and three measured repeats with fresh random inputs.

HeapSort samples full-width integer keys and replaces both extrema with adjacent values. Monotone workloads sample offsets in `[0,2^24)` for i32 and `[0,2^48)` for i64. RandomWiggle samples keys from same bounded ranges. C++ uses its own random generator. Original Rust uses unseeded `fastrand::Rng::new()` and exports no keys, offsets, or seed. Inputs therefore match distributions, not bytes. Rust fixture digest stays empty. C++ input digest identifies its own generated trial only.

Traversal follows element type, upstream heap registration order, size, then active workload order. Sizes span exponents 10 through 25. After median exceeds 32 ns per `ops * log2(n)`, larger sizes skip for that heap and workload. Table uses i64, MonotoneConstantSize, exponent step 5, and cutoff 128. Original table returns after SIMD registrations. C++ exposes available production counterpart; missing development variants remain catalog gaps.

C++ heap and workload filters select attributed subsets of active registrations. Original Rust launcher accepts original size, width, table, and build-feature controls. It rejects controlled fixture and trial controls. Compatibility timing excludes historical diagnostics modes, which retain their existing entrypoints.

Unrepaired reference source contains an unsafe SIMD call that fails on current nightly. Launcher returns that build failure. A declared bounded-load repair permits original-profile execution and retains its repair revision alongside the reference revision. Results from that source describe the repaired reference. Launcher never substitutes the controlled runner.

## Controlled operations

`quickheap-cpp-v1` defines paired experiments from C++ workload semantics. Historical source pins define registration provenance. Modified runners receive separate build identities.

### Inputs

C++ generates fixtures once. Both runners consume exact fixture bytes. Each fixture contains workload name, key type, size, and independent input streams. Monotone streams contain offsets. Other streams contain unsigned key bits, decoded before timing.

`QHB1 <elem> <workload> <n>` starts fixture. `setup <count>` precedes setup values. `timed <count>` precedes timed values. Decimal unsigned integers use whitespace separators. Reader rejects invalid fields, sizes, counts, widths, and trailing values. Digest `fnv1a64:<16 hexadecimal digits>` covers exact bytes. Digest identifies inputs and has no security role.
Fixture generator accepts `--seed` as unsigned 64-bit decimal. Default seed
1363692610 preserves existing deterministic streams. Each selected seed yields
independently attributed fixture bytes; seed selection changes no timing policy.

### Lifecycle

| Region | Work |
| --- | --- |
| Before timer | Parse fixture, validate full workload, construct fresh queue, apply initial reservation, execute setup. |
| Inside timer | Direct workload loop, queue growth, pop-to-push arithmetic, compiler observation barrier. |
| After timer | Read counters, serialize result, destroy queue and fixture. |

Validation uses separate queue state. It checks every popped key and payload, then drains retained keys. Timed execution writes no output buffer. Workload dispatch selects loop before clock starts. Queue destruction uses normal RAII outside timer in both languages.

Initial reservation defaults to backend construction and natural growth. Required fixed-capacity competitors disclose capacity. Historical Rust binary-heap reservation and iterator destruction do not define this protocol.

### Schedule

Defaults select active workloads `HeapSort`, `MonotoneConstantSize`, `MonotoneWiggle`, and `RandomWiggle`. Explicit selection may include dormant workloads. Sizes span powers 10 through 25 with exponent step 1. Trials use one warmup and three measured repeats. Cutoff is disabled to preserve identical size sets. Explicit runner options remain result provenance.

`ops` counts push/pop pairs. Actual queue calls equal `2 * ops`. `opslogn` equals `ops * log2(n)`. Comparison normalization applies workload factor once.

| Workload | Setup values | Timed values | Pair factor |
| --- | --- | --- | --- |
| HeapSort | 0 | n keys | 1 |
| MonotoneConstantSize | 2(n+50) offsets | 10n offsets | 10 |
| RandomConstantSize | 2(n+50) keys | 10n keys | 10 |
| MonotoneWiggle | 0 | 3n offsets | 3 |
| GeometricMonotoneWiggle | 0 | 3n offsets | 3 |
| RandomWiggle | 0 | 3n keys | 3 |
| WorstCaseDescending | 0 | 9n keys | 9 |
| MostlyPushDecreasing | 0 | 10n keys | 10 |

MonotoneConstantSize setup repeats push, pop, push `n+50` times from initial last key zero. Each dependent push adds next offset to actual popped key. Wiggle grows with push, pop, push `n` times, then shrinks with pop, push, pop `n` times. Shrinking round's second pop does not update next round's last key.

### CPU and process identity

Comparison launcher and verification matrix run measured subprocesses through
`taskset -c` on one allowed online P core. Kernel `cpu_core/cpus` proves core
class. Frequency is not proof. Child affinity probe must observe exactly the
selected CPU. `BENCH_CPU` selects CPU or defaults to first allowed P core.
Missing kernel proof and E-core selections fail. Compiler builds remain parallel.

`RUNS` defaults to seven independent comparison process runs. Runner order
alternates per process. Original runner retains exactly three repeats and one
warmup per invocation. Each invocation gets separate validated report. Aggregate
report contains median and range of process medians, attributed report manifests,
CPU metadata, and observed process count. Original cutoffs may leave incomplete
larger groups; counts expose this. Controlled missing process groups or a missing
language fail aggregation. Reports never relabel original trial indices.

### Result identity

Each paired row retains protocol, source build digest, compiler/version/flags, implementation name, backend, fixture digest, reservation, timer boundary, repeats, and warmups. Historical opcode-replay rows cannot share paired identity. Standard-library and independently rewritten competitors remain declared substitutes until algorithm parity is verified.

## Milestones

1. Comparable active benchmarks. Fixture agreement, direct timing, scalar/count semantics, catalog status, graphs, verification matrix, and attributed reports pass gates.
2. Complete historical port. Dormant registrations, historical diagnostics, archived results, and paper assets retain explicit provenance and omission status.

Each gate must pass before dependent implementation starts. Matrix availability and external-source failures remain explicit verification gaps.

## Known upstream defect

Unrepaired Rust source calls unsafe `push_position` through a safe caller and requires initialized padding beyond the live pivot slice. Compiler rejects the call; adding an unsafe block alone cannot prove padding. The declared repair uses bounded loads and enables the original runner. Paired fixture tools compile independently of the development crate. C++ development SIMD variants still retain substitute status until their algorithm parameters match upstream.

## Scalar count semantics

Paired scalar engine uses singleton terminals, decreasing pivots, and equality routed below equal pivots. Samples draw indexed values with replacement from deterministic SplitMix64 stream. Insertion sort orders samples by key and index. Each sample comparator call counts one key comparison; index ties add no key comparisons. Perfect selection excludes oracle sort from counts.

Partition compares positions through selected index with `<=` and later positions with `<`. Each element contributes one relational comparison. Empty upper partition triggers retry and retains reconstructed input order. Search uses ordinary linear scan or C++ count-halving binary search. Every pivot probe contributes one comparison. Setup and final validation drain contribute no reported counts.

These definitions belong to C++ paired protocol. They do not reproduce Rust standard-library sorting or binary-search implementation counts. Rust paired runner implements same definitions for scalar comparisons.

## Explicit SIMD selection

`--backend native|scalar|avx2|avx512` selects Highway dispatch before queue construction. Explicit targets require compiled kernels and CPU support. AVX512 maps to Highway AVX3. Conan ties benchmark scalar option to packaged Quickheap scalar option. Independent executables build with `QUICKHEAP_BENCH_FIXED_TARGETS=ON` or Conan `fixed_targets=True`. Their backend cannot be overridden.

## Graph semantics

Graph checker contract follows C++. Dijkstra uses directed arcs, source zero, packed distance/vertex keys, and rejects distances reaching UINT32_MAX. Prim uses minimum-weight undirected conversion, packed weight/CSR edge IDs, and visits source component. CSR sorts by source, destination, weight. Counts include every actual queue call, including stale entries. `--verify-only` exposes complete distances or parents/tree edges/weight and counts outside timing. Historical Rust directed Prim uses separate identity.

Direct CMake configuration supplies `QUICKHEAP_BENCH_LIBRARY_SCALAR` from imported package metadata and must match `QUICKHEAP_SCALAR`. Conan supplies both automatically. Configuration without metadata fails before claiming backend identity. Runtime CPU selection cannot add kernels absent from packaged library.
