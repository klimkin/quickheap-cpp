# Original benchmark competitors

Enable `QUICKHEAP_BENCH_EXTERNAL` to fetch and build the original C++
competitors without Rust or FFI. Link `quickheap_bench_external` into the
benchmark runner. Configuration requires network access.

| Competitor | Source | Pinned revision |
| --- | --- | --- |
| SequenceHeap | https://github.com/raphinesse/SequenceHeap | `ddbf7507b9ddb74a80cbca368575b8a052a90bc3` |
| S3Q | https://github.com/raphinesse/s3q | `3621d1e79408164d942e9169a1ef4c5f6d695c52` |
| Original Quickheap | https://github.com/RagnarGrootKoerkamp/quickheap | `a9f2518d9ed452130361cfe1681c68625a68ab35` |

S3Q dependencies use the benchmark's original gitlink revisions. CMake fetches
ips4o, range-v3, and Xoshiro-cpp without configuring their upstream projects.
Fetched source retains its upstream copyright and license notices.

| Dependency | Upstream license |
| --- | --- |
| SequenceHeap | Academic and educational use; commercial use requires contacting Peter Sanders |
| S3Q | MIT, Raphael von der Grün |
| ips4o | BSD 2-Clause |
| range-v3 | Boost Software License 1.0 |
| Xoshiro-cpp | MIT, Ryo Suzuki |

Original Quickheap uses Rodrigo Paredes' `MinRandQH2.hpp` and `Stack.hpp`,
identified by upstream README as implementations for *Stronger Quickheaps*.
These files carry an all-rights-reserved notice. The pinned quickheap repository
contains no license grant for these files. This project fetches those files on
request and does not vendor or relicense them.

SequenceHeap and S3Q reserve the minimum and maximum values of each key type.
Adapters reject both values before insertion. Original Quickheap requires a
fixed capacity; construct its adapter with the workload's maximum live item
count. The adapter rejects capacity above `2^29` and throws if insertion fills
the buffer. External competitors accept key-only workloads.

The implementation compiles as C++17 because pinned S3Q uses unqualified
`ssize` calls that conflict with `std::ssize` in C++20 and later. The public
adapter header supports the main benchmark's C++23 build.

Configuration patches SequenceHeap's single-leaf loser-tree mask. Upstream
evaluates `1 << -1` when `logK == 0`; the patch uses zero because a tree with
one leaf has no descent. The patch checks the pinned source expression and
rejects unexpected changes.

Configuration also guards SequenceHeap merge-level exhaustion. Upstream can
recurse past its four allocated trees; the patch throws before either the
current or next tree index exceeds the array. Vendor assertions remain enabled
in assertion builds.
