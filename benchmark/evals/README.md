# Generate attributed benchmark reports

Select one profile per report. Controlled reports require fixture files used by both runners.

```sh
python3 benchmark/evals/report.py cpp.csv rust.csv \
    --fixtures benchmark/fixtures --output results/report
```

For original-compatible results, select original profile explicitly.

```sh
python3 benchmark/evals/report.py original-cpp.csv original-rust.csv \
    --profile original-rust --output results/original
```

Original report accepts fresh random trials with matching workload distributions. It groups repeats by build, backend, heap, workload, size, and lifecycle policy. Original Rust fixture digest remains unavailable. C++ trial digests remain in copied source CSVs. Report makes no claim that original inputs match bytes.

Expect `PASS` and generated `summary.csv`, `report.tex`, `paper.md`, and `manifest.json`.
`--plot` adds SVG and PDF plots per metric and requires matplotlib.

For generated workloads, export each input through `quickheap_fixture` and run both benchmarks with `--fixture`.
Pass directory containing exported `.qhf` files through `--fixtures`.

Each group retains profile, language, full compiler build, protocol, backend, input policy, heap, workload, reservation, timer, cutoff, repeats, and warmup. Controlled groups also retain shared fixture digest.
Different C++ compiler builds remain distinct. Short build IDs label tables and plots. Manifest retains complete build descriptions.
`summary.csv` records units beside values. Timing and comparison metrics divide by `ops * log2(n)` once.
`ops` counts push/pop pairs and includes workload factor. Raw storage and diagnostic metrics retain original units.
`n=1` cannot use `opslogn` normalization and fails explicitly.

Report rejects mixed profiles, unattributed historical rows, absent build identity, corrupted controlled fixture identities, duplicate or missing repeats, and inconsistent schedules or policies.
Count-only rows omit zero placeholder timing. Unavailable metric fields remain absent.
Unattributed graph and legacy diagnostic CSVs require separate plots through `plot.py`. Those results cannot enter paired reports.

Report copies CSV inputs, referenced fixtures, protocol, source catalog, historical inventory, and generator sources into output.
Manifest records SHA256 for each copied input. Repeated generation with same inputs preserves tables and manifest bytes.
Plot export metadata may differ between matplotlib versions. Manifest records Python and matplotlib versions.

## Archive pinned historical inputs

Copy six original upstream data files without assigning benchmark build provenance.

```sh
python3 benchmark/evals/report.py cpp.csv rust.csv \
    --output results/report \
    --archive ../quickheap-rs/quickheap-dev/bench/evals/data
```

`historical.json` inventories exact source bytes at Rust `a9f2518d9ed452130361cfe1681c68625a68ab35`.
Archive verifies sizes and SHA256, copies raw bytes, and marks copies read-only.
Conflicting existing archives fail. Compiler, flags, CPU, fixture, and timer provenance remain unknown.
Archived rows cannot become paired results through this command.
Generated paper lists historical omissions and links copied source catalog.

## Verify report contract

```sh
python3 benchmark/evals/report_test.py
python3 benchmark/evals/arithmetic_test.py
```

Tests exercise arithmetic across workload factors, separate builds and backends, rejected identities, repeat integrity, literal LaTeX names, and deterministic attributed outputs.
