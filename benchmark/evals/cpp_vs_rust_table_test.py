#!/usr/bin/env python3
"""Verify paired C++ versus Rust intervals on synthetic compare.sh runs."""

import csv
import importlib.util
from pathlib import Path
import random
import subprocess
import sys
import tempfile

script = Path(__file__).with_name('cpp_vs_rust_table.py')
spec = importlib.util.spec_from_file_location('cpp_vs_rust_table', script)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

for count, critical in ((7, 3), (10, 10), (21, 67), (30, 151)):
    counts = module.signed_rank_counts(count)
    cumulative = sum(counts[:critical + 1])
    if not cumulative / 2 ** count <= 0.05 < (cumulative + counts[critical + 1]) / 2 ** count:
        raise AssertionError(f'n={count}: one-sided 5% critical value differs from {critical}')


def write_run(directory, workload, ratio, rounds=9):
    rng = random.Random(7)
    for process in range(rounds):
        drift = rng.uniform(0.7, 1.3)
        for seed in (1, 2, 3):
            for side, scale in (('cpp', ratio), ('rust', 1.0)):
                path = directory / f'process-{process}' / f'{workload}-i64-10-seed-{seed}.{side}.csv'
                path.parent.mkdir(parents=True, exist_ok=True)
                with path.open('w', newline='') as stream:
                    writer = csv.writer(stream)
                    writer.writerow(('workload', 'n', 'ops', 'fixture_digest', 'repeat', 'nanos'))
                    for repeat in range(3):
                        nanos = 1000 * seed * drift * scale * rng.uniform(0.995, 1.005)
                        writer.writerow((workload, 1024, 1024, f'fnv1a64:{seed:016x}', repeat, nanos))


def table(*runs):
    return subprocess.run([sys.executable, script, *runs], check=True, capture_output=True, text=True).stdout


with tempfile.TemporaryDirectory() as root:
    faster, equal = Path(root) / 'faster', Path(root) / 'equal'
    write_run(faster, 'HeapSort', 0.95)
    write_run(equal, 'HeapSort', 1.0)
    row = next(line for line in table(faster).splitlines() if line.startswith('| HeapSort |'))
    if row != '| HeapSort | -5.0% |':
        raise AssertionError(f'shared 30% drift hid a 5% paired gap: {row}')
    row = next(line for line in table(equal).splitlines() if line.startswith('| HeapSort |'))
    if not row.startswith('| HeapSort | ~'):
        raise AssertionError(f'equal sides not marked as overlapping 0: {row}')
    row = next(line for line in table(faster, equal).splitlines() if line.startswith('| HeapSort |'))
    if not row.startswith('| HeapSort | ~'):
        raise AssertionError(f'later run did not replace the cell: {row}')
print('PASS Wilcoxon critical values, paired drift cancellation, and cell replacement')
