# Quickheap C++

Pure C++ port of [quickheap](https://github.com/RagnarGrootKoerkamp/quickheap).
Descending pivots divide reusable buckets. Pop partitions oversized minimum
buckets, then sorts at most 16 entries. Highway 1.4.0 provides runtime SIMD
dispatch and a scalar build.

## Build and test

Requires Conan 2, CMake 4, Ninja, and a C++20 compiler. Public C++ headers
require C++17. Conan supplies Highway, GoogleTest, and Google Benchmark.

```sh
conan build . -pr:h gcc-native -pr:b gcc-native-build --build=missing
conan create . -pr:h gcc-native -pr:b gcc-native-build --build=missing
```

Tests run by default. Add `-c tools.build:skip_test=True` to skip execution.

Central Conan profiles own compiler discovery, linkers, ccache, and sanitizer
flags. Install your workspace Conan configuration before building. Set
`CONAN_HOME` before invoking Conan to select its cache. Use `clang-native`
as host profile for Clang. Build profile remains `gcc-native-build`.

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
Public headers do not expose Highway. Installed headers and libraries support
Conan editable packages.

## Run benchmarks

```sh
conan build . -pr:h gcc-native -pr:b gcc-native-build --build=missing \
    -o '&:with_benchmarks=True'
build/Release/benchmark/quickheap_benchmark --benchmark_filter=MonotoneConstantSize
```

Eight Rust workload ports cover heapsort, constant-size queues, wiggle,
descending input, and push-heavy queues. Cases compare key and key/value
quickheap with `std::priority_queue` for unsigned 32-bit and 64-bit keys.
Inputs use fixed seeds. Construction, setup, output validation, and destruction
stay outside timed replay. Each iteration checks popped keys, payloads, and
final size. Throughput counts individual push and pop calls.

## Verify the port

```sh
scripts/verify.sh
scripts/verify.sh gcc-release-static clang-debug-scalar
python3 scripts/differential.py build/Release/test/quickheap_trace
```

Matrix checks GCC and Clang, static and shared packages, scalar Debug builds,
ASan, UBSan, TSan, and external C/C++ consumers. Full invocation also checks
editable consumption and regenerates a full-feature lockfile under `build/verify`.
Preflight rejects unknown or duplicate axes and existing Quickheap editables
before building. Full invocation requires both compilers.
Run editable operations and verification sequentially within one Conan home.
Differential checks compare deterministic traces with Rust and Python's
min-heap oracle. Rust is needed only for this comparison, never for building
or using the C++ library.

See [verification record](docs/verification.md) for executed checks and
platform coverage.
