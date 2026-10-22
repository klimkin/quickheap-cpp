#!/usr/bin/env python3
"""Summarize native benchmark CSVs and plot repeat medians and ranges."""

import argparse
import csv
from dataclasses import dataclass
import math
from pathlib import Path
import statistics
import sys


@dataclass(frozen=True)
class Sample:
    kind: str
    elem: str
    heap: str
    workload: str
    graph: str
    n: int
    units: str
    value: float

    def key(self):
        return (self.kind, self.elem, self.heap, self.workload, self.graph,
                self.n, self.units)


@dataclass(frozen=True)
class Summary:
    sample: Sample
    median: float
    minimum: float
    maximum: float
    repeats: int


def number(row, column, source, line, integer=False):
    value = row.get(column, "")
    try:
        result = int(value) if integer else float(value)
    except (TypeError, ValueError):
        raise ValueError(f"{source}:{line}: invalid {column} {value!r}") from None
    if result < 0 or not math.isfinite(result):
        raise ValueError(f"{source}:{line}: {column} must be finite and nonnegative")
    return result


def read_samples(source, args):
    with source.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        columns = reader.fieldnames or []
        if len(columns) != len(set(columns)):
            raise ValueError(f"{source}: duplicate CSV columns")
        if args.metric == "nanos" and "nanos" not in columns:
            if {"capacity", "capacity_ratio"} <= set(columns) or {"median", "min", "max"} <= set(columns):
                print(f"{source}: skipped non-timing CSV", file=sys.stderr)
                return []
            raise ValueError(f"{source}: missing columns nanos")
        graph = "graph" in columns
        partition = "variant" in columns and "upper" in columns and "lower" in columns
        kind = "graph" if graph else "partition" if partition else "synthetic"
        heap_column = "variant" if partition else "heap"
        required = {heap_column, "workload", "repeat"}
        required |= ({"graph", "vertices", "edges", "push_count", "pop_count"} if graph
                     else {"elem", "n", "upper", "lower", "backend"} if partition
                     else {"elem", "n", "ops"})
        metrics = ("push_comparisons", "pop_comparisons") if args.metric == "comparisons" else (args.metric,)
        required.update(metrics)
        missing = required - set(columns)
        if missing:
            raise ValueError(f"{source}: missing columns {', '.join(sorted(missing))}")
        samples = []
        unavailable = 0
        for line, row in enumerate(reader, 2):
            if None in row or any(value is None for value in row.values()):
                raise ValueError(f"{source}:{line}: malformed CSV row")
            if args.heap and row[heap_column] not in args.heap:
                continue
            if args.workload and row["workload"] not in args.workload:
                continue
            if args.elem and row.get("elem", "") not in args.elem:
                continue
            for column in required - set(metrics):
                if not row[column]:
                    raise ValueError(f"{source}:{line}: empty {column}")
            number(row, "repeat", source, line, True)
            n = number(row, "vertices" if graph else "n", source, line, True)
            ops = (number(row, "push_count", source, line, True)
                   + number(row, "pop_count", source, line, True)) if graph else n if partition else number(row, "ops", source, line, True)
            if graph:
                number(row, "edges", source, line, True)
            if partition and (number(row, "upper", source, line, True)
                              + number(row, "lower", source, line, True)) != n:
                raise ValueError(f"{source}:{line}: partition counts must sum to n")
            if n == 0 or ops == 0:
                raise ValueError(f"{source}:{line}: size and operation count must be positive")
            if any(not row[column] for column in metrics):
                unavailable += 1
                continue
            value = sum(number(row, column, source, line) for column in metrics)
            unit = "ns" if args.metric == "nanos" else args.metric
            denominator = 1
            if args.normalize == "ops":
                denominator = ops
                unit += "/queue call" if graph else "/element" if partition else "/push-pop pair"
            elif args.normalize == "pushes":
                if not graph:
                    raise ValueError(f"{source}: pushes normalization requires graph CSV")
                denominator = number(row, "push_count", source, line, True)
                unit += "/push"
            elif args.normalize == "nlogn":
                denominator = n * math.log2(n)
                unit += "/(n log2 n)"
            if denominator == 0:
                raise ValueError(f"{source}:{line}: normalization denominator is zero")
            heap = row[heap_column] + (f" [{row['backend']}]" if partition else "")
            samples.append(Sample(kind, row.get("elem", ""),
                                  heap, row["workload"], row.get("graph", ""),
                                  n, unit, value / denominator))
        if unavailable:
            print(f"{source}: skipped {unavailable} rows with unavailable {args.metric}", file=sys.stderr)
        return samples


