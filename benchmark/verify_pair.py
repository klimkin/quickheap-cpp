#!/usr/bin/env python3
"""Check fixture agreement before accepting paired benchmark results."""

import argparse
import csv
import io
import re
from pathlib import Path
import subprocess
import os

from cpu import pinned_command, select_cpu


def run(command):
    command = pinned_command(command, select_cpu(os.environ.get("BENCH_CPU", "auto"))["cpu"])
    return subprocess.run(command, check=True, text=True, capture_output=True).stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cpp-fixture', type=Path, required=True)
    parser.add_argument('--rust-fixture', type=Path, required=True)
    parser.add_argument('--fixtures', type=Path, default=Path(__file__).parent / 'fixtures')
    parser.add_argument('--cpp-bench', type=Path)
    parser.add_argument('--rust-bench', type=Path)
    args = parser.parse_args()
    if bool(args.cpp_bench) != bool(args.rust_bench):
        parser.error('--cpp-bench and --rust-bench must be supplied together')
    fixtures = sorted(args.fixtures.glob('*.qhf'))
    if not fixtures:
        raise ValueError('no fixtures found')
    for fixture in fixtures:
        cpp = run([str(args.cpp_fixture), '--inspect', str(fixture), '--dump'])
        rust = run([str(args.rust_fixture), '--inspect', str(fixture), '--dump'])
        if cpp != rust:
            raise ValueError(f'{fixture}: decoded input differs\nC++ {cpp}\nRust {rust}')
        metadata = next(csv.DictReader(io.StringIO('\n'.join(cpp.splitlines()[:2]))))
        if metadata['protocol'] != 'quickheap-cpp-v1' or not metadata['fixture_digest']:
            raise ValueError(f'{fixture}: missing fixture identity')
        print(f'PASS {fixture.name} {metadata["fixture_digest"]}')
        if args.cpp_bench:
            cpp_trace = run([str(args.cpp_bench), '--fixture', str(fixture), '--heap',
                             'std_priority_queue', '--profile', 'controlled-operations', '--verify-only'])
            rust_trace = run([str(args.rust_bench), '--fixture', str(fixture), '--heap',
                              'binary_heap', '--profile', 'controlled-operations', '--verify-only'])
            if cpp_trace != rust_trace:
                raise ValueError(f'{fixture}: full correctness trace differs')
            rows = []
            for binary, heap in ((args.cpp_bench, 'std_priority_queue'),
                                 (args.rust_bench, 'binary_heap')):
                output = run([str(binary), '--fixture', str(fixture), '--heap', heap,
                              '--profile', 'controlled-operations', '--repeat', '1', '--warmup', '0'])
                result = list(csv.DictReader(io.StringIO(output)))
                if len(result) != 1 or None in result[0] or None in result[0].values():
                    raise ValueError(f'{binary}: malformed paired result')
                row = result[0]
                for column in ('protocol', 'build', 'backend', 'fixture_digest', 'reservation',
                               'timer', 'repeats', 'warmup'):
                    if not row.get(column):
                        raise ValueError(f'{binary}: missing {column}')
                rows.append(row)
            for row in rows:
                if (row.get('profile'), row.get('input_policy'), row.get('cutoff')) != (
                        'controlled-operations', 'shared-fixture', 'disabled'):
                    raise ValueError(f'{fixture}: explicit controlled profile identity differs')
                for column in ('protocol', 'elem', 'workload', 'n', 'fixture_digest'):
                    if row[column] != metadata[column]:
                        raise ValueError(f'{fixture}: result does not identify decoded {column}')
                if row['ops'] != metadata['timed_count']:
                    raise ValueError(f'{fixture}: result operation count differs')
                if row['timer'] != 'workload-only-raii' or row['reservation'] != 'backend-default':
                    raise ValueError(f'{fixture}: result lifecycle or reservation differs')
                if row['repeats'] != '1' or row['warmup'] != '0' or row['repeat'] != '0':
                    raise ValueError(f'{fixture}: result trial identity differs')
                if not row.get('nanos', '').isdigit() or int(row['nanos']) <= 0:
                    raise ValueError(f'{fixture}: invalid elapsed time')
            if rows[0]['heap'] != 'std_priority_queue' or rows[1]['heap'] != 'binary_heap':
                raise ValueError(f'{fixture}: unexpected standard-library substitute')
            if rows[0]['backend'] != 'scalar' or rows[1]['backend'] != 'rust-std-substitute':
                raise ValueError(f'{fixture}: unexpected backend')
            if (not re.search(r'source=sha256:[0-9a-f]{64}', rows[0]['build'])
                    or not re.search(r'dependencies=sha256:[0-9a-f]{64}', rows[0]['build'])
                    or not re.search(r'(GNU|Clang)-\d+\.\d+', rows[0]['build'])
                    or not re.search(r'rustc \d+\.\d+\.\d+', rows[1]['build'])
                    or not re.search(r'source=fnv1a64:[0-9a-f]{16}', rows[1]['build'])):
                raise ValueError(f'{fixture}: missing build provenance')
            for column in ('protocol', 'elem', 'workload', 'n', 'ops', 'fixture_digest',
                           'reservation', 'timer', 'repeats', 'warmup'):
                if rows[0][column] != rows[1][column]:
                    raise ValueError(f'{fixture}: result contract differs in {column}')
            print(f'PASS trace and result identity {fixture.name}')
    print(f'PASS fixture gate, {len(fixtures)} identical decoded inputs')


if __name__ == '__main__':
    main()
