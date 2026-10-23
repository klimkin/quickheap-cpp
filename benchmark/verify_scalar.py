#!/usr/bin/env python3
"""Compare scalar bucket transitions and counts under shared fixtures."""

import argparse
import csv
import io
import re
from pathlib import Path
import subprocess

HEAPS = tuple(f'scalar_{pivot}_{search}' for pivot in ('median1', 'median3', 'perfect')
              for search in ('linear', 'binary'))


def output(binary, fixture, heap, *flags):
    return subprocess.run([str(binary), '--fixture', str(fixture), '--heap', heap, *flags],
                          text=True, capture_output=True, check=True).stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cpp-bench', type=Path, required=True)
    parser.add_argument('--rust-bench', type=Path, required=True)
    parser.add_argument('--fixtures', type=Path, default=Path(__file__).parent / 'fixtures')
    args = parser.parse_args()
    fixtures = sorted(args.fixtures.glob('*.qhf'))
    if not fixtures:
        raise ValueError('no scalar fixtures')
    for fixture in fixtures:
        tokens = fixture.read_text().split()
        elem, workload, size = tokens[1:4]
        fixture_hash = 0xcbf29ce484222325
        for byte in fixture.read_bytes():
            fixture_hash = ((fixture_hash ^ byte) * 0x100000001b3) & ((1 << 64) - 1)
        fixture_digest = f'fnv1a64:{fixture_hash:016x}'
        factor = {'HeapSort': 1, 'MonotoneConstantSize': 10, 'RandomConstantSize': 10,
                  'MonotoneWiggle': 3, 'RandomWiggle': 3, 'GeometricMonotoneWiggle': 3,
                  'WorstCaseDescending': 9, 'MostlyPushDecreasing': 10}[workload]
        for heap in HEAPS:
            cpp = output(args.cpp_bench, fixture, heap, '--scalar-trace')
            rust = output(args.rust_bench, fixture, heap, '--scalar-trace')
            if cpp != rust:
                left, right = cpp.splitlines(), rust.splitlines()
                for index in range(max(len(left), len(right))):
                    a = left[index] if index < len(left) else '<end>'
                    b = right[index] if index < len(right) else '<end>'
                    if a != b:
                        raise ValueError(f'{fixture}/{heap}: transition {index} differs\nC++ {a}\nRust {b}')
            results = [list(csv.DictReader(io.StringIO(output(binary, fixture, heap,
                    '--comparisons', '--repeat', '1', '--warmup', '0'))))
                    for binary in (args.cpp_bench, args.rust_bench)]
            if any(len(result) != 1 for result in results):
                raise ValueError(f'{fixture}/{heap}: require one result')
            rows = [result[0] for result in results]
            final_rows = [line.split(',') for line in cpp.splitlines() if line.startswith('final,')]
            if len(final_rows) != 1 or len(final_rows[0]) != 4:
                raise ValueError(f'{fixture}/{heap}: missing final count snapshot')
            for row in rows:
                expected = {'protocol': 'quickheap-cpp-v1', 'fixture_digest': fixture_digest,
                            'elem': elem, 'workload': workload, 'n': size, 'ops': str(int(size) * factor),
                            'heap': heap, 'repeat': '0', 'repeats': '1', 'warmup': '0', 'nanos': '0',
                            'reservation': 'backend-default', 'timer': 'workload-only-raii'}
                if None in row or None in row.values():
                    raise ValueError(f'{fixture}/{heap}: malformed result')
                for column, value in expected.items():
                    if row.get(column) != value:
                        raise ValueError(f'{fixture}/{heap}: incorrect {column}')
                for column in ('push_comparisons', 'pop_comparisons'):
                    if not row.get(column, '').isdigit():
                        raise ValueError(f'{fixture}/{heap}: undefined {column}')
                if (row['push_comparisons'], row['pop_comparisons']) != tuple(final_rows[0][2:]):
                    raise ValueError(f'{fixture}/{heap}: exported counts disagree with trace')
                if not re.search(r'source=(sha256:[0-9a-f]{64}|fnv1a64:[0-9a-f]{16})', row.get('build', '')):
                    raise ValueError(f'{fixture}/{heap}: missing build digest')
            for column in ('ops', 'push_comparisons', 'pop_comparisons', 'fixture_digest', 'protocol'):
                if rows[0][column] != rows[1][column]:
                    raise ValueError(f'{fixture}/{heap}: {column} differs')
            print(f'PASS {fixture.name} {heap}')
    duplicate = args.fixtures / 'HeapSort-i64-duplicates.qhf'
    if not duplicate.is_file():
        raise ValueError('missing scripted retry fixture')
    for heap in ('scalar_median1_linear', 'scalar_median1_binary'):
        flags = ('--scalar-trace', '--samples', '3,0')
        cpp = output(args.cpp_bench, duplicate, heap, *flags)
        rust = output(args.rust_bench, duplicate, heap, *flags)
        if cpp != rust or 'partition,4,3,1,0,4,,4:4:4:4' not in cpp:
            raise ValueError(f'{heap}: scripted empty-upper retry differs')
        print(f'PASS scripted retry {heap}')
    print(f'PASS scalar gate, {len(fixtures) * len(HEAPS)} matching traces and counts')


if __name__ == '__main__':
    main()
