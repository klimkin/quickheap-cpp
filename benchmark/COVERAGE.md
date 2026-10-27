# Benchmark coverage

Source catalog follows Rust `a9f2518d9ed452130361cfe1681c68625a68ab35`. Status describes implementation identity, separate from workload timing protocol. Matching operation totals does not prove algorithm parity.

Two profiles select workload policy independently of implementation identity.
`original-rust` reproduces original active workload distributions and lifecycle.
`controlled-operations` uses shared fixtures and C++ timer boundaries.
C++ original defaults omit development SIMD configurations still marked gaps.
Paired scalar and Orx substitutes retain their existing implementation names and catalog status.
Unrepaired original Rust source fails on current nightly. An explicitly attributed bounded-load repair enables execution; missing initialized FFI dependencies can still block optional configurations.

## Active catalog

| Upstream configuration | C++ counterpart | Status | Remaining gate |
| --- | --- | --- | --- |
| Production SIMD AVX2 | `quickheap` | Port; AVX2 intrinsics | `--backend avx2`, independent executable option |
| Production SIMD AVX512 | `quickheap` | Explicit `--backend avx512`, independent executable option | Execute capable host |
| Development flat SIMD AVX2/AVX512 | Experimental SIMD variants | Declared substitute | Flat parameters and exact algorithm evidence |
| Scalar median3 linear | `scalar_median3_linear` | C++ paired scalar engine | Verified shared transitions and defined counts |
| Six scalar comparison variants | `scalar_median1/median3/perfect_linear/binary` | C++ paired scalar engine | Verified126 shared transitions/counts |
| SequenceHeap | Optional `sequence_heap` | Pinned source adapter | Shared trace and required capacity attribution |
| S3Q | Optional `s3q` | Pinned source adapter | Shared trace and sentinel restrictions |
| Original Quickheap | Optional `original_quickheap` | Pinned source adapter | Shared trace and capacity policy |
| FFI radix heap | Optional `external_radix_heap` | Pinned upstream source adapter | Shared monotone fixtures |
| Standard BinaryHeap | `std_priority_queue` | Declared standard-library substitute | Same fixture and C++ protocol |
| OrxDary8 | Native `dary8` | Declared substitute | Distinct Orx implementation absent |
| WeakHeap | Native `weak_heap` | Algorithm counterpart; parity unproved | Shared trace and count semantics |
| Dev SIMD median1/3/5 per backend | Scalar/SIMD ablation | Declared substitute | Independent targets and indexed samples |
| AVX512 flat256 partitions 2/3/4 | Experimental partition variants | Declared substitute | Exact flat parameters absent |
| NEON flat128 and list112/cap128 | Highway flat/linked-block variants | Declared substitute | Native Arm host execution absent |
| Directed Dijkstra | Native graph runner | C++ semantics | Overflow contract and queue trace |
| Prim on loaded directed graph | Native undirected minimum-edge Prim | Distinct C++ graph semantics | Shared paired graph identity required |

Rust table registrations after unconditional return are dormant. Active table contains production/development AVX2 and optional AVX512 only.

## Historical omissions

| Asset | Status |
| --- | --- |
| Dormant Boost, pairing, Fibonacci, reimplemented Quickheap, extra dary registrations | Some C++ competitors exist; complete historical activation deferred |
| Spill/copy and block-arena counters | Tracked gap; C++ RAII blocks use different ownership |
| Full historical memory and layer statistics | Tracked gap |
| Attributed variant plots, LaTeX and paper reports | Implemented; historical figure layouts remain tracked gap |
| Historical results archive | Six verified source files archived; original measurement build provenance unknown |
| Graph algorithm lifetime and overflow differences | Retain distinct C++ identity |
| Rust development SIMD unsafe-call compilation | Unrepaired reference defect; declared bounded-load repair enables original-profile execution |

## Verification

`verify.py` owns compiler, backend, sanitizer, optional competitor, package, and reporting axes. Missing tools or profiles produce explicit skipped status and nonzero aggregate exit. Existing build trees can support local checks but cannot establish fresh package matrix acceptance.

`catalog.csv` records active `run.rs` development configurations individually. These remain declared substitutes because terminal, sampling, partition, and backend parameters differ. Exact Rust compiler behavior is outside C++ paired contract. Active `measure_rebal_speedup.rs` historical instrumentation remains tracked separately.
