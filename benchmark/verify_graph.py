#!/usr/bin/env python3
"""Verify C++ graph semantics against an independent paired checker."""

import argparse
import hashlib
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cpp-bench', type=Path, required=True)
    parser.add_argument('--rust-checker', type=Path, required=True)
    parser.add_argument('--fixtures', type=Path, default=Path(__file__).parent / 'data')
    args = parser.parse_args()
    fixtures = sorted(args.fixtures.glob('*.gr'))
    if not fixtures:
        raise ValueError('no graph fixtures')
    for fixture in fixtures:
        cpp = subprocess.check_output([str(args.cpp_bench), str(fixture), '--verify-only',
                                       '--heap', 'std_priority_queue'], text=True)
        rust = subprocess.check_output([str(args.rust_checker), str(fixture)], text=True)
        if cpp != rust:
            raise ValueError(f'{fixture}: graph result or queue counts differ\nC++ {cpp}\nRust {rust}')
        for heap in ('quickheap', 'dary8', 'weak_heap'):
            counterpart = subprocess.check_output([str(args.cpp_bench), str(fixture),
                                '--verify-only', '--heap', heap], text=True)
            if counterpart != cpp:
                raise ValueError(f'{fixture}: {heap} graph result differs')
        print(f'PASS {fixture.name} sha256:{hashlib.sha256(fixture.read_bytes()).hexdigest()}')
    print(f'PASS graph gate, {len(fixtures)} matching results and queue counts')


if __name__ == '__main__':
    main()
