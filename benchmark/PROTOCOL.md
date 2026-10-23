# Benchmark protocol

`quickheap-cpp-v1` defines paired experiments from C++ workload semantics. Historical source pins define registration provenance. Modified runners receive separate build identities.

## Inputs

C++ generates fixtures once. Both runners consume exact fixture bytes. Each fixture contains workload name, key type, size, and independent input streams. Monotone streams contain offsets. Other streams contain unsigned key bits, decoded before timing.

`QHB1 <elem> <workload> <n>` starts fixture. `setup <count>` precedes setup values. `timed <count>` precedes timed values. Decimal unsigned integers use whitespace separators. Reader rejects invalid fields, sizes, counts, widths, and trailing values. Digest `fnv1a64:<16 hexadecimal digits>` covers exact bytes. Digest identifies inputs and has no security role.

## Lifecycle

| Region | Work |
| --- | --- |
| Before timer | Parse fixture, validate full workload, construct fresh queue, apply initial reservation, execute setup. |
| Inside timer | Direct workload loop, queue growth, pop-to-push arithmetic, compiler observation barrier. |
| After timer | Read counters, serialize result, destroy queue and fixture. |

Validation uses separate queue state. It checks every popped key and payload, then drains retained keys. Timed execution writes no output buffer. Workload dispatch selects loop before clock starts. Queue destruction uses normal RAII outside timer in both languages.

Initial reservation defaults to backend construction and natural growth. Required fixed-capacity competitors disclose capacity. Historical Rust binary-heap reservation and iterator destruction do not define this protocol.

## Schedule

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

## Result identity

Each paired row retains protocol, source build digest, compiler/version/flags, implementation name, backend, fixture digest, reservation, timer boundary, repeats, and warmups. Historical opcode-replay rows cannot share paired identity. Standard-library and independently rewritten competitors remain declared substitutes until algorithm parity is verified.

## Milestones

1. Comparable active benchmarks. Fixture agreement, direct timing, scalar/count semantics, catalog status, graphs, verification matrix, and attributed reports pass gates.
2. Complete historical port. Dormant registrations, historical diagnostics, archived results, and paper assets retain explicit provenance and omission status.

Each gate must pass before dependent implementation starts. Matrix availability and external-source failures remain explicit verification gaps.

## Known upstream defect

Rust development SIMD build at pinned source calls unsafe `push_position` through safe caller without an unsafe block. Helper requires initialized padding beyond live pivot slice. Compiler rejects call, and adding unsafe block alone cannot prove padding. Paired fixture tools therefore compile independently of development crate. Development SIMD coverage remains a tracked gap until bounded loads or valid initialized padding establish safety.

## Scalar count semantics

Paired scalar engine uses singleton terminals, decreasing pivots, and equality routed below equal pivots. Samples draw indexed values with replacement from deterministic SplitMix64 stream. Insertion sort orders samples by key and index. Each sample comparator call counts one key comparison; index ties add no key comparisons. Perfect selection excludes oracle sort from counts.

Partition compares positions through selected index with `<=` and later positions with `<`. Each element contributes one relational comparison. Empty upper partition triggers retry and retains reconstructed input order. Search uses ordinary linear scan or C++ count-halving binary search. Every pivot probe contributes one comparison. Setup and final validation drain contribute no reported counts.

These definitions belong to C++ paired protocol. They do not reproduce Rust standard-library sorting or binary-search implementation counts. Rust paired runner implements same definitions for scalar comparisons.

## Explicit SIMD selection

`--backend native|scalar|avx2|avx512` selects Highway dispatch before queue construction. Explicit targets require compiled kernels and CPU support. AVX512 maps to Highway AVX3. Conan ties benchmark scalar option to packaged Quickheap scalar option. Independent executables build with `QUICKHEAP_BENCH_FIXED_TARGETS=ON` or Conan `fixed_targets=True`. Their backend cannot be overridden.

## Graph semantics

Graph checker contract follows C++. Dijkstra uses directed arcs, source zero, packed distance/vertex keys, and rejects distances reaching UINT32_MAX. Prim uses minimum-weight undirected conversion, packed weight/CSR edge IDs, and visits source component. CSR sorts by source, destination, weight. Counts include every actual queue call, including stale entries. `--verify-only` exposes complete distances or parents/tree edges/weight and counts outside timing. Historical Rust directed Prim uses separate identity.

Direct CMake configuration supplies `QUICKHEAP_BENCH_LIBRARY_SCALAR` from imported package metadata and must match `QUICKHEAP_SCALAR`. Conan supplies both automatically. Configuration without metadata fails before claiming backend identity. Runtime CPU selection cannot add kernels absent from packaged library.
