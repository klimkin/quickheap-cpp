#!/usr/bin/env python3
"""Plot the README figure from algorithms.sh plot.csv in the upstream Rust README layout."""

import argparse
from pathlib import Path
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

sys.path.insert(0, str(Path(__file__).resolve().parent))
import plot  # noqa: E402


def rgb(r, g, b):
    return (r / 255, g / 255, b / 255)


# Labels, colours, and line styles follow upstream quickheap-dev/bench/evals/plot.py.
HEAPS = {
    "std_priority_queue": ("BinaryHeap", rgb(152, 78, 163), "-"),
    "dary8": ("8-aryHeap", rgb(55, 126, 184), "-"),
    "weak_heap": ("WeakHeap", rgb(77, 175, 74), "-"),
    "radix_heap": ("RadixHeap", rgb(228, 26, 28), "-"),
    "sequence_heap": ("SequenceHeap", rgb(255, 127, 0), "-"),
    "s3q": ("S3qHeap", rgb(166, 86, 40), "-"),
    "original_quickheap": ("OriginalQuickHeap", rgb(247, 129, 191), "-"),
    "scalar_quickheap": ("ScalarQuickHeapL3", "black", ":"),
    "quickheap": ("SimdQuickHeapAvx2", "black", "--"),
}
ELEMS = ("i32", "i64")
WORKLOAD = "MonotoneConstantSize"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path, nargs="+")
    parser.add_argument("--output", type=Path, required=True)
    options = parser.parse_args()
    args = argparse.Namespace(metric="nanos", normalize="opslogn", heap=None, workload=WORKLOAD, elem=None)
    medians = {(s.sample.heap.split(" [")[0], s.sample.elem, s.sample.n): s.median
               for s in plot.summarize([sample for path in options.csv for sample in plot.read_samples(path, args)])}

    fig, axes = plt.subplots(1, len(ELEMS), figsize=(21 / 2.54, 3.6), sharex=True, sharey=True)
    for axis, elem in zip(axes, ELEMS):
        for heap, (label, colour, style) in HEAPS.items():
            points = sorted((n, value) for (h, e, n), value in medians.items() if h == heap and e == elem)
            if not points:
                raise SystemExit(f"{heap} {elem}: no {WORKLOAD} samples")
            axis.plot(*zip(*points), color=colour, ls=style, lw=1.5, label=label)
        axis.set_title(elem)
        axis.set_xscale("log", base=2)
        axis.set_yscale("log", base=2)
        axis.set_xticks([2**e for e in (10, 15, 20, 25)])
        axis.set_yticks([2**e for e in range(5)])
        axis.grid(axis="both", which="both", linestyle="-")
    axes[0].set_ylabel(WORKLOAD, fontsize=8)
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper center", ncol=4, fontsize=8, bbox_to_anchor=(0.5, 0.0))
    fig.supylabel(r"Runtime ($\mathsf{ns} \:/\: (\mathsf{Elems} \cdot \lg n)$)")
    fig.supxlabel("$n$", y=0.02)
    fig.subplots_adjust(left=0.12, bottom=0.16, wspace=0.12)
    fig.savefig(options.output, bbox_inches="tight")


if __name__ == "__main__":
    main()
