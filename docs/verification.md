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
| Independent review | Correctness and package reviewers report no remaining defect; [decision trail](../.audit/port.tsv) records verdicts |
| GCC 17 and Clang 24 | Native/scalar tests, differential traces, static/shared and editable consumers pass |
| GCC 17 and Clang 24 ASan/UBSan/TSan | Local tests and packaged C/C++17 consumers pass |
| Conan CMakeDeps | External C and C++17 callers require only Quickheap |
| Natural-owner matrix | All 20 axes pass with fresh build trees, including 12 static/shared sanitizer axes |
| Full-feature lockfile | Regenerated graph matches committed lockfile |
| Compiler cache | Central profiles set C and C++ ccache launchers for both compiler families |

Independent reviewer reports additional scalar and AVX2 partition and pivot
probes. Those probes were temporary review artifacts, not committed regression
checks. Public behavior tests and differential scripts provide repeatable
verification. Host lacks AVX-512 and Arm execution. Highway compiles
available x86 targets, but those platforms need native runtime validation.

No performance comparison is claimed. Smoke timings verify execution only.
Benchmark replay includes script traversal, output recording, and payload
checks, so timings measure that workload driver plus heap operations.

Highway 1.4.0 emitted an upstream CMake policy deprecation warning. Earlier
GoogleTest 1.17.0 build emitted a Clang 24 character-conversion warning.
Retained GoogleTest 1.18.0 build log contains no compiler warning.
Quickheap compiles with `-Wall -Wextra -Werror` without project warnings.
Benchmark smoke reports CPU scaling warnings. Full warning-free acceptance
remains unmet because of upstream and measurement-environment warnings.

Natural-owner refit uses central Conan profiles for compilers, linkers,
ccache, and sanitizers. CMake target construction follows EBI source/test
subdirectories. Conan CMakeDeps owns dependency propagation for both static
and shared packages. See [refit decision trail](../.audit/refit.tsv) and
[independent closure review](../.audit/refit-review.md).
