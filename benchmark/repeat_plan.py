#!/usr/bin/env python3
"""Choose one fixture's shared repeat and warmup counts from C++ and Rust pilot trials."""

import argparse
import csv
import math
from pathlib import Path
import random
import statistics
from typing import NamedTuple

RESAMPLES = 2000
SEED = 1363692610


class Plan(NamedTuple):
    repeat: int
    warmup: int
    pilot_trials: int
    cpp_median_ns: float
    cpp_rel_se: float
    rust_median_ns: float
    rust_rel_se: float


def median_error(times):
    """Return the median and the bootstrap relative standard error of the median."""
    if len(times) < 3:
        raise ValueError("bootstrap requires at least three pilot trials")
    if any(not math.isfinite(value) or value <= 0 for value in times):
        raise ValueError("pilot trial times must be finite and positive")
    rng = random.Random(SEED)
    medians = [statistics.median(rng.choices(times, k=len(times))) for _ in range(RESAMPLES)]
    median = statistics.median(times)
    return median, statistics.stdev(medians) / median


def plan(cpp, rust, precision=0.005, min_repeat=5, max_repeat=1000, warmup_ns=10e6, max_timed_ns=2e9):
    if len(cpp) != len(rust):
        raise ValueError("C++ and Rust pilot trial counts differ")
    if not 0 < precision or not 1 <= min_repeat <= max_repeat or warmup_ns < 0 or max_timed_ns <= 0:
        raise ValueError("invalid plan limits")
    (cpp_median, cpp_se), (rust_median, rust_se) = median_error(cpp), median_error(rust)
    # The standard error of a median shrinks as 1/sqrt(trials).
    needed = max(math.ceil(len(times) * (se / precision) ** 2) for times, se in ((cpp, cpp_se), (rust, rust_se)))
    slowest = max(cpp_median, rust_median)
    cap = min(max_repeat, max(min_repeat, math.floor(max_timed_ns / slowest)))
    warmup = min(max_repeat, max(1, math.ceil(warmup_ns / slowest)))
    return Plan(min(cap, max(min_repeat, needed)), warmup, len(cpp), cpp_median, cpp_se, rust_median, rust_se)


def read_trials(path):
    with path.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        if "nanos" not in (reader.fieldnames or []):
            raise ValueError(f"{path}: missing nanos column")
        try:
            times = [float(row["nanos"]) for row in reader]
        except (TypeError, ValueError):
            raise ValueError(f"{path}: invalid nanos value") from None
    if not times:
        raise ValueError(f"{path}: no pilot trials")
    return times


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", type=Path, required=True, help="C++ pilot runner CSV")
    parser.add_argument("--rust", type=Path, required=True, help="Rust pilot runner CSV")
    parser.add_argument("--precision", type=float, default=0.005, help="target relative standard error of each median")
    parser.add_argument("--min-repeat", type=int, default=5)
    parser.add_argument("--max-repeat", type=int, default=1000)
    parser.add_argument("--warmup-ns", type=float, default=10e6, help="minimum untimed warmup time")
    parser.add_argument("--max-timed-ns", type=float, default=2e9, help="timed budget per process before min-repeat")
    args = parser.parse_args()
    try:
        result = plan(read_trials(args.cpp), read_trials(args.rust), args.precision, args.min_repeat,
                      args.max_repeat, args.warmup_ns, args.max_timed_ns)
    except (OSError, ValueError, csv.Error) as error:
        parser.exit(2, f"error: {error}\n")
    print("\t".join([str(result.repeat), str(result.warmup), str(result.pilot_trials),
                     f"{result.cpp_median_ns:.9g}", f"{result.cpp_rel_se:.6g}",
                     f"{result.rust_median_ns:.9g}", f"{result.rust_rel_se:.6g}"]))


if __name__ == "__main__":
    main()
