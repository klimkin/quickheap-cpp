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
| Installed CMake config | External C and C++17 callers pass |

Independent reviewer reports additional scalar and AVX2 partition and pivot
probes. Those probes were temporary review artifacts, not committed regression
checks. Public behavior tests and differential scripts provide repeatable
verification. Host lacks AVX-512 and Arm execution. Highway compiles
available x86 targets, but those platforms need native runtime validation.

No performance comparison is claimed. Smoke timings verify execution only.
Benchmark replay includes script traversal, output recording, and payload
checks, so timings measure that workload driver plus heap operations.

Highway 1.4.0 emits an upstream CMake policy deprecation warning. GoogleTest
1.18.0 emits a Clang 24 character-conversion warning during its own build.
Quickheap compiles with `-Wall -Wextra -Werror` without project warnings.

Long matrix processes started before `verify.sh` cleanup edits completed their
checks, then failed while reading changed script offsets. Final driver reruns
exit successfully and remove their temporary consumers. The GCC driver rerun filters to C ABI smoke because its
full TSan suite already passed. These reruns check driver completion separately
from the full suite results above.
