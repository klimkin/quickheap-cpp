# Verification

Conan build and package creation run correctness tests by default. Use a host
profile for the target compiler and a build profile for tools. Host C++ language
mode must be C++20.

## Check library configurations

Run commands from repository root. Replace `default` with your profile names or
paths. Configure profiles as described in [build instructions](../README.md).

```sh
conan build . -pr:h default -pr:b default -s:h compiler.cppstd=20 --build=missing
conan create . -pr:h default -pr:b default -s:h compiler.cppstd=20 --build=missing
conan create . -pr:h default -pr:b default -s:h compiler.cppstd=20 \
    -o:h 'quickheap/*:shared=True' --build=missing
conan build . -pr:h default -pr:b default -s:h compiler.cppstd=20 \
    -s:h build_type=Debug -o:h 'quickheap/*:scalar=True' --build=missing
```

Repeat with GCC and Clang host profiles. To check ASan, UBSan, or TSan, use a
host profile with the corresponding sanitizer flags. Test static and shared
packages separately. Run editable package operations sequentially within one
Conan cache.

## Correctness coverage

| Check | Coverage |
| --- | --- |
| Library tests | Signed and unsigned 32-bit and 64-bit keys, key-only and key/value heaps, duplicates, extrema, vector boundaries, moves, and capacity retention |
| C API tests | C declarations, empty pops, null arguments, and abort behavior |
| Allocation retry | Queue state remains valid after allocation failure and retry |
| Package consumers | External C and C++ callers link against static and shared packages |
| Differential traces | Deterministic traces agree with Rust and Python's min-heap oracle |
| Benchmark tests | Shared fixtures, workload dependencies, timer boundaries, graph results, and report attribution |

Run differential checks against a built trace executable.

```sh
python3 scripts/differential.py build/Release/test/quickheap_trace
```

Rust is required only for differential checks. Native and scalar Highway
configurations require separate builds.

## Benchmark verification

The separate benchmark package checks installed executables, fixtures, optional
competitors, and reporting tools. Follow
[benchmark verification instructions](../benchmark/verification.md) to select
compiler, backend, sanitizer, and optional-dependency axes.

[Protocol](../benchmark/PROTOCOL.md) defines timing and attribution.
[Coverage catalog](../benchmark/COVERAGE.md) identifies ports, substitutes, and
remaining gaps. Correctness checks and benchmark smoke runs establish execution;
they do not establish a performance advantage.

## Platform coverage

Linux GCC and Clang, native and scalar Highway, sanitizers, and external package
consumers have verification coverage. Native Windows, Arm, and AVX-512 execution
remain unverified. Benchmark measurement requires the CPU-affinity prerequisites
documented in the benchmark package.
