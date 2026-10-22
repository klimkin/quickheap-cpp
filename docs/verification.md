# Verification record

Catch2 3.16.0 replaces GoogleTest and Google Benchmark. GCC 15 verification
preserves all 97 unit cases and 28 null-pointer abort operations. Native
Release and scalar Debug tests pass. All 360 benchmark configurations pass
output, payload, and size validation; CTest selects 64 measurements at size
1024. Native and scalar builds each pass 20 Rust/C++ differential traces.
Static and shared Conan packages pass external C/C++ consumer tests.

Negative control replaces child operations with a process that returns zero.
All 28 abort assertions fail, confirming that normal exit cannot pass.
Windows launcher uses a space-free child program name and searches PATH.
Windows runtime checks remain unverified on this Linux host.

Catch2 reports time per replay. Independent queue/output state per measured
run keeps setup, validation, and destruction outside timing. Batch memory
scales with Catch2's calibrated iteration count. Smoke runs verify execution;
no performance comparison is claimed.

Conan lockfile pins Catch2 3.16.0 and available Highway 1.4.0 recipe revision
`96e316cc0703d31a89fa299e24228050`. Previous Highway recipe revision is absent
from Conan Center. Build-tool versions remain pinned. Current host lacks
documented native profiles and Clang, so these checks use an isolated GCC 15
profile. Earlier verification records follow.

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
| Static/shared Conan packages | External C and C++ callers pass |
| Editable package | External C and C++ callers pass |
| Benchmark smoke | 64 cases execute with validated outputs |
| Independent review | Correctness and package reviewers report no remaining defect; [decision trail](../.audit/port.tsv) records verdicts |
| GCC 17 and Clang 24 | Native/scalar tests, differential traces, static/shared and editable consumers pass |
| GCC 17 and Clang 24 ASan/UBSan/TSan | Local tests and packaged C/C++ consumers pass |
| Conan CMakeDeps | External C and C++ callers require only Quickheap |
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
