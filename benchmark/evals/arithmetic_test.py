#!/usr/bin/env python3
"""Verify synthetic workload factors enter normalization exactly once."""

import argparse
import csv
import importlib.util
from pathlib import Path
import tempfile

spec = importlib.util.spec_from_file_location('benchmark_plot', Path(__file__).with_name('plot.py'))
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / 'arithmetic.csv'
    with path.open('w', newline='') as stream:
        writer = csv.writer(stream)
        writer.writerow(('elem', 'heap', 'n', 'workload', 'ops', 'repeat', 'nanos',
                         'push_comparisons', 'pop_comparisons'))
        for workload, factor in (('HeapSort', 1), ('MonotoneConstantSize', 10),
                                 ('MonotoneWiggle', 3), ('RandomWiggle', 3),
                                 ('WorstCaseDescending', 9), ('MostlyPushDecreasing', 10)):
            writer.writerow(('i64', 'fixture', 8, workload, 8 * factor, 0,
                             8 * factor * 3 * 7, 8 * factor * 3 * 2, 8 * factor * 3 * 5))
    for metric in ('nanos', 'comparisons'):
        args = argparse.Namespace(metric=metric, normalize='opslogn', heap=None, workload=None, elem=None)
        values = [sample.value for sample in module.read_samples(path, args)]
        if values != [7.0] * 6:
            raise AssertionError(f'{metric}: workload factor applied incorrectly: {values}')
print('PASS opslogn arithmetic across six workload factors and two metrics')
