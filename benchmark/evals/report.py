#!/usr/bin/env python3
"""Generate attributed C++ benchmark tables, LaTeX, and paper reports."""

import argparse
import csv
from dataclasses import dataclass
import hashlib
import io
import json
import math
from pathlib import Path
import re
import statistics
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from cpu import validate_record


FACTORS = {
    "HeapSort": 1, "MonotoneConstantSize": 10, "RandomConstantSize": 10,
    "MonotoneWiggle": 3, "RandomWiggle": 3, "GeometricMonotoneWiggle": 3,
    "WorstCaseDescending": 9, "MostlyPushDecreasing": 10,
}
ORIGINAL_PROTOCOL = "quickheap-original-rust-a9f2518-v1"
PROVENANCE = ("protocol", "build", "backend", "fixture_digest", "reservation",
              "timer", "repeats", "warmup")
METRICS = ("nanos", "comparisons", "capacity", "capacity_ratio", "allocated_key_bytes",
           "spare_slots", "peak_size", "partitions", "elements_partitioned", "retries",
           "samples", "rebalances", "multi_partitions", "blocks_partitioned",
           "active_buckets", "largest_bucket", "terminal_size", "size",
           "push_comparisons", "pop_comparisons")


def sha256(raw):
    return hashlib.sha256(raw).hexdigest()


def fixture_digest(raw):
    value = 14695981039346656037
    for byte in raw:
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return f"fnv1a64:{value:016x}"


def integer(value, name, minimum=0):
    if not re.fullmatch(r"[0-9]+", value or "") or int(value) < minimum:
        raise ValueError(f"invalid {name} {value!r}")
    return int(value)


def numeric(value, name):
    try:
        result = float(value)
    except (TypeError, ValueError):
        raise ValueError(f"invalid {name} {value!r}") from None
    if result < 0 or not math.isfinite(result):
        raise ValueError(f"{name} must be finite and nonnegative")
    return result


def build_language(build):
    if re.search(r"(?:GNU|Clang)-\d+\.\d+", build):
        if not re.search(r"source=sha256:[0-9a-f]{64}(?:/|$)", build):
            raise ValueError("C++ build lacks source sha256")
        if not re.search(r"dependencies=sha256:[0-9a-f]{64}(?:/|$)", build):
            raise ValueError("C++ build lacks dependency sha256")
        return "C++"
    if re.search(r"rustc \d+\.\d+\.\d+", build):
        if not re.search(r"source=fnv1a64:[0-9a-f]{16}(?:;|$)", build):
            raise ValueError("Rust build lacks source digest")
        if any(f"{field}=" not in build for field in ("target", "opt", "features", "flags")):
            raise ValueError("Rust build lacks target, optimization, features, or flags")
        return "Rust"
    raise ValueError("build lacks identified C++ compiler or rustc")


def read_fixtures(directory):
    fixtures = {}
    for path in sorted(directory.glob("*.qhf")):
        raw = path.read_bytes()
        words = raw.decode("ascii").split()
        if len(words) < 8 or words[0] != "QHB1" or words[1] not in ("i32", "i64", "u32", "u64"):
            raise ValueError(f"{path}: invalid fixture header")
        elem, workload = words[1:3]
        n = integer(words[3], "fixture n", 1)
        if n > 33554432 or workload not in FACTORS or words[4] != "setup":
            raise ValueError(f"{path}: invalid fixture shape")
        setup = integer(words[5], "setup count")
        expected_setup = 2 * (n + 50) if workload in ("MonotoneConstantSize", "RandomConstantSize") else 0
        cursor = 6 + setup
        if setup != expected_setup or cursor + 2 > len(words) or words[cursor] != "timed":
            raise ValueError(f"{path}: invalid setup region")
        timed = integer(words[cursor + 1], "timed count")
        if timed != n * FACTORS[workload] or len(words) != cursor + 2 + timed:
            raise ValueError(f"{path}: invalid timed region")
        offset = "Monotone" in workload
        limit = 1 << (64 if offset else int(elem[1:]))
        for word in words[6:cursor] + words[cursor + 2:]:
            if integer(word, "fixture value") >= limit:
                raise ValueError(f"{path}: fixture value exceeds width")
        digest = fixture_digest(raw)
        fixtures[digest] = {"elem": elem, "workload": workload, "n": n,
                            "ops": timed, "raw": raw, "source": str(path)}
    if not fixtures:
        raise ValueError(f"{directory}: no shared fixtures")
    return fixtures


