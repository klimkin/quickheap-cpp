# Benchmark verification matrix

`verify.py` builds C++ benchmark packages, runs CTest through Conan recipes, and verifies installed CLIs and reports. Each selected axis forces local QuickHeap and benchmark builds. The driver resolves installed binaries from that invocation's Conan JSON graph.

The matrix keeps compiler, scalar backend, sanitizer, and optional competitors under distinct axis names.

| Axis suffix | Build | Scalar | Optional competitors |
|---|---|---|---|
| `native` | Release | No | None |
| `scalar` | Release | Yes | None |
| `asan` | Debug with address sanitizer | Yes | None |
| `ubsan` | Debug with undefined behavior sanitizer | No | None |
| `tsan` | Debug with thread sanitizer | No | None |
| `boost` | Release | No | Boost |
| `external` | Release | No | Pinned upstream C++ dependencies |

Each suffix has `gcc-` and `clang-` axes. Sanitizer axes require matching `gcc-native-*` or `clang-native-*` Conan profiles. Remaining axes use `gcc-native` or `clang-native`. Missing profiles and unavailable compiler executables produce `skipped` axes. Skips keep the aggregate result incomplete and exit nonzero.

Run these commands from repository root.

```sh
python3 benchmark/verify.py --list
python3 benchmark/verify.py --axis gcc-native --axis gcc-scalar \
    --build-profile gcc-native-build --output build/benchmark-verify
python3 benchmark/verify.py --build-profile gcc-native-build
```

`--axis` accepts repeated selections. Duplicate or unknown selections exit with code 2 before builds. Without `--axis`, the driver runs every axis. `--build-profile` defaults to `QUICKHEAP_BUILD_PROFILE`, then `gcc-native-build`. Host and build profiles remain separate. Host C++ language mode is C++20.

The driver requires Conan and compilers named by the resolved profiles. Report plotting requires matplotlib in the Python interpreter that runs `verify.py`. Failed plotting keeps the axis failed. The driver refuses QuickHeap editable registrations and never changes the editable registry.

Each run creates a fresh `run-*` directory under `--output`. Commands run sequentially. Compiler version logs, resolved profiles, Conan build graphs, command arguments, exit codes, and installed asset digests remain in that directory. `summary.json` records source revision, dirty state, source digests, selected axes, and aggregate status. Each axis has `provenance.json` and separate stdout and stderr logs.

Installed checks require these benchmark assets.

- `quickheap_bench`, `quickheap_bench_graph`, `quickheap_experiments`, and `quickheap_fixture`.
- `share/quickheap-bench/protocol.json` and `fixtures/*.qhf`.
- Installed graph fixtures, reporting scripts, `run-all.sh`, and README.

The fixture CLI inspects every installed input. `quickheap_bench --verify-only` validates every fixture with `std_priority_queue` outside timing. One fixture runs through the timed CLI. Native smoke runs cover all four integer types. Installed original-profile smoke checks fixed repeats, reservation metadata, timer identity, and original report generation. Lifecycle CTest verifies actual destruction boundaries. All five experiment modes and both graph fixtures emit valid CSV. Reporting consumes installed benchmark output and emits summary CSV, graph-weight CSV, and an SVG plot.

Conan creation also runs each package's own `test_package`. The driver copies those consumers into axis-specific directories to preserve independent artifacts. Benchmark creation must resolve the same local QuickHeap recipe revision, package ID, and package revision as the preceding QuickHeap build.

Exit code 0 means every selected axis passed. Exit code 1 means failed checks, unavailable prerequisites, or skipped axes. Compiler activation and package cache contents cannot turn unexecuted checks green.

This driver verifies package behavior. It does not compare performance across sanitizer axes or certify cross-language timing parity. Shared fixture correctness, workload dependency checks, lifecycle tests, and report provenance remain acceptance gates in their owning benchmark components.

For local environments with different profile names, select axes from one compiler family and supply `--host-profile PATH` and `--build-profile PATH`. Sanitizer axes still require actual sanitizer flags in resolved profile. Profile overrides stay in provenance.

Verification processes serialize access to shared Conan cache through a cache-owned lock. Independent runs retain separate output directories. This prevents simultaneous optional-dependency fetches from corrupting package-reference registration.
