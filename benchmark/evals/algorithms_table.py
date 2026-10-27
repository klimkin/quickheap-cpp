#!/usr/bin/env python3
"""Render README algorithm tables from benchmark/evals/algorithms.sh output."""

import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import plot  # noqa: E402

PLOTTED = {
    "std_priority_queue": "`std::priority_queue` binary heap",
    "dary8": "8-ary heap, hole sift",
    "weak_heap": "Weak heap",
    "radix_heap": "Radix heap, native",
    "sequence_heap": "Sanders' sequence heap",
    "s3q": "S3Q sample-sort queue",
    "original_quickheap": "Original quickheap, Paredes & Navarro",
    "scalar_quickheap": "Scalar quickheap",
    "quickheap": "SimdQuickHeap, AVX2",
}
EXTRA = {
    "custom_binary_heap": "Binary heap, swap sift",
    "dary4": "4-ary heap, hole sift",
    "dary16": "16-ary heap, hole sift",
    "custom_dary4": "4-ary heap, swap sift",
    "custom_dary8": "8-ary heap, swap sift",
    "boost_dary4": "Boost 4-ary heap",
    "external_radix_heap": "Radix heap, `iwiwi/radix-heap`",
    "blocked_quickheap": "Quickheap, linked 128-slot blocks",
    "boost_fibonacci": "Boost Fibonacci heap",
    "boost_pairing": "Boost pairing heap",
    "boost_binomial": "Boost binomial heap",
    "boost_skew": "Boost skew heap",
}
SIZES = (10, 15, 20, 25)


def summaries(paths, metric):
    args = argparse.Namespace(metric=metric, normalize="ops", heap=None, workload=None, elem=None)
    return {(s.sample.heap, s.sample.elem, s.sample.n): s
            for s in plot.summarize([sample for path in paths for sample in plot.read_samples(path, args)])}


def cell(summary, digits):
    if summary is None:
        return "n/a"
    return f"{summary.median:.{digits}f}" + ("\\*" if summary.repeats == 1 else "")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    directory = parser.parse_args().directory
    timing = [directory / f"{name}.csv" for name in ("table", "table-25", "slow-25")]
    nanos = summaries(timing, "nanos")
    misses = summaries(timing, "hw_cache_misses")
    print("| Heap | Algorithm | " + " | ".join(f"$2^{{{e}}}$" for e in SIZES) + " | LLC misses |")
    print("| --- | --- | " + "---: | " * len(SIZES) + "---: |")
    for group in (PLOTTED, None, EXTRA):
        if group is None:
            print("| | | " + "| " * len(SIZES) + "|")
            continue
        for heap, label in group.items():
            row = [cell(nanos.get((heap, "u64", 1 << e)), 1) for e in SIZES]
            print(f"| `{heap}` | {label} | " + " | ".join(row) + f" | {cell(misses.get((heap, 'u64', 1 << 25)), 3)} |")
    types = summaries([directory / "types.csv"], "nanos")
    print()
    print("| Data type | Heap | ns/pair |")
    print("| --- | --- | ---: |")
    for heap, label in (("quickheap", "{}"), ("kv_quickheap", "({0},{0})")):
        for elem in ("u32", "u64"):
            print(f"| `{label.format(elem)}` | `{heap}` | {cell(types.get((heap, elem, 1 << 20)), 1)} |")


if __name__ == "__main__":
    main()
