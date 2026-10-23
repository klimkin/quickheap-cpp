#!/usr/bin/env python3
"""Select a kernel-identified performance core for benchmark subprocesses."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


def cpu_list(value):
    if not re.fullmatch(r"[0-9]+(?:-[0-9]+)?(?:,[0-9]+(?:-[0-9]+)?)*", value.strip()):
        raise ValueError("invalid kernel CPU list")
    result = set()
    for part in value.strip().split(","):
        bounds = list(map(int, part.split("-")))
        first, last = bounds[0], bounds[-1]
        if first > last:
            raise ValueError("reversed kernel CPU range")
        result.update(range(first, last + 1))
    return result


def select_cpu(requested="auto", sysfs=Path("/sys"), allowed=None):
    if allowed is None:
        if not hasattr(os, "sched_getaffinity"):
            raise ValueError("P-core pinning requires Linux CPU affinity")
        allowed = os.sched_getaffinity(0)
    proof = sysfs / "bus/event_source/devices/cpu_core/cpus"
    try:
        raw = proof.read_text().strip()
    except OSError as error:
        raise ValueError(f"cannot prove P-core identity from {proof}: {error}") from None
    performance = cpu_list(raw)
    online = cpu_list((sysfs / "devices/system/cpu/online").read_text())
    available = performance & set(allowed) & online
    if requested == "auto":
        if not available:
            raise ValueError("no allowed online kernel-identified P core")
        selected = min(available)
    else:
        if not re.fullmatch(r"[0-9]+", str(requested)):
            raise ValueError("BENCH_CPU requires one unsigned CPU number or auto")
        selected = int(requested)
        if selected not in available:
            raise ValueError(f"CPU {selected} is not an allowed online P core")
    topology = sysfs / f"devices/system/cpu/cpu{selected}/topology"
    return {"schema": "quickheap-cpu-v1", "cpu": selected, "core_class": "P",
            "proof": str(proof), "performance_cpus": raw,
            "allowed_cpus": sorted(allowed), "online_cpus": sorted(online),
            "core_id": int((topology / "core_id").read_text()),
            "package_id": int((topology / "physical_package_id").read_text())}


def validate_record(record):
    if (record.get("schema") != "quickheap-cpu-v1" or record.get("core_class") != "P"
            or type(record.get("cpu")) is not int or record["cpu"] < 0
            or record["cpu"] not in record.get("allowed_cpus", [])
            or record["cpu"] not in record.get("online_cpus", [])
            or record["cpu"] not in cpu_list(record.get("performance_cpus", ""))
            or record.get("observed_affinity") != [record["cpu"]]):
        raise ValueError("invalid observed P-core metadata")
    return record


def observe_affinity(record):
    command = pinned_command([sys.executable, "-c", "import json,os; print(json.dumps(sorted(os.sched_getaffinity(0))))"], record["cpu"])
    result = subprocess.run(command, check=True, text=True, capture_output=True)
    record["observed_affinity"] = json.loads(result.stdout)
    return validate_record(record)


def pinned_command(command, cpu):
    return ["taskset", "-c", str(cpu), *command]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--select", default=os.environ.get("BENCH_CPU", "auto"))
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        if not shutil.which("taskset"):
            raise ValueError("taskset unavailable")
        record = observe_affinity(select_cpu(args.select))
        if args.output:
            args.output.write_text(json.dumps(record, indent=2) + "\n")
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(2, f"error: {error}\n")
    print(record["cpu"])


if __name__ == "__main__":
    main()
