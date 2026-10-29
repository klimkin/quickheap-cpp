#!/usr/bin/env python3
"""Render C++ versus Rust tables from compare.sh controlled-operations runs.

Each process round times C++ and Rust back to back on one fixture, so the
ratio of their process medians cancels drift that both sides share. A round
times the seeds of a cell one after another, so they share host conditions.
Each round therefore contributes one value, the mean log ratio over seeds.
A cell's estimate is the Hodges-Lehmann median of Walsh averages over rounds,
and its interval is the exact Wilcoxon signed-rank interval.

With several run directories, a later run replaces every cell it measured.
"""

import argparse
import csv
import math
from pathlib import Path
import statistics

ORDER = ("HeapSort", "MonotoneConstantSize", "RandomConstantSize", "MonotoneWiggle", "RandomWiggle",
         "GeometricMonotoneWiggle", "WorstCaseDescending", "MostlyPushDecreasing")
SIDES = {"cpp": "C++", "rust": "Rust"}


def process_median(path):
    rows = list(csv.DictReader(path.open(newline="", encoding="utf-8")))
    if not rows:
        raise SystemExit(f"{path}: no trials")
    keys = {(row["workload"], int(row["n"]), int(row["ops"]), row["fixture_digest"]) for row in rows}
    if len(keys) != 1:
        raise SystemExit(f"{path}: mixed fixtures")
    return keys.pop(), statistics.median(float(row["nanos"]) for row in rows)


def load_run(directory):
    """Return {(workload, n): {digest: [(cpp_ns, rust_ns, ops), ...]}} with one pair per process round."""
    cells = {}
    processes = sorted(directory.glob("process-*"))
    if not processes:
        raise SystemExit(f"{directory}: no process-* directories")
    for process in processes:
        for cpp in sorted(process.glob("*.cpp.csv")):
            rust = cpp.with_name(cpp.name.removesuffix(".cpp.csv") + ".rust.csv")
            if not rust.exists():
                raise SystemExit(f"{rust}: missing")
            (workload, n, ops, digest), cpp_ns = process_median(cpp)
            key, rust_ns = process_median(rust)
            if key != (workload, n, ops, digest):
                raise SystemExit(f"{cpp} and {rust}: different fixtures")
            cells.setdefault((workload, n), {}).setdefault(digest, []).append((cpp_ns, rust_ns, ops))
    return cells


def signed_rank_counts(count):
    """Number of sign assignments giving each signed-rank sum T+ for ranks 1..count."""
    counts = [1]
    for rank in range(1, count + 1):
        grown = counts + [0] * rank
        for total, ways in enumerate(counts):
            grown[total + rank] += ways
        counts = grown
    return counts


def hodges_lehmann(differences, confidence):
    """Estimate and exact two-sided confidence interval for the center of paired differences."""
    count = len(differences)
    walsh = sorted((a + b) / 2 for i, a in enumerate(differences) for b in differences[i:])
    counts = signed_rank_counts(count)
    tail, total, cumulative, k = (1 - confidence) / 2, 2 ** count, 0, 0
    for ways in counts:
        if (cumulative + ways) / total > tail:
            break
        cumulative += ways
        k += 1
    if k == 0:
        raise SystemExit(f"{count} paired differences cannot give a {confidence:.0%} interval")
    return statistics.median(walsh), walsh[k - 1], walsh[len(walsh) - k]


def percent(value):
    return 100 * math.expm1(value)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("runs", type=Path, nargs="+", help="controlled-operations directories, later runs win")
    parser.add_argument("--confidence", type=float, default=0.9)
    parser.add_argument("--unresolved", type=float, metavar="PERCENT",
                        help="print only the CELLS list of cells whose interval half-width exceeds PERCENT")
    arguments = parser.parse_args()
    cells = {}
    for run in arguments.runs:
        cells.update(load_run(run))
    if len({len(seeds) for seeds in cells.values()}) > 1:
        raise SystemExit("seed count differs between workload and size cells")
    results = {}
    for key, seeds in cells.items():
        if len({len(pairs) for pairs in seeds.values()}) != 1:
            raise SystemExit(f"{key[0]} n={key[1]}: seeds have different round counts")
        differences = [statistics.fmean(math.log(cpp / rust) for cpp, rust, _ in rounds)
                       for rounds in zip(*seeds.values())]
        estimate, low, high = hodges_lehmann(differences, arguments.confidence)
        per_op = [(statistics.median(c / ops for c, _, ops in pairs), statistics.median(r / ops for _, r, ops in pairs))
                  for pairs in seeds.values()]
        results[key] = dict(estimate=estimate, low=low, high=high, rounds=len(differences),
                            cpp=statistics.median(c for c, _ in per_op), rust=statistics.median(r for _, r in per_op))
    workloads = [w for w in ORDER if any(key[0] == w for key in results)]
    sizes = sorted({n for _, n in results})
    if arguments.unresolved is not None:
        wide = [f"{w}:{n.bit_length() - 1}" for w in workloads for n in sizes
                if (w, n) in results and (percent(results[w, n]["high"]) - percent(results[w, n]["low"])) / 2
                > arguments.unresolved]
        print(",".join(wide))
        return

    def cell(result):
        mark = "~" if result["low"] <= 0 <= result["high"] else ""
        return f"{mark}{percent(result['estimate']):+.1f}%"

    def interval(result):
        return f"{percent(result['low']):+.1f}% to {percent(result['high']):+.1f}%"

    print("| Workload | " + " | ".join(f"$2^{{{n.bit_length() - 1}}}$" for n in sizes) + " |")
    print("| --- |" + " ---: |" * len(sizes))
    for workload in workloads:
        print(f"| {workload} | " + " | ".join(cell(results[workload, n]) for n in sizes) + " |")
    print()
    print(f"{arguments.confidence:.0%} intervals:")
    print()
    print("| Workload | " + " | ".join(f"$2^{{{n.bit_length() - 1}}}$" for n in sizes) + " |")
    print("| --- |" + " ---: |" * len(sizes))
    for workload in workloads:
        print(f"| {workload} | " + " | ".join(interval(results[workload, n]) for n in sizes) + " |")
    largest = sizes[-1]
    print()
    print(f"| Workload | C++ ns/op | Rust ns/op | {arguments.confidence:.0%} interval | Paired rounds |")
    print("| --- | ---: | ---: | ---: | ---: |")
    for workload in workloads:
        result = results[workload, largest]
        print(f"| {workload} | {result['cpp']:.2f} | {result['rust']:.2f} | {interval(result)} | {result['rounds']} |")


if __name__ == "__main__":
    main()
