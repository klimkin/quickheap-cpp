# Quickheap C++

Pure C++ port of [quickheap](https://github.com/RagnarGrootKoerkamp/quickheap).
Descending pivots divide reusable buckets. Pop partitions oversized minimum
buckets, then sorts at most 16 entries. The algorithm mirrors upstream Rust
`SimdQuickHeap` step for step. x86-64 builds use AVX2 intrinsics and require an
AVX2 CPU. `-o quickheap/*:scalar=True` builds portable scalar kernels.

## Build and test

Requires Conan 2, CMake 4, Ninja, and a C++20 compiler. Public C++ headers
support C++17 consumers. Conan profiles select build standard. Conan supplies
Catch2 3.16.0 for library tests.

If no Conan profile is configured, detect the installed compiler first.

```sh
conan profile detect
```

Build with the detected profile or substitute your own host and build profiles.

```sh
conan build . -pr:h default -pr:b default -s:h compiler.cppstd=20 --build=missing
conan create . -pr:h default -pr:b default -s:h compiler.cppstd=20 --build=missing
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