def summarize(samples):
    groups = {}
    for sample in samples:
        groups.setdefault(sample.key(), []).append(sample)
    return [Summary(group[0], statistics.median(sample.value for sample in group),
                    min(sample.value for sample in group), max(sample.value for sample in group),
                    len(group)) for _, group in sorted(groups.items())]


def write_table(summaries):
    writer = csv.writer(sys.stdout)
    writer.writerow(("kind", "elem", "heap", "workload", "graph", "n", "units",
                     "median", "min", "max", "repeats"))
    for summary in summaries:
        writer.writerow((*summary.sample.key(), summary.median,
                         summary.minimum, summary.maximum, summary.repeats))


def plot(summaries, output):
    if output.suffix.lower() not in {".svg", ".png", ".pdf"}:
        raise ValueError("plot output must end in .svg, .png, or .pdf")
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        raise ValueError("plotting requires matplotlib; --table uses Python standard library") from None
    panels = {}
    for summary in summaries:
        sample = summary.sample
        panels.setdefault((sample.kind, sample.workload, sample.elem, sample.units), []).append(summary)
    heaps = sorted({summary.sample.heap for summary in summaries})
    legend_columns = max(1, min(4, len(heaps)))
    panel_height = 4 + 0.25 * math.ceil(len(heaps) / legend_columns)
    figure, axes = plt.subplots(len(panels), 1, squeeze=False,
                                figsize=(11, panel_height * len(panels)))
    colors = {heap: plt.get_cmap("tab20")(index % 20) for index, heap in enumerate(heaps)}
    for axis, (panel, group) in zip(axes[:, 0], sorted(panels.items())):
        kind, workload, elem, units = panel
        graphs = sorted({summary.sample.graph for summary in group})
        for heap in heaps:
            series = sorted((summary for summary in group if summary.sample.heap == heap),
                            key=lambda summary: (summary.sample.graph, summary.sample.n))
            if not series:
                continue
            x = [graphs.index(summary.sample.graph) if kind == "graph" else summary.sample.n for summary in series]
            y = [summary.median for summary in series]
            errors = [[summary.median - summary.minimum for summary in series],
                      [summary.maximum - summary.median for summary in series]]
            axis.errorbar(x, y, yerr=errors, marker="o", capsize=3,
                          color=colors[heap], label=heap)
        if kind == "graph":
            axis.set_xticks(range(len(graphs)), [Path(name).name for name in graphs], rotation=20)
            axis.set_xlabel("Graph")
        else:
            axis.set_xscale("log", base=2)
            axis.set_xlabel("n")
        axis.set_title(" ".join(filter(None, (workload, elem))))
        axis.set_ylabel(units)
        axis.grid(alpha=0.3)
        axis.legend(fontsize="small", ncol=legend_columns, loc="upper center",
                    bbox_to_anchor=(0.5, -0.18))
    figure.tight_layout()
    output.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(output, bbox_inches="tight")
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path, nargs="+", help="CSV files or directories")
    parser.add_argument("--table", action="store_true", help="write summary CSV to stdout")
    parser.add_argument("--output", type=Path, help="plot path ending in .svg, .png, or .pdf")
    parser.add_argument("--metric", default="nanos", help="numeric column, or comparisons for push plus pop")
    parser.add_argument("--normalize", choices=("ops", "pushes", "nlogn", "none"), default="ops")
    parser.add_argument("--heap", action="append", help="include exact heap name; repeat to include more")
    parser.add_argument("--workload", action="append", help="include exact workload name")
    parser.add_argument("--elem", action="append", help="include exact element type")
    args = parser.parse_args()
    try:
        sources = []
        for path in args.csv:
            sources.extend(sorted(path.glob("*.csv")) if path.is_dir() else [path])
        summaries = summarize([sample for source in sources for sample in read_samples(source, args)])
        if not summaries:
            raise ValueError("no usable benchmark rows")
        if args.table or not args.output:
            write_table(summaries)
        if args.output:
            plot(summaries, args.output)
    except (OSError, ValueError, csv.Error) as error:
        parser.exit(2, f"error: {error}\n")


if __name__ == "__main__":
    main()
