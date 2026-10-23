# Generate attributed benchmark reports

Run `report.py` on complete paired CSVs. Supply fixture files used by both runners.

```sh
python3 benchmark/evals/report.py cpp.csv rust.csv \
    --fixtures benchmark/fixtures --output results/report
```

Expect `PASS` and generated `summary.csv`, `report.tex`, `paper.md`, and `manifest.json`.
`--plot` adds SVG and PDF plots per metric and requires matplotlib.

For generated workloads, export each input through `quickheap_fixture` and run both benchmarks with `--fixture`.
Pass directory containing exported `.qhf` files through `--fixtures`.

Each group retains language, full compiler build, protocol, backend, fixture digest, heap, workload, reservation, timer, repeats, and warmup.
Different C++ compiler builds remain distinct. Short build IDs label tables and plots. Manifest retains complete build descriptions.
`summary.csv` records units beside values. Timing and comparison metrics divide by `ops * log2(n)` once.
`ops` counts push/pop pairs and includes workload factor. Raw storage and diagnostic metrics retain original units.
`n=1` cannot use `opslogn` normalization and fails explicitly.

Report rejects historical rows, absent attribution, corrupted fixture identities, duplicate or missing repeats, and inconsistent schedules or policies.
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