@dataclass(frozen=True, order=True)
class Identity:
    profile: str
    input_policy: str
    cutoff: str
    language: str
    build: str
    protocol: str
    backend: str
    fixture_digest: str
    elem: str
    heap: str
    workload: str
    n: int
    ops: int
    reservation: str
    timer: str
    repeats: int
    warmup: int

    @property
    def build_id(self):
        return sha256(self.build.encode())[:16]


@dataclass(frozen=True)
class Summary:
    identity: Identity
    metric: str
    units: str
    median: float
    minimum: float
    maximum: float


def read_results(sources, fixtures, protocol, profile="controlled-operations"):
    groups, policies, schedule, inputs, trial_digests = {}, {}, set(), [], {}
    for source in sorted(set(sources), key=str):
        raw = source.read_bytes()
        inputs.append({"source": str(source), "sha256": sha256(raw), "raw": raw})
        reader = csv.DictReader(io.StringIO(raw.decode("utf-8"), newline=""))
        columns = reader.fieldnames or []
        if len(columns) != len(set(columns)):
            raise ValueError(f"{source}: duplicate CSV columns")
        required = set(PROVENANCE) | {"elem", "heap", "workload", "n", "ops", "repeat"}
        missing = required - set(columns)
        if missing:
            raise ValueError(f"{source}: missing attributed columns {', '.join(sorted(missing))}")
        rows = 0
        for line, row in enumerate(reader, 2):
            try:
                if None in row or any(value is None for value in row.values()):
                    raise ValueError("malformed CSV row")
                original = profile == "original-rust"
                expected_protocol = ORIGINAL_PROTOCOL if original else protocol["protocol"]
                if any(not row[column] for column in required - {"fixture_digest"}):
                    raise ValueError("empty attributed field")
                if row["protocol"] != expected_protocol or row.get("profile", profile) != profile:
                    raise ValueError("historical or mixed profile/protocol")
                input_policy = "fresh-random-original" if original else "shared-fixture"
                timer = "workload-input-queue-destruction" if original else "workload-only-raii"
                cutoff = row.get("cutoff", "disabled")
                if row["timer"] != timer or row.get("input_policy", input_policy) != input_policy:
                    raise ValueError("timer or input policy differs from selected profile")
                if original and (row.get("profile") != profile or row.get("input_policy") != input_policy):
                    raise ValueError("original results require explicit profile and input policy")
                if (original and cutoff not in ("32", "128")) or (not original and cutoff != "disabled"):
                    raise ValueError("cutoff differs from selected profile")
                language = build_language(row["build"])
                n, ops = integer(row["n"], "n", 1), integer(row["ops"], "ops", 1)
                digest = row["fixture_digest"]
                if original:
                    if row["elem"] not in ("i32", "i64") or row["workload"] not in (
                            "HeapSort", "MonotoneConstantSize", "MonotoneWiggle", "RandomWiggle"):
                        raise ValueError("original result uses inactive type or workload")
                    if n < 2 or n > 33554432 or n & (n - 1) or ops != n * FACTORS[row["workload"]]:
                        raise ValueError("original size or workload normalization differs")
                    if language == "C++" and not digest:
                        raise ValueError("C++ original result lacks trial input digest")
                    if language == "Rust" and digest:
                        raise ValueError("unchanged original Rust exports no trial input digest")
                    if digest and not re.fullmatch(r"fnv1a64:[0-9a-f]{16}", digest):
                        raise ValueError("invalid original trial input digest")
                    if cutoff == "128" and (row["elem"], row["workload"]) != ("i64", "MonotoneConstantSize"):
                        raise ValueError("original table type or workload differs")
                    if language == "C++":
                        trial_key = (row["build"], row["backend"], row["heap"], row["elem"], row["workload"], n)
                        seen = trial_digests.setdefault(trial_key, set())
                        if digest in seen:
                            raise ValueError("C++ original trials reuse identical inputs")
                        seen.add(digest)
                    digest = ""
                else:
                    if not re.fullmatch(r"fnv1a64:[0-9a-f]{16}", digest) or digest not in fixtures:
                        raise ValueError("fixture digest does not match supplied fixture bytes")
                    fixture = fixtures[digest]
                    if any(row[field] != str(fixture[field]) for field in ("elem", "workload", "n", "ops")):
                        raise ValueError("result metadata differs from shared fixture")
                repeats, warmup = integer(row["repeats"], "repeats", 1), integer(row["warmup"], "warmup")
                repeat = integer(row["repeat"], "repeat")
                if repeat >= repeats:
                    raise ValueError("repeat index outside declared schedule")
                if original and (repeats, warmup) != (3, 1):
                    raise ValueError("original profile requires three repeats and one warmup")
                schedule.add((repeats, warmup))
                reservation = row["reservation"]
                allowed_reserves = ("reserve:1048576", "reserve:268435456") if original else ()
                if reservation != "backend-default" and reservation not in allowed_reserves and not (
                        not original and re.fullmatch(r"required-capacity:[1-9][0-9]*", reservation)):
                    raise ValueError("unknown reservation policy")
                if original:
                    heap = row["heap"]
                    expected = ("reserve:1048576" if heap == "std_priority_queue" or
                                "collections::binary_heap::BinaryHeap" in heap else
                                "reserve:268435456" if heap == "original_quickheap" or
                                "original_quickheap::OriginalQuickHeap" in heap else "backend-default")
                    if reservation != expected:
                        raise ValueError("reservation differs from original heap policy")
                identity = Identity(profile, input_policy, cutoff, language, row["build"], row["protocol"], row["backend"], digest,
                                    row["elem"], row["heap"], row["workload"], n, ops,
                                    reservation, row["timer"], repeats, warmup)
                policy_key = (digest, row["heap"])
                policy = (reservation, row["timer"], cutoff)
                if policy_key in policies and policies[policy_key] != policy:
                    raise ValueError("inconsistent reservation or timer policy")
                policies[policy_key] = policy
                measured = ("nanos",) if original else METRICS
                values = {metric: numeric(row[metric], metric) for metric in measured
                          if metric in row and row[metric] and metric != "comparisons"}
                pair = [metric in values for metric in ("push_comparisons", "pop_comparisons")]
                if any(pair) and not all(pair):
                    raise ValueError("comparison counts require both push and pop counts")
                if all(pair):
                    values["comparisons"] = values["push_comparisons"] + values["pop_comparisons"]
                if values.get("nanos") == 0 and len(values) > 1:
                    del values["nanos"]
                if not values:
                    raise ValueError("row has no supported measured metric")
                if "nanos" in values and values["nanos"] == 0:
                    raise ValueError("elapsed nanos must be positive")
                for metric, value in values.items():
                    group = groups.setdefault((identity, metric), {})
                    if repeat in group:
                        raise ValueError("duplicate repeat for build, backend, fixture, and metric")
                    group[repeat] = value
                rows += 1
            except ValueError as error:
                raise ValueError(f"{source}:{line}: {error}") from None
        if not rows:
            raise ValueError(f"{source}: no measured rows")
    if len(schedule) != 1:
        raise ValueError("inconsistent repeats or warmup across input results")
    summaries = []
    for (identity, metric), values in sorted(groups.items()):
        if set(values) != set(range(identity.repeats)):
            raise ValueError(f"{identity.heap}: missing declared repeats for {metric}")
        normalized = metric == "nanos" or "comparisons" in metric
        denominator = identity.ops * math.log2(identity.n) if normalized else 1
        if denominator == 0:
            raise ValueError("opslogn normalization undefined for n=1")
        units = ("ns/(ops log2 n)" if metric == "nanos" else "comparisons/(ops log2 n)") if normalized else metric
        values = [value / denominator for value in values.values()]
        summaries.append(Summary(identity, metric, units, statistics.median(values), min(values), max(values)))
    return summaries, inputs


