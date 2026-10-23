#!/usr/bin/env python3
"""Aggregate independently validated process medians without merging trials."""

import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import statistics
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from cpu import validate_record
import report


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def aggregate(reports, output):
    groups = {}
    manifests = []
    cpu = None
    profile = None
    summaries = []
    fixture_inventory = None
    for process, directory in enumerate(reports):
        manifest_raw = (directory / "manifest.json").read_bytes()
        manifest = json.loads(manifest_raw)
        if manifest.get("schema") != "quickheap-report-v1":
            raise ValueError("aggregate requires validated per-process reports")
        if profile is not None and profile != manifest["profile"]:
            raise ValueError("aggregate cannot mix profiles")
        profile = manifest["profile"]
        metadata = manifest.get("run_metadata", {}).get("value")
        if not metadata:
            raise ValueError("aggregate requires attributed P-core metadata")
        validate_record(metadata)
        if cpu is not None and cpu != metadata:
            raise ValueError("aggregate cannot mix CPU identities")
        cpu = metadata
        if profile == "controlled-operations":
            inventory = manifest.get("fixture_inventory")
            if not inventory:
                raise ValueError("controlled aggregate lacks expected fixture inventory")
            if fixture_inventory is not None and inventory != fixture_inventory:
                raise ValueError("process fixture inventories differ")
            fixture_inventory = inventory
        csv_sources = []
        for item in manifest.get("inputs", []):
            source = directory / item["path"]
            if digest(source.read_bytes()) != item["sha256"]:
                raise ValueError("report input digest differs")
            csv_sources.append(source)
        if not csv_sources:
            raise ValueError("report contains no attributed inputs")
        protocol = json.loads((directory / "provenance/protocol.json").read_text())
        fixtures = report.read_fixtures(directory / "fixtures") if profile == "controlled-operations" else {}
        rebuilt, _ = report.read_results(csv_sources, fixtures, protocol, profile)
        raw = (directory / "summary.csv").read_bytes()
        rows = list(csv.DictReader(raw.decode().splitlines()))
        if not rows or len(rows) != len(rebuilt):
            raise ValueError("process summary differs from attributed inputs")
        expected = {}
        for summary in rebuilt:
            identity = {name: str(getattr(summary.identity, name)) for name in report.Identity.__dataclass_fields__}
            identity.update(build_id=summary.identity.build_id, metric=summary.metric, units=summary.units)
            expected[tuple(identity.items())] = (summary.median, summary.minimum, summary.maximum)
        for row in rows:
            identity = tuple((key, value) for key, value in row.items() if key not in ("median", "min", "max"))
            actual = tuple(float(row[name]) for name in ("median", "min", "max"))
            if expected.get(identity) != actual:
                raise ValueError("process summary differs from attributed inputs")
            value = actual[0]
            if value <= 0 or not math.isfinite(value):
                raise ValueError("invalid process median")
            values = groups.setdefault(identity, {})
            if process in values:
                raise ValueError("duplicate process group")
            values[process] = value
        manifests.append({"process": process, "report": str(directory),
                          "manifest_sha256": digest(manifest_raw), "summary_sha256": digest(raw)})
    if not groups:
        raise ValueError("no process reports")
    if profile == "controlled-operations":
        if any(len(values) != len(reports) for values in groups.values()):
            raise ValueError("controlled aggregate has missing process groups")
        fixtures = {}
        for identity in groups:
            row = dict(identity)
            pair = (row["fixture_digest"], row["elem"], row["workload"], row["n"], row["metric"])
            fixtures.setdefault(pair, set()).add(row["language"])
        if {pair[0] for pair in fixtures} != set(fixture_inventory):
            raise ValueError("controlled aggregate has missing expected fixtures")
        if any(languages != {"C++", "Rust"} for languages in fixtures.values()):
            raise ValueError("controlled aggregate requires both languages for every fixture")
    fields = [key for key, _ in next(iter(groups))] + ["processes", "expected_processes", "complete", "process_median", "process_min", "process_max"]
    output.mkdir(parents=True, exist_ok=True)
    with (output / "process-summary.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for identity, process_values in sorted(groups.items()):
            values = list(process_values.values())
            row = dict(identity)
            row.update(processes=len(values), expected_processes=len(reports), complete=len(values) == len(reports), process_median=statistics.median(values),
                       process_min=min(values), process_max=max(values))
            writer.writerow(row)
            if len(values) == len(reports):
                identity_value = dict(identity)
                parsed = {name: identity_value[name] for name in report.Identity.__dataclass_fields__}
                for name in ("n", "ops", "repeats", "warmup"):
                    parsed[name] = int(parsed[name])
                summaries.append(report.Summary(report.Identity(**parsed), identity_value["metric"], identity_value["units"],
                                                statistics.median(values), min(values), max(values)))
    provenance = []
    (output / "provenance").mkdir(exist_ok=True)
    for name in ("historical.json", "catalog.csv", "protocol.json", "report.py", "plot.py"):
        raw = (reports[0] / "provenance" / name).read_bytes()
        (output / "provenance" / name).write_bytes(raw)
        provenance.append({"path": f"provenance/{name}", "sha256": digest(raw)})
    historical = json.loads((output / "provenance/historical.json").read_text())
    report.write_reports(summaries, output, historical)
    (output / "summary.csv").rename(output / "complete-groups.csv")
    (output / "process-summary.csv").rename(output / "summary.csv")
    paper = output / "paper.md"
    paper.write_text("# Independent process comparison\n\nStatistics are medians and ranges of process medians. Only complete groups appear below. "
                     "See summary.csv for observed and expected process counts, including cutoff omissions.\n\n" + paper.read_text())
    import plot
    for metric in sorted({summary.metric for summary in summaries}):
        plotted = [plot.Summary(plot.Sample("synthetic", summary.identity.elem,
                    f"{summary.identity.language} {summary.identity.heap} [{summary.identity.backend}] {summary.identity.build_id} {summary.identity.fixture_digest}",
                    summary.identity.workload, "", summary.identity.n, summary.units, summary.median),
                    summary.median, summary.minimum, summary.maximum, len(reports))
                   for summary in summaries if summary.metric == metric]
        for suffix in ("svg", "pdf"):
            plot.plot(plotted, output / f"{metric}.{suffix}")
    (output / "manifest.json").write_text(json.dumps({"schema": "quickheap-process-report-v1",
        "profile": profile, "cpu": cpu, "processes": len(reports), "reports": manifests,
        "fixture_inventory": fixture_inventory, "source_sha256": digest(Path(__file__).read_bytes()),
        "provenance": provenance,
        "statistic": "median and range of independently measured process medians",
        "incomplete_groups": sum(len(values) != len(reports) for values in groups.values())}, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reports", type=Path, nargs="+")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        aggregate(args.reports, args.output)
    except (OSError, ValueError, KeyError, csv.Error) as error:
        parser.exit(2, f"error: {error}\n")
    print(f"PASS {len(args.reports)} process reports aggregated")


if __name__ == "__main__":
    main()
