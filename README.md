# Quickheap C++

Pure C++ port of [quickheap](https://github.com/RagnarGrootKoerkamp/quickheap).
Descending pivots divide reusable buckets. Pop partitions oversized minimum
buckets, then sorts at most 16 entries. The algorithm mirrors upstream Rust
`SimdQuickHeap` step for step. x86-64 builds use AVX2 intrinsics and require an
AVX2 CPU. `-o quickheap/*:scalar=True` builds portable scalar kernels.

## Build and test

Requires Conan 2, CMake 4, Ninja, and a C++20 compiler. Public C++ headers
support C++17 consumers. Conan profiles select build standard. Conan supplies
Catch2 3.16 or newer for library tests.

If no Conan profile is configured, detect the installed compiler first.

```sh
conan profile detect
```

Build with the detected profile or substitute your own host and build profiles.

```sh
conan build . --build=missing
conan create . --build=missing
```

Tests run by default. Add `-c tools.build:skip_test=True` to skip execution.

Conan profiles select compilers, linkers, compiler caches, and sanitizer flags.
Use separate host and build profiles when cross-compiling. Set `CONAN_HOME`
before invoking Conan to select another cache.

## Use C++

```cpp
#include <quickheap.hpp>

quickheap::Heap<std::uint64_t> queue;
queue.push(7);
queue.push(1);
auto minimum = queue.try_pop();  // 1

quickheap::KvSimdQuickHeap<std::int64_t> events;
events.push(-4, 42);
auto event = events.try_pop();   // pair(-4, 42)
```

Both heaps support `int32_t`, `uint32_t`, `int64_t`, and `uint64_t`.
Duplicate keys are valid. Equal-key payload order is unspecified. Heaps are
move-only. After moving, destroy or assign the source before using it.
`pop()` requires a nonempty heap. `try_pop()` returns `std::nullopt` when empty.

## Use C

```c
#include <quickheap.h>

QuickHeapU64 *queue = quickheap_u64_new();
quickheap_u64_push(queue, 7);
uint64_t value;
bool found = quickheap_u64_pop(queue, &value);
quickheap_u64_free(queue);
```

Existing `quickheap_<type>_*` declarations match the Rust headers. Empty pop
returns false and leaves output untouched. Null arguments and allocation
failures abort. Capacity counts allocated element slots across buckets,
including retained empty buckets. Key/value capacity sums both columns.

## Link with CMake

```cmake
find_package(quickheap REQUIRED CONFIG)
target_link_libraries(app PRIVATE quickheap::quickheap)
```

Conan's CMakeDeps generator provides `quickheap::quickheap` and propagates
dependencies for static and shared packages. Consumers require only Quickheap.
Public headers do not expose SIMD types. Installed headers and libraries support
Conan editable packages.

`quickheap/detail/` holds the header-only heap that the library wraps. The
benchmarks include it directly so heap calls inline, as the Rust harness does.
These headers require C++20, select AVX2 kernels only when compiled with AVX2
enabled, and are not a stable API.

## Performance

![Runtime per element divided by lg n for i32 and i64 keys](docs/img/algorithms.svg)

Each heap holds `n` keys and repeats `10n` times: pop the minimum, then push
it back plus a random offset (`MonotoneConstantSize`). The plot shows `i32`
and `i64` keys and divides time per pair by $\log_2 n$, so a comparison heap
with constant per-level cost draws a flat line. Each point is the median of
three separate passes over all sizes. The figure follows the layout of the
upstream Rust README.

The table gives nanoseconds per push and pop pair for `u64` keys, the median
of 3 trials. LLC misses counts `PERF_COUNT_HW_CACHE_MISSES` per pair at
$2^{25}$. A `*` marks a single trial: each of those runs takes 10 to 20
minutes. The top half lists the plotted heaps; the bottom half lists other
implementations.

| Heap | Algorithm | $2^{10}$ | $2^{15}$ | $2^{20}$ | $2^{25}$ | LLC misses |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `std_priority_queue` | `std::priority_queue` binary heap | 34.4 | 51.8 | 82.0 | 180.6 | 28.244 |
| `dary8` | 8-ary heap, hole sift | 15.1 | 17.6 | 41.6 | 254.2 | 7.739 |
| `weak_heap` | Weak heap | 18.4 | 27.8 | 69.5 | 425.6 | 14.389 |
| `radix_heap` | Radix heap, native | 25.4 | 30.1 | 33.9 | 40.6 | 0.857 |
| `sequence_heap` | Sanders' sequence heap | 28.0 | 45.1 | 63.3 | 80.7 | 0.332 |
| `s3q` | S3Q sample-sort queue | 18.5 | 19.5 | 23.7 | 32.6 | 1.102 |
| `original_quickheap` | Original quickheap, Paredes & Navarro | 40.4 | 48.5 | 59.3 | 72.1 | 0.638 |
| `scalar_quickheap` | Scalar quickheap | 35.6 | 48.5 | 61.8 | 78.2 | 1.197 |
| `quickheap` | SimdQuickHeap, AVX2 | 15.5 | 16.7 | 19.6 | 24.3 | 1.021 |
| | | | | | | |
| `custom_binary_heap` | Binary heap, swap sift | 18.2 | 28.2 | 86.1 | 595.5 | 15.826 |
| `dary4` | 4-ary heap, hole sift | 11.1 | 21.2 | 39.5 | 284.7 | 8.055 |
| `dary16` | 16-ary heap, hole sift | 18.2 | 26.0 | 45.1 | 221.5 | 9.531 |
| `custom_dary4` | 4-ary heap, swap sift | 13.4 | 24.0 | 62.4 | 339.2 | 11.214 |
| `custom_dary8` | 8-ary heap, swap sift | 17.9 | 22.8 | 57.9 | 271.6 | 10.281 |
| `boost_dary4` | Boost 4-ary heap | 16.1 | 27.9 | 56.5 | 343.8 | 13.093 |
| `external_radix_heap` | Radix heap, `iwiwi/radix-heap` | 34.7 | 36.4 | 39.7 | 43.1 | 0.519 |
| `blocked_quickheap` | Quickheap, linked 128-slot blocks | 65.6 | 85.4 | 106.5 | 153.0 | 6.056 |
| `boost_fibonacci` | Boost Fibonacci heap | 60.5 | 118.3 | 495.1 | 1581.2\* | 62.352\* |
| `boost_pairing` | Boost pairing heap | 123.5 | 218.0 | 807.3 | 2917.9\* | 101.229\* |
| `boost_binomial` | Boost binomial heap | 246.8 | 382.9 | 857.0 | 2075.3\* | 62.431\* |
| `boost_skew` | Boost skew heap | 56.9 | 126.2 | 486.5 | 2491.0\* | 68.490\* |

Notes on the table:

- Radix heaps require monotone keys. This workload satisfies that; general
  workloads do not.
- The Rust README reports an "L2 misses" column measured on AMD. Its counter
  differs, so the miss columns are not comparable.

Data types at $n = 2^{20}$, median of 5 trials:

| Data type | Heap | ns/pair |
| --- | --- | ---: |
| `u32` | `quickheap` | 16.1 |
| `u64` | `quickheap` | 19.7 |
| `(u32,u32)` | `kv_quickheap` | 27.6 |
| `(u64,u64)` | `kv_quickheap` | 35.1 |

Measured 2026-10-29 on an Intel Core Ultra 9 285K, pinned to P-core 5, with
Clang 24 `-O3 -mavx2 -mbmi -mpopcnt`. The host was otherwise idle.
[benchmark/ALGORITHMS.md](benchmark/ALGORITHMS.md) reproduces the plot and
tables. [docs/cpp-vs-rust.md](docs/cpp-vs-rust.md) compares this port with
the Rust crate on the same algorithm.

## Run benchmarks

Local performance checks use Catch2 benchmarks in `test/`. A normal test
build creates `quickheap_benchmark`. Run benchmarks explicitly.

```sh
build/Release/test/quickheap_benchmark '[!benchmark]'
build/Release/test/quickheap_benchmark '[!benchmark]' --section 1024 \
    --benchmark-samples 1 --benchmark-warmup-time 0 --benchmark-no-analysis
```

Eight workloads cover signed and unsigned 32-bit and 64-bit keys with
key-only and key/value Quickheap. Catch2 controls calibration, warmup, and
samples. Input generation, setup, output validation, and destruction stay
outside timing. Each calibrated iteration uses fresh queue and output state.
Benchmark tests use hidden `[!benchmark]` tags and run separately from CTest
correctness checks. `BUILD_TESTING=OFF` excludes local test targets.

Algorithm and configuration comparisons use the separate Conan application
under `benchmark/`. Build Quickheap with `conan create .`, then follow
[comparison build and run instructions](benchmark/README.md). The main
package builds and tests without the `benchmark/` directory.

## Verify the port

Conan build and package creation run correctness checks by default. Tests cover
all integer types, both heap variants, C callers, and external C/C++ consumers.
Differential checks compare deterministic traces with Rust and Python's
min-heap oracle.

```sh
python3 scripts/differential.py build/Release/test/quickheap_trace
```

Rust is needed only for this comparison. See
[verification instructions](docs/verification.md) for package configurations
and platform coverage.