def latex(value):
    escapes = {"\\": r"\textbackslash{}", "&": r"\&", "%": r"\%", "$": r"\$",
               "#": r"\#", "_": r"\_", "{": r"\{", "}": r"\}",
               "~": r"\textasciitilde{}", "^": r"\textasciicircum{}"}
    return "".join(escapes.get(char, char) for char in str(value))


def markdown(value):
    return str(value).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace("|", "&#124;").replace("\n", " ").replace("`", "&#96;")


def write_reports(summaries, output, historical):
    fields = list(Identity.__dataclass_fields__) + ["build_id", "metric", "units", "median", "min", "max"]
    with (output / "summary.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(fields)
        for summary in summaries:
            identity = summary.identity
            writer.writerow([getattr(identity, field) for field in Identity.__dataclass_fields__] +
                            [identity.build_id, summary.metric, summary.units, summary.median, summary.minimum, summary.maximum])
    heading = ("Language", "Build", "Heap", "Backend", "Fixture", "Workload", "n", "Metric", "Units", "Median", "Min", "Max")
    table = ["| " + " | ".join(heading) + " |", "| " + " | ".join(["---"] * len(heading)) + " |"]
    tex = [r"\documentclass{article}", r"\usepackage[landscape,margin=1cm]{geometry}",
           r"\usepackage{longtable}", r"\begin{document}", r"\section*{Attributed benchmark results}",
           "Medians and observed ranges retain distinct compiler builds and fixtures.",
           r"\scriptsize", r"\begin{longtable}{llllllllllll}", " & ".join(heading) + r" \\", r"\hline"]
    for summary in summaries:
        identity = summary.identity
        row = (identity.language, identity.build_id, identity.heap, identity.backend,
               identity.fixture_digest.split(":")[-1] or "fresh random", identity.workload,
               identity.n, summary.metric, summary.units, f"{summary.median:.9g}", f"{summary.minimum:.9g}", f"{summary.maximum:.9g}")
        table.append("| " + " | ".join(map(markdown, row)) + " |")
        tex.append(" & ".join(map(latex, row)) + r" \\")
    tex.extend([r"\end{longtable}", "Timing and comparison values use ops log2(n) once. Other metrics retain raw units.",
                "Full build, fixture, policy, and unit identity appears in summary.csv and manifest.json.", r"\end{document}"])
    (output / "report.tex").write_text("\n".join(tex) + "\n", encoding="utf-8")
    paper = ["# Attributed benchmark results", "",
             "Rows retain selected profile and distinct language, compiler build, backend, and input-policy groups.",
             "Median and range describe supplied repeats. Results establish no speedup or algorithm equivalence claim.", "",
             "Timing and comparison metrics divide by `ops * log2(n)` once. Other metrics retain raw units.",
             "[Summary CSV](summary.csv) records full group identity and units. [Manifest](manifest.json) attributes copied inputs.", "",
             *table, "", "## Historical inventory", "",
             "[Historical inventory](provenance/historical.json) records source bytes and unknown measurement build provenance.",
             "[Source catalog](provenance/catalog.csv) records exact adapters, substitutes, and tracked gaps.", "",
             "| Omission | Status | Detail |", "| --- | --- | --- |"]
    paper.extend("| " + " | ".join(markdown(item[field]) for field in ("item", "status", "detail")) + " |"
                 for item in historical["omissions"])
    (output / "paper.md").write_text("\n".join(paper) + "\n", encoding="utf-8")


def generate(args):
    base = Path(__file__).resolve().parent
    protocol_path = base.parent / "protocol.json"
    protocol_raw = protocol_path.read_bytes()
    protocol = json.loads(protocol_raw)
    historical_raw = (base / "historical.json").read_bytes()
    historical = json.loads(historical_raw)
    profile = getattr(args, "profile", "controlled-operations")
    fixtures = read_fixtures(args.fixtures) if profile == "controlled-operations" else {}
    summaries, inputs = read_results(args.csv, fixtures, protocol, profile)
    run_metadata = None
    metadata_path = getattr(args, "run_metadata", None)
    if metadata_path:
        raw = metadata_path.read_bytes()
        value = json.loads(raw)
        validate_record(value)
        run_metadata = {"source": str(metadata_path), "sha256": sha256(raw), "value": value}
    archive = []
    if args.archive:
        for asset in historical["assets"]:
            source = args.archive / Path(asset["path"]).name
            raw = source.read_bytes()
            if len(raw) != asset["bytes"] or sha256(raw) != asset["sha256"]:
                raise ValueError(f"{source}: archive bytes differ from pinned historical inventory")
            archive.append((asset, raw))
    output = args.output
    for directory in ("inputs", "fixtures", "provenance"):
        (output / directory).mkdir(parents=True, exist_ok=True)
    manifest = {"schema": "quickheap-report-v1", "profile": profile,
                "protocol": ORIGINAL_PROTOCOL if profile == "original-rust" else protocol["protocol"],
                "normalization": "ops * log2(n), applied once to timing and comparisons",
                "inputs": [], "fixtures": [], "builds": [], "provenance": [], "archive": [],
                "python": sys.version.split()[0], "plotting": None}
    if run_metadata:
        manifest["run_metadata"] = run_metadata
        manifest["fixture_inventory"] = sorted(fixtures)
        (output / "provenance" / "cpu.json").write_bytes(metadata_path.read_bytes())
    for source in inputs:
        target = f"inputs/{source['sha256']}.csv"
        (output / target).write_bytes(source["raw"])
        manifest["inputs"].append({"source": source["source"], "path": target, "sha256": source["sha256"]})
    for digest in sorted({summary.identity.fixture_digest for summary in summaries if summary.identity.fixture_digest}):
        fixture = fixtures[digest]
        target = f"fixtures/{sha256(fixture['raw'])}.qhf"
        (output / target).write_bytes(fixture["raw"])
        manifest["fixtures"].append({"source": fixture["source"], "path": target,
                                     "fixture_digest": digest, "sha256": sha256(fixture["raw"])})
    for language, build in sorted({(summary.identity.language, summary.identity.build) for summary in summaries}):
        manifest["builds"].append({"language": language, "build": build, "build_id": sha256(build.encode())[:16]})
    for name, raw in (("protocol.json", protocol_raw), ("historical.json", historical_raw),
                      ("catalog.csv", (base.parent / "catalog.csv").read_bytes()),
                      ("report.py", Path(__file__).read_bytes()), ("plot.py", (base / "plot.py").read_bytes())):
        target = f"provenance/{name}"
        (output / target).write_bytes(raw)
        manifest["provenance"].append({"path": target, "sha256": sha256(raw)})
    for asset, raw in archive:
        target = "archive/" + Path(asset["path"]).name
        (output / "archive").mkdir(exist_ok=True)
        path = output / target
        if path.exists() and path.read_bytes() != raw:
            raise ValueError(f"{path}: refusing to overwrite different historical bytes")
        if not path.exists():
            path.write_bytes(raw)
        path.chmod(0o444)
        manifest["archive"].append({**asset, "path": target})
    write_reports(summaries, output, historical)
    if args.plot:
        import plot
        import matplotlib
        manifest["plotting"] = {"matplotlib": matplotlib.__version__, "formats": ["svg", "pdf"]}
        for metric in sorted({summary.metric for summary in summaries}):
            plotted = [plot.Summary(plot.Sample("synthetic", summary.identity.elem,
                         f"{summary.identity.language} {summary.identity.heap} [{summary.identity.backend}] {summary.identity.build_id} {summary.identity.fixture_digest}",
                         summary.identity.workload, "", summary.identity.n, summary.units, summary.median),
                         summary.median, summary.minimum, summary.maximum, summary.identity.repeats)
                       for summary in summaries if summary.metric == metric]
            for suffix in ("svg", "pdf"):
                plot.plot(plotted, output / f"{metric}.{suffix}")
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return summaries


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path, nargs="+", help="attributed paired CSV files")
    parser.add_argument("--profile", choices=("original-rust", "controlled-operations"), default="controlled-operations")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--fixtures", type=Path, default=Path(__file__).resolve().parent.parent / "fixtures")
    parser.add_argument("--run-metadata", type=Path, help="kernel-identified P-core metadata")
    parser.add_argument("--plot", action="store_true", help="emit SVG and PDF metric plots with matplotlib")
    parser.add_argument("--archive", type=Path, help="pinned upstream evals/data directory to copy without reinterpretation")
    args = parser.parse_args()
    try:
        summaries = generate(args)
    except (OSError, ValueError, csv.Error, UnicodeError) as error:
        parser.exit(2, f"error: {error}\n")
    print(f"PASS {len(summaries)} attributed groups written to {args.output}")


if __name__ == "__main__":
    main()
