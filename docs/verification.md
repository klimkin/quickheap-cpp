# Verification record

Port preserves production bucket algorithm and existing C/C++ declarations.
Runtime tests cover all four signed/unsigned widths, both heap variants,
random/increasing/decreasing/extreme-heavy input, vector boundaries, 100,000
entries, moves, null C arguments, and capacity retention.

| Check | Result |
| --- | --- |
| Rust baseline | 81 tests and two doctests pass |
| GCC 15 and Clang 21 | 97 GoogleTest cases pass |
| Scalar Highway | 97 cases and 20 differential traces pass |
| Native Highway | 20 Rust/Python/C++ differential traces pass |
| GCC 15 ASan, UBSan, TSan | 97 cases pass under each sanitizer |
| Static/shared Conan packages | External C and C++17 callers pass |
| Editable package | External C and C++17 callers pass |
| Benchmark smoke | 64 cases execute with validated outputs |
| Independent review | Correctness and package reviewers report no remaining defect |
| GCC 17 and Clang 24 | Matrix running in requested Conan home |

Independent partition probes check signed extrema, duplicates, payload
association, and output guards for lengths 17 through 513. Pivot probes check
lengths zero through 257, including binary lookup above 64 pivots. Both scalar
and AVX2 probes pass. Host lacks AVX-512 and Arm execution. Highway compiles
available x86 targets, but those platforms need native runtime validation.

No performance comparison is claimed. Smoke timings verify execution only.
Benchmark replay includes script traversal, output recording, and payload
checks, so timings measure that workload driver plus heap operations.
