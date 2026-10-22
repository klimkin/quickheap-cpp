#!/usr/bin/env python3
"""Report weight distributions for DIMACS shortest-path graphs."""

import argparse
import csv
from pathlib import Path
import statistics
import sys


def weights(path):
    vertices = edges = None
    values = []
    with path.open(encoding="utf-8") as stream:
        for line, text in enumerate(stream, 1):
            fields = text.split()
            if not fields or fields[0] == "c":
                continue
            try:
                if fields[0] == "p" and len(fields) == 4 and fields[1] == "sp" and vertices is None:
                    vertices, edges = map(int, fields[2:])
                    if vertices < 1 or edges < 0:
                        raise ValueError("invalid graph size")
                elif fields[0] == "a" and len(fields) == 4 and vertices is not None:
                    source, target, weight = map(int, fields[1:])
                    if not (1 <= source <= vertices and 1 <= target <= vertices) or weight < 0:
                        raise ValueError("invalid arc")
                    values.append(weight)
                else:
                    raise ValueError("expected p sp header or arc")
            except ValueError as error:
                raise ValueError(f"{path}:{line}: {error}") from None
    if vertices is None or len(values) != edges:
        raise ValueError(f"{path}: missing header or edge count mismatch")
    return (str(path), vertices, edges, min(values, default=""),
            statistics.median(values) if values else "", max(values, default=""), values.count(0))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("graphs", nargs="+", type=Path, help=".gr files or directories")
    args = parser.parse_args()
    try:
        paths = []
        for path in args.graphs:
            paths.extend(sorted(path.glob("*.gr")) if path.is_dir() else [path])
        if not paths:
            raise ValueError("no graph files")
        rows = [weights(path) for path in paths]
        writer = csv.writer(sys.stdout)
        writer.writerow(("graph", "vertices", "edges", "min", "median", "max", "zero_weights"))
        writer.writerows(rows)
    except (OSError, ValueError) as error:
        parser.exit(2, f"error: {error}\n")


if __name__ == "__main__":
    main()
