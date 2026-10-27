#!/usr/bin/env python3
"""Render C++ versus Rust tables from a compare.sh aggregate summary.csv."""

import argparse
import csv
import math
from pathlib import Path
import statistics

ORDER = ("HeapSort", "MonotoneConstantSize", "RandomConstantSize", "MonotoneWiggle", "RandomWiggle",
         "GeometricMonotoneWiggle", "WorstCaseDescending", "MostlyPushDecreasing")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("summary", type=Path, help="controlled-operations/report/summary.csv")
    rows = list(csv.DictReader(parser.parse_args().summary.open(newline="", encoding="utf-8")))
    pairs = {}
    for row in rows:
        if row["metric"] != "nanos":
            continue
        if row["complete"] != "True":
            raise SystemExit(f"{row['workload']} n={row['n']} {row['language']}: incomplete")
        scale = math.log2(int(row["n"]))
        value = tuple(float(row[k]) * scale for k in ("process_median", "process_min", "process_max"))
        pairs.setdefault((row["workload"], int(row["n"]), row["fixture_digest"]), {})[row["language"]] = value
    digests = {}
    for workload, n, digest in pairs:
        digests.setdefault((workload, n), set()).add(digest)
    if len({len(d) for d in digests.values()}) > 1:
        raise SystemExit("seed count differs between workload and size cells")
    cells = {}
    for (workload, n, _), sides in pairs.items():
        if set(sides) != {"C++", "Rust"}:
            raise SystemExit(f"{workload} n={n}: missing language")
        cpp, rust = sides["C++"], sides["Rust"]
        separated = cpp[2] < rust[1] or rust[2] < cpp[1]
        cells.setdefault((workload, n), []).append((cpp[0] / rust[0], separated, cpp[0], rust[0]))
    workloads = [w for w in ORDER if any(key[0] == w for key in cells)]
    sizes = sorted({n for _, n in cells})
    print("| Workload | " + " | ".join(f"$2^{{{n.bit_length() - 1}}}$" for n in sizes) + " |")
    print("| --- |" + " ---: |" * len(sizes))
    for workload in workloads:
        out = []
        for n in sizes:
            seeds = cells[(workload, n)]
            ratio = math.exp(statistics.fmean(math.log(r) for r, _, _, _ in seeds))
            mark = "" if all(s for _, s, _, _ in seeds) else "~"
            out.append(f"{mark}{100 * (ratio - 1):+.1f}%")
        print(f"| {workload} | " + " | ".join(out) + " |")
    largest = sizes[-1]
    print()
    print("| Workload | C++ ns/op | Rust ns/op | Seeds C++ faster, separated |")
    print("| --- | ---: | ---: | ---: |")
    for workload in workloads:
        seeds = cells[(workload, largest)]
        cpp = statistics.median(c for _, _, c, _ in seeds)
        rust = statistics.median(r for _, _, _, r in seeds)
        faster = sum(r < 1 and s for r, s, _, _ in seeds)
        print(f"| {workload} | {cpp:.2f} | {rust:.2f} | {faster}/{len(seeds)} |")


if __name__ == "__main__":
    main()
