#!/usr/bin/env python3
"""Measure matched C++ and Rust fixtures on a verified performance core."""

import argparse
import csv
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
import gzip
import hashlib
import io
import json
import math
import os
from pathlib import Path
import re
import statistics
import subprocess
import sys

from cpu import observe_affinity, pinned_command, select_cpu


WORKLOAD_FACTORS = {"HeapSort": 1, "MonotoneConstantSize": 10,
                    "MonotoneWiggle": 3, "RandomWiggle": 3}


@dataclass(frozen=True)
class Case:
    workload: str
    n: int
    seed: int
    elem: str
    path: Path
    fixture_digest: str
    sha256: str

    @property
    def name(self):
        return f"{self.workload}-{self.elem}-n{self.n}-seed{self.seed}"

    @property
    def ops(self):
        return self.n * WORKLOAD_FACTORS[self.workload]


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def fixture_digest(raw):
    value = 0xCBF29CE484222325
    for byte in raw:
        value = ((value ^ byte) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return f"fnv1a64:{value:016x}"


def load_cases(directory, elem, sizes, seeds):
    cases = []
    for workload in WORKLOAD_FACTORS:
        for n in sizes:
            for seed in seeds:
                path = directory / f"{workload}-{elem}-n{n}-seed{seed}.qhf"
                raw = path.read_bytes()
                if raw.split(None, 4)[:4] != [b"QHB1", elem.encode(), workload.encode(), str(n).encode()]:
                    raise ValueError(f"fixture header differs: {path}")
                cases.append(Case(workload, n, seed, elem, path, fixture_digest(raw), digest(raw)))
    return cases


def parse_trials(raw, case, side, repeats, warmup):
    rows = list(csv.DictReader(io.StringIO(raw)))
    if len(rows) != repeats:
        raise ValueError(f"expected {repeats} trials, received {len(rows)}")
    expected = {"elem": case.elem, "n": str(case.n), "workload": case.workload,
                "ops": str(case.ops), "profile": "controlled-operations",
                "protocol": "quickheap-cpp-v1", "fixture_digest": case.fixture_digest,
                "reservation": "backend-default", "timer": "workload-only-raii",
                "repeats": str(repeats), "warmup": str(warmup),
                "input_policy": "shared-fixture", "cutoff": "disabled",
                "heap": "quickheap_avx2" if side == "rust" else "quickheap",
                "backend": "rust-production-avx2" if side == "rust" else "AVX2",
                "push_comparisons": "", "pop_comparisons": ""}
    trials = {}
    builds = set()
    for row in rows:
        if None in row or any(row.get(key) != value for key, value in expected.items()):
            raise ValueError("trial identity or workload contract differs")
        build = row.get("build", "")
        if (not build.startswith("rustc " if side == "rust" else ("GNU-", "Clang-"))
                or "source=" not in build):
            raise ValueError("missing attributed compiler build")
        builds.add(build)
        try:
            index, nanos = int(row["repeat"]), int(row["nanos"])
        except (KeyError, TypeError, ValueError):
            raise ValueError("invalid trial index or duration") from None
        if str(index) != row["repeat"] or str(nanos) != row["nanos"] or nanos <= 0 or index in trials:
            raise ValueError("duplicate or invalid trial index or duration")
        trials[index] = nanos
    if set(trials) != set(range(repeats)) or len(builds) != 1:
        raise ValueError("incomplete trial indices or changing build identity")
    return [trials[index] for index in range(repeats)], builds.pop()


def validate_trace(raw, case):
    retained = case.n + 50 if case.workload == "MonotoneConstantSize" else 0
    setup_pushes = 2 * retained
    setup_pops = retained
    expected = {b"push,": case.ops + setup_pushes,
                b"pop,": case.ops + setup_pops, b"drain,": retained}
    for prefix, count in expected.items():
        actual = raw.count(b"\n" + prefix) + int(raw.startswith(prefix))
        if actual != count:
            raise ValueError(f"trace {prefix.decode()} count {actual} differs from {count}")
    if raw.count(f"final,{retained}\n".encode()) != 1 or raw.count(b"\n") != sum(expected.values()) + 1:
        raise ValueError("trace final state or operation total differs")


def summarize(processes, cases, labels, runs):
    groups = {}
    for row in processes:
        key = (row["case"], row["side"])
        values = groups.setdefault(key, {})
        if row["process"] in values or row["median_ns"] <= 0 or not math.isfinite(row["median_ns"]):
            raise ValueError("duplicate process or invalid process median")
        values[row["process"]] = row["median_ns"]
    expected = {(case.name, label) for case in cases for label in [*labels, "rust"]}
    if set(groups) != expected or any(set(values) != set(range(runs)) for values in groups.values()):
        raise ValueError("missing or unexpected case, implementation, or process group")
    summary = []
    for case in cases:
        rust = list(groups[(case.name, "rust")].values())
        rust_median = statistics.median(rust)
        for label in labels:
            cpp = list(groups[(case.name, label)].values())
            cpp_median = statistics.median(cpp)
            summary.append({"case": case.name, "workload": case.workload, "n": case.n,
                            "seed": case.seed, "elem": case.elem, "fixture_digest": case.fixture_digest,
                            "side": label, "processes": runs, "ops": case.ops, "cpp_median_ns": cpp_median,
                            "cpp_min_ns": min(cpp), "cpp_max_ns": max(cpp),
                            "rust_median_ns": rust_median, "rust_min_ns": min(rust),
                            "rust_max_ns": max(rust),
                            "slowdown_percent": 100 * (cpp_median / rust_median - 1),
                            "passed": cpp_median * 100 < rust_median * 103})
    return summary


def write_csv(path, rows):
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def run(args):
    cpu = observe_affinity(select_cpu(args.cpu))
    cases = load_cases(args.fixtures.resolve(), args.elem, args.sizes, args.seeds)
    binaries = dict(args.cpp)
    binaries["rust"] = args.rust.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    output = args.output.resolve()
    metadata = {"schema": "quickheap-parity-v1", "profile": "controlled-operations", "units": "ns per complete timed workload",
                "cpu": cpu, "started": datetime.now(timezone.utc).isoformat(),
                "load_average": os.getloadavg(), "runs": args.runs, "repeats": args.repeat,
                "warmup": args.warmup, "target": "C++ slowdown strictly below 3%; faster passes",
                "script_sha256": digest(Path(__file__).read_bytes()),
                "cpu_validator_sha256": digest(Path(__file__).with_name("cpu.py").read_bytes()),
                "binaries": {label: {"path": str(path), "sha256": digest(path.read_bytes())}
                             for label, path in binaries.items()},
                "fixtures": [{**asdict(case), "path": str(case.path)} for case in cases]}
    (output / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    commands, traces, processes, builds = [], [], [], {}

    def execute(name, label, case, verify=False):
        command = [str(binaries[label]), "--profile", "controlled-operations", "--fixture", str(case.path),
                   "--heap", "quickheap"]
        if label != "rust":
            command += ["--backend", "avx2"]
        command += ["--verify-only"] if verify else ["--repeat", str(args.repeat), "--warmup", str(args.warmup)]
        command = pinned_command(command, cpu["cpu"])
        stdout = output / f"{name}.stdout"
        with stdout.open("wb") as stream, (output / f"{name}.stderr").open("wb") as stderr:
            result = subprocess.run(command, stdout=stream, stderr=stderr)
        commands.append({"name": name, "command": command, "returncode": result.returncode})
        (output / "commands.json").write_text(json.dumps(commands, indent=2) + "\n")
        if result.returncode:
            raise ValueError(f"{name} exited {result.returncode}; see {output / (name + '.stderr')}")
        return stdout

    try:
        for case in cases:
            reference = None
            for label in ["rust", *dict(args.cpp)]:
                path = execute(f"verify-{case.name}-{label}", label, case, True)
                raw = path.read_bytes()
                validate_trace(raw, case)
                hashed = digest(raw)
                if reference is not None and hashed != reference:
                    raise ValueError(f"complete trace mismatch: {case.name}, {label}")
                reference = hashed
                archived = path.with_suffix(".stdout.gz")
                archived.write_bytes(gzip.compress(raw, compresslevel=1, mtime=0))
                path.unlink()
                traces.append({"case": case.name, "side": label, "sha256": hashed,
                               "bytes": len(raw), "path": str(archived), "work_pairs": case.ops})
            (output / "traces.json").write_text(json.dumps(traces, indent=2) + "\n")
        print(f"Validated {len(cases)} full fixture traces per implementation.", flush=True)
        for process in range(args.runs):
            for index, case in enumerate(cases):
                labels = list(binaries)
                if (process + index) % 2:
                    labels.reverse()
                for label in labels:
                    path = execute(f"timing-{process}-{case.name}-{label}", label, case)
                    values, build = parse_trials(path.read_text(), case, label, args.repeat, args.warmup)
                    if label in builds and builds[label] != build:
                        raise ValueError(f"build identity changed for {label}")
                    builds[label] = build
                    processes.append({"case": case.name, "side": label, "process": process,
                                      "median_ns": statistics.median(values), "min_ns": min(values),
                                      "max_ns": max(values), "trials": len(values), "work_pairs": case.ops})
            write_csv(output / "processes.csv", processes)
            print(f"Process {process + 1}/{args.runs} complete.", flush=True)
        for label, path in binaries.items():
            if digest(path.read_bytes()) != metadata["binaries"][label]["sha256"]:
                raise ValueError(f"binary changed during measurement: {label}")
        for case in cases:
            if digest(case.path.read_bytes()) != case.sha256:
                raise ValueError(f"fixture changed during measurement: {case.name}")
        summary = summarize(processes, cases, list(dict(args.cpp)), args.runs)
        write_csv(output / "summary.csv", summary)
        result = {"status": "passed" if all(row["passed"] for row in summary) else "above-target",
                  "errors": 0, "groups": len(cases), "runs": args.runs, "repeats": args.repeat,
                  "work_pairs_per_implementation": sum(case.ops for case in cases) * args.runs * args.repeat,
                  "maximum_slowdown_percent": {label: max(row["slowdown_percent"] for row in summary if row["side"] == label)
                                               for label in dict(args.cpp)}, "builds": builds}
        (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
        print(json.dumps(result, indent=2))
        return 0 if result["status"] == "passed" else 1
    except (OSError, ValueError, csv.Error) as error:
        (output / "result.json").write_text(json.dumps({"status": "error", "errors": 1, "reason": str(error)}, indent=2) + "\n")
        raise


def integers(value, minimum=1, maximum=(1 << 64) - 1):
    parts = value.split(",")
    if any(not re.fullmatch(r"[0-9]+", part) or not minimum <= int(part) <= maximum for part in parts):
        raise argparse.ArgumentTypeError("requires comma-separated integers within range")
    numbers = list(map(int, parts))
    if len(set(numbers)) != len(numbers):
        raise argparse.ArgumentTypeError("requires distinct integers")
    return numbers


def cpp_binary(value):
    label, separator, path = value.partition("=")
    if not separator or not re.fullmatch(r"[A-Za-z][A-Za-z0-9_-]*", label) or label == "rust" or not path:
        raise argparse.ArgumentTypeError("--cpp requires LABEL=EXE; label rust is reserved")
    return label, Path(path).resolve()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", type=cpp_binary, action="append", required=True)
    parser.add_argument("--rust", type=Path, required=True)
    parser.add_argument("--fixtures", type=Path, required=True)
    parser.add_argument("--elem", choices=("i32", "i64"), default="i64")
    parser.add_argument("--sizes", type=lambda value: integers(value, 1, 1 << 25), default=[4096, 65536])
    parser.add_argument("--seeds", type=lambda value: integers(value, 0), default=[1363692610, 1, 12345])
    parser.add_argument("--runs", type=int, default=7)
    parser.add_argument("--repeat", type=int, default=15)
    parser.add_argument("--warmup", type=int, default=1)
    parser.add_argument("--cpu", default=os.environ.get("BENCH_CPU", "2"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.runs < 1 or args.repeat < 1 or args.warmup < 0 or len(dict(args.cpp)) != len(args.cpp):
        parser.error("require positive runs/repeats, nonnegative warmup, and distinct C++ labels")
    try:
        return run(args)
    except (OSError, ValueError, csv.Error, subprocess.CalledProcessError) as error:
        parser.exit(2, f"error: {error}\n")


if __name__ == "__main__":
    sys.exit(main())
